#include "AD5693R.h"
#include "delay.h"
#include "PWMDIM.h"
#include "INA219.h"
#include "ADC.h"
#include "cfgfile.h"
#include "LEDMgmt.h"
#include "modelogic.h"
#include "runtimelogger.h"
#include "FRU.h"
#include <string.h>

//静态变量和函数声明
int iroundf(float IN);
float fminf(float x,float y);
INADoutSreDef RunTimeBattTelemResult;
float UsedCapacity=0;
float UnLoadBattVoltage=12; //用于判断电池质量的电压变量
static char LVFlashTimer=0;
extern bool IsDisableBattCheck; //是否关闭电池质量警报
extern float MaximumBatteryPower;//最大电池电流

//低压告警标记
void WriteRunLogWithLVAlert(void)
 {
 if(RunLogEntry.Data.DataSec.LowVoltageShutDownCount<32766)
		RunLogEntry.Data.DataSec.LowVoltageShutDownCount++; //低电压告警次数+1
 RunLogEntry.CurrentDataCRC=CalcRunLogCRC32(&RunLogEntry.Data);//重新计算CRC-32  
 WriteRuntimeLogToROM();//尝试写ROM
 }

//当库仑计关闭时显示电池电压
void DisplayBattVoltage(void)
{
 INADoutSreDef INADO;
 LED_Reset();//复位LED管理器
 memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
 //获取电压
 INADO.TargetSensorADDR=INA219ADDR; //指定地址
 if(SysPstatebuf.Pstate==PState_LEDOn||SysPstatebuf.Pstate==PState_LEDOnNonHold) //LED开启，直接读取
   {
	 if(!INA219_GetBusInformation(&INADO))//获取电池电压
     {
		 //INA219转换失败,这是严重故障,立即写log并停止驱动运行
		 RunTimeErrorReportHandler(Error_ADC_Logic);
		 return;
	   }
	 strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1); //点亮模式需要额外的延时
	 }
 else //停机状态，打开219，获取电池电压后再关闭
   {	   
	 if(!INA219_SetConvMode(INA219_Cont_Both,INA219ADDR)) //启用INA219
			{
			SysPstatebuf.Pstate=PState_Error;
			SysPstatebuf.ErrorCode=Error_ADC_Logic;
			return;
			}
	 delay_ms(10);
	 if(!INA219_GetBusInformation(&INADO))//获取电池电压
			{
			SysPstatebuf.Pstate=PState_Error;
			SysPstatebuf.ErrorCode=Error_ADC_Logic;
			return;
			}
	 if(!INA219_SetConvMode(INA219_PowerDown,INA219ADDR))//关闭INA219
			{
			SysPstatebuf.Pstate=PState_Error;
			SysPstatebuf.ErrorCode=Error_ADC_Logic;
			return;
			}
	  }
 //显示电池电压	 
 strncat(LEDModeStr,"12030D",sizeof(LEDModeStr)-1);  //红切换到绿色，熄灭然后马上黄色闪一下，延迟1秒
 if(INADO.BusVolt>=10) //大于等于10V，附加红色闪
   strncat(LEDModeStr,"20D",sizeof(LEDModeStr)-1);
 LED_AddStrobe((int)(INADO.BusVolt)%10,"30"); //使用黄色显示个位数
 strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
 LED_AddStrobe(iroundf(INADO.BusVolt*(float)10)%10,"10");//绿色显示小数点后1位
 if(SysPstatebuf.Pstate==PState_LEDOn||SysPstatebuf.Pstate==PState_LEDOnNonHold) //如果手电筒点亮状态则延迟一下再恢复正常显示
	 strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
 strncat(LEDModeStr,"E",sizeof(LEDModeStr)-1);//结束闪烁
 ExtLEDIndex=&LEDModeStr[0];//传指针过去	
}
//库仑计开启时显示百分比（精确到1%）
void DisplayBatteryCapacity(void)
{
 float BatteryMidLevel;
 //计算剩余电量
 BatteryMidLevel=UsedCapacity/RunLogEntry.Data.DataSec.BattUsage.DesignedCapacity;//求已用容量和实际容量的百分比
 if(BatteryMidLevel>1.00)BatteryMidLevel=100;
 if(BatteryMidLevel<0)BatteryMidLevel=0;  //数值限幅
 BatteryMidLevel*=(float)100;//转百分比
 BatteryMidLevel=100-BatteryMidLevel; //转剩余电量百分比
 //启动显示
 LED_Reset();//复位LED管理器
 memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
 if(SysPstatebuf.Pstate==PState_LEDOn||SysPstatebuf.Pstate==PState_LEDOnNonHold) //如果手电筒点亮状态则延迟一下再恢复正常显示
	 strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
 strncat(LEDModeStr,"23010D",sizeof(LEDModeStr)-1);  //红切换到黄色，熄灭然后马上绿色闪一下，延迟1秒
 if(BatteryMidLevel>=100)
	 strncat(LEDModeStr,"20D",sizeof(LEDModeStr)-1);	//使用红色代表电压百位数
 LED_AddStrobe(((int)(BatteryMidLevel)%100)/10,"30");//使用黄色代表十位数
 strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
 LED_AddStrobe((iroundf(BatteryMidLevel)%100)%10,"10");//使用绿色代表点亮个位数
 if(SysPstatebuf.Pstate==PState_LEDOn||SysPstatebuf.Pstate==PState_LEDOnNonHold) //如果手电筒点亮状态则延迟一下再恢复正常显示
	 strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
 strncat(LEDModeStr,"E",sizeof(LEDModeStr)-1);//结束闪烁
 ExtLEDIndex=&LEDModeStr[0];//传指针过去	
}
//低电压警告(每6秒闪1次)
void LowVoltageIndicate(void)
{
 ModeConfStr *CurrentMode;
 if(SysPstatebuf.Pstate!=PState_LEDOn&&SysPstatebuf.Pstate!=PState_LEDOnNonHold)return;//LED没开启
 CurrentMode=GetCurrentModeConfig();//获取目前挡位信息
 if(CurrentMode==NULL)return; //字符串为NULL
 if(CurrentMode->Mode!=LightMode_On)return; //常亮档没有定时器的才靠这个
 if(RunLogEntry.Data.DataSec.IsLowVoltageAlert)
   {
	 if(LVFlashTimer==64)LVFlashTimer=0;
	 else if(LVFlashTimer<2)
	   {
		 if(LVFlashTimer==0)SysPstatebuf.ToggledFlash=false;
		 else SysPstatebuf.ToggledFlash=true;
		 LVFlashTimer++;
		 }
	 else LVFlashTimer++;
	 }
 else LVFlashTimer=0;	 	
}
//在手电筒运行期间测量电池参数的函数
void RunTimeBatteryTelemetry(void)
 {
 float BatteryMidLevel,voltDiff,VoltAlert,BatteryFullLevel;
 bool IsNeedToUpdateCapacity;
 ADCOutTypeDef ADCO;
 //令INA219获取电池(输入电源)信息,同时令ADC获取温度信息
 if(SysPstatebuf.Pstate!=PState_LEDOn&&SysPstatebuf.Pstate!=PState_LEDOnNonHold)return;//LED没开启
 RunTimeBattTelemResult.TargetSensorADDR=INA219ADDR; //指定地址
 if(!ADC_GetResult(&ADCO)||!INA219_GetBusInformation(&RunTimeBattTelemResult))
     {
		 //INA219转换失败,这是严重故障,立即写log并停止驱动运行
		 RunTimeErrorReportHandler(Error_ADC_Logic);
		 return;
	   }
 //根据每次启动手电空载的电压判断电池质量的前置准备 
 BatteryFullLevel=CfgFile.VoltageFull; //获取满电电压
 if(fminf(ADCO.LEDTemp,ADCO.SPSTemp)<10)BatteryFullLevel-=0.5; //低温环境，满电电压-0.5
 if((BatteryFullLevel-CfgFile.VoltageAlert)<0.3)BatteryFullLevel=CfgFile.VoltageAlert+0.3;//防止满电电压被减的过低	 
 BatteryMidLevel=((BatteryFullLevel-CfgFile.VoltageAlert)*0.5)+CfgFile.VoltageAlert;//计算还有一半电量以上的电压
 voltDiff=UnLoadBattVoltage-RunTimeBattTelemResult.BusVolt;
 if(voltDiff<0)voltDiff=0;//计算负载条件下的电压差
 if(SysPstatebuf.TargetCurrent>=(0.6*FusedMaxCurrent))//如果开启极亮挡位，则下调低电压检测阈值电压避免一上来就黄灯
    {
    VoltAlert=CfgFile.VoltageAlert-0.1;
    BatteryMidLevel=VoltAlert+0.5; 
		}
 else VoltAlert=CfgFile.VoltageAlert; //正常执行
 //电池质量检测		
 if(IsDisableBattCheck)UnLoadBattVoltage=RunTimeBattTelemResult.BusVolt; //电池质量检测被关闭，不执行下面的所有判断逻辑（并且同步空载和当前电压信息）		
 else if(UnLoadBattVoltage>(BatteryFullLevel-0.5)&&voltDiff>=2.4) //在大电流输出下电压差大于2.5或者电压低于警告值，电池质量太次触发警告
    {
    if(SysPstatebuf.TargetCurrent>=(0.6*FusedMaxCurrent))RunLogEntry.Data.DataSec.IsLowQualityBattAlert=true; 			
		}
 else if(voltDiff<2.4)UnLoadBattVoltage=RunTimeBattTelemResult.BusVolt; //小电流下对压差进行修正	
 //根据读到的电池电压/容量控制电量指示灯
 if(!RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone)
    {   
	  if(RunLogEntry.Data.DataSec.IsLowVoltageAlert)CurrentLEDIndex=3;//低压告警触发，电池电量不足，红灯常亮
    else if(RunTimeBattTelemResult.BusVolt>=BatteryMidLevel)CurrentLEDIndex=2;//电池电量充足，绿灯常亮		 
    else if(RunTimeBattTelemResult.BusVolt>VoltAlert)CurrentLEDIndex=23;//电池电量一般，黄灯常亮
    else if(RunTimeBattTelemResult.BusVolt>CfgFile.VoltageTrip)CurrentLEDIndex=3;//电池电量不足，红灯常亮
    else CurrentLEDIndex=9;//电池电量严重不足，红色慢闪
		}
 else if(RunTimeBattTelemResult.BusVolt<CfgFile.VoltageAlert)//电池电压过低，启用警告
    {
		if(RunTimeBattTelemResult.BusVolt>CfgFile.VoltageTrip)CurrentLEDIndex=3;//电池电量不足，红灯常亮
		else CurrentLEDIndex=9;//电池电量严重不足，红色慢闪
		}
 else //使用库仑计的办法控制电量指示灯
    {
		BatteryMidLevel=UsedCapacity/RunLogEntry.Data.DataSec.BattUsage.DesignedCapacity;//求已用容量和实际容量的百分比
		if(BatteryMidLevel>1.00)BatteryMidLevel=100;
		if(BatteryMidLevel<0)BatteryMidLevel=0;  //数值限幅
		BatteryMidLevel*=(float)100;//转百分比
		BatteryMidLevel=100-BatteryMidLevel; //转剩余电量百分比
		if(RunLogEntry.Data.DataSec.IsLowVoltageAlert)CurrentLEDIndex=3;//低压告警触发，电池电量不足，红灯常亮	
		else if(BatteryMidLevel>70)CurrentLEDIndex=2;//电池电量充足，绿灯常亮	
		else if(BatteryMidLevel>20)CurrentLEDIndex=23;//电池电量一般，黄灯常亮
		else if(BatteryMidLevel>10)CurrentLEDIndex=3;//电池电量不足，红灯常亮
		else CurrentLEDIndex=9;//电池电量严重不足，红色慢闪
		}
 //电池电压和电流保护逻辑
 if(RunTimeBattTelemResult.BusVolt>CfgFile.VoltageOverTrip)
     {
	   ProgramWarrantySign(Void_BattOverVoltage); //电池输入超压，自动注销保修
     RunTimeErrorReportHandler(Error_Input_OVP);//电池过压保护,这是严重故障,立即写log并停止驱动运行
		 }
 if(RunTimeBattTelemResult.BusPower>MaximumBatteryPower)
     {
	   ProgramWarrantySign(Void_BattOverPower); //电池输入超过驱动允许值，自动注销保修
		 RunTimeErrorReportHandler(Error_Input_OCP);//电池过功率保护,这是严重故障,立即写log并停止驱动运行
		 }
 #ifndef FlashLightOS_Debug_Mode
 //如果电压低于警告值则强制锁定驱动的输出电流为指定值
 if(RunTimeBattTelemResult.BusVolt<CfgFile.VoltageAlert)
		RunLogEntry.Data.DataSec.IsLowVoltageAlert=true;
 #endif
 if(RunTimeBattTelemResult.BusVolt<CfgFile.VoltageTrip)//电池电量过低，关机保护的同时根据需要更新电池容量	
     {
		 //根据需要更新电池容量
		 if(!IsRunTimeLoggingEnabled)IsNeedToUpdateCapacity=false; //logger未启动
		 else if(RunLogEntry.Data.DataSec.BattUsage.IsLearningEnabled&&UsedCapacity>1500)IsNeedToUpdateCapacity=true; //已用容量大于1500且使能自学习,完成校准
		 else if(!RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone)IsNeedToUpdateCapacity=false;//校准没有完成不主动写容量
		 else if(UsedCapacity<1500)IsNeedToUpdateCapacity=false; //测量出来的容量小于1500，不更新
		 else if(UsedCapacity<RunLogEntry.Data.DataSec.BattUsage.DesignedCapacity*0.85)IsNeedToUpdateCapacity=true;
		 else if(UsedCapacity>RunLogEntry.Data.DataSec.BattUsage.DesignedCapacity*1.15)IsNeedToUpdateCapacity=true; //容量数值偏差过大，自动更新容量数据
		 else IsNeedToUpdateCapacity=false; //不需要更新
		 //如果需要更新则开始更新容量
     if(IsNeedToUpdateCapacity)
		   {
			 RunLogEntry.Data.DataSec.BattUsage.IsLearningEnabled=false;
       RunLogEntry.Data.DataSec.BattUsage.DesignedCapacity=UsedCapacity;
       RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone=true;//自检完毕
			 RunLogEntry.CurrentDataCRC=CalcRunLogCRC32(&RunLogEntry.Data);//重新计算CRC-32  
		   WriteRuntimeLogToROM();//尝试写ROM
			 }
		 //不是更新容量，标记低电压告警触发
		 else WriteRunLogWithLVAlert();
		 //关机
		 #ifndef FlashLightOS_Debug_Mode
		 SysPstatebuf.Pstate=PState_Error;
		 SysPstatebuf.ErrorCode=Error_Input_UVP;
		 TurnLightOFFLogic(); 
		 #endif
		 }
 }
