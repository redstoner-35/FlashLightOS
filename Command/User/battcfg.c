#include "console.h"
#include "cfgfile.h"
#include "runtimelogger.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//外部变量
const char *IllegalVoltageConfig="\r\n您应当指定一个在%.1f-%.1f(V)之间的数值作为电池的%s值.";
const char *BatteryVoltagePoint="\r\n 电池%s电压 : %.2fV";
extern float UsedCapacity;
#ifndef Firmware_DIY_Mode
extern const char *NeedRoot;
#endif

//参数帮助entry
const char *battcfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return "指定电池组的低电压警告电压(低于此电压后驱动输出电流被限制以保护电池)";
		case 2:
		case 3:return "指定电池组的低电压保护电压(低于此电压后驱动自动关机以保护电池)";
    case 4:
		case 5:return "设置电池组的满电电压(用于提高电量估算的精确度)";
		case 6:
		case 7:return "设置电池输入的过压保护电压(高于此电压驱动将停止运行以避免驱动损坏.)";
		case 8:
		case 9:return "启用库仑计的自学习模式学习电池容量";
		case 10:
		case 11:return "手动设置库仑计的设计电池容量.";
		case 12:
		case 13:return "复位库仑计的电池统计和容量数据";
	  case 14:
		case 15:return "查看系统中电池的参数和库仑计的统计信息";
		case 16:
		case 17:return "设置电池组的过流保护电流阈值";
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
	//设置电池组的过流保护值
  Param=IsParameterExist("GH",24,&ParamOK);
  if(Param!=NULL)
	  {
		buf=atof(Param);
    if(buf==NAN||buf<10||buf>32)
		  {
			DisplayIllegalParam(Param,24,16);//显示用户输入了非法参数
			UARTPuts("\r\n您应当指定一个在10-32(A)之间的数值作为电池的过流保护值.");
			}
		else
		  {
			CfgFile.OverCurrentTrip=buf;
			UartPrintf("\r\n电池过流保护电流设置值已被更新为%.2fA.",CfgFile.OverCurrentTrip);
			}
		IsCmdParamOK=true;
		}
	//查看电池参数和库仑计状态(-v)
	IsParameterExist("EF",24,&ParamOK);
	if(ParamOK)
	  {
		UARTPuts("\r\n---------- 电池参数查看器 ----------\r\n");
		UartPrintf((char *)BatteryVoltagePoint,"过压保护",CfgFile.VoltageOverTrip);	
		UartPrintf((char *)BatteryVoltagePoint,"满电",CfgFile.VoltageFull);
		UartPrintf((char *)BatteryVoltagePoint,"低电量警告",CfgFile.VoltageAlert);
		UartPrintf((char *)BatteryVoltagePoint,"低电量保护",CfgFile.VoltageTrip);
		UartPrintf("\r\n 电池过流保护电流 : %.2fA",CfgFile.OverCurrentTrip);
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
	   UartPrintf((char *)NeedRoot,"重置系统中库仑计的统计信息!"); 
	   IsCmdParamOK=true;
	   }
  else if(ParamOK)
  #else
  if(ParamOK)
  #endif
	  {
		if(!IsRunTimeLoggingEnabled)//日志被关闭
		  UARTPuts("\r\n由于运行日志连带库仑计已被管理员禁用,因此您无法对库仑计进行复位操作.\r\n");
		else
		  {
			CalcLastLogCRCBeforePO();//计算旧的CRC-32值
			RunLogEntry.Data.DataSec.BattUsage.DesignedCapacity=3500;
	    RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone=false;
			RunLogEntry.Data.DataSec.BattUsage.IsLearningEnabled=false;
			RunLogEntry.Data.DataSec.TotalBatteryCapDischarged=0;//复位数据
			RunLogEntry.CurrentDataCRC=CalcRunLogCRC32(&RunLogEntry.Data);//算新的CRC-32值
			WriteRuntimeLogToROM();//更新运行日志数据
			UARTPuts("\r\n库仑计的统计数据已被复位且库仑计已暂时禁用,您需要重新运行一次电池容量自学习以启用库仑计.\r\n");
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
		  UARTPuts("\r\n由于运行日志连带库仑计已被管理员禁用,因此您无法设置库仑计的参数.\r\n");
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
		  UARTPuts("\r\n由于运行日志连带库仑计已被管理员禁用,因此您无法启用库仑计的自学习功能.\r\n");
		else
		  {
			CalcLastLogCRCBeforePO();//计算旧的CRC-32值
			RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone=false;
			RunLogEntry.Data.DataSec.BattUsage.IsLearningEnabled=true;
			RunLogEntry.CurrentDataCRC=CalcRunLogCRC32(&RunLogEntry.Data);//算新的CRC-32值
			WriteRuntimeLogToROM();//更新运行日志数据
			UARTPuts("\r\n库仑计的自学习模式已启用,在这期间,系统的容量测量功能将会暂时禁用.");
			UARTPuts("\r\n为了确保自学习过程顺利完成,请务必查看如下的步骤说明:");
			UARTPuts("\r\n1:将需要使用的电池从手电中拆下,在充电器上彻底将其充满电.");	
			UARTPuts("\r\n2:正常使用手电筒(不要开极亮)直到手电筒电池耗尽并自动关机(侧按按\r\n钮红色慢闪).");
		  UARTPuts("\r\n完成以上步骤后库仑计将完成校准并激活,为您提供准确的电量信息.");
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
			UartPrintf("\r\n电池过压保护电压值已被更新为%.2fV.",CfgFile.VoltageOverTrip);
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
			UartPrintf("\r\n电池组的满电电压值已被更新为%.2fV.",CfgFile.VoltageFull);
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
			UartPrintf("\r\n电池组的低电压断电保护门限值已被更新为%.2fV.",CfgFile.VoltageTrip);
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
			UartPrintf("\r\n电池组的低电压警告门限值已被更新为%.2fV.",CfgFile.VoltageAlert);
			}
		IsCmdParamOK=true;
		}
	if(!IsCmdParamOK)UartPrintCommandNoParam(24);//显示啥也没找到的信息 
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕
	}
