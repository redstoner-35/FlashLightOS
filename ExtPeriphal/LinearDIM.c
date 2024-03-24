#include "console.h"
#include "AD5693R.h"
#include "delay.h"
#include "LEDMgmt.h"
#include "INA219.h"
#include "ADC.h"
#include "cfgfile.h"
#include "runtimelogger.h"
#include "CurrentReadComp.h"
#include "LinearDIM.h"
#include "MCP3421.h"
#include "FRU.h"
#include <math.h>
#include <stdlib.h>

//声明一下函数
float fmaxf(float x,float y);
float fminf(float x,float y);
float PIDThermalControl(void);//PID温控
void FillThermalFilterBuf(ADCOutTypeDef *ADCResult);//填充温度stepdown的缓冲区

//内部变量
static float LEDVfFilterBuf[12];
static char ShortCount=0;
static char MainAuxBuckCDTimer=0;
unsigned char PSUState=0x00; //辅助电源的状态
unsigned char NotifyUserTIM=0; //软件计时器，用于提示用户挡位发生了较小的改动
static bool BuckPowerState=false;//主副buck的电源状态
static bool MainAuxBuckSwitchState=false; //主副buck的电源使能切换状态，false=副buck开启
static float LastEdgeSswitchCurrent=0; //上次边沿切换的电流

//外部变量
extern float InplValue; //输入限流环的电流值
extern int CurrentTactalDim; //反向战术模式设定亮度的变量
extern bool IsMoonDimmingLocked; //反馈给无极调光模块，告诉调光模块电流是否调节完毕
extern bool IsRampAdjusting; //外部调光是否在调节
extern float UnLoadBattVoltage; //没有负载时的电池电压
extern float LEDVfMin;
extern float LEDVfMax; //LEDVf限制
extern bool IsInplAlertOccurred;//输入限流告警是否触发
extern bool IsEnabledCINL;
//字符串
const char *DACInitError="Failed to init %s DAC.";
const char *SPSFailure="SPS %sreports %s during init.";
const char *DidnotStr="did Not ";
const char *AUXBuckADCError="Aux Buck Isens ADC %s.";

//在挡位电流变化小于35%的情况下指示用户的处理函数
void NotifyUserForGearChgHandler(void)	
  {
	//计时器递减
  if(MainAuxBuckCDTimer>0)MainAuxBuckCDTimer--;
	if(NotifyUserTIM>0)NotifyUserTIM--;
	}
	
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

//检测主机USB是否连接(当主机连接时，PWM引脚会被主机的5V输入拉高,可以利用这个功能检测主机的存在)
FlagStatus IsHostConnectedViaUSB(void)
 {
 FlagStatus result;
 AFIO_GPxConfig(ToggleFlash_IOB,ToggleFlash_IOP, AFIO_FUN_GPIO);//GPIO功能
 GPIO_DirectionConfig(ToggleFlash_IOG,ToggleFlash_IOP,GPIO_DIR_IN);//配置为输入
 GPIO_InputConfig(ToggleFlash_IOG,ToggleFlash_IOP,ENABLE);//启用IDR 
 delay_ms(1);
 result=GPIO_ReadInBit(ToggleFlash_IOG,ToggleFlash_IOP); //读取结果完毕
 if(!result) //输入为低，重新配置为输出
   {
	 GPIO_InputConfig(ToggleFlash_IOG,ToggleFlash_IOP,DISABLE);//禁用IDR 
	 GPIO_DirectionConfig(ToggleFlash_IOG,ToggleFlash_IOP,GPIO_DIR_OUT);//配置为输出
	 GPIO_ClearOutBits(ToggleFlash_IOG,ToggleFlash_IOP); //输出强制set0
	 }
 return result;
 }

//设置爆闪控制pin
void SetTogglePin(bool IsPowerEnabled)
 {
 if(!IsPowerEnabled)GPIO_ClearOutBits(ToggleFlash_IOG,ToggleFlash_IOP); //输出强制set0
 else GPIO_SetOutBits(ToggleFlash_IOG,ToggleFlash_IOP); //输出强制set0
 }	

#ifndef SkipMoonDLCTest
//在自检失败的时候关闭辅助buck
static void DisableAuxBuckForFault(void)
 {
	DACInitStrDef DACInitStr;
  SetTogglePin(false); //关闭PWM pin
  DACInitStr.DACPState=DAC_Normal_Mode;
  DACInitStr.DACRange=DAC_Output_REF;	
	DACInitStr.IsOnchipRefEnabled=false; 
	AD5693R_SetChipConfig(&DACInitStr,AuxBuckAD5693ADDR); //关闭基准
  AD5693R_SetOutput(0,AuxBuckAD5693ADDR); //将DAC设置为初始输出
	SelfTestErrorHandler();
 }
#endif

