#include "console.h"
#include "AD5693R.h"
#include "delay.h"
#include "LEDMgmt.h"
#include "PWMDIM.h"
#include "INA219.h"
#include "ADC.h"
#include "cfgfile.h"
#include "runtimelogger.h"
#include <math.h>
#include <stdlib.h>

//声明一下函数
float fmaxf(float x,float y);
float fminf(float x,float y);
float PIDThermalControl(ADCOutTypeDef *ADCResult);//PID温控
void FillThermalFilterBuf(ADCOutTypeDef *ADCResult);//填充温度stepdown的缓冲区

//内部变量
static bool LastAdjustDirection; //存储上一次调节的方向
static float LEDVfFilterBuf[12];
static float LEDIfFilter[14];
static float CurrentSynthRatio=100;  //电流合成比例
static char ShortCount=0;
static bool IsDisableMoon; //是否关闭月光档
bool IsDisableBattCheck; //是否关闭电池质量检测
unsigned char PSUState=0x00; //辅助电源的状态

//外部变量
extern bool IsMoonDimmingLocked; //反馈给无极调光模块，告诉调光模块电流是否调节完毕
extern bool IsRampAdjusting; //外部调光是否在调节
extern float UnLoadBattVoltage; //没有负载时的电池电压
extern float LEDVfMin;
extern float LEDVfMax; //LEDVf限制

//外部调光参数
#if (HardwareMinorVer == 2)
//补偿硬件电流DAC调光偏差的offset (Ver 1.2) 0.05907 for 30A
const float DimmingCompTable[]=
{
0.1,1.65,4.95,9.9,19.8,
2.1,2.02,1.29,1.1966,1.05907
};

#else
const float DimmingCompTable[]=

{
0.1,1.65,4.95,9.9,19.8,
1.007,1.005,1.004,1.003,1.002
};
#endif


//字符串
const char *SPSFailure="SPS %sreports %s during init.";
const char *DidnotStr="did Not ";

//LED短路,温度检测和电流合成环检测部分的简易数字滤波器（避免PWM调光时某一瞬间的波动）
float LEDFilter(float DIN,float *BufIN,int bufsize)
{
 int i;
 float buf,min,max;
 //搬数据
 for(i=(bufsize-1);i>0;i--)BufIN[i]=BufIN[i-1];
 BufIN[i]=DIN;
 //找最高和最低的同时累加数据
 min=2000;
 max=-2000;
 buf=0;
 for(i=0;i<bufsize;i++)
	{
	buf+=BufIN[i];//累加数据
	min=fminf(min,BufIN[i]);
	max=fmaxf(max,BufIN[i]); //取最小和最大
	}
 buf-=(min+max);
 buf/=(float)(bufsize-2);//去掉最高和最低值，取剩下的8个值里面的平均值
 return buf;
}

