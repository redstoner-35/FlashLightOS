#include "console.h"
#include "cfgfile.h"
#include "runtimelogger.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//外部变量
const char *BattParamHasUpdated="\r\n电池的%s电压值已被更新为%.2fV.";
const char *IllegalVoltageConfig="\r\n您应当指定一个在%.1f-%.1f(V)之间的数值作为电池的%s值.";
const char *BatteryVoltagePoint="\r\n 电池%s电压 : %.2fV";
const char *FailedToDoSthColumb="\r\n由于运行日志(含库仑计)已被禁用,故您无法对库仑计进行%s操作.\r\n";
extern float UsedCapacity;
extern float MaximumBatteryPower;
#ifndef Firmware_DIY_Mode
extern const char *NeedRoot;
#endif

//参数帮助entry
const char *battcfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return "指定电池组的低电压警告电压(低于此电压后驱动减少输出电流)";
		case 2:
		case 3:return "指定电池组的低电压关机保护电压.";
    case 4:
		case 5:return "设置电池组的满电电压";
		case 6:
		case 7:return "设置电池输入的过压保护电压";
		case 8:
		case 9:return "启用库仑计的自学习模式学习电池容量";
		case 10:
		case 11:return "手动设置库仑计的设计电池容量.";
		case 12:
		case 13:return "复位库仑计的电池统计数据以及电池质量警告";
	  case 14:
		case 15:return "查看系统中电池的参数和库仑计的统计信息";
		}
	return NULL;
	}
	
