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
static float LEDVfFilterBuf[12];
static float LEDIfFilter[14];
static float CurrentSynthRatio=100;  //电流合成比例
static float LED_Current_Integral;
static float LED_Current_Last_Error;
static char ShortCount=0;
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
1.005,1.005,1.005,1.005,1.005
};
#endif


//字符串
const char *SPSFailure="SPS Reports Invalid %s signal during init.";

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
 UartPost(Msg_info,"LineDIM","Checking dimming/DAC circult functionality...");
 DACInitStr.DACPState=DAC_Normal_Mode;
 DACInitStr.DACRange=DAC_Output_REF;
 DACInitStr.IsOnchipRefEnabled=true; 
 if(!AD5693R_SetChipConfig(&DACInitStr))
  { 
 	UartPost(Msg_critical,"LineDIM","Failed to push config into DAC for initialization.");
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
		UartPost(Msg_critical,"LineDIM","DAC calibration failure,Expected %.2fV but get %.2fV.",VSet,VGet);
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
		UartPost(Msg_critical,"LineDIM","PWM Dimming MUX failure detected,expected near 0V but get %.2fV.",VGet);
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
		UartPost(Msg_critical,"LineDIM","DAC Failed to reset into zero scale,near 0V but get %.2fV.",VGet);
	  CurrentLEDIndex=20;
	  SelfTestErrorHandler();  		
		}
  /**********************************************************************
 自检过程中的最后一步：我们将DAC输出的PWM Mux设置为常闭(0%占空比)确保主
 buck不会意外输出电流到LED,然后我们让辅助电源上电,测量智能功率级是否正确
 的输出用于指示温度的电压信号确保温度传感器正常运行.
 ***********************************************************************/
 VSet=0.01;
 SetPWMDuty(100);
 AD5693R_SetOutput(VSet); //将DAC设置为初始输出，100%占空比
 SetAUXPWR(true); 
 retry=0;//清零等待标志位
 for(i=0;i<200;i++)//等待辅助电源上电稳定检测电压
		{
	  delay_ms(1);
		if(!ADC_GetResult(&ADCO))OnChipADC_FaultHandler();//让ADC获取信息
		if(ADCO.SPSTMONState==SPS_TMON_OK)
		   {
		   if(ADCO.LEDVf>=LEDVfMax)retry++;
		   else if(ADCO.LEDIf<0.35)
			   {
				 if(VSet<0.1)VSet+=0.01;  
			   AD5693R_SetOutput(VSet); //设置DAC输出提高电压直到检测通过
			   retry=0;
				 }
		   else //电流检测正常，累加数值
			   {
				 IMONOKFlag=true;
			   retry++; 
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
		  UartPost(Msg_critical,"LineDIM",(char *)SPSFailure,"CATERR");
	 else if(ADCO.SPSTMONState!=SPS_TMON_OK)
	    UartPost(Msg_critical,"LineDIM",(char *)SPSFailure,"TMON");	 	 
	 else if(!IMONOKFlag)
		  UartPost(Msg_critical,"LineDIM",(char *)SPSFailure,"IMON");
	 //严重错误,死循环使驱动锁定
	 SetAUXPWR(false);
	 SelfTestErrorHandler();  
	 }
 /**********************************************************************
 自检成功完成,此时我们可以让辅助电源下电了。然后我们将DAC输出重置为0V并且
 让PWM模块输出0%占空比使得不会有意外的电压信号进入主Buck的控制脚使得buck
 在我们不想要的情况下意外启动。 
 ***********************************************************************/
 SetAUXPWR(false);
 delay_ms(1);
 AD5693R_SetOutput(0);
 SetPWMDuty(0);
 LED_Current_Integral=0;
 LED_Current_Last_Error=0; //初始化PID
}
//从开灯状态切换到关灯状态的逻辑
void TurnLightOFFLogic(void)
 {
 TimerHasStarted=false;
 ShortCount=0;//复位短路次数统计
 DisableFlashTimer();//关闭闪烁定时器
 SetPWMDuty(0);
 AD5693R_SetOutput(0); //将DAC输出和PWM调光模块都关闭
 INA219_SetConvMode(INA219_PowerDown,INA219ADDR);//关闭INA219功率计
 SetAUXPWR(false);//切断3.3V辅助电源
 CurrentLEDIndex=0;
 LED_Reset();//复位LED管理器
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
 float VID,detectOKCurrent;
 int i,retry; 
 /********************************************************
 我们首先需要检查传进来的模式组是否有效。
 ********************************************************/
 CurrentMode=GetCurrentModeConfig();
 if(CurrentMode==NULL)return Error_Mode_Logic; //挡位逻辑异常
 detectOKCurrent=CurrentMode->LEDCurrentHigh;
 if(CurrentMode->LEDCurrentHigh>=1)SysPstatebuf.Duty=100;
 else SysPstatebuf.Duty=35;  //设置初始占空比
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
 delay_ms(1);
 SetAUXPWR(true);
 retry=0;//清零等待标志位
 for(i=0;i<2000;i++)//等待辅助电源上电稳定
		{
	  delay_ms(1);
		Result[1]=ADC_GetResult(&ADCO);
		if(!Result[1])
			return Error_ADC_Logic;//让ADC获取信息
		if(ADCO.SPSTMONState==SPS_TMON_OK)
			retry++; //SPS正常运行，结果++
		else 
			retry=0; //SPS运行不正常
		if(retry==5)break;
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
 //检测LED基板温度,如果超过热跳闸温度则保护
 if(ADCO.NTCState==LED_NTC_OK&&ADCO.LEDTemp>CfgFile.LEDThermalTripTemp)
	 return Error_LED_ThermTrip;
 //检查INA219读取到的电池电压确保电压合适
 if(BattOutput->BusVolt<=CfgFile.VoltageTrip||BattOutput->BusVolt>CfgFile.VoltageOverTrip) 
	 return BattOutput->BusVolt>CfgFile.VoltageOverTrip?Error_Input_OVP:Error_Input_UVP;
 /********************************************************
 电流协商开始,此时系统将会逐步增加DAC的输出值使得电流缓慢
 爬升到0.5A的小电流.系统将会在协商结束后检测LED的If和Vf和
 DAC的VID判断是否短路
 ********************************************************/
 VID=9.5;
 while(VID<100)
		 {
		 if(!AD5693R_SetOutput(VID/(float)1000))return Error_DAC_Logic;
		 ADC_GetResult(&ADCO);
		 if(ADCO.LEDIf>=detectOKCurrent)break; //电流足够，退出
		 VID+=0.5; //继续增加VID
		 }
 SysPstatebuf.CurrentDACVID=VID;
 /********************************************************
 电流协商已经完成,此时系统将会检测LED的Vf来确认LED情况,确保
 LED没有发生短路和开路.
 ********************************************************/
 if(!ADC_GetResult(&ADCO))return Error_ADC_Logic;
 //DAC的输出电压已经到了最高允许值,但是电流仍然未达标,这意味着LED可能短路或PWM逻辑异常
 if(VID==100)return  ADCO.LEDVf>=LEDVfMax?Error_LED_Open:Error_PWM_Logic;
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
 SetPWMDuty(SysPstatebuf.Duty); //设置占空比
 CurrentSynthRatio=100; //默认合成比例设置为100%
 LED_Current_Last_Error=0; //上次error清零
 if(LED_Current_Integral==0)LED_Current_Integral=-40; //默认设置为-30的积分值
 if(CurrentMode->LEDCurrentHigh<1)AD5693R_SetOutput(0.08); //电流小于1A设置DAC输出电压为0.08进入PWM调光
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
 bool IsCurrentControlledByMode;
 float Current,Throttle,DACVID,delta,Duty;
 /********************************************************
 运行时挡位处理的第一步.首先获取当前的挡位，然后我们使能ADC
 的转换负责获取LED的电流,温度和驱动功率管的温度,接下来就是
 根据我们读取到的温度和电流执行各种保护
 ********************************************************/
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
 SysPstatebuf.TargetCurrent=Current;//存储下目标设置的电流给短路保护模块用
 /********************************************************
 运行时挡位处理的第四步,如果目前LED电流为0，为了避免红色LED
 鬼火，程序会把LED的主buck相关的电路关闭。
 ********************************************************/
 if(Current==0||!SysPstatebuf.ToggledFlash)	 
	 SetAUXPWR(false);
 else
	 SetAUXPWR(true);  	 
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
   if(!SysPstatebuf.ToggledFlash)//特殊功能控制量请求关闭LED,不执行电流控制输出	
	   SetPWMDuty(0);		 
	 else //正常求解
		   {
		   //根据实际LED电流和预设的偏差情况,对占空比进行PID控制
		   delta=Current-LEDFilter(ADCO.LEDIf,LEDIfFilter,14); //求误差
       LED_Current_Integral+=delta; 
		   if(LED_Current_Integral<-97.3)LED_Current_Integral=-97.3;
		   if(LED_Current_Integral>150)LED_Current_Integral=150; //积分限幅	   
			 Duty=35+(delta*PWMDimmingKp+LED_Current_Integral*PWMDimmingKi+PWMDimmingKd*(delta-LED_Current_Last_Error));
		   LED_Current_Last_Error=delta;
			 delta=fabsf(SysPstatebuf.Duty-Duty);
       if(fabsf(delta)>4)SysPstatebuf.Duty=Duty; //避免PID输出的过小变化影响占空比导致闪烁
			 else if(fabsf(delta)>0.02)
			   {
				 if(SysPstatebuf.Duty>Duty)SysPstatebuf.Duty-=0.0025;
				 else SysPstatebuf.Duty+=0.0025;			   
				 }		   
			 SetPWMDuty(SysPstatebuf.Duty); //设置占空比
			 }
	 DACVID=0.08;  //80mV
	 SysPstatebuf.IsLinearDim=false;
	 }
 //大于1A电流,使用纯线性调光实现无频闪
 else
	 {
   DACVID=Current*30;  //计算DAC的VID,公式为:VID=(30mV*offset*LEDIf(A))+23mV 
	 DACVID/=QueueLinearTable(5,Current,(float *)&DimmingCompTable[0],(float *)&DimmingCompTable[5]);//除以补偿系数得到补偿后的VID
	 if(fabsf(Current-ADCO.LEDIf)>0.02) 
	   { 
	   if(Current>ADCO.LEDIf)CurrentSynthRatio=CurrentSynthRatio<110?CurrentSynthRatio+0.1:110; 
	   else CurrentSynthRatio=CurrentSynthRatio>50?CurrentSynthRatio-0.1:50;
		 }
	 DACVID+=23;//加上23mV的offset 
	 DACVID*=CurrentSynthRatio/(float)100; //乘上自适应电流补偿系数
	 DACVID/=1000;//mV转换为V
	 SysPstatebuf.IsLinearDim=true;
	 SetPWMDuty(!SysPstatebuf.ToggledFlash?0:100); //纯线性调光,PWM占空比仅受特殊功能控制量在0-100之间转换
	 }
 /*  根据目标的DAC VID(电压值) 发送指令控制DAC输出  */
 SysPstatebuf.CurrentDACVID=DACVID;//将当前DAC的VID记录下来用于事件日志
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
 if(Current>0&&SysPstatebuf.ToggledFlash)//额定电流大于0且LED被启动才积分
     {
     if(LEDFilter(ADCO.LEDVf,LEDVfFilterBuf,12)>LEDVfMin)SysPstatebuf.IsLEDShorted=false; //LEDVf大于短路保护值，正常运行
     else SysPstatebuf.IsLEDShorted=true;
     }
 }
