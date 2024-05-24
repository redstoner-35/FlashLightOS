#include "console.h"
#include "CfgFile.h"
#include "runtimelogger.h"
#include <stdlib.h>
#include <math.h>
#include "ADC.h"

static const char *Rlvstr[]=
{
"\r\n  LED",//0
"\r\n  驱动",//1
"\r\n  ",//2
"\r\n  电池",//3
"数据不可用",//4
"%.2f'C",//5
"\r\n  %s告警次数 : %d 次",//6
"\r\n  运行日志", //7
"\r\n  库仑计", //8
"\r\n由于运行日志%s,故无法查看.",//9
};

static void DisplayLampTime(double time)
  {
  //秒	
	if(time<60)UartPrintf("%ld秒",(long)time%60);
	//分钟+秒
	else if(time<(60*60))UartPrintf("%ld分%ld秒",(long)time/60,(long)time%60);
	//小时分钟+秒
	else UartPrintf("%ld小时%ld分%ld秒",(long)time/3600,((long)time%3600)/60,((long)time%3600)%60);
	}
static void PrintStatuBar(char *CompType)
  {
	UARTPuts("\r\n  ");
	UARTPutc('-',12);
	UartPrintf("  %s信息  ",CompType);
	UARTPutc('-',12);
	}
//外部变量和函数声明
extern float UsedCapacity;
float GetActualTemp(ADCOutTypeDef *ADCResult);
	