//线性调光模块相关的电路的上电自校准程序
void LinearDIM_POR(void)
{
 DACInitStrDef DACInitStr;
 ADCOutTypeDef ADCO;	
 float VSet,VGet;
 int i,retry;
 bool IMONOKFlag=false;
 /**********************************************************************
 自检过程中的第1步：我们首先需要配置好AD5693R DAC,将DAC内置基准打开并转
 换为工作模式。
 ***********************************************************************/
 UartPost(Msg_info,"LineDIM","Checking Hybrid dim circult...");
 DACInitStr.DACPState=DAC_Normal_Mode;
 DACInitStr.DACRange=DAC_Output_REF;
 DACInitStr.IsOnchipRefEnabled=true; 
 if(!AD5693R_SetChipConfig(&DACInitStr))
  { 
 	UartPost(Msg_critical,"LineDIM","Failed to init DAC.");
	CurrentLEDIndex=20;//DAC无法启动,保护
	SelfTestErrorHandler(); 
	}
 /**********************************************************************
 自检过程中的第2步：首先需要确保辅助电源处于关闭状态,然后我们将DAC输出的
 PWM Mux设置为常通(100%占空比)，然后通过电流反馈MUX将调光信号反馈给电流
 检测ADC。然后通过ADC测量DAC的输出.这一步是为了验证DAC的实际电压输出值和
 设置值是一致的
 ***********************************************************************/
 SetAUXPWR(false);
 SetPWMDuty(100.0);
 delay_ms(2);
 i=1;
 for(VSet=0.5;VSet<=2.5;VSet+=0.5)
	{
	AD5693R_SetOutput(VSet);
	if(!ADC_GetLEDIfPinVoltage(&VGet))OnChipADC_FaultHandler();//ADC寮傚父
	if(VGet<(VSet-0.05)||VGet>(VSet+0.05))
	  {
		UartPost(Msg_critical,"LineDIM","Expected DAC out %.2fV but get %.2fV.",VSet,VGet);
	  CurrentLEDIndex=30;//电压误差过大,说明PWM MUX或者电流反馈MUX相关的电流反馈电路有问题。
	  SelfTestErrorHandler();  		
		}
	i++;//成功完成校准的次数+1
	}
  /**********************************************************************
 自检过程中的第3步：我们将DAC输出的PWM Mux设置为常闭(0%占空比)然后令DAC输
 出一个1.25V的电压并通过ADC测量DAC的输出.这一步是为了测试PWM调光的MUX和
 PWM调光电路能否正常运行并根据PWM信号切断LED电流控制输入.
 ***********************************************************************/
 SetPWMDuty(0);
 AD5693R_SetOutput(1.25);
 if(!ADC_GetLEDIfPinVoltage(&VGet))OnChipADC_FaultHandler();//令ADC采样电压
 if(VGet>=0.05)
    {
	  //有超过0.05的电压,说明PWM MUX无法切断电压.PWM相关电路有问题
		UartPost(Msg_critical,"LineDIM","PWM Dimming MUX Error.");
	  CurrentLEDIndex=30;
	  SelfTestErrorHandler();  		
		}
  /**********************************************************************
 自检过程中的第4步：我们将DAC输出的PWM Mux设置为常开(100%占空比)然后令DAC
 输出0V的电压并通过ADC测量DAC的输出.这一步是为了测试DAC的输出能否正确的归
 零确保线性调光的准确性.
 ***********************************************************************/
 AD5693R_SetOutput(0);
 SetPWMDuty(100);		
 if(!ADC_GetLEDIfPinVoltage(&VGet))OnChipADC_FaultHandler();//令ADC采样电压
 if(VGet>=0.05)
	  {
		//有超过0.05的电压,说明DAC有问题输出无法归零
		UartPost(Msg_critical,"LineDIM","DAC output zero failure.");
	  CurrentLEDIndex=20;
	  SelfTestErrorHandler();  		
		}
  /**********************************************************************
 自检过程中的最后一步：我们打开主buck输出小电流检查电流反馈电路，温度传感
 器(MOSFET)是否运行正常。
 ***********************************************************************/
 VSet=0.01;
 SetPWMDuty(100);
 AD5693R_SetOutput(VSet); //将DAC设置为初始输出，100%占空比
 SetAUXPWR(true); 
 retry=0;//清零等待标志位
 for(i=0;i<200;i++)//等待辅助电源上电稳定检测电压
		{
	  delay_us(100);
		if(!ADC_GetResult(&ADCO))OnChipADC_FaultHandler();//让ADC获取信息
		if(ADCO.SPSTMONState==SPS_TMON_OK)
		   {
		   if(ADCO.LEDVf>=LEDVfMax)retry++;
		   else if(ADCO.LEDIf>0.35)//电流检测正常，累加数值
			   {
				 IMONOKFlag=true;
			   retry++; 
				 }
		   else 
			   {				 
				 if(VSet<0.1)VSet+=0.01;  
			   AD5693R_SetOutput(VSet); //设置DAC输出提高电压直到检测通过
			   retry=0;
				 }
	     }
		else retry=0;
		if(retry==5)break;
		}
 if(i==200)
   {	 
	 //SPS没有正确返回温度信号或者直接将温度信号拉高到3.3V,或者电流检测电流没有信号输入.这说明SPS或者辅助电源出现灾难性异常,需要锁死。
	 CurrentLEDIndex=24;
	 if(ADCO.SPSTMONState==SPS_TMON_CriticalFault)
		  UartPost(Msg_critical,"LineDIM",(char *)SPSFailure,"","CATERR");
	 else if(ADCO.SPSTMONState!=SPS_TMON_OK)
	    UartPost(Msg_critical,"LineDIM",(char *)SPSFailure,DidnotStr,"TMON");	 	 
	 else if(!IMONOKFlag)
		  UartPost(Msg_critical,"LineDIM",(char *)SPSFailure,DidnotStr,"IMON");
	 //严重错误,死循环使驱动锁定
	 SetAUXPWR(false);
	 SelfTestErrorHandler();  
	 }
 if(ADCO.SPSTemp>85)IsDisableMoon=true; //MOSFET温度大于85度此时月光档会出现问题因此需要限制使用
 else IsDisableMoon=false;
 /**********************************************************************
 自检成功完成,此时我们可以让辅助电源下电了。然后我们将DAC输出重置为0V并且
 让PWM模块输出0%占空比使得不会有意外的电压信号进入主Buck的控制脚使得buck
 在我们不想要的情况下意外启动。 
 ***********************************************************************/
 SetAUXPWR(false);
 delay_ms(1);
 AD5693R_SetOutput(0);
 SetPWMDuty(0);
 /***********************************************************************
 最后，我们还需要从运行日志里面取出月光档的额定PWM占空比。
 ***********************************************************************/
 SysPstatebuf.Duty=RunLogEntry.Data.DataSec.MoonPWMDuty;  //加载占空比
 LastAdjustDirection=true; //初始调节方向设置为反向
 IsDisableBattCheck=false; //默认开启电池质量检测
 if(SysPstatebuf.Duty>100||SysPstatebuf.Duty<1) //占空比非法值，启动修正处理
  {
	SysPstatebuf.Duty=40;
	RunLogEntry.Data.DataSec.MoonPWMDuty=40; //修正错误的占空比设置
	RunLogEntry.Data.DataSec.MoonCurrent=0; //占空比设置已经损坏，需要重新学习月光档的电流设置
	}
}
//从开灯状态切换到关灯状态的逻辑
void TurnLightOFFLogic(void)
 {
 TimerHasStarted=false;
 IsDisableBattCheck=false; //每次关机都要复位电池检测禁用位
 ShortCount=0;//复位短路次数统计
 DisableFlashTimer();//关闭闪烁定时器
 SetPWMDuty(0);
 AD5693R_SetOutput(0); //将DAC输出和PWM调光模块都关闭
 INA219_SetConvMode(INA219_PowerDown,INA219ADDR);//关闭INA219功率计
 SetAUXPWR(false);//切断3.3V辅助电源
 CurrentLEDIndex=0;
 PSUState=0x00;//关闭主电源控制的定时器
 LED_Reset();//复位LED管理器
 RunLogEntry.CurrentDataCRC=CalcRunLogCRC32(&RunLogEntry.Data); //计算新的CRC-32
 } 
