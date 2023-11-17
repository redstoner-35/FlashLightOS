#include "console.h"
#include "CfgFile.h"
#include "ADC.h"
#include <stdlib.h>
#include <string.h>

//字符串
#ifdef FlashLightOS_Debug_Mode
static const char *IllegalTripTemperature="您设置的%s阈值不合法.合法的数值范围是%d-%d(摄氏度).";
static const char *TripTemperatureHasSet="%s阈值已被成功更新为%d摄氏度.";
#endif

//参数帮助entry
const char *thremaltripcfgArgument(int ArgCount)
  {
	#ifdef FlashLightOS_Debug_Mode
	switch(ArgCount)
	  {
    case 0:
		case 1:return "设置LED过热关机的极限温度.";
    case 2:
		case 3:return "设置驱动MOS过热关机的极限温度";
		}
	#endif
	return NULL;
	}

void thermaltripcfgHandler(void)
  {
	#ifdef FlashLightOS_Debug_Mode
	int buf;
	char *Param;
  bool IsCmdParamOK=false;
	//设置LED过热关机温度
	Param=IsParameterExist("01",22,NULL);
	if(Param!=NULL)
	  {
		IsCmdParamOK=true;
		if(!CheckIfParamOnlyDigit(Param))buf=atoi(Param);
		else buf=-1;
		if(buf>90||buf<60)
			UartPrintf((char *)IllegalTripTemperature,"LED过热关机",60,90);
		else
		  {
			UartPrintf((char *)TripTemperatureHasSet,"LED过热关机",buf);
			CfgFile.LEDThermalTripTemp=(char)buf;
			}
		}	
	//设置MOSFET过热关机温度
	Param=IsParameterExist("23",22,NULL);
	if(Param!=NULL)
	  {
		IsCmdParamOK=true;
		if(!CheckIfParamOnlyDigit(Param))buf=atoi(Param);
		else buf=-1;
		if(buf>110||buf<80)
			UartPrintf((char *)IllegalTripTemperature,"驱动过热关机",80,110);
		else
		  {
			UartPrintf((char *)TripTemperatureHasSet,"驱动过热关机",buf);
			CfgFile.MOSFETThermalTripTemp=(char)buf;
			}
		}	
	if(!IsCmdParamOK)UartPrintCommandNoParam(21);//显示啥也没找到的信息
	#endif
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕
	}
