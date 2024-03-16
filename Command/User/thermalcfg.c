#include "console.h"
#include "CfgFile.h"
#include "ADC.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>


//字符串
static const char *IllegalTemperature="\r\n错误:PID温控模块的%s温度应当在%.1f-%.1f(单位摄氏度)之间!";
static const char *TemperatureHasSet="\r\nPID温控模块的启动温度阈值已被设置为%.1f摄氏度.";
static const char *IllegalPIDParam="\r\n错误:PID温控模块的%s参数值应在0-10之间!";
static const char *PIDParamHasSet="\r\nPID温控模块的%s参数值已被设置为%.2f.";
static const char *PIDTCName="\r\nPID温控";
static const char *LEDThermalWeight="%s降档权重: LED温度=%.2f%%,驱动MOS温度=%.2f%%";

//参数帮助entry
const char *thremalcfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
    case 0:
		case 1:return "设置LED基板和驱动MOS管温度的温度权重值";
    case 2:
		case 3:return "设置触发PID温控启动的手电内部温度阈值.";
		case 4:
		case 5:return "设置PID温控器的目标手电内部温度.";
		case 6:
		case 7:return "设置PID温控禁用的手电内部温度阈值.";	  
		case 8:
		case 9:return "设置PID温控的P参数";
		case 10:
		case 11:return "设置PID温控的I参数";	
		case 12:
		case 13:return "设置PID温控的D参数";	    		
		}
	return NULL;
	}