//放在系统延时里面进行积分的函数
void LEDShortCounter(void)
 {
 //常亮档的短路检测
 if(!SysPstatebuf.IsLEDShorted)ShortCount=0;//短路没发生
 else if(ShortCount==3)
    {
		SysPstatebuf.IsLEDShorted=false;//复位标志位
		ShortCount=0;
		//LED短路超过0.375秒,这是严重故障,立即写log并停止驱动运行
	  RunTimeErrorReportHandler(Error_LED_Short);
		return;
	  }
 else ShortCount++;//时间没到
 }

//从关灯状态转换到开灯状态,上电自检的逻辑
SystemErrorCodeDef TurnLightONLogic(INADoutSreDef *BattOutput)
 {
 ADCOutTypeDef ADCO;
 bool Result[2];
 ModeConfStr *CurrentMode;
 float VID,detectOKCurrent,VIDIncValue;
 int i,retry; 
 /********************************************************
 我们首先需要检查传进来的模式组是否有效。
 ********************************************************/
 CurrentMode=GetCurrentModeConfig();
 if(CurrentMode==NULL)return Error_Mode_Logic; //挡位逻辑异常
 detectOKCurrent=CurrentMode->LEDCurrentHigh;
 if(detectOKCurrent>0.5)detectOKCurrent=0.5;
 /********************************************************
 首先我们需要将负责电池遥测的INA219功率级启动,然后先将DAC
 输出设置为0,PWM调光模块设置为100%,接着就可以送辅助电源然
 后等待200ms直到辅助电源启动完毕
 ********************************************************/
 BattOutput->TargetSensorADDR=INA219ADDR;
 if(!INA219_SetConvMode(INA219_Cont_Both,INA219ADDR))
	 return Error_ADC_Logic;//设置INA219遥测的器件地址,然后让INA219进入工作模式
 if(!AD5693R_SetOutput(0))
	 return Error_DAC_Logic;//将DAC输出设置为0V,确保送主电源时主Buck变换器不工作
 delay_ms(1);
 SetPWMDuty(100);//自检过程中我们使用线性调光,因此PWM占空比设置为100%
 SetAUXPWR(true);
 retry=0;//清零等待标志位
 for(i=0;i<2000;i++)//等待辅助电源上电稳定
		{
	  delay_us(50);
		Result[1]=ADC_GetResult(&ADCO);
		if(!Result[1])//让ADC获取信息
			return Error_ADC_Logic;
		if(ADCO.SPSTMONState==SPS_TMON_OK)
			retry++; //SPS正常运行，结果++
		else 
			retry=0; //SPS运行不正常
		if(retry==StartupAUXPSUPGCount)break;
		}
 if(i==2000)return Error_PWM_Logic;//启动超时		
 /********************************************************
 系统主电源启动完毕,开始通过片内ADC和INA219初步获取电池电压
 电流和LED以及MOS的温度,确保关键的传感器工作正常且电池没有
 过压或欠压
 ********************************************************/
 Result[0]=INA219_GetBusInformation(BattOutput);
 Result[1]=ADC_GetResult(&ADCO);
 if(!Result[0]||!Result[1])return Error_ADC_Logic;//ADC或INA219无法正确的获取数据,报错
 //智能功率级温度传感器断线和致命错误,以及智能功率级严重过热警告
 if(ADCO.SPSTMONState!=SPS_TMON_OK)
	 return ADCO.SPSTMONState==SPS_TMON_Disconnect?Error_SPS_TMON_Offline:Error_SPS_CATERR;
 if(ADCO.SPSTMONState==SPS_TMON_OK&&ADCO.SPSTemp>CfgFile.MOSFETThermalTripTemp)
	 return Error_SPS_ThermTrip;
 //检测LED基板温度,如果超过热跳闸温度则保护。同时检测SPS和电池温度来决定是否启用电池质量检测
 if(ADCO.NTCState==LED_NTC_OK)
   {
	 //LED过热 
	 if(ADCO.LEDTemp>CfgFile.LEDThermalTripTemp)return Error_LED_ThermTrip;
	 //在低温下电池放电性能会锐减，因此我们需要在温度低于10度的时候关闭电池质量检测
	 IsDisableBattCheck=fminf(ADCO.LEDTemp,ADCO.SPSTemp)<10?true:false;
	 }
 //检查INA219读取到的电池电压确保电压合适
 if(BattOutput->BusVolt<=CfgFile.VoltageTrip||BattOutput->BusVolt>CfgFile.VoltageOverTrip) 
	 return BattOutput->BusVolt>CfgFile.VoltageOverTrip?Error_Input_OVP:Error_Input_UVP;
 UnLoadBattVoltage=BattOutput->BusVolt;//存储空载时的电池电压
 /********************************************************
 电流协商开始,此时系统将会逐步增加DAC的输出值使得电流缓慢
 爬升到0.5A的小电流.系统将会在协商结束后检测LED的If和Vf和
 DAC的VID判断是否短路
 ********************************************************/
 VID=StartUpInitialVID;
 VIDIncValue=0.5;
 while(VID<60)
		 {
		 if(!AD5693R_SetOutput(VID/(float)1000))return Error_DAC_Logic;
		 ADC_GetResult(&ADCO);
		 if(ADCO.LEDIf>=detectOKCurrent)break; //电流足够，退出
		 VID+=VIDIncValue; //继续增加VID
		 VIDIncValue+=StartupLEDVIDStep; //每次VID增加的数值
		 }
 SysPstatebuf.CurrentDACVID=VID;
 /********************************************************
 电流协商已经完成,此时系统将会检测LED的Vf来确认LED情况,确保
 LED没有发生短路和开路.
 ********************************************************/
 if(!ADC_GetResult(&ADCO))return Error_ADC_Logic;
 //DAC的输出电压已经到了最高允许值,但是电流仍然未达标,这意味着LED可能短路或PWM逻辑异常
 if(VID>=60)return (ADCO.LEDVf>=LEDVfMax)?Error_LED_Open:Error_PWM_Logic;
 if(ADCO.LEDVf<LEDVfMin)return Error_LED_Short;		 //LEDVf过低,LED可能短路
 /********************************************************
 LED自检顺利结束,驱动硬件和负载工作正常,此时返回无错误代码
 交由驱动的其余逻辑完成处理
 ********************************************************/
 SysPstatebuf.IsLEDShorted=false;
 SysPstatebuf.ToggledFlash=true;// LED没有短路，上电点亮
 FillThermalFilterBuf(&ADCO); //填写温度信息
 for(i=0;i<12;i++)LEDVfFilterBuf[i]=ADCO.LEDVf;//填写LEDVf 
 for(i=0;i<14;i++)LEDIfFilter[i]=ADCO.LEDIf;
 CurrentSynthRatio=100; //默认合成比例设置为100%
 if(CurrentMode->LEDCurrentHigh<1)//电流小于1A设置DAC输出电压为0.08进入PWM调光
   {
	 SetPWMDuty(SysPstatebuf.Duty); //设置占空比
	 AD5693R_SetOutput(0.08);
   } 
 return Error_None;
 }