//命令处理主函数
void runlogviewHandler(void)
  {
	float buf,DriverIntTemp,DriverMaxIntTemp;
	ADCOutTypeDef ADCO;
	//显示数据
	if(!IsRunTimeLoggingEnabled)
		UartPrintf((char *)Rlvstr[9],"已被管理员禁用");
	else if(!RunLogEntry.Data.DataSec.IsRunlogHasContent)
	  UartPrintf((char *)Rlvstr[9],"内容为空");
	else
	  {
		//填写结构体并运算出驱动加权平均温度
	  ADCO.NTCState=RunLogEntry.Data.DataSec.AverageLEDTemp!=NAN?LED_NTC_OK:LED_NTC_Open;
		ADCO.SPSTMONState=RunLogEntry.Data.DataSec.AverageSPSTemp!=NAN?SPS_TMON_OK:SPS_TMON_Disconnect; //根据LED和SPS的温度数据设置传感器状态
    ADCO.LEDTemp=RunLogEntry.Data.DataSec.AverageLEDTemp;		
	  ADCO.SPSTemp=RunLogEntry.Data.DataSec.AverageSPSTemp;
		DriverIntTemp=GetActualTemp(&ADCO);
	  ADCO.NTCState=RunLogEntry.Data.DataSec.MaximumLEDTemp!=NAN?LED_NTC_OK:LED_NTC_Open;
		ADCO.SPSTMONState=RunLogEntry.Data.DataSec.MaximumSPSTemp!=NAN?SPS_TMON_OK:SPS_TMON_Disconnect; //根据LED和SPS的温度数据设置传感器状态
    ADCO.LEDTemp=RunLogEntry.Data.DataSec.MaximumLEDTemp;		
	  ADCO.SPSTemp=RunLogEntry.Data.DataSec.MaximumSPSTemp;			
		DriverMaxIntTemp=GetActualTemp(&ADCO);	
		//开始显示系统日志
		UARTPuts("\r\n");
		UARTPutc('-',16);
		UARTPuts(" 系统运行日志查看器 ");
		UARTPutc('-',16);
		UartPrintf("\r\n%sCRC-32 : 0x%08X",Rlvstr[7],CalcRunLogCRC32(&RunLogEntry.Data));	
		#ifdef FlashLightOS_Init_Mode 
		UartPrintf("%s总记录次数 : %d次",Rlvstr[7],RunLogEntry.Data.DataSec.TotalLogCount);	
		UartPrintf("%s序号 : #%d\r\n",Rlvstr[7],RunLogEntry.Data.DataSec.LogIncrementCode);		
		#endif
		PrintStatuBar("系统主LED");
		UartPrintf("%s平均/最大电流 : %.2fA / %.2fA",Rlvstr[0],RunLogEntry.Data.DataSec.AverageLEDIf,RunLogEntry.Data.DataSec.MaximumLEDIf);
		UartPrintf("%s平均/最大压降 : %.2fV / %.2fV",Rlvstr[0],RunLogEntry.Data.DataSec.AverageLEDVf,RunLogEntry.Data.DataSec.MaximumLEDVf);
		buf=RunLogEntry.Data.DataSec.MaximumLEDIf*RunLogEntry.Data.DataSec.MaximumLEDVf; //计算最大功率
		UartPrintf("%s平均/最大功率 : %.2fW / %.2fW",Rlvstr[0],RunLogEntry.Data.DataSec.AverageLEDIf*RunLogEntry.Data.DataSec.AverageLEDVf,buf);
		UartPrintf("%s平均/最高运行温度 : ",Rlvstr[0]);	
		if(RunLogEntry.Data.DataSec.AverageLEDTemp!=NAN)
		  UartPrintf((char *)Rlvstr[5],RunLogEntry.Data.DataSec.AverageLEDTemp);
		else
			UARTPuts((char *)Rlvstr[4]);
		UARTPuts(" / ");
		if(RunLogEntry.Data.DataSec.MaximumLEDTemp!=NAN)
		  UartPrintf((char *)Rlvstr[5],RunLogEntry.Data.DataSec.MaximumLEDTemp);				
	  else
			UARTPuts((char *)Rlvstr[4]);
	  UARTPuts("\r\n  LED总运行时长 : ");DisplayLampTime(RunLogEntry.Data.DataSec.LEDRunTime);
		PrintStatuBar("驱动模块");
		UartPrintf("\r\n  手电内部加权平均/最高温度 :  %.2f'C / %.2f'C",DriverIntTemp,DriverMaxIntTemp);
		UartPrintf("%sMOS平均/最高运行温度 : ",Rlvstr[1]);
		if(RunLogEntry.Data.DataSec.AverageSPSTemp!=NAN)
		  UartPrintf((char *)Rlvstr[5],RunLogEntry.Data.DataSec.AverageSPSTemp);
		else
			UARTPuts((char *)Rlvstr[4]);
	  UARTPuts(" / ");
		if(RunLogEntry.Data.DataSec.MaximumSPSTemp!=NAN)
		  UartPrintf((char *)Rlvstr[5],RunLogEntry.Data.DataSec.MaximumSPSTemp);				
	  else
			UARTPuts((char *)Rlvstr[4]);
		UartPrintf("%s平均温度降档比例 : %.1f%%",Rlvstr[1],RunLogEntry.Data.DataSec.ThermalStepDownValue);	
	  UartPrintf("%s平均/峰值运行效率 : ",Rlvstr[1]);
		if(RunLogEntry.Data.DataSec.AverageDriverEfficiency!=101&&RunLogEntry.Data.DataSec.MaximumEfficiency!=101) //当前效率计算值有效,显示效率	
		  UartPrintf("%.1f%% / %.1f%%",RunLogEntry.Data.DataSec.AverageDriverEfficiency,RunLogEntry.Data.DataSec.MaximumEfficiency);
		else
			UARTPuts((char *)Rlvstr[4]); //显示未知
		UartPrintf("%s强制极亮次数 : %d",Rlvstr[2],RunLogEntry.Data.DataSec.TotalMomtTurboCount);
	  UartPrintf("%s是否锁定 : %s",Rlvstr[2],RunLogEntry.Data.DataSec.IsFlashLightLocked?"是":"否");
		PrintStatuBar("电池输入");
	  UartPrintf("%s放电能力/输入低压告警 : %s / %s",Rlvstr[3],RunLogEntry.Data.DataSec.IsLowQualityBattAlert?"是":"否",RunLogEntry.Data.DataSec.IsLowVoltageAlert?"是":"否");
	  UartPrintf("%s最低/平均电压 : %.3fV / %.3fV",Rlvstr[3],RunLogEntry.Data.DataSec.MinimumBatteryVoltage,RunLogEntry.Data.DataSec.AverageBatteryVoltage);
		UartPrintf("%s最高电压 : %.3fV",Rlvstr[3],RunLogEntry.Data.DataSec.MaximumBatteryVoltage);	
		UartPrintf("%s平均/最大输出电流 : %.2fA / %.2fA",Rlvstr[3],RunLogEntry.Data.DataSec.AverageBatteryCurrent,RunLogEntry.Data.DataSec.MaximumBatteryCurrent);
		UartPrintf("%s平均/最大输出功率 : %.2fW / %.2fW",Rlvstr[3],RunLogEntry.Data.DataSec.AverageBatteryPower,RunLogEntry.Data.DataSec.MaximumBatteryPower);
    UartPrintf("%s总放电量 : %.1fmAH",Rlvstr[3],RunLogEntry.Data.DataSec.TotalBatteryCapDischarged);			
		if(RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone)
		  {
			UartPrintf("(相当于电池的%.3f个循环)",RunLogEntry.Data.DataSec.TotalBatteryCapDischarged/(double)RunLogEntry.Data.DataSec.BattUsage.DesignedCapacity);
			UartPrintf("%s设计容量 : %.1fmAH",Rlvstr[3],RunLogEntry.Data.DataSec.BattUsage.DesignedCapacity);
			UartPrintf("%s已用容量 : %.1fmAH",Rlvstr[3],UsedCapacity);
			}
		UartPrintf("%s校准完毕 : %s",Rlvstr[8],RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone?"是":"否");
		UartPrintf("%s自学习已启用 : %s\r\n",Rlvstr[8],RunLogEntry.Data.DataSec.BattUsage.IsLearningEnabled?"是":"否");
		PrintStatuBar("告警统计");
		UartPrintf((char *)Rlvstr[6],"电池欠压",RunLogEntry.Data.DataSec.LowVoltageShutDownCount);
		UartPrintf((char *)Rlvstr[6],"LED过流或电池限流(包括过载)",RunLogEntry.Data.DataSec.OCPFaultCount);	
		UartPrintf((char *)Rlvstr[6],"LED开/短路",RunLogEntry.Data.DataSec.LEDOpenShortCount);
		UartPrintf((char *)Rlvstr[6],"LED过热",RunLogEntry.Data.DataSec.LEDThermalFaultCount);
		UartPrintf((char *)Rlvstr[6],"驱动过热",RunLogEntry.Data.DataSec.DriverThermalFaultCount);
		UartPrintf((char *)Rlvstr[6],"其余",RunLogEntry.Data.DataSec.OtherFaultCount);
		}
	//命令完毕的回调处理	 
  ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕	
	}