void thermalcfghandler(void)
  {
	char *Param;
	char ParamOK;
  float buf2,maxtemp;		
  bool IsCmdParamOK=false;
	ADCOutTypeDef ADCResult;
  //识别权重控制的参数
	Param=IsParameterExist("01",22,NULL);
  if(Param!=NULL)
	  {
		IsCmdParamOK=true;
    buf2=atof(Param);
		if(buf2==NAN||buf2<5||buf2>95)
		  {
			DisplayIllegalParam(Param,22,8);//显示用户输入了非法参数
			UARTPuts("\r\n您应当将指定一个5-95(单位%)的数值作为LED降档曲线的权重值.");
			}
		else
		  {
			CfgFile.LEDThermalWeight=buf2;
			UartPrintf((char *)LEDThermalWeight,"\r\n降档权重值已更新成功.新的",buf2,100-buf2);
			}
		}		
	//计算最高的温控触发温度
  if(!ADC_GetResult(&ADCResult))
	  {
		UARTPuts("错误:读取LED NTC状态时遇到错误.");
	  ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕
		return;
		}
  if(ADCResult.NTCState==LED_NTC_OK)//LED温度NTC就绪，取加权值
	  { 
		maxtemp=CfgFile.LEDThermalTripTemp*CfgFile.LEDThermalWeight/100;
		maxtemp+=CfgFile.MOSFETThermalTripTemp*(100-CfgFile.LEDThermalWeight)/100;
		}
	else maxtemp=CfgFile.MOSFETThermalTripTemp;//温度完全使用驱动MOS温度
	maxtemp-=15;//比最高温度低15度
	//设置PID触发的温度
	Param=IsParameterExist("23",22,NULL);
	if(Param!=NULL)
	  {
		IsCmdParamOK=true;
		buf2=atof(Param);
		if(buf2==NAN||buf2>=maxtemp||buf2<=CfgFile.PIDTargetTemp)
		   UartPrintf((char *)IllegalTemperature,"启动阈值",CfgFile.PIDTargetTemp+0.1,maxtemp-0.1);
		else
		  {
			UartPrintf((char *)TemperatureHasSet,"启动阈值",buf2);
			CfgFile.PIDTriggerTemp=buf2;
			}
		}
	//设置PID目标维持的温度
	Param=IsParameterExist("45",22,NULL);
	if(Param!=NULL)
	  {
		IsCmdParamOK=true;
		buf2=atof(Param);
		if(buf2==NAN||buf2>=CfgFile.PIDTriggerTemp||buf2<=CfgFile.PIDRelease)
		   UartPrintf((char *)IllegalTemperature,"目标维持",CfgFile.PIDRelease+0.1,CfgFile.PIDTriggerTemp-0.1);
		else
		  {
			UartPrintf((char *)TemperatureHasSet,"目标维持",buf2);
			CfgFile.PIDTargetTemp=buf2;
			}
		}
	//设置PID温控器停止介入的温度
	Param=IsParameterExist("67",22,NULL);
	if(Param!=NULL)
	  {
		IsCmdParamOK=true;
		buf2=atof(Param);
		if(buf2==NAN||buf2>=CfgFile.PIDTargetTemp||buf2<40)
			UartPrintf((char *)IllegalTemperature,"解除",40,CfgFile.PIDTargetTemp-0.1);
		else
		  {
			UartPrintf((char *)TemperatureHasSet,"解除",buf2);
			CfgFile.PIDRelease=buf2;
			}
		}	
	//显示温控器的参数
	IsParameterExist("EF",22,&ParamOK);
	if(ParamOK)
	  {
		IsCmdParamOK=true;
		UARTPuts("\r\n---------  手电温控参数设置值  ---------");
		#ifdef Firmware_DIY_Mode
		UartPrintf("%sKp值 : %.2f",PIDTCName,CfgFile.ThermalPIDKp);
		UartPrintf("%sKi值 : %.2f",PIDTCName,CfgFile.ThermalPIDKi);
		UartPrintf("%sKd值 : %.2f",PIDTCName,CfgFile.ThermalPIDKd);
		#else
		if(AccountState==Log_Perm_Root)
			{
		  UartPrintf("%sKp值 : %.2f",PIDTCName,CfgFile.ThermalPIDKp);
		  UartPrintf("%sKi值 : %.2f",PIDTCName,CfgFile.ThermalPIDKi);
		  UartPrintf("%sKd值 : %.2f",PIDTCName,CfgFile.ThermalPIDKd);	
		  }
		#endif
		UartPrintf("%s介入温度 : %.1f'C",PIDTCName,CfgFile.PIDTriggerTemp);
		UartPrintf("%s目标维持温度 : %.1f'C",PIDTCName,CfgFile.PIDTargetTemp);
		UartPrintf("%s停止降档温度 : %.1f'C",PIDTCName,CfgFile.PIDRelease);
		UartPrintf((char *)LEDThermalWeight,"\r\n",CfgFile.LEDThermalWeight,100-CfgFile.LEDThermalWeight);
		}	
	#ifndef Firmware_DIY_Mode
	IsParameterExist("6789AB",22,&ParamOK);
	if(ParamOK)
	  {	
		UARTPuts("\r\n错误:您需要厂家工程师权限以编辑PID温控模块的参数!");
		ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕
		return;
		}
	#endif
  //设置温控器的Kp
	Param=IsParameterExist("89",22,NULL);
	if(Param!=NULL)
	  {
		IsCmdParamOK=true;
		buf2=atof(Param);
		if(buf2==NAN||buf2>10||buf2<0)
		   UartPrintf((char *)IllegalPIDParam,"Kp");
		else
		  {
			UartPrintf((char *)PIDParamHasSet,"Kp",buf2);
			CfgFile.ThermalPIDKp=buf2;
			}
		}	
  //设置温控器的Ki
	Param=IsParameterExist("AB",22,NULL);
	if(Param!=NULL)
	  {
		IsCmdParamOK=true;
		buf2=atof(Param);
		if(buf2==NAN||buf2>10||buf2<0)
		   UartPrintf((char *)IllegalPIDParam,"Ki");
		else
		  {
			UartPrintf((char *)PIDParamHasSet,"Ki",buf2);
			CfgFile.ThermalPIDKi=buf2;
			}
		}	
	//设置温控器的Kd
	Param=IsParameterExist("CD",22,NULL);
	if(Param!=NULL)
	  {
		IsCmdParamOK=true;
		buf2=atof(Param); 
		if(buf2==NAN||buf2>10||buf2<0)
		   UartPrintf((char *)IllegalPIDParam,"Kd");
		else
		  {
			UartPrintf((char *)PIDParamHasSet,"Kd",buf2);
			CfgFile.ThermalPIDKd=buf2;
			}
		}	
  if(!IsCmdParamOK)UartPrintCommandNoParam(22);//显示啥也没找到的信息
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕
	}