/*
手电筒正常运行时根据挡位处理电流控制、过流保护、温控降档功
能和温度保护的函数
*/
void RuntimeModeCurrentHandler(void)
 {
 ModeConfStr *CurrentMode;
 ADCOutTypeDef ADCO;
 bool IsCurrentControlledByMode,CurrentAdjDirection;
 volatile bool IsMainLEDEnabled,IsNeedToEnableBuck;
 float Current,Throttle,DACVID,delta,corrvaule;
 /********************************************************
 运行时挡位处理的第一步.首先获取当前的挡位，然后我们使能ADC
 的转换负责获取LED的电流,温度和驱动功率管的温度,接下来就是
 根据我们读取到的温度和电流执行各种保护
 ********************************************************/
 IsMainLEDEnabled=SysPstatebuf.ToggledFlash; //读取当前主LED的控制位
 CurrentMode=GetCurrentModeConfig();
 if(!ADC_GetResult(&ADCO))//令ADC得到温度和电流
     {
		 //ADC转换失败,这是严重故障,立即写log并停止驱动运行
		 RunTimeErrorReportHandler(Error_ADC_Logic);
		 return;
	   }	
 if(ADCO.SPSTMONState==SPS_TMON_OK&&ADCO.SPSTemp>CfgFile.MOSFETThermalTripTemp)
	   {		 
		 //MOS温度到达过热跳闸点,这是严重故障,立即写log并停止驱动运行
	   RunTimeErrorReportHandler(Error_SPS_ThermTrip);
		 return;
	   }
 if(ADCO.NTCState==LED_NTC_OK&&ADCO.LEDTemp>CfgFile.LEDThermalTripTemp)
	 	 {
		 //LED温度到达过热跳闸点,这是严重故障,立即写log并停止驱动运行
		 RunTimeErrorReportHandler(Error_LED_ThermTrip);
		 return;
	   }
 if(ADCO.LEDIf>1.2*FusedMaxCurrent)
     {
		 //在LED启用时，LED电流超过允许值(熔断电流限制的1.2倍),这是严重故障,立即写log并停止驱动运行
		 RunTimeErrorReportHandler(Error_LED_OverCurrent);
		 return;
	   }	 
 if(ADCO.LEDVf>LEDVfMax)
     {
		 //LED的两端电压过高(LED开路),这是严重故障,立即写log并停止驱动运行
	   RunTimeErrorReportHandler(Error_LED_Open);
		 return;
	   }
 /********************************************************
 运行时挡位处理的第二步.我们根据用户配置的PID温控参数和当前
 ADC读取到的各组件温度计算出降档的幅度，并且最终汇总为一个
 降档系数用于限制电流
 ********************************************************/
 Throttle=PIDThermalControl(&ADCO);//执行PID温控
 if(Throttle>100)Throttle=100;
 if(Throttle<5)Throttle=5;//温度降档值限幅
 if(!CurrentMode->IsModeAffectedByStepDown)SysPstatebuf.CurrentThrottleLevel=0;
 else SysPstatebuf.CurrentThrottleLevel=(float)100-Throttle;//将最后的降档系数记录下来用于事件日志使用
 /********************************************************
 运行时挡位处理的第三步.我们需要从当前用户选择的挡位设置
 里面取出电流配置，然后根据用户设置(是否受降档影响)以及特
 殊挡位功能的要求(比如说呼吸功能)对电流配置进行处理最后生成
 实际使用的电流
 ********************************************************/
 Current=CurrentMode->LEDCurrentHigh;//取出电流设置
 if(CurrentMode->IsModeAffectedByStepDown)
	 Current*=Throttle/(float)100;//该挡位受降档影响，应用算出来的温控降档设置
 /* 判断算出的电流是否受控于特殊模式的操作*/
 if(CurrentMode->Mode==LightMode_Breath)IsCurrentControlledByMode=true;
 else if(CurrentMode->Mode==LightMode_Ramp)IsCurrentControlledByMode=true;
 else if(CurrentMode->Mode==LightMode_CustomFlash)IsCurrentControlledByMode=true;
 else IsCurrentControlledByMode=false;
 if(IsCurrentControlledByMode&&Current>BreathCurrent)Current=BreathCurrent;//最高电流和对应模式的计算值同步
 if(Current<0)Current=0;
 if(Current>FusedMaxCurrent)Current=FusedMaxCurrent;//限制算出的电流值,最小=0,最大为熔断限制值
 if(RunLogEntry.Data.DataSec.IsLowVoltageAlert&&Current>LVAlertCurrentLimit)
	 Current=LVAlertCurrentLimit;//当低电压告警发生时限制输出电流 
 if(RunLogEntry.Data.DataSec.IsLowQualityBattAlert)
   Current=(CurrentMode->LEDCurrentHigh>(0.5*FusedMaxCurrent)) ?Current*0.6:Current;  //电池质量太次，限制电流
 /* 如果当前的MOS管温度大于80度，月光档会出现问题（没有输出）
 所以需要限制最小电流，温度下去后才能恢复月光档的使用*/
 if(ADCO.SPSTMONState==SPS_TMON_OK) //检测MOS温度来控制是否允许月光档
   {
   if(ADCO.SPSTemp>80)IsDisableMoon=true; //MOSFET温度过高关闭月光档
	 else if(ADCO.SPSTemp<60)IsDisableMoon=false; //恢复月光档的使用
	 }
 if(IsDisableMoon&&Current>0&&Current<2.1)Current=2.1;//MOSFET温度过高，限制月光档的使用
 /* 存储处理之后的目标电流 */
 SysPstatebuf.TargetCurrent=Current;//存储下目标设置的电流给短路保护模块用
 /********************************************************
 运行时挡位处理的第四步,如果目前LED电流为0，为了避免红色LED
 鬼火，程序会把LED的主buck相关的电路关闭。
 ********************************************************/
 if(Current==0||!IsMainLEDEnabled)	 
	 PSUState|=0x80; //额定LED电流为0或者toggle模式需要关闭主灯,启动计时器
 else
	 PSUState=0x00; //电源开启 
 SetAUXPWR((PSUState==(0x80|MainBuckOffTimeOut))?false:true); //控制辅助电源引脚
 /********************************************************
 运行时挡位处理的第五步,将计算出的电流通过PWM调光和DAC线
 性调光混合的方式把LED调节到目标的电流实现挡位控制（这是对
 于其他挡位来说的）
 ********************************************************/
 if(Current==0)//LED关闭
   {
	 SysPstatebuf.IsLinearDim=true;
	 SetPWMDuty(0); 
	 DACVID=0; //令DAC的输出和PWM占空比都为0使主Buck停止输出
	 }
 //小于等于1A电流,因为电流环路无法在小于此点的条件下稳定因此使用PWM方式调光(仅非呼吸模式)
 else if(Current<=1.00&&CurrentMode->Mode!=LightMode_Breath)
   {
	 if(Current<MinimumLEDCurrent)Current=MinimumLEDCurrent;//最小电流限制
   if(!ADC_GetResult(&ADCO))//令ADC得到电流
       {
			 //ADC转换失败,这是严重故障,立即写log并停止驱动运行
		   RunTimeErrorReportHandler(Error_ADC_Logic);
		   return;
	     }
   if(!IsMainLEDEnabled)//特殊功能控制量请求关闭LED,不执行电流控制输出	
	   SetPWMDuty(0);		 
	 else //正常求解
		   {
		   //当前月光档电流未锁相,对占空比进行控制以达到目标电流
       if(Current!=RunLogEntry.Data.DataSec.MoonCurrent)
			   {
				 delta=Current-LEDFilter(ADCO.LEDIf,LEDIfFilter,14); //求误差
			   if(fabsf(delta)>0.75)corrvaule=0.5;//电流误差严重过高，开始大幅度修正
				 else if(fabsf(delta)>0.3)corrvaule=0.01;//电流误差过大，开始大幅度修正				 
				 else if(fabsf(delta)>0.03)corrvaule=0.005;//电流误差未到额定值，继续小幅度修正
				 else //电流到误差范围，锁定
				   {
					 corrvaule=0; //不需要修正
					 RunLogEntry.Data.DataSec.MoonCurrent=Current;
			     if(SysPstatebuf.Duty>100)SysPstatebuf.Duty=100;
			     else if(SysPstatebuf.Duty<1)SysPstatebuf.Duty=1; //PWM占空比限幅
				   RunLogEntry.Data.DataSec.MoonPWMDuty=SysPstatebuf.Duty; //存下已锁相的占空比和电流，关闭电流环路
					 } 
				 //如果是正在无极调光则占空比调整范围加大50倍来提高调节速率
				 if(IsRampAdjusting)
				    {
						CurrentAdjDirection=(delta>=0)?true:false; //计算出当前的调节方向
						corrvaule*=(float)50; //默认在无极调光模式下调节倍率为50倍
				    if(LastAdjustDirection!=CurrentAdjDirection)corrvaule/=(float)5; //为了避免震荡，如果上一次的调节方向和本次不同则采用较小的调节倍率
						LastAdjustDirection=CurrentAdjDirection;//存储上一次的调节方向
						}
				 //根据电流误差修正占空比并对占空比进行限幅
				 SysPstatebuf.Duty+=(delta>=0)?corrvaule:-corrvaule; //电流过大减小占空比,电流过小增大占空比		 
				 if(SysPstatebuf.Duty>100)SysPstatebuf.Duty=100;
			   if(SysPstatebuf.Duty<1)SysPstatebuf.Duty=1; //占空比限幅
				 }
			 else IsMoonDimmingLocked=true; //月光档已经锁相
			 //占空比应用
			 SetPWMDuty(SysPstatebuf.Duty); //设置占空比
			 }
	 DACVID=0.08;  //80mV
	 SysPstatebuf.IsLinearDim=false;
	 }
 //大于1A电流,使用纯线性调光实现无频闪
 else
	 {
	 IsMoonDimmingLocked=true; //线性调光不需要锁相，直接置true
   DACVID=Current*30;  //计算DAC的VID,公式为:VID=(30mV*offset*LEDIf(A))+23mV 
	 DACVID/=QueueLinearTable(5,Current,(float *)&DimmingCompTable[0],(float *)&DimmingCompTable[5]);//除以补偿系数得到补偿后的VID
	 if(CurrentMode->Mode!=LightMode_On&&CurrentMode->Mode!=LightMode_Ramp)CurrentSynthRatio=100;//不是常亮和无极调光模式，不使用电流补偿
	 else if(fabsf(Current-ADCO.LEDIf)>0.02) 
	   { 
	   if(Current>ADCO.LEDIf)CurrentSynthRatio=CurrentSynthRatio<110?CurrentSynthRatio+0.1:110; 
	   else CurrentSynthRatio=CurrentSynthRatio>50?CurrentSynthRatio-0.1:50;
		 }
	 DACVID+=23;//加上23mV的offset 
	 DACVID*=(CurrentSynthRatio/(float)100); //乘上自适应电流补偿系数
	 DACVID/=1000;//mV转换为V
	 SysPstatebuf.IsLinearDim=true;
	 SetPWMDuty((IsMainLEDEnabled==false)?0:100); //纯线性调光,PWM占空比仅受特殊功能控制量在0-100之间转换
	 }
 /*  根据目标的DAC VID(电压值) 发送指令控制DAC输出  */
 SysPstatebuf.CurrentDACVID=DACVID*1000;//将当前DAC的VID记录下来用于事件日志
 if(!AD5693R_SetOutput(DACVID))
   {
	 //DAC对于发送的指令没有回应无法设置输出,这是严重故障,立即写log并停止驱动运行
	 RunTimeErrorReportHandler(Error_DAC_Logic);
	 return;
	 }
 TimerCanTrigger=true;//允许GPTM1定时器执行特殊功能
 /***********************************************************
 最后一步，系统采样LED的Vf加入数字滤波器中用于判断是否发生短
 路
 ***********************************************************/
 if(!ADC_GetResult(&ADCO))//令ADC得到温度和电流
     {
		 //ADC转换失败,这是严重故障,立即写log并停止驱动运行
		 RunTimeErrorReportHandler(Error_ADC_Logic);
		 return;
	   }
 if(IsMainLEDEnabled&&Current>0)//额定电流大于0且LED被启动才积分
     {
		 if(ADCO.LEDIf<MinimumLEDCurrent)SysPstatebuf.IsLEDShorted=false;//当前LED没有电流，禁止检测	 
     else if(LEDFilter(ADCO.LEDVf,LEDVfFilterBuf,12)>LEDVfMin)SysPstatebuf.IsLEDShorted=false; //LEDVf大于短路保护值，正常运行
     else SysPstatebuf.IsLEDShorted=true;
     }
 }
