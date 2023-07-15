#include "console.h"
#include "CfgFile.h"
#include "runtimelogger.h"
#include <stdlib.h>
#include <math.h>

void DisplayLampTime(double time)
  {
  //秒	
	if(time<60)UartPrintf("%ld秒",(long)time%60);
	//分钟+秒
	else if(time<(60*60))UartPrintf("%ld分%ld秒",(long)time/60,(long)time%60);
	//小时分钟+秒
	else UartPrintf("%ld小时%ld分%ld秒",(long)time/3600,((long)time%3600)/60,((long)time%3600)%60);
	}
void PrintStatuBar(char *CompType)
  {
	UARTPuts("\r\n  ");
	UARTPutc('-',12);
	UartPrintf("  %s信息  ",CompType);
	UARTPutc('-',12);
	}
extern float UsedCapacity;
	
//命令处理主函数
void runlogviewHandler(void)
  {
	if(!IsRunTimeLoggingEnabled)
		UARTPuts("\r\n运行日志已被系统管理员禁用,无法查看.");
	else if(!RunLogEntry.Data.DataSec.IsRunlogHasContent)
	  UARTPuts("\r\n当前运行日志内容为空因此无法查看,请让手电运行一会后再试.");
	else
	  {
		//正常显示
		  UARTPuts("\r\n----------- 系统运行日志查看器(详细视图) -----------\r\n");
		UartPrintf("\r\n  运行日志CRC-32 : 0x%08X\r\n",CalcRunLogCRC32(&RunLogEntry.Data));	
		PrintStatuBar("系统主LED");
		UartPrintf("\r\n  LED平均电流 : %.2fA",RunLogEntry.Data.DataSec.AverageLEDIf);
		UartPrintf("\r\n  LED最大电流 : %.2fA",RunLogEntry.Data.DataSec.MaximumLEDIf);	
		UartPrintf("\r\n  LED平均压降 : %.2fV",RunLogEntry.Data.DataSec.AverageLEDVf);
		UartPrintf("\r\n  LED最大压降 : %.2fV",RunLogEntry.Data.DataSec.AverageLEDVf);	
		UartPrintf("\r\n  LED平均功率 : %.2fW",RunLogEntry.Data.DataSec.AverageLEDIf*RunLogEntry.Data.DataSec.AverageLEDVf);
		UartPrintf("\r\n  LED最大功率 : %.2fW",RunLogEntry.Data.DataSec.MaximumLEDIf*RunLogEntry.Data.DataSec.AverageLEDVf);		
			
		if(RunLogEntry.Data.DataSec.AverageLEDTemp!=NAN)
		  UartPrintf("\r\n  LED平均运行温度 : %.2f'C",RunLogEntry.Data.DataSec.AverageLEDTemp);
		else
			UARTPuts("\r\n  LED平均运行温度 : 数据不可用");
		if(RunLogEntry.Data.DataSec.MaximumLEDTemp!=NAN)
		  UartPrintf("\r\n  LED最高运行温度 : %.2f'C",RunLogEntry.Data.DataSec.MaximumLEDTemp);				
	  else
			UARTPuts("\r\n  LED最高运行温度 : 数据不可用");
	  UARTPuts("\r\n  LED总计运行时长 : ");DisplayLampTime(RunLogEntry.Data.DataSec.LEDRunTime);
		PrintStatuBar("驱动模块");
		if(RunLogEntry.Data.DataSec.AverageSPSTemp!=NAN)
		  UartPrintf("\r\n  驱动MOS平均运行温度 : %.2f'C",RunLogEntry.Data.DataSec.AverageSPSTemp);
		else
			UARTPuts("\r\n  驱动MOS平均运行温度 : 数据不可用");
		if(RunLogEntry.Data.DataSec.MaximumSPSTemp!=NAN)
		  UartPrintf("\r\n  驱动MOS最高运行温度 : %.2f'C",RunLogEntry.Data.DataSec.MaximumSPSTemp);				
	  else
			UARTPuts("\r\n  驱动MOS最高运行温度 : 数据不可用");
		UartPrintf("\r\n  驱动温度降档比例 : %.1f%%",RunLogEntry.Data.DataSec.ThermalStepDownValue);	
	  UartPrintf("\r\n  驱动平均运行效率 : %.1f%%",RunLogEntry.Data.DataSec.AverageDriverEfficiency);
	  UartPrintf("\r\n  无极调光亮度百分比 : %.1f%%",RunLogEntry.Data.DataSec.RampModeConf*(float)100);
		UartPrintf("\r\n  目前无极调光方向 : %s",RunLogEntry.Data.DataSec.RampModeDirection?"从暗到亮":"从亮到暗");
		UartPrintf("\r\n  驱动是否锁定 : %s",RunLogEntry.Data.DataSec.IsFlashLightLocked?"是":"否");
		PrintStatuBar("电池输入");
	  UartPrintf("\r\n  电池输入低压告警 : %s",RunLogEntry.Data.DataSec.IsLowVoltageAlert?"是":"否");
	  UartPrintf("\r\n  电池最低电压 : %.3fV",RunLogEntry.Data.DataSec.MinimumBatteryVoltage);
    UartPrintf("\r\n  电池平均电压 : %.3fV",RunLogEntry.Data.DataSec.AverageBatteryVoltage);
		UartPrintf("\r\n  电池最高电压 : %.3fV",RunLogEntry.Data.DataSec.MaximumBatteryVoltage);	
		UartPrintf("\r\n  电池平均输出电流 : %.2fA",RunLogEntry.Data.DataSec.AverageBatteryCurrent);
		UartPrintf("\r\n  电池最大输出电流 : %.2fA",RunLogEntry.Data.DataSec.MaximumBatteryCurrent);	
		UartPrintf("\r\n  电池平均输出功率 : %.2fW",RunLogEntry.Data.DataSec.AverageBatteryPower);
		UartPrintf("\r\n  电池最大输出功率 : %.2fW",RunLogEntry.Data.DataSec.MaximumBatteryPower);	
    UartPrintf("\r\n  电池总放电量 : %.1fmAH",RunLogEntry.Data.DataSec.TotalBatteryCapDischarged);			
		if(RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone)
		  {
			UartPrintf("(相当于电池的%.3f个循环)",RunLogEntry.Data.DataSec.TotalBatteryCapDischarged/(double)RunLogEntry.Data.DataSec.BattUsage.DesignedCapacity);
			UartPrintf("\r\n  电池设计容量 : %.1fmAH",RunLogEntry.Data.DataSec.BattUsage.DesignedCapacity);
			UartPrintf("\r\n  当前已用容量 : %.1fmAH",UsedCapacity);
			}
		UartPrintf("\r\n  库仑计校准完毕 : %s",RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone?"是":"否");
		UartPrintf("\r\n  库仑计自学习已启用 : %s\r\n",RunLogEntry.Data.DataSec.BattUsage.IsLearningEnabled?"是":"否");
		}
	//命令完毕的回调处理	 
  ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕	
	}
