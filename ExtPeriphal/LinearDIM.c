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

//声明一下fmax和fmin的定义
float fmaxf(float x,float y);
float fminf(float x,float y);

//内部变量
float StepDownFilterBuf[10];
static float LEDVfFilterBuf[12];
static char ShortCount=0;

//LED短路检测部分的简易数字滤波器（避免PWM调光时某一瞬间的波动）
float LEDVfFilter(float VfIN)
{
 int i;
 float buf,min,max;
 //搬数据
 for(i=11;i>0;i--)LEDVfFilterBuf[i]=LEDVfFilterBuf[i-1];
 LEDVfFilterBuf[i]=VfIN;
 //找最高和最低的同时累加数据
 min=2000;
 max=-2000;
 buf=0;
 for(i=0;i<12;i++)
	{
	buf+=LEDVfFilterBuf[i];//累加数据
	min=fminf(min,LEDVfFilterBuf[i]);
	max=fmaxf(max,LEDVfFilterBuf[i]); //取最小和最大
	}
 buf-=(min+max);
 buf/=(float)10;//去掉最高和最低值，取剩下的8个值里面的平均值
 return buf;
}

//温控降档部分的简易数字滤波器（避免降档不稳）
float StepDownFilter(float StepIN)
{
 int i;
 float buf,min,max;
 //搬数据
 for(i=9;i>0;i--)StepDownFilterBuf[i]=StepDownFilterBuf[i-1];
 StepDownFilterBuf[i]=StepIN;
 //找最高和最低的同时累加数据
 min=2000;
 max=-2000;
 buf=0;
 for(i=0;i<10;i++)
	{
	buf+=StepDownFilterBuf[i];//累加数据
	min=fminf(min,StepDownFilterBuf[i]);
	max=fmaxf(max,StepDownFilterBuf[i]); //取最小和最大
	}
 buf-=(min+max);
 buf/=(float)8;//去掉最高和最低值，取剩下的8个值里面的平均值
 if(buf>100)buf=100;
 if(buf<5)buf=5; //限制幅度最大95%，最小0%
 return buf;
}

