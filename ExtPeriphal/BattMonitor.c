#include "AD5693R.h"
#include "delay.h"
#include "PWMDIM.h"
#include "INA219.h"
#include "ADC.h"
#include "cfgfile.h"
#include "LEDMgmt.h"
#include "modelogic.h"
#include "runtimelogger.h"

//静态变量声明
INADoutSreDef RunTimeBattTelemResult;
float UsedCapacity=0;
static char LVFlashTimer=0;

 //低电压警告(每6秒闪2次)
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
 float BatteryMidLevel;
 bool IsNeedToUpdateCapacity;
 //令INA219获取电池(输入电源)信息
 if(SysPstatebuf.Pstate!=PState_LEDOn&&SysPstatebuf.Pstate!=PState_LEDOnNonHold)return;//LED没开启
 RunTimeBattTelemResult.TargetSensorADDR=INA219ADDR; //指定地址
 if(!INA219_GetBusInformation(&RunTimeBattTelemResult))
     {
		 //INA219转换失败,这是严重故障,立即写log并停止驱动运行
		 RunTimeErrorReportHandler(Error_ADC_Logic);
		 return;
	   }	
 //根据读到的电池电压/容量控制电量指示灯
 if(!RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone)
    {
    BatteryMidLevel=((CfgFile.VoltageFull-CfgFile.VoltageAlert)*0.5)+CfgFile.VoltageAlert;//计算还有一半电量以上的电压
	  if(RunLogEntry.Data.DataSec.IsLowVoltageAlert)CurrentLEDIndex=3;//低压告警触发，电池电量不足，红灯常亮
    else if(RunTimeBattTelemResult.BusVolt>=BatteryMidLevel)CurrentLEDIndex=2;//电池电量充足，绿灯常亮		 
    else if(RunTimeBattTelemResult.BusVolt>CfgFile.VoltageAlert)CurrentLEDIndex=23;//电池电量一般，黄灯常亮
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
     RunTimeErrorReportHandler(Error_Input_OVP);//电池过压保护,这是严重故障,立即写log并停止驱动运行
 if(RunTimeBattTelemResult.BusCurrent>CfgFile.OverCurrentTrip)
		 RunTimeErrorReportHandler(Error_Input_OCP);//电池过流保护,这是严重故障,立即写log并停止驱动运行
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
		 //关机
		 #ifndef FlashLightOS_Debug_Mode
		 SysPstatebuf.Pstate=PState_Error;
		 SysPstatebuf.ErrorCode=Error_Input_UVP;
		 TurnLightOFFLogic(); 
		 #endif
		 }
 }