//在DAC自检器件等待电压达到预定值
bool CheckDACOutVoltage(float VSet,float *VGetO,float ErrorRange)
 {
 int j=0;
 float VGet;
 //等待电压到达预定值
 while(j<500)
	  {
		delay_ms(1);
	  if(!ADC_GetLEDIfPinVoltage(&VGet))OnChipADC_FaultHandler();//ADC读取数据
		if(VGetO!=NULL)*VGetO=VGet; //写数据
	  if(fabsf(VSet-VGet)<=ErrorRange)return true; //电压在误差范围内，结束本次检测
		j++;//检测失败，重试
		}
 //等待500mS后电压仍未达到标称值，检测超时
 return false;	
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
 自检过程中的第1步：我们首先读配置好AD5693R DAC,将主DAC内置基准打开并转
 换为工作模式。对于辅助DAC因为我们不希望辅助buck运行因此我们将其基准设为
 关闭以便禁用辅助buck
 ***********************************************************************/
 UartPost(Msg_info,"LineDIM","Checking dimming circult...");
 DACInitStr.DACPState=DAC_Normal_Mode;
 DACInitStr.DACRange=DAC_Output_REF;
 DACInitStr.IsOnchipRefEnabled=true; 
 if(!AD5693R_SetChipConfig(&DACInitStr,MainBuckAD5693ADDR))
  { 
 	UartPost(Msg_critical,"LineDIM",(char *)DACInitError,"master");
	CurrentLEDIndex=20;//DAC无法启动,保护
	SelfTestErrorHandler(); 
	}
 DACInitStr.IsOnchipRefEnabled=false; //初始化Slave DAC
 if(!AD5693R_SetChipConfig(&DACInitStr,AuxBuckAD5693ADDR))
   { 
	 #ifndef SkipMoonDLCTest
   UartPost(Msg_critical,"LineDIM",(char *)DACInitError,"aux");
	 CurrentLEDIndex=20;//DAC无法启动,保护
	 SelfTestErrorHandler(); 
	 #else
	 UartPost(msg_error,"LineDIM",(char *)DACInitError,"aux");	 
	 #endif
	 }	
 /**********************************************************************
 自检过程中的第2步：首先需要确保辅助电源处于关闭状态,然后我们将DAC输出的
 PWM Mux设置为常通(100%占空比)，然后通过电流反馈MUX将调光信号反馈给电流
 检测ADC。然后通过ADC测量DAC的输出.这一步是为了验证DAC的实际电压输出值和
 设置值是一致的
 ***********************************************************************/
 SetBUCKSEL(false);
 SetTogglePin(true);
 delay_ms(2);
 i=1;
 for(VSet=0.5;VSet<=2.5;VSet+=0.5)
	{
	AD5693R_SetOutput(VSet,MainBuckAD5693ADDR);
	if(!CheckDACOutVoltage(VSet,&VGet,0.25))//电压超范围
	  {
		UartPost(Msg_critical,"LineDIM","Expected Master DAC out %.2fV but get %.2fV.",VSet,VGet);
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
 SetTogglePin(false);
 AD5693R_SetOutput(1.25,MainBuckAD5693ADDR);
 if(!CheckDACOutVoltage(0,NULL,0.05))
    {
	  //有超过0.05的电压,说明PWM MUX无法切断电压.PWM相关电路有问题
		UartPost(Msg_critical,"LineDIM","Dimming MUX Error.");
	  CurrentLEDIndex=30;
	  SelfTestErrorHandler();  		
		}
  /**********************************************************************
 自检过程中的第4步：我们将DAC输出的PWM Mux设置为常开(100%占空比)然后令DAC
 输出0V的电压并通过ADC测量DAC的输出.这一步是为了测试DAC的输出能否正确的归
 零确保线性调光的准确性.
 ***********************************************************************/
 AD5693R_SetOutput(0,MainBuckAD5693ADDR);
 SetTogglePin(true);	
 if(!CheckDACOutVoltage(0,NULL,0.05))
	  {
		//有超过0.05的电压,说明DAC有问题输出无法归零
		UartPost(Msg_critical,"LineDIM","Master DAC ZRST error.");
	  CurrentLEDIndex=20;
	  SelfTestErrorHandler();  		
		}
  /**********************************************************************
 自检过程中的第五步：我们打开主buck输出小电流检查电流反馈电路，片内温度传
 感器(MOSFET)以及辅助DCDC是否运行正常。
 ***********************************************************************/
 VSet=0.01;
 Set3V3AUXDCDC(true); //打开3V3 DCDC
 delay_ms(10);
 SetBUCKSEL(true); //10mS后送上控制器的EN 	
 delay_ms(10);
 AD5693R_SetOutput(VSet,MainBuckAD5693ADDR); //将DAC设置为初始输出开始试运行
 retry=0;//清零等待标志位
 for(i=0;i<200;i++)//等待辅助电源上电稳定检测电压
		{
	  delay_us(100);
		if(!ADC_GetResult(&ADCO))OnChipADC_FaultHandler();//让ADC获取信息
		if(ADCO.SPSTMONState==SPS_TMON_OK)
		   {
		   if(ADCO.LEDVf>=LEDVfMax)retry++;
		   else if(ADCO.LEDIf>0.4&&ADCO.LEDVf>=LEDVfMin)//电流检测正常且LED没有短路，累加数值
			   {
				 IMONOKFlag=true;
			   retry++; 
				 }
		   else
			   {				 
				 if(VSet<0.1&&ADCO.LEDVf<LEDVfMax)VSet+=0.01;  //Vset电压低于100mV且当前LED的Vf小于VfMax的时候增加DAC输出
			   AD5693R_SetOutput(VSet,MainBuckAD5693ADDR); //设置DAC输出电压直到检测通过
			   retry=0;
				 }
	     }
		else retry=0;
		if(retry==5)break;
		}
 if(i==200)
   {	 
	 /*
	 SPS没有正确返回温度信号或者直接将温度信号拉高到3.3V,
	 或者电流检测电流没有信号输入以及驱动在电流检测期间的输
	 出电压过低.这说明SPS或者辅助电源出现灾难性异常,需要锁死。
   */
	 CurrentLEDIndex=ADCO.LEDVf<LEDVfMin?15:24; //点亮LED报告异常
	 if(ADCO.LEDVf<LEDVfMin)
		  UartPost(Msg_critical,"LineDIM","Driver output short-circuit detected."); 
	 else if(ADCO.SPSTMONState==SPS_TMON_CriticalFault)
		  UartPost(Msg_critical,"LineDIM",(char *)SPSFailure,"","CATERR");
	 else if(ADCO.SPSTMONState!=SPS_TMON_OK)
	    UartPost(Msg_critical,"LineDIM",(char *)SPSFailure,DidnotStr,"TMON");	 	 
	 else if(!IMONOKFlag)
		  UartPost(Msg_critical,"LineDIM",(char *)SPSFailure,DidnotStr,"IMON");
	 //严重错误,死循环使驱动锁定
	 SetBUCKSEL(false);
	 SelfTestErrorHandler();  
	 }
 /**********************************************************************
 主buck自检成功完成,此时我们可以让辅助电源下电了。然后我们将DAC输出重置为
 0V并且让PWM模块输出0%占空比使得不会有意外的电压信号进入主Buck的控制脚使
 得主buck不会在我们不想要的情况下意外启动。 
 ***********************************************************************/
 SetTogglePin(false);
 AD5693R_SetOutput(0,MainBuckAD5693ADDR); 
 SetBUCKSEL(false);
 #ifndef SkipMoonDLCTest
 delay_ms(100); //等待主buck退出运行
 /***********************************************************************
 下一步，我们需要进行副buck相关电路的检查。首先我们配置好读取电流的MCP3421
 ADC，然后启用辅助buck DAC的基准使辅助buck运行，然后检查读回的电流是否正常
 ***********************************************************************/	 
 VSet=0.35;//初始VID 0.35
 VGet=0;//电流为0
 if(!MCP3421_SetChip(AuxBuckIsenADCGain,AuxBuckIsenADCRes,false))
   {
	 UartPost(Msg_critical,"LineDIM",(char *)AUXBuckADCError,"init FAIL.");
	 SelfTestErrorHandler();  
	 }	 
 AD5693R_SetOutput(0,AuxBuckAD5693ADDR); //将辅助buck输出设置为0，在上电之前辅助buck调光引脚不能有电压否则芯片不运行
 DACInitStr.IsOnchipRefEnabled=true; 
 AD5693R_SetChipConfig(&DACInitStr,AuxBuckAD5693ADDR); //启动基准使buck运行
 delay_ms(10);
 SetTogglePin(true);
 AD5693R_SetOutput(VSet,AuxBuckAD5693ADDR); //10mS等待辅助buck POR之后，将DAC设置为初始输出
 for(i=0;i<50;i++)//等待buck启动
		{
	  delay_ms(MCP_waitTime);
		if(!ADC_GetResult(&ADCO))OnChipADC_FaultHandler();//让ADC获取信息
	  if(!MCP3421_ReadVoltage(&VGet))//读取电流
		   {
			 UartPost(Msg_critical,"LineDIM",(char *)AUXBuckADCError,"no response");
			 DisableAuxBuckForFault();//关闭辅助buck并报错
			 }
		if(ADCO.LEDVf>=LEDVfMax)break; //电压超过VfMax
	  if(ConvertAuxBuckIsense(VGet)>=MinimumLEDCurrent&&ADCO.LEDVf>=LEDVfMin)break; //电压达LED最低电压且电流达到最小的LED电流，退出
		//VID加一点,设置输出之后继续尝试
		if(VSet<0.8&&ADCO.LEDVf<LEDVfMax)VSet+=0.01;  
		AD5693R_SetOutput(VSet,AuxBuckAD5693ADDR); //增大DAC输出电压直到检测通过
		}
 if(i==50) //出错
    {
		UartPost(Msg_critical,"LineDIM","Failed to start Aux Buck.");
    DisableAuxBuckForFault(); //关闭辅助buck并报错
		}
 
 /***********************************************************************
 副buck自检成功结束，这个时候我们通过设置DAC基准强制关闭副buck并关闭ADC,
 进入低功耗休眠阶段。
 ***********************************************************************/
 MCP3421_SetChip(AuxBuckIsenADCGain,AuxBuckIsenADCRes,true); //关闭ADC
 SetTogglePin(false); //PWM pin关闭
 AD5693R_SetOutput(0,AuxBuckAD5693ADDR); //将DAC设置为0V输出	
 DACInitStr.DACPState=DAC_Disable_HiZ;
 DACInitStr.IsOnchipRefEnabled=false; 
 AD5693R_SetChipConfig(&DACInitStr,AuxBuckAD5693ADDR); //关闭基准并设置为输出高阻使buck停止运行
 delay_ms(10);
 Set3V3AUXDCDC(false); //主buck的控制器关闭10mS后禁用3V3 DCDC		
 #else
 /***********************************************************************
 在跳过辅助buck检查的情况下成功的结束了主buck的检测，关闭主buck并重置变量
 ***********************************************************************/ 
 SetTogglePin(false); //PWM pin关闭
 delay_ms(10);
 Set3V3AUXDCDC(false); //主buck的控制器关闭10mS后禁用3V3 DCDC		
 IsDisableBattCheck=false; //默认开启电池质量检测
 #endif
}
//从开灯状态切换到关灯状态的逻辑
void TurnLightOFFLogic(void)
 {
 DACInitStrDef DACInitStr;
//复位变量
 TimerHasStarted=false;
 ShortCount=0;//复位短路次数统计
 PSUState=0x00;//关闭主电源控制的定时器
 //关闭外设
 DisableFlashTimer();//关闭闪烁定时器
 SetTogglePin(false);
 DACInitStr.DACPState=DAC_Disable_HiZ;
 DACInitStr.DACRange=DAC_Output_REF;
 DACInitStr.IsOnchipRefEnabled=false; //关闭副buck的基准电源,辅助DAC进入低功耗模式
 AD5693R_SetOutput(0,AuxBuckAD5693ADDR);	
 AD5693R_SetOutput(0,MainBuckAD5693ADDR); //将主buck和副buck的输出都set0
 AD5693R_SetChipConfig(&DACInitStr,AuxBuckAD5693ADDR);//关闭基准，彻底让副buck下电
 INA219_SetConvMode(INA219_PowerDown,INA219ADDR);//关闭INA219功率计
 MCP3421_SetChip(AuxBuckIsenADCGain,AuxBuckIsenADCRes,true);//关闭辅助buck的功率测量模块
 SetBUCKSEL(false);//令BuckSel=0，同时关闭主buck和辅助buck
 delay_ms(10);
 Set3V3AUXDCDC(false); //10毫秒后令辅助3V3 DCDC下电 
 //复位侧按LED管理器并计算运行日志的CRC32
 CurrentLEDIndex=0;
 LED_Reset();//复位LED管理器
 } 
//放在系统延时里面进行积分的函数
void LEDShortCounter(void)
 {
 //常亮档的短路检测
 if(!SysPstatebuf.IsLEDShorted)ShortCount=0;//短路没发生
 else if(ShortCount>=6)
    {
		SysPstatebuf.IsLEDShorted=false;//复位标志位
		ShortCount=0;
		//LED短路超过0.75秒,这是严重故障,立即写log并停止驱动运行
	  RunTimeErrorReportHandler(Error_LED_Short);
		return;
	  }
 else ShortCount++;//时间没到
 }
/*
这个函数会在I2C ADC正常运行(手电筒启动的阶段)每0.2秒采集一次
辅助buck的电流信息然后将电流信息写入到参数里面去。
*/ 
void GetAuxBuckCurrent(void)
 {
 float result=3072;
 if(SysPstatebuf.Pstate!=PState_LEDOn&&SysPstatebuf.Pstate!=PState_LEDOnNonHold)return;//主灯没启动
 if(!MCP3421_ReadVoltage(&result))
   {
	 //ADC转换失败,这是严重故障,立即写log并停止驱动运行
	 RunTimeErrorReportHandler(Error_ADC_Logic);
	 return;
	 }
 //开始进行数据转换
 if(result==3072)return; //DAC本次数据未更新，不赋值
 SysPstatebuf.AuxBuckCurrent=ConvertAuxBuckIsense(result); //数值已更新，计算参数
 }
/*
这个函数负责接收手电运行的逻辑处理函数传入的LED是否启动和
电流设置参数然后对主副buck进行控制得到需要的电流
*/ 
void DoLinearDimControl(float Current,bool IsMainLEDEnabled)
 {
 DACInitStrDef DACInitStr;
 bool IsBuckPowerOff,resultOK=true,DAResult[2],IsAuxBuckEnabled; 
 static bool AuxDACSetState=true;
 static float LastCurrent=0;
 int Dimmratio;
 float DACVID,Comp;
 bool IsReturnFromPowerDown=false;
 /*********************************************************
 首先系统会对传入的电流参数进行控制和限幅，然后系统会根据处
 理完毕的电流数值计算补偿参数，这个补偿参数用于处理LED电流
 的非线性特性。	 
 *********************************************************/
 if(Current<0)Current=0;
 if(Current>FusedMaxCurrent)Current=FusedMaxCurrent;//限制传入的电流值范围为0-熔断限制值
 if(NotifyUserTIM>0)Current=((Current*0.5)>=MinimumLEDCurrent)?Current*0.5:0; //如果用户挡位发生了较小的变动则让电流短时间减低到原始值的50%,如果减低到50%时差距仍然不够大则直接熄灭
 if(Current>0&&Current<MinimumLEDCurrent)Current=MinimumLEDCurrent; //电流不是0且低于最低允许值，强制设为最低值 
 Current*=(InplValue/100);
 #ifdef FlashLightOS_Init_Mode
 if(CheckCompData()!=Database_No_Error) //debug模式下如果补偿数据库未就绪则不取补偿数据库
   Comp=1.00;
 else //正常读取数据
   {
	 if(Current<3.9) //辅助Buck
		 Comp=QueueLinearTable(50,Current,CompData.CompDataEntry.CompData.Data.AuxBuckDimmingThreshold,CompData.CompDataEntry.CompData.Data.AuxBuckDimmingValue,&resultOK); 
	 else //主Buck
		 Comp=QueueLinearTable(50,Current,CompData.CompDataEntry.CompData.Data.MainBuckDimmingThreshold,CompData.CompDataEntry.CompData.Data.MainBuckDimmingValue,&resultOK); 
	 }
 #else	 
 if(Current<3.9) //辅助Buck
	 Comp=QueueLinearTable(50,Current,CompData.CompDataEntry.CompData.Data.AuxBuckDimmingThreshold,CompData.CompDataEntry.CompData.Data.AuxBuckDimmingValue,&resultOK); 
 else //主Buck
	 Comp=QueueLinearTable(50,Current,CompData.CompDataEntry.CompData.Data.MainBuckDimmingThreshold,CompData.CompDataEntry.CompData.Data.MainBuckDimmingValue,&resultOK); 
 if(!resultOK) //校准数据库异常
	 {
	 RunTimeErrorReportHandler(Error_Calibration_Data);
	 return;
	 }
 #endif  
 /*********************************************************
 为了节省电力，当主LED不需要运行的时候，经过0.5秒我们可以让
 主buck和副buck都下电来节省能量
 *********************************************************/	  
 if(Current==0||!IsMainLEDEnabled)	 
	 PSUState|=0x80; //额定LED电流为0或者toggle模式需要关闭主灯,启动计时器
 else
	 PSUState=0x00; //电源开启 
 IsBuckPowerOff=PSUState==(0x80|MainBuckOffTimeOut)?true:false; //检测主buck是否关闭
 if(BuckPowerState!=IsBuckPowerOff) //设置电源
   {
	 BuckPowerState=IsBuckPowerOff; //同步参数
	 AD5693R_SetOutput(0,MainBuckAD5693ADDR);	
	 AD5693R_SetOutput(0,AuxBuckAD5693ADDR);	//清除主和副buck的VID，确保下次buck启动前VID=0
	 if(BuckPowerState) //主buck下电
	   {
		 SetBUCKSEL(false); //关闭主buck
	   delay_ms(5);
	   Set3V3AUXDCDC(false); //在关闭控制器之后等待5mS再关闭辅助DCDC
		 }
   else
	   {
		 Set3V3AUXDCDC(true); //辅助DCDC先上电
		 delay_ms(15); //延时15mS后再启动下面的操作
		 if(Current<3.9)IsReturnFromPowerDown=true; //在关机前是辅助buck操作，加入额外的处理
		 }
	 DACInitStr.DACPState=DAC_Normal_Mode;
   DACInitStr.DACRange=DAC_Output_REF;
   DACInitStr.IsOnchipRefEnabled=!BuckPowerState; 
	 if(!AD5693R_SetChipConfig(&DACInitStr,AuxBuckAD5693ADDR)) //设置副buck的基准状态
     {
		 //DAC无响应,这是严重故障,立即写log并停止驱动运行
	   RunTimeErrorReportHandler(Error_DAC_Logic);
		 return;
	   } 
	 if(!MCP3421_SetChip(AuxBuckIsenADCGain,AuxBuckIsenADCRes,BuckPowerState))//如果主副buck都掉电则使MCP3421停止运行，节省功耗
		 {
	   //ADC配置失败,这是严重故障,立即写log并停止驱动运行
	   RunTimeErrorReportHandler(Error_ADC_Logic);
	   return;
	   } 
	 delay_us(30); //这个延时是为了确保buck POR阶段DAC的VID维持0一段时间然后再送,否则LT的芯片将会不响应
	 }
 /*********************************************************
 当前LED电流为0，且主LED设置为关闭，因此设置PWMPin=0使得两
 个buck全部关闭输出令LED熄灭
 *********************************************************/	 
 if(Current==0||!IsMainLEDEnabled)
    { 
		#ifndef FlashLightOS_Init_Mode
		//如果电流过大的时候立即关闭控制器将会导致哒哒哒的声音，因此需要生成ramp让电流缓降
		if(LastCurrent>(FusedMaxCurrent/3))for(Dimmratio=100;Dimmratio>=0;Dimmratio-=5)
			{
			DACVID=((LastCurrent*30)+40)*(float)Dimmratio;
		  DACVID/=100*Comp;//计算VID
			if(!AD5693R_SetOutput(DACVID/1000,MainBuckAD5693ADDR))RunTimeErrorReportHandler(Error_DAC_Logic); //设置DAC,如果出错就退出
			delay_us(HighCurrentRampSpeed); //生成从最高电流逐步slew到0的ramp，避免ΔI过高引起驱动发出哒哒哒得声音
			}
		#endif
    SetTogglePin(false);
		}
 /*********************************************************
 当前LED处于运行状态，根据输入的电流指令进行主副buck和DAC的
 VID控制，以及toggle引脚的控制
 *********************************************************/	
 else 
   {   
	 IsAuxBuckEnabled=Current<3.9?true:false;//判别AUXbuck的状态
	 SetBUCKSEL(!IsAuxBuckEnabled); //电流小于3.9切换到副buck,否则使用主buck	
	 SetTogglePin(true); //将两个buck的总使能信号设置为1	 
	 if(IsAuxBuckEnabled)DACVID=250+(Current*AuxBuckIsensemOhm*10); //LT3935 VIset=250mV(offset)+[额定电流(A)*电流检测电阻数值(mΩ)*电流检测放大器倍数(10X)]
	 else DACVID=(Current*30)+40; //主Buck VIset=30mv/A+40mV offset
	 DACVID*=Comp; //乘以查表得到的补偿系数
	 SysPstatebuf.CurrentDACVID=DACVID;//存储计算后的DACVID
	 DACVID/=1000; //mV转V
	 delay_us(30); //这个延时是为了确保buck POR阶段DAC的VID维持0一段时间然后再送,否则LT的芯片将会不响应
	 if(AuxDACSetState!=IsAuxBuckEnabled) //检测函数，作用是如果辅助或者主buck发生状态变化，则将被禁用的部分的DAC设置为0V
	   {
		 AuxDACSetState=IsAuxBuckEnabled; //同步辅助DAC状态
		 DAResult[1]=AD5693R_SetOutput(0,AuxDACSetState?AuxBuckAD5693ADDR:MainBuckAD5693ADDR);	//主buck启用，设置副buck的VID=0，否则设置主buck的VID=0
		 }	 
	 else DAResult[1]=true; //不需要更新主buck的状态 
	 /**********************************************
	 如果在省电阶段重新复位之后需要工作的是辅助buck
	 就需要让辅助buck先工作在270mV一段时间然后再降到
	 额定电流所对应的VID确保VID能高于芯片启动阈值
	 避免辅助buck阈值漂移导致芯片无法启动
	 **********************************************/
	 if(IsReturnFromPowerDown&&DACVID<270) 
	   {
		 AD5693R_SetOutput(0.27,AuxBuckAD5693ADDR);
		 delay_ms(2);
		 }
	 #ifndef FlashLightOS_Init_Mode
	 if(LastCurrent==0&&!IsAuxBuckEnabled)for(Dimmratio=0;Dimmratio<=100;Dimmratio+=5)
	   {
		 DAResult[0]=AD5693R_SetOutput((DACVID*(float)Dimmratio)/100,MainBuckAD5693ADDR);
		 if(!DAResult[0])break;//DAC无响应，结束循环
		 delay_us(HighCurrentRampSpeed); //生成从0逐步slew到最高电流的ramp，避免ΔI过高引起驱动发出哒哒哒的声音 
		 }
	 else DAResult[0]=AD5693R_SetOutput(DACVID,IsAuxBuckEnabled?AuxBuckAD5693ADDR:MainBuckAD5693ADDR); //设置VID
	 #else
	 DAResult[0]=AD5693R_SetOutput(DACVID,IsAuxBuckEnabled?AuxBuckAD5693ADDR:MainBuckAD5693ADDR); //设置VID
	 #endif
	 if(!DAResult[0]||!DAResult[1])RunTimeErrorReportHandler(Error_DAC_Logic); //DAC无响应,这是严重故障,立即写log并停止驱动运行 
	 }
   //存储上一次进行调节的电流(如果LED启用则存储电流值否则写0)
	 LastCurrent=IsMainLEDEnabled?Current:0;
 }	
 
//从关灯状态转换到开灯状态,上电自检的逻辑
SystemErrorCodeDef TurnLightONLogic(INADoutSreDef *BattOutput)
 {
 ADCOutTypeDef ADCO;
 bool Result;
 ModeConfStr *CurrentMode;
 float VID,VIDIncValue,ILED;
 int i,retry,AuxPSURecycleCount; 
 DACInitStrDef DACInitStr;
 /********************************************************
 我们首先需要检查传进来的模式组是否有效。然后检查校准数据库
 里面的数值和数据本身完整性是否有效。如果无效则报异常
 ********************************************************/
 CurrentMode=GetCurrentModeConfig();
 if(CurrentMode==NULL)return Error_Mode_Logic; //挡位逻辑异常
 if(CurrentMode->LEDCurrentHigh<InputCurrentLimitRelease)IsEnabledCINL=false; //输入电流小于限制值，复位控制器
 if(CheckCompData()!=Database_No_Error)return Error_Calibration_Data; //校准数据库错误
 /********************************************************
 首先我们需要将负责电池遥测的INA219功率级启动,然后先将DAC
 输出设置为0,PWM调光模块设置为100%,接着就可以送辅助电源然
 后等待200ms直到辅助电源启动完毕
 ********************************************************/
 BattOutput->TargetSensorADDR=INA219ADDR;
 if(!INA219_SetConvMode(INA219_Cont_Both,INA219ADDR))
	 return Error_ADC_Logic;//设置INA219遥测的器件地址,然后让INA219进入工作模式
 if(!AD5693R_SetOutput(0,MainBuckAD5693ADDR))
	 return Error_DAC_Logic;//将DAC输出设置为0V,确保送主电源时主Buck变换器不工作
 if(!MCP3421_SetChip(AuxBuckIsenADCGain,AuxBuckIsenADCRes,false))
	 return Error_ADC_Logic; //初始化ADC逻辑
 SetBUCKSEL(false); //令BUCKSEL=0，选择使用辅助BUCK
 delay_ms(1); 
 Set3V3AUXDCDC(true); 
 AuxPSURecycleCount=0;
 while(AuxPSURecycleCount<5) //送上辅助DCDC电源，开始进行SPS的检查
   {
   retry=0;//清零等待标志位
   for(i=0;i<5000;i++)//等待辅助电源上电稳定
		  {
	    delay_us(50);
		  Result=ADC_GetResult(&ADCO);
		  if(!Result)return Error_ADC_Logic;//让ADC获取信息
			retry=ADCO.SPSTMONState==SPS_TMON_OK?retry+1:0;
		  if(retry>=StartupAUXPSUPGCount)break;
		  }	 
	 //启动成功，跳出循环
	 if(i<5000)break;
	 //本次启动失败，关闭辅助电源，100mS后重新打开并再次启动
	 Set3V3AUXDCDC(false);
	 delay_ms(100);
	 Set3V3AUXDCDC(true);
	 AuxPSURecycleCount++;//启动重试次数+1
	 }
 if(AuxPSURecycleCount==5)//启动重试次数到达限制
   {
   if(ADCO.SPSTMONState==SPS_TMON_Disconnect) //SPS的温度检测掉线
			 return Error_SPS_TMON_Offline;
	 else //SPS致命错误
			 return Error_SPS_CATERR;
	 }   	
 /********************************************************
 系统主电源启动完毕,开始通过片内ADC和INA219初步获取电池电压
 电流和LED以及MOS的温度,确保关键的传感器工作正常且电池没有
 过压或欠压
 ********************************************************/
 Result=INA219_GetBusInformation(BattOutput);
 if(!Result||!ADC_GetResult(&ADCO))return Error_ADC_Logic;//INA219或者ADC无法正确的获取数据,报错
 //智能功率级严重过热警告
 if(ADCO.SPSTMONState==SPS_TMON_OK&&ADCO.SPSTemp>CfgFile.MOSFETThermalTripTemp)
	 return ProgramWarrantySign(Void_DriverCriticalOverTemp)?Error_SPS_ThermTrip:Error_Mode_Logic;
 //检测LED基板温度,如果超过热跳闸温度则保护。同时检测SPS和电池温度来决定是否启用电池质量检测
 if(ADCO.NTCState==LED_NTC_OK)
   {
	 //LED过热 
	 if(ADCO.LEDTemp>CfgFile.LEDThermalTripTemp)
		 return ProgramWarrantySign(Void_LEDCriticalOverTemp)?Error_LED_ThermTrip:Error_Mode_Logic;
	 }
 //检查INA219读取到的电池电压确保电压合适
 if(BattOutput->BusVolt<=CfgFile.VoltageTrip||BattOutput->BusVolt>CfgFile.VoltageOverTrip) 
   {
	 if(BattOutput->BusVolt>CfgFile.VoltageOverTrip)ProgramWarrantySign(Void_BattOverVoltage); //电池输入超压，自动注销保修
	 return BattOutput->BusVolt>CfgFile.VoltageOverTrip?Error_Input_OVP:Error_Input_UVP;
   }
 UnLoadBattVoltage=BattOutput->BusVolt;//存储空载时的电池电压
 //如果电压低于警告值则强制锁定驱动的输出电流为指定值
 if(BattOutput->BusVolt<CfgFile.VoltageAlert)
		RunLogEntry.Data.DataSec.IsLowVoltageAlert=true;
 /********************************************************
 电流协商开始,此时系统将会关闭主buck的电源，然后设置DAC启
 用基准来启动副buck，然后副buck开始向LED输出一个小电流，检
 查LED是否发生短路，以及副buck的输出线是否正确连接到基板
 ********************************************************/
 VID=StartUpInitialVID;
 VIDIncValue=0.5;
 ILED=0;
 SetTogglePin(true);//控制引脚设置为常通
 DACInitStr.DACPState=DAC_Normal_Mode;
 DACInitStr.DACRange=DAC_Output_REF;
 DACInitStr.IsOnchipRefEnabled=true; //启用副buck的基准电源,让副buck开始运作
 if(!AD5693R_SetChipConfig(&DACInitStr,AuxBuckAD5693ADDR))return Error_DAC_Logic;
 while(VID<100)
		 {
		 if(!AD5693R_SetOutput((VID+250)/(float)1000,AuxBuckAD5693ADDR))return Error_DAC_Logic;
		 delay_ms(MCP_waitTime);
		 if(!MCP3421_ReadVoltage(&ILED))return Error_ADC_Logic; //读取电流
	   if(!ADC_GetResult(&ADCO))return Error_ADC_Logic; //读取LED Vf
		 if(ConvertAuxBuckIsense(ILED)>=MinimumLEDCurrent&&ADCO.LEDVf>LEDVfMin)break; //电流足够且电压达标
		 VID=((100-VID)<VIDIncValue)?VID+0.5:VID+VIDIncValue;//增加VID，如果快到上限就慢慢加，否则继续快速增加VID
		 VIDIncValue+=StartupLEDVIDStep; //每次VID增加的数值
		 }
 SysPstatebuf.CurrentDACVID=VID;
 /********************************************************
 电流协商已经完成,此时系统将会检测LED的Vf来确认LED情况,确保
 LED没有发生短路和开路.
 ********************************************************/
 if(!ADC_GetResult(&ADCO))return Error_ADC_Logic;
 //DAC的输出电压已经到了最高允许值,但是电流仍然未达标,这意味着LED可能短路或PWM逻辑异常
 if(VID>=100)return (ADCO.LEDVf>=LEDVfMax)?Error_LED_Open:Error_PWM_Logic;
 i=0;
 if(ADCO.LEDVf<LEDVfMin)while(i<10)//检测到LED疑似短路，再次进行确认来判断LED是否短路
   {
	 delay_ms(1);
	 ADC_GetResult(&ADCO);
	 if(ADCO.LEDVf>LEDVfMin)break;
	 i++;
	 }
 if(i==10)return Error_LED_Short;		 //LEDVf过低持续10mS仍未改善,LED短路了
 /********************************************************
 LED自检顺利结束,驱动硬件和负载工作正常,此时返回无错误代码
 交由驱动的其余逻辑完成处理
 ********************************************************/
 SysPstatebuf.IsLEDShorted=false;
 IsInplAlertOccurred=false;//输入限流告警没有发生
 SysPstatebuf.ToggledFlash=true;// LED没有短路，上电点亮
 PSUState=0;//复位主副buck关机计时器
 SysPstatebuf.AuxBuckCurrent=ConvertAuxBuckIsense(ILED);//填写辅助buck的输出电流
 FillThermalFilterBuf(&ADCO); //填写温度信息
 for(i=0;i<12;i++)LEDVfFilterBuf[i]=ADCO.LEDVf;//填写LEDVf 
 BuckPowerState=false; //上电成功，当前处于正常运行阶段
 MainAuxBuckSwitchState=false; //自检结束后默认是以副buck运行，复位该标志位
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
 bool IsCurrentControlledByMode,IsEnableStepDown;
 volatile bool IsMainLEDEnabled,CurrentMainBuckState;
 float Current,Throttle,ThermalUnLimCurrent;
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
 if(ADCO.SPSTMONState==SPS_TMON_CriticalFault)
     {
		 //SPS报告了CATERR，立即停止驱动的运行
	   RunTimeErrorReportHandler(Error_SPS_CATERR);
		 return;		 
		 }
 if(ADCO.SPSTMONState==SPS_TMON_OK&&ADCO.SPSTemp>CfgFile.MOSFETThermalTripTemp)
	   {		 
		 //MOS温度到达过热跳闸点,这是严重故障,立即写log并停止驱动运行
		 ProgramWarrantySign(Void_DriverCriticalOverTemp); //驱动严重过热，自动注销保修
	   RunTimeErrorReportHandler(Error_SPS_ThermTrip);
		 return;
	   }
 if(ADCO.NTCState==LED_NTC_OK&&ADCO.LEDTemp>CfgFile.LEDThermalTripTemp)
	 	 {
		 //LED温度到达过热跳闸点,这是严重故障,立即写log并停止驱动运行
		 ProgramWarrantySign(Void_LEDCriticalOverTemp); //LED严重过热，自动注销保修
		 RunTimeErrorReportHandler(Error_LED_ThermTrip);
		 return;
	   }
 if(ADCO.LEDIf>(1.5*FusedMaxCurrent))
     {
		 //在LED启用时，LED电流超过允许值(熔断电流限制的1.5倍),这是严重故障,立即写log并停止驱动运行
		 ProgramWarrantySign(Void_OutputOCP); //输出电流过高，自动注销保修
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
 //判断是否需要应用降档设置 
 if(!IsMainLEDEnabled)IsEnableStepDown=false;//当前LED处于关闭阶段，不进行温控降档的计算
 else if(CurrentMode->IsModeAffectedByStepDown)IsEnableStepDown=true;//挡位启用温控降档 		 
 else if(CurrentTactalDim==101)IsEnableStepDown=true;//瞬时极亮启用，强制开启温控降档 		 
 else if(ADCO.NTCState==LED_NTC_OK&&ADCO.LEDTemp>(CfgFile.LEDThermalTripTemp-5))IsEnableStepDown=true; //LED温度逼近临界值，立即启动降档
 else if(ADCO.SPSTMONState==SPS_TMON_OK&&(ADCO.SPSTemp>CfgFile.MOSFETThermalTripTemp-10))IsEnableStepDown=true; //MOS温度逼近临界值，立即启动降档
 else IsEnableStepDown=false; //不需要应用降档设置
 //执行温控
 if(!IsEnableStepDown)ResetThermalPID(); //禁用温控降档的时候复位温控PID
 Throttle=IsEnableStepDown?PIDThermalControl():100;//根据是否降档来执行PID温控计算降档参数
 if(Throttle>100)Throttle=100;
 if(Throttle<5)Throttle=5;//温度降档值限幅
 SysPstatebuf.CurrentThrottleLevel=(float)100-Throttle;//将最后的降档系数记录下来用于事件日志使用
 /********************************************************
 运行时挡位处理的第三步.我们需要从当前用户选择的挡位设置
 里面取出电流配置，然后根据用户设置(是否受降档影响)以及特
 殊挡位功能的要求(比如说呼吸功能)对电流配置进行处理最后生成
 实际使用的电流
 ********************************************************/
 Current=(CurrentTactalDim==101)?FusedMaxCurrent*0.95:CurrentMode->LEDCurrentHigh;//取出电流设置(如果瞬时极亮开启则按照0.95倍最大电流运行，否则按照挡位设置)	 
 if(CurrentMode->Mode==LightMode_Breath)IsCurrentControlledByMode=true;
 else if(CurrentMode->Mode==LightMode_Ramp)IsCurrentControlledByMode=true;
 else if(CurrentMode->Mode==LightMode_CustomFlash)IsCurrentControlledByMode=true;
 else IsCurrentControlledByMode=false;
 if(IsCurrentControlledByMode&&Current>BreathCurrent)Current=BreathCurrent;//最高电流和对应模式的计算值同步
 if(RunLogEntry.Data.DataSec.IsLowVoltageAlert&&Current>LVAlertCurrentLimit)Current=LVAlertCurrentLimit;  //当低电压告警发生时限制输出电流
 Current*=(float)(CurrentTactalDim>100?100:CurrentTactalDim)/(float)100; //根据反向战术模式的设置取设定亮度
 /* 应用温度控制设置算出来的电流 */
 if(Throttle<5)Throttle=5;
 if(Throttle>100)Throttle=100; //PID调节值限幅
 if(Throttle<100) //PID调节值低于100，温控启动
   {
	 ThermalUnLimCurrent=Current; //存储未设置温控之前的挡位电流
	 Current*=Throttle/(float)100;//应用算出来的温控降档数值
	 /*
	 保护措施，如果经过PID调节器之前的电流大于
	 主buck运行阈值，且经过温控之后的电流小于阈值，
	 则强制使输送到调光函数里面的电流值为3.9A。这
	 个保护措施是为了避免温控动作后主灯电流频繁在
	 3.9A左右跳跃造成主副buck频繁切换启动引起炸机。
	 */
	 if(ThermalUnLimCurrent>=3.9&&Current<3.9)Current=3.9;
	 }
 /* 存储处理之后的目标电流 */
 SysPstatebuf.TargetCurrent=Current;//存储下目标设置的电流给短路保护模块用
 /********************************************************
 运行时挡位处理的第四步,将算出来的主LED是否开启的参数放入
 电流控制函数里面去进行调整，控制主灯的电流输出。在控制主
 灯前，电流函数还会进入一个限制器，确保电流参数不会让主副
 buck频繁的切换状态来避免炸机。 
 ********************************************************/
 CurrentMainBuckState=Current<3.9?false:true; //判断主副buck的状态
 if(CurrentMainBuckState!=MainAuxBuckSwitchState) //主副buck发生状态交换
   {
	 //时间未到，对电流值进行限制阻止主副buck频繁开关
	 if(MainAuxBuckCDTimer>0)
	   {
		 if(!MainAuxBuckSwitchState) //副buck开启，如果当前电流数值大于副buck切换到主buck的阈值则限制电流避免切换
		   Current=Current>=LastEdgeSswitchCurrent?LastEdgeSswitchCurrent:Current;
		 else //主buck开启，如果电流值小于等于主buck切换到副buck的阈值则限制电流避免切换
			 Current=Current<=LastEdgeSswitchCurrent?LastEdgeSswitchCurrent:Current;
		 }
	 //时间到，允许边沿切换
	 else
	   {
		 MainAuxBuckSwitchState=CurrentMainBuckState; //同步边沿状态
		 LastEdgeSswitchCurrent=Current;//存储下目标的边沿切换值
		 MainAuxBuckCDTimer=2; //设置计时器，0.4秒内不允许切换
		 }
	 }
 DoLinearDimControl(Current,IsMainLEDEnabled);
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
     else if(LEDFilter(ADCO.LEDVf,LEDVfFilterBuf,12)>LEDVfMin)SysPstatebuf.IsLEDShorted=false; //LEDVf大于1.5，正常运行
     else SysPstatebuf.IsLEDShorted=true;
     }
 }