//线性调光模块相关的电路的上电自校准程序
void LinearDIM_POR(void)
{
 DACInitStrDef DACInitStr;
 ADCOutTypeDef ADCO;	
 float VSet,VGet;
 int i,retry;
 /**********************************************************************
 自检过程中的第1步：我们首先需要配置好AD5693R DAC,将DAC内置基准打开并转
 换为工作模式。
 ***********************************************************************/
 UartPost(Msg_info,"LineDIM","Checking onboard DAC and IMON circult functionality...");
 DACInitStr.DACPState=DAC_Normal_Mode;
 DACInitStr.DACRange=DAC_Output_REF;
 DACInitStr.IsOnchipRefEnabled=true; 
 if(!AD5693R_SetChipConfig(&DACInitStr))
  { 
 	UartPost(Msg_critical,"LineDIM","DAC Did not response during init process.");
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
		UartPost(Msg_critical,"LineDIM","DAC Loop calibration failure at state #%d.Expected %.2fV but result is %.2fV",i,VSet,VGet);
	  CurrentLEDIndex=20;//电压误差过大,说明PWM MUX或者电流反馈MUX相关的电流反馈电路有问题。
	  SelfTestErrorHandler();  		
		}
	i++;//成功完成校准的次数+1
	}
  /**********************************************************************
 自检过程中的第3步：我们将DAC输出的PWM Mux设置为常闭(0%占空比)然后令DAC输
 出一个1.25V的电压并通过ADC测量DAC的输出.这一步是为了测试PWM调光的MUX和
 PWM调光电路能否正常运行并根据PWM信号切断LED电流控制输入.
 ***********************************************************************/
 UartPost(Msg_info,"LineDIM","Checking dimming MUX functionality...");
 SetPWMDuty(0);
 delay_ms(2);
 AD5693R_SetOutput(1.25);
 if(!ADC_GetLEDIfPinVoltage(&VGet))OnChipADC_FaultHandler();//令ADC采样电压
 if(VGet>=0.05)
    {
	  //有超过0.05的电压,说明PWM MUX无法切断电压.PWM相关电路有问题
		UartPost(Msg_critical,"LineDIM","PWM Dimming MUX did not cut DAC output correctly,Expected <0.05V but result is %.2fV",VGet);
	  CurrentLEDIndex=20;
	  SelfTestErrorHandler();  		
		}
  /**********************************************************************
 自检过程中的第4步：我们将DAC输出的PWM Mux设置为常开(100%占空比)然后令DAC
 输出0V的电压并通过ADC测量DAC的输出.这一步是为了测试DAC的输出能否正确的归
 零确保线性调光的准确性.
 ***********************************************************************/
 UartPost(Msg_info,"LineDIM","Checking DAC zero scale reset functionality...");
 AD5693R_SetOutput(0);
 SetPWMDuty(100);		
 if(!ADC_GetLEDIfPinVoltage(&VGet))OnChipADC_FaultHandler();//令ADC采样电压
 if(VGet>=0.05)
	  {
		//有超过0.05的电压,说明DAC有问题输出无法归零
		UartPost(Msg_critical,"LineDIM","DAC Failed to reset into zero scale.Expected <0.05V but result is %.2fV",VGet);
	  CurrentLEDIndex=20;
	  SelfTestErrorHandler();  		
		}
  /**********************************************************************
 自检过程中的最后一步：我们将DAC输出的PWM Mux设置为常闭(0%占空比)确保主
 buck不会意外输出电流到LED,然后我们让辅助电源上电,测量智能功率级是否正确
 的输出用于指示温度的电压信号确保温度传感器正常运行.
 ***********************************************************************/
 UartPost(Msg_info,"LineDIM","Checking Auxiliary PS and Smart Power Stage...");
 SetPWMDuty(0);
 delay_ms(2);		
 SetAUXPWR(true);
 for(i=0;i<200;i++)//等待辅助电源上电稳定
		{
	  delay_ms(2);
		if(!ADC_GetResult(&ADCO))OnChipADC_FaultHandler();//让ADC获取信息
		if(ADCO.SPSTMONState==SPS_TMON_OK)retry++;
		else retry=0;
		if(retry==20)break;
		}
 if(i==200)
   {	 
	 //SPS没有正确返回温度信号或者直接将温度信号拉高到3.3V.这说明SPS或者辅助电源出现灾难性异常,需要锁死。
	 CurrentLEDIndex=24;
	 if(ADCO.SPSTMONState==SPS_TMON_CriticalFault)
		  UartPost(Msg_critical,"LineDIM","Smart Power Stage reports catastrophic error during initialization.");
	 else
	    UartPost(Msg_critical,"LineDIM","Smart Power Stage did not provide temperature feedback signal.");
	 //严重错误,死循环使驱动锁定
	 SetAUXPWR(false);
	 SelfTestErrorHandler();  
	 }
 UartPost(Msg_info,"LineDIM","Smart Power Stage initialization completed.");
 if(!ADC_GetResult(&ADCO))OnChipADC_FaultHandler();//让ADC获取信息
 UartPost(Msg_info,"LineDIM","Current SPS Temperature is %.2f'C.",ADCO.SPSTemp);
 /**********************************************************************
 自检成功完成,此时我们可以让辅助电源下电了。然后我们将DAC输出重置为0V并且
 让PWM模块输出0%占空比使得不会有意外的电压信号进入主Buck的控制脚使得buck
 在我们不想要的情况下意外启动。 
 ***********************************************************************/
 SetAUXPWR(false);
 delay_ms(10);
 AD5693R_SetOutput(0);
 SetPWMDuty(0);			 
 for(i=0;i<10;i++)StepDownFilterBuf[i]=100;//填上100的初始值
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
 float VID;
 int i,retry;
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
 for(i=0;i<2000;i++)//等待辅助电源上电稳定
		{
	  delay_ms(2);
		if(!ADC_GetResult(&ADCO))return Error_ADC_Logic;//让ADC获取信息
		if(ADCO.SPSTMONState==SPS_TMON_OK)retry++;
		else retry=0;
		if(retry==10)break;
		}
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
 爬升到0.6A的小电流.系统将会在协商结束后检测LED的If和Vf和
 DAC的VID判断是否短路
 ********************************************************/
 VID=27.0;
 retry=0;
 while(VID<100)
		 {
		 if(!AD5693R_SetOutput(VID/(float)1000))return Error_DAC_Logic;
		 ADC_GetResult(&ADCO);
		 if(ADCO.LEDIf>=0.5)retry++; //重试
		 else 
		   {
			 retry=0;
		   VID+=0.5;
			 }
		 if(retry==10)break;
		 }
 SysPstatebuf.CurrentDACVID=VID;
 /********************************************************
 电流协商已经完成,此时系统将会检测LED的Vf来确认LED情况,确保
 LED没有发生短路和开路.
 ********************************************************/
 if(!ADC_GetResult(&ADCO))return Error_ADC_Logic;
 //DAC的输出电压已经到了最高允许值,但是电流仍然未达标,这意味着LED可能短路或PWM逻辑异常
 if(VID==100)return  ADCO.LEDVf>=4.5?Error_LED_Open:Error_PWM_Logic;
 if(ADCO.LEDVf<2.1)return Error_LED_Short;		 //LEDVf过低,LED可能短路
 /********************************************************
 LED自检顺利结束,驱动硬件和负载工作正常,此时返回无错误代码
 交由驱动的其余逻辑完成处理
 ********************************************************/
 SysPstatebuf.IsLEDShorted=false;
 SysPstatebuf.ToggledFlash=true;// LED没有短路，上电点亮
 for(i=0;i<12;i++)LEDVfFilterBuf[i]=ADCO.LEDVf;//填写LEDVf 
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
 float Current,Throttle,SPSThrottle,DACVID,delta;
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
 if(ADCO.SPSTemp>CfgFile.MOSFETThermalTripTemp)
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
 if(ADCO.LEDVf>4.5)
     {
		 //LED的两端电压过高(LED开路),这是严重故障,立即写log并停止驱动运行
	   RunTimeErrorReportHandler(Error_LED_Open);
		 return;
	   }
 /********************************************************
 运行时挡位处理的第二步.我们根据用户配置的温度曲线和当前
 ADC读取到的各组件温度计算出降档的幅度，并且最终汇总为一个
 降档系数用于限制电流
 ********************************************************/
 Throttle=QueueLinearTable(5,ADCO.LEDTemp,CfgFile.LEDThermalStepThr,CfgFile.LEDThermalStepRatio);//根据LED的温度取数值
 if(Throttle==NAN)
	  {
		//LED的线性降档温度表数据有问题,这是严重的逻辑错误,立即写log并停止驱动运行
	  RunTimeErrorReportHandler(Error_Thremal_Logic);
		return;
	  }
 SPSThrottle=QueueLinearTable(5,ADCO.SPSTemp,CfgFile.SPSThermalStepThr,CfgFile.SPSThermalStepRatio);//根据功率管的降档曲线取出对应的数值
 if(SPSThrottle==NAN)
	  {
	  //功率管的线性降档温度表数据有问题,这是严重的逻辑错误,立即写log并停止驱动运行
    RunTimeErrorReportHandler(Error_Thremal_Logic);
		return;
	  }
 if(ADCO.NTCState==LED_NTC_OK)//LED的NTCOK,正常执行权重逻辑
    {
		Throttle*=(float)CfgFile.LEDThermalStepWeight/(float)100;
    SPSThrottle*=(float)(100-CfgFile.LEDThermalStepWeight)/(float)100;
    Throttle+=SPSThrottle;//按照权重设置对LED和SPS的降档数值进行加权得到最终的降档系数
		}
 else	Throttle=SPSThrottle;//LED的NTC故障，此时SPS的降档数值不经过任何处理直接写入到最终的降档数值buffer里面
 if(Throttle<5)Throttle=5;//最多只可以降到5%
 Throttle=StepDownFilter(Throttle);//将算出来的值写入到数字滤波器中然后从滤波器取数值
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
 对于呼吸挡位，为了实现平滑的熄灭，我们需要完全使用线性调
 光。这时候我们会有一个专门的部分来处理。这部分和正常的逻
 辑是独立的
 ********************************************************/
 if(CurrentMode->Mode==LightMode_Breath)
   {
   DACVID=Current*(float)30; //计算DAC的VID,公式为:VID=30mV*LEDIf(A)
	 if(DACVID>10)DACVID+=23;//大于10mV的时候为了保证电压准确自动的加上23mV的offset
	 DACVID/=(float)1000; //除以1000转V
	 SysPstatebuf.IsLinearDim=true;
	 SetPWMDuty(100); //纯线性调光,PWM占空比100%
	  /*  根据目标的DAC VID(电压值) 发送指令控制DAC输出  */
   SysPstatebuf.CurrentDACVID=DACVID;//将当前DAC的VID记录下来用于事件日志
   if(!AD5693R_SetOutput(DACVID))
     {
	   //DAC对于发送的指令没有回应无法设置输出,这是严重故障,立即写log并停止驱动运行
	   RunTimeErrorReportHandler(Error_DAC_Logic);
	   return;
	   }
	 TimerCanTrigger=true;//允许GPTM1定时器执行特殊功能
	 return;
	 }
 /********************************************************
 运行时挡位处理的第四步,将计算出的电流通过PWM调光和DAC线
 性调光混合的方式把LED调节到目标的电流实现挡位控制（这是对
 于其他挡位来说的）
 ********************************************************/
 if(Current==0)//LED关闭
   {
	 SysPstatebuf.IsLinearDim=true;
	 SetPWMDuty(0); 
	 DACVID=0; //令DAC的输出和PWM占空比都为0使主Buck停止输出
	 }
 //小于等于1A电流,因为电流环路无法在小于此点的条件下稳定因此使用PWM方式调光
 else if(Current<=1.00)
   {
	 if(Current<0.5)Current=0.5;//最小电流0.5
   DACVID=80/(float)1000;
	 SysPstatebuf.IsLinearDim=false;
	 }
 //大于1A电流,使用纯线性调光实现无频闪
 else
	 {
   DACVID=Current*(float)30/(float)1000;
	 DACVID+=(float)23/(float)1000; //计算DAC的VID,公式为:VID=(30mV*LEDIf(A))+23mV 
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
	/*  对于PWM调光模式来说,我们需要利用一个简单的电流环控制占空比实现<1A的月光档输出  */
 if(!SysPstatebuf.ToggledFlash)
	 SetPWMDuty(0);//特殊功能控制量请求关闭LED,不执行电流控制输出
 else if(Current>0&&Current<=1.00)do
    { 
		if(!ADC_GetResult(&ADCO))//令ADC得到电流
       {
			 //ADC转换失败,这是严重故障,立即写log并停止驱动运行
		   RunTimeErrorReportHandler(Error_ADC_Logic);
		   return;
	     }	
		//特殊功能控制量=false表示请求关闭电流输出,立即退出电流环
		if(!SysPstatebuf.ToggledFlash)
		   {
			 SetPWMDuty(0);
			 return;
			 }
		//根据实际LED电流和预设的偏差情况,设置占空比
		delta=Current-ADCO.LEDIf;
		if(delta>0)SysPstatebuf.Duty=SysPstatebuf.Duty<100?SysPstatebuf.Duty+0.1:SysPstatebuf.Duty;
	  else if(delta<0)SysPstatebuf.Duty=SysPstatebuf.Duty>30?SysPstatebuf.Duty-0.1:SysPstatebuf.Duty;
		SetPWMDuty(SysPstatebuf.Duty);
		}
 while(fabs(delta)>0.05);	
 TimerCanTrigger=true;//允许GPTM1定时器执行特殊功能
 /***********************************************************
 最后一步，系统采样LED的Vf加入数字滤波器中用于判断是否发生短
 路。
 ***********************************************************/
 if(!ADC_GetResult(&ADCO))//令ADC得到温度和电流
     {
		 //ADC转换失败,这是严重故障,立即写log并停止驱动运行
		 RunTimeErrorReportHandler(Error_ADC_Logic);
		 return;
	   }
 if(Current>0&&SysPstatebuf.ToggledFlash)//额定电流大于0且LED被启动才积分
     {
     if(LEDVfFilter(ADCO.LEDVf)>2.1)SysPstatebuf.IsLEDShorted=false; //LEDVf大于2.1正常运行
     else SysPstatebuf.IsLEDShorted=true;
     }
 }
