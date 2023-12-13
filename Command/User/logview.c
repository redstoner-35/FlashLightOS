#include "console.h"
#include "CfgFile.h"
#include "logger.h"
#include "ADC.h"
#include <stdlib.h>
#include <math.h>

//警告和其他可重用的字符串
static const char *LogVIewAlertStr[]=
  {
	"\r\n  输入电压过低警告 : %s", //0
	"\r\n  输入电压过低故障 : %s", //1
	"\r\n  输入电压超压故障 : %s", //2
	"\r\n  输入过流故障 : %s", //3
  "\r\n   ", //4
  "未知", //5
  "\r\n----------- 系统错误日志查看器(%s视图) -----------\r\n",//6
  "\033[40;31m是\033[0m", //7
  "\033[40;32m否\033[0m"  //8
	};

//参数帮助entry
const char *logviewArgument(int ArgCount)
  {
	return "查看指定条目的详细数据";
	}

//显示遥测结果
static void DisaplayTelemResult(float Value,const char *Unit,const char *Name,bool DataOK)
  {
	UartPrintf("%s%s : ",LogVIewAlertStr[4],Name);
	if(DataOK&&Value!=NAN)UartPrintf("%.2f%s",Value,Unit);	
	else UARTPuts("数据不可用");
	}
	