//命令处理主函数
void battcfghandler(void)
  {
	bool IsCmdParamOK=false;
	char *Param;
	char ParamOK;
	float buf;
	//查看电池参数和库仑计状态(-v)
	IsParameterExist("EF",24,&ParamOK);
	if(ParamOK)
	  {
		UARTPuts("\r\n---------- 电池参数查看器 ----------\r\n");
		UartPrintf((char *)BatteryVoltagePoint,"过压保护",CfgFile.VoltageOverTrip);	
		UartPrintf((char *)BatteryVoltagePoint,"满电",CfgFile.VoltageFull);
		UartPrintf((char *)BatteryVoltagePoint,"低电量警告",CfgFile.VoltageAlert);
		UartPrintf((char *)BatteryVoltagePoint,"低电量保护",CfgFile.VoltageTrip);
		UartPrintf("\r\n 电池最高允许功率 : %.2fW",MaximumBatteryPower);
		UARTPuts("\r\n   ------ 库仑计信息 ------\r\n");
		if(!IsRunTimeLoggingEnabled)//日志被关闭
		  UARTPuts("\r\n  由于运行日志已被禁用\r\n库仑计功能不可用.\r\n");
		else //正常显示
		  {
			UartPrintf("\r\n 库仑计自学习已启用 : %s\r\n",RunLogEntry.Data.DataSec.BattUsage.IsLearningEnabled?"是":"否");
		  if(RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone)
		    {
			  UartPrintf("\r\n 电池设计容量 : %.1fmAH",RunLogEntry.Data.DataSec.BattUsage.DesignedCapacity);
			  UartPrintf("\r\n 当前已用容量 : %.1fmAH",UsedCapacity);
			  }
		  UartPrintf("\r\n 库仑计校准完毕 : %s",RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone?"是":"否");
			}
		IsCmdParamOK=true;
		}
	//复位库仑计的统计数据
	IsParameterExist("CD",24,&ParamOK);
	#ifndef Firmware_DIY_Mode
  if(ParamOK&&AccountState!=Log_Perm_Root)
     {
		 if(RunLogEntry.Data.DataSec.IsLowQualityBattAlert)
		   {
	     CalcLastLogCRCBeforePO();//计算旧的CRC-32值
		   RunLogEntry.Data.DataSec.IsLowQualityBattAlert=false;//重置警告
		   RunLogEntry.CurrentDataCRC=CalcRunLogCRC32(&RunLogEntry.Data);//算新的CRC-32值
		   WriteRuntimeLogToROM();//更新运行日志数据
		   UARTPuts("\r\n电池质量警告已被复位.\r\n");
		   }
		 else
	     UartPrintf((char *)NeedRoot,"重置系统中库仑计的统计信息!"); 
	   IsCmdParamOK=true;
	   }
  else if(ParamOK)
  #else
  if(ParamOK)
  #endif
	  {
		if(!IsRunTimeLoggingEnabled)//日志被关闭
		  UartPrintf((char *)FailedToDoSthColumb,"复位");
		else if(RunLogEntry.Data.DataSec.IsLowQualityBattAlert) //电池质量警告生效
		   {
			 CalcLastLogCRCBeforePO();//计算旧的CRC-32值
		   RunLogEntry.Data.DataSec.IsLowQualityBattAlert=false;//重置警告
		   RunLogEntry.CurrentDataCRC=CalcRunLogCRC32(&RunLogEntry.Data);//算新的CRC-32值
		   WriteRuntimeLogToROM();//更新运行日志数据
		   UARTPuts("\r\n电池质量警告已被复位.\r\n");
		   }
		else
		  {
			CalcLastLogCRCBeforePO();//计算旧的CRC-32值
			RunLogEntry.Data.DataSec.BattUsage.DesignedCapacity=3500;
	    RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone=false;
			RunLogEntry.Data.DataSec.BattUsage.IsLearningEnabled=false;
			RunLogEntry.Data.DataSec.TotalBatteryCapDischarged=0;//复位数据
			RunLogEntry.Data.DataSec.IsLowQualityBattAlert=false;//重置警告
			RunLogEntry.CurrentDataCRC=CalcRunLogCRC32(&RunLogEntry.Data);//算新的CRC-32值
			WriteRuntimeLogToROM();//更新运行日志数据
			UARTPuts("\r\n库仑计的统计数据已被复位,您需要重新运行一次电池容量自学习以启用库仑计.\r\n");
			}
		IsCmdParamOK=true;
		}
	//手动设置库仑计的电池容量
	Param=IsParameterExist("AB",24,&ParamOK);
	#ifndef Firmware_DIY_Mode
  if(Param!=NULL&&AccountState!=Log_Perm_Root)
     {
	   UartPrintf((char *)NeedRoot,"手动设置库仑计的设计电池容量信息!"); 
	   IsCmdParamOK=true;
	   }
  else if(Param!=NULL)
  #else
  if(Param!=NULL)
  #endif
	  {
		buf=atof(Param);
		if(!IsRunTimeLoggingEnabled)//日志被关闭
		  UartPrintf((char *)FailedToDoSthColumb,"参数设置");
		else if(buf==NAN||buf<2000||buf>100000)
		  {
			DisplayIllegalParam(Param,24,10);//显示用户输入了非法参数
			UARTPuts("\r\n您应当指定一个在2000-100000(mAH)之间的数值来手动设置库仑计的设计电池容量.");
			}
		 else //设置参数
		  {
			CalcLastLogCRCBeforePO();//计算旧的CRC-32值
			RunLogEntry.Data.DataSec.BattUsage.DesignedCapacity=buf;
			RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone=true;
			RunLogEntry.CurrentDataCRC=CalcRunLogCRC32(&RunLogEntry.Data);//算新的CRC-32值
			WriteRuntimeLogToROM();//更新运行日志数据
			UartPrintf("\r\n库仑计的设计电池容量已被设置为%.1fmAH(%.2fAH).",buf,buf/(float)1000);
			}
		IsCmdParamOK=true;
		}
	//启动自动的库仑计自学习功能
	IsParameterExist("89",24,&ParamOK);
	if(ParamOK)
	  {
		if(!IsRunTimeLoggingEnabled)//日志被关闭
		  UartPrintf((char *)FailedToDoSthColumb,"启用自学习");
		else
		  {
			CalcLastLogCRCBeforePO();//计算旧的CRC-32值
			RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone=false;
			RunLogEntry.Data.DataSec.BattUsage.IsLearningEnabled=true;
			RunLogEntry.CurrentDataCRC=CalcRunLogCRC32(&RunLogEntry.Data);//算新的CRC-32值
			WriteRuntimeLogToROM();//更新运行日志数据
			UARTPuts("\r\n库仑计的自学习模式已启用.");
			}
		IsCmdParamOK=true;
		}
	//设置电池组的过压保护值
  Param=IsParameterExist("67",24,&ParamOK);
  if(Param!=NULL)
	  {
		buf=atof(Param);
    if(buf==NAN||buf<=CfgFile.VoltageFull||buf>15.5)
		  {
			DisplayIllegalParam(Param,24,6);//显示用户输入了非法参数
			UartPrintf((char *)IllegalVoltageConfig,CfgFile.VoltageFull+0.05,15.5,"过压保护");
			}
		else
		  {
			CfgFile.VoltageOverTrip=buf;
			UartPrintf((char *)BattParamHasUpdated,"过压保护",CfgFile.VoltageOverTrip);
			}
		IsCmdParamOK=true;
		}
	//设置电池组的满电电压
	Param=IsParameterExist("45",24,&ParamOK);
  if(Param!=NULL)
	  {
		buf=atof(Param);
    if(buf==NAN||buf<=CfgFile.VoltageAlert||buf>=CfgFile.VoltageOverTrip)
		  {
			DisplayIllegalParam(Param,24,4);//显示用户输入了非法参数
			UartPrintf((char *)IllegalVoltageConfig,CfgFile.VoltageAlert+0.05,CfgFile.VoltageOverTrip-0.05);
			}
		else
		  {
			CfgFile.VoltageFull=buf;
			UartPrintf((char *)BattParamHasUpdated,"满电",CfgFile.VoltageFull);
			}
		IsCmdParamOK=true;
		}
	//设置电池组的低电压保护拉闸电压
	Param=IsParameterExist("23",24,&ParamOK);
  if(Param!=NULL)
	  {
		buf=atof(Param);
    if(buf==NAN||buf<6||buf>=CfgFile.VoltageAlert)
		  {
			DisplayIllegalParam(Param,24,2);//显示用户输入了非法参数
			UartPrintf((char *)IllegalVoltageConfig,6,CfgFile.VoltageAlert-0.05,"低电压断电保护门限");
			}
		else
		  {
			CfgFile.VoltageTrip=buf;
			UartPrintf((char *)BattParamHasUpdated,"过放保护",CfgFile.VoltageTrip);
			}
		IsCmdParamOK=true;
		}
	//设置电池组的低电压保护警告电压
	Param=IsParameterExist("01",24,&ParamOK);
  if(Param!=NULL)
	  {
		buf=atof(Param);
    if(buf==NAN||buf<CfgFile.VoltageTrip||buf>=CfgFile.VoltageFull)
		  {
			DisplayIllegalParam(Param,24,0);//显示用户输入了非法参数
			UartPrintf((char *)IllegalVoltageConfig,CfgFile.VoltageTrip+0.05,CfgFile.VoltageFull-0.05,"低电压警告门限");
			}
		else
		  {
			CfgFile.VoltageAlert=buf;
			UartPrintf((char *)BattParamHasUpdated,"低压警告",CfgFile.VoltageAlert);
			}
		IsCmdParamOK=true;
		}
	if(!IsCmdParamOK)UartPrintCommandNoParam(24);//显示啥也没找到的信息 
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕
	}