//主处理函数
void logviewhandler(void)
  {
  char *ParamPtr;	
  int i,errorcount;
	LoggerDataUnion LogData;
	const char *ErrorString;
	const char *PstateString;
	const char *ModeGroupString;
	ParamPtr=IsParameterExist("01",11,NULL);
  if(ParamPtr==NULL)//简易显示
	  {
	  //统计日志中的错误项
	  errorcount=0;
	  for(i=0;i<MaximumLoggerDepth;i++)
			if(LoggerHdr.LoggerHeader.IsEntryHasError[i])errorcount++;
	  //显示
		UartPrintf((char *)LogVIewAlertStr[6],"概述");
		UartPrintf("\r\n   目前日志数: %d / %d    ",errorcount,MaximumLoggerDepth);
		UartPrintf("(已用%.1f%%)\r\n",((float)errorcount/(float)MaximumLoggerDepth)*(float)100);
		UARTPuts((char *)LogVIewAlertStr[4]);UARTPutc('-',46);
		UARTPuts("\r\n   | 日志编号 |  CRC-32校验和 |  是否有错误记录 |");
		UARTPuts((char *)LogVIewAlertStr[4]);UARTPutc('-',46);
		errorcount=LoggerHdr.LoggerHeader.CurrentLoggerIndex-1;
		if(errorcount<=0)errorcount=MaximumLoggerDepth-1; //计算最新的日志所在的位置	
		for(i=0;i<MaximumLoggerDepth;i++)
			{
	    if(i==errorcount)UartPrintf("%s|$ NO. %d",LogVIewAlertStr[4],i);		
	    else UartPrintf("%s|  NO. %d",LogVIewAlertStr[4],i);
		  if(i<10)UARTPuts("   |");
		  else UARTPuts("  |"); //打印日志编号
		  UartPrintf("   0x%08X  |",LoggerHdr.LoggerHeader.EntryCRC[i]);
		  UartPrintf("  %s   |",LoggerHdr.LoggerHeader.IsEntryHasError[i]?"\033[40;31m存在错误记录\033[0m":"日志条目为空");
			}
		UARTPuts((char *)LogVIewAlertStr[4]);UARTPutc('-',46);
		}		
	 else //详细显示
	  {
		i=!CheckIfParamOnlyDigit(ParamPtr)?atoi(ParamPtr):-1;
		if(i>MaximumLoggerDepth-1||i<0)
		 {
		 UartPrintf("\r\n错误:您指定的日志编号在系统中不存在!系统中包含的日志编号为0-%d.",MaximumLoggerDepth-1);
		 ClearRecvBuffer();//清除接收缓冲
     CmdHandle=Command_None;//命令执行完毕	
		 return;
		 }
    if(!FetchLoggerData(&LogData,i))
     {
		 UARTPuts("\r\n获取日志信息时发生错误,请重试.");
		 ClearRecvBuffer();//清除接收缓冲
     CmdHandle=Command_None;//命令执行完毕	
		 return;
	   }
		if(LogData.LoggerDateSection.ErrorCode==Error_None)
		 UARTPuts("\r\n您指定的日志中没有内容.");
		else
		 {
		 //判断日志的参数
		 switch(LogData.LoggerDateSection.ErrorCode)//根据错误代码显示的LED状态代码
			  {
			  case Error_Input_OVP:ErrorString="输入电压过高保护";break;//输入过压保护
			  case Error_Thremal_Logic:ErrorString="内部温控降档逻辑异常(温控表阈值参数错误)"; //温控逻辑异常
				case Error_PWM_Logic:ErrorString="内部PWM逻辑异常";  //PWM逻辑异常	
				case Error_SPS_CATERR:ErrorString="智能功率级(驱动内部)灾难性故障";break; //智能功率级反馈灾难性错误
				case Error_SPS_ThermTrip:ErrorString="智能功率级(驱动内部)过热保护";break; //智能功率级过热保护
				case Error_LED_Open:ErrorString="LED灯珠压降过高或开路";break; //灯珠开路
				case Error_LED_Short:ErrorString="LED灯珠短路";break; //灯珠短路
				case Error_LED_OverCurrent:ErrorString="LED灯珠过流保护";break; //灯珠过流保护
				case Error_LED_ThermTrip:ErrorString="LED灯珠严重过热保护";break; //灯珠过热保护
				case Error_Input_UVP:ErrorString="输入电压过低,欠压保护";break; //输入欠压保护
				case Error_DAC_Logic:ErrorString="内部线性调光DAC异常";break; //DAC异常
				case Error_ADC_Logic:ErrorString="内部测量ADC异常";break; //ADC异常
				case Error_Mode_Logic:ErrorString="驱动挡位配置异常";break; //驱动挡位配置异常
				case Error_Input_OCP:ErrorString="输入电流超过允许值";break; //输入过流
				case Error_SPS_TMON_Offline:ErrorString="智能功率级(驱动内部)温度反馈输出异常";break; //温度反馈异常
				default:ErrorString=LogVIewAlertStr[5];
				}	 
		 switch(LogData.LoggerDateSection.SystemPstate)
		    {
			  case PState_DeepSleep:PstateString="深度睡眠";break;
				case PState_Error:PstateString="故障闭锁";break;
				case PState_LEDOn:PstateString="正常工作";break;
				case PState_LEDOnNonHold:PstateString="点射(战术模式)";break;
				case PState_NonHoldStandBy:PstateString="战术模式(待机)";break;
				case PState_Standby:PstateString="正常模式(待机)";break;
			  case PState_Locked:PstateString="锁定状态";break;
				default:PstateString=LogVIewAlertStr[5];
				}
		 switch(LogData.LoggerDateSection.CurrentModeSel.ModeGrpSel)
		    {
			  case ModeGrp_Regular:ModeGroupString="常规挡位组";break;
				case ModeGrp_DoubleClick:ModeGroupString="双击功能挡位";break;
				case ModeGrp_Special:ModeGroupString="特殊功能挡位组";break;
				default:ModeGroupString=LogVIewAlertStr[5];
				}
		 errorcount=LoggerHdr.LoggerHeader.CurrentLoggerIndex-1;
		 if(errorcount<=0)errorcount=MaximumLoggerDepth-1; //计算最新的日志所在的位置
		 //显示
		 UartPrintf((char *)LogVIewAlertStr[6],"详细");
		 UartPrintf("%s日志CRC32数值 : 0x%08X",LogVIewAlertStr[4],calculateLogEntryCRC32(&LogData));
		 UartPrintf("%s错误类型 : %s",LogVIewAlertStr[4],ErrorString);
		 UartPrintf("%s错误位置 : %s",LogVIewAlertStr[4],LogData.LoggerDateSection.ErrorStageText);
		 UartPrintf("%s------------  详细参数记录  ------------",LogVIewAlertStr[4]);
		 UartPrintf("%s系统电源状态(P-State) : %s",LogVIewAlertStr[4],PstateString);
		 UartPrintf("%s调光模式 : %s",LogVIewAlertStr[4],LogData.LoggerDateSection.IsLinearDim?"线性":"PWM");
     UartPrintf("%s温度降档幅度 : %.2f%%",LogVIewAlertStr[4],LogData.LoggerDateSection.CurrentThrottleLevel);	
     UartPrintf("%s线性调光DAC输出 : %.1fmV",LogVIewAlertStr[4],LogData.LoggerDateSection.CurrentDACVID);			 
		 UartPrintf("%sLED基板NTC温度检测状态 : %s",LogVIewAlertStr[4],NTCStateString[LogData.LoggerDateSection.LEDBaseNTCStatu]);
     UartPrintf("%s驱动SPS温度检测状态 : %s",LogVIewAlertStr[4],SPSTMONString[LogData.LoggerDateSection.SPSTMONStatu]);			
		 UartPrintf("%s目标的挡位组 : %s",LogVIewAlertStr[4],ModeGroupString);	
		 if(LogData.LoggerDateSection.CurrentModeSel.ModeGrpSel==ModeGrp_Regular)
				 errorcount=LogData.LoggerDateSection.CurrentModeSel.RegularGrpMode;
		 else
				 errorcount=LogData.LoggerDateSection.CurrentModeSel.SpecialGrpMode; //获取挡位数量
		 if(LogData.LoggerDateSection.CurrentModeSel.ModeGrpSel!=ModeGrp_DoubleClick)
			   UartPrintf("%s目标的挡位 : 第%d个挡位",LogVIewAlertStr[4],errorcount+1);
     //显示遥测数据
     DisaplayTelemResult(LogData.LoggerDateSection.DriverTelem.LEDTemp,"'C","LED基板温度",true);			
		 DisaplayTelemResult(LogData.LoggerDateSection.DriverTelem.LEDVf,"V","LED两端电压(驱动输出)",true);	
		 DisaplayTelemResult(LogData.LoggerDateSection.DriverTelem.LEDIf,"A","LED电流(驱动输出)",true);		
		 DisaplayTelemResult(LogData.LoggerDateSection.DriverTelem.SPSTemp,"'C","驱动集成MOS温度",true);				
		 DisaplayTelemResult(LogData.LoggerDateSection.DriverTelem.BattVoltage,"V","驱动输入电压",LogData.LoggerDateSection.IsBattTelemWorking);
		 DisaplayTelemResult(LogData.LoggerDateSection.DriverTelem.BattCurrent,"A","驱动输入电流",LogData.LoggerDateSection.IsBattTelemWorking);
		 DisaplayTelemResult(LogData.LoggerDateSection.DriverTelem.BattPower,"W","驱动输入功率",LogData.LoggerDateSection.IsBattTelemWorking);
		 if(LogData.LoggerDateSection.IsBattTelemWorking)
		 	 {
			 UartPrintf((char *)LogVIewAlertStr[0],LogVIewAlertStr[LogData.LoggerDateSection.DriverTelem.BatteryVoltageAlert?7:8]);
		   UartPrintf((char *)LogVIewAlertStr[1],LogVIewAlertStr[LogData.LoggerDateSection.DriverTelem.BatteryVoltageUVLO?7:8]);
			 UartPrintf((char *)LogVIewAlertStr[2],LogVIewAlertStr[LogData.LoggerDateSection.DriverTelem.BatteryVoltageOVLO?7:8]);
			 UartPrintf((char *)LogVIewAlertStr[3],LogVIewAlertStr[LogData.LoggerDateSection.DriverTelem.BatteryOverCurrent?7:8]);
			 }
     else for(i=0;i<4;i++)UartPrintf((char *)LogVIewAlertStr[3],"未知");
		 }
		}	
  //命令完毕的回调处理	 
  ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕					
	}
