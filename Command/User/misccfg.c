#include "console.h"
#include "CfgFile.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//字符串
const char *OptionsOnlyAcceptNumber="\r\n该选项的参数仅接受范围从%d-%d(单位为%s)的纯数字输入.%s";

//帮助菜单
const char *termcfgArgument(int ArgCount)
 {
	switch(ArgCount)
	 {
		case 0:
		case 1:return "设置终端的超时时间(单位:秒),如果输入0则终端登录永不超时.";
	  case 2:
		case 3:return "设置终端的波特率(需要重启方可生效).";
	  case 4:
		case 5:return "设置驱动的自动省电睡眠延时(秒)";
		case 6:
		case 7:return "设置侧按的定位LED功能是否激活";
		case 8:
		case 9:return "设置手电筒的开机操作方式";
	 }
 return NULL;
 }


void termcfgHandler(void)
 {
 bool IsParameterOK=false;
 char *Param;
 int Value;
 UserInputTrueFalseDef UserInput;
 //更改超时时间
 Param=IsParameterExist("01",8,NULL);
 if(Param!=NULL)
   {
	 Value=-1;
	 IsParameterOK=true;
	 if(!CheckIfParamOnlyDigit(Param))Value=atoi(Param);
	 if(CheckIfParamOnlyDigit(Param)||Value<0||Value>2040)
	    {
      DisplayIllegalParam(Param,8,0);//显示用户输入了非法参数
			UartPrintf((char *)OptionsOnlyAcceptNumber,1,2040,"秒","如果该参数数值为0,则终端永不超时");
			}
	 else //更新数值
	    {      
      CfgFile.IdleTimeout=Value*8;
			if(CfgFile.IdleTimeout==0)UARTPuts("\r\n终端登录的超时时间已被禁用.");
			else UartPrintf("\r\n终端超时时间已更新为%d秒.",Value);
			}
	 }
 //更改波特率
 Param=IsParameterExist("23",8,NULL);
 if(Param!=NULL)
   {
	 Value=-1;
	 IsParameterOK=true;
	 if(!CheckIfParamOnlyDigit(Param))Value=atoi(Param);
	 if(CheckIfParamOnlyDigit(Param)||Value<9600||Value>576000)
	    {
      DisplayIllegalParam(Param,8,2);//显示用户输入了非法参数
			UartPrintf((char *)OptionsOnlyAcceptNumber,9600,57600,"bps","");
			}
	 else //更新数值
	    {    
      CfgFile.USART_Baud=Value;
			UartPrintf("\r\n终端的波特率已更新为%dbps.",Value);
			}
	 }
 //更改睡眠时间
 Param=IsParameterExist("45",8,NULL);
 if(Param!=NULL)
   {
	 Value=-1;	
	 IsParameterOK=true; 
	 if(!CheckIfParamOnlyDigit(Param))Value=atoi(Param);
	 if(CheckIfParamOnlyDigit(Param)||Value<0||Value>2040)
	    {
      DisplayIllegalParam(Param,8,4);//显示用户输入了非法参数
      UartPrintf((char *)OptionsOnlyAcceptNumber,"驱动将永远保持在较高功耗的待机模式");
			}
	 else //更新数值
	    {
      CfgFile.DeepSleepTimeOut=Value;
			if(CfgFile.DeepSleepTimeOut==0)
				 {
				 UARTPuts("\r\n驱动的自动省电睡眠功能已被禁用.");
				 }
			else UartPrintf("\r\n驱动的自动省电睡眠延时已更新为%d秒.",Value);
			}
	 }
 //更改侧按LED的定位灯
 Param=IsParameterExist("67",8,NULL);
 if(Param!=NULL)
   {
	 UserInput=CheckUserInputIsTrue(Param);
	 IsParameterOK=true; 
	 if(UserInput==UserInput_Nothing)
	    {
      DisplayIllegalParam(Param,8,6);//显示用户输入了非法参数
      UARTPuts("\r\n请输入'true'以启用侧按定位LED,'false'以禁用.");
			}
	 else
	    {
			CfgFile.EnableLocatorLED=(UserInput==UserInput_True)?true:false;
			UartPrintf("\r\n驱动的侧按LED已被成功%s用.",CfgFile.EnableLocatorLED?"启":"禁");
			}
	 }
 //设置开机模式
 Param=IsParameterExist("89",8,NULL);
 if(Param!=NULL)
   {
	 UserInput=CheckUserInputIsTrue(Param);
	 IsParameterOK=true; 
	 if(UserInput==UserInput_Nothing)
	    {
      DisplayIllegalParam(Param,8,6);//显示用户输入了非法参数
			UARTPuts("\r\n输入'true'为长按开机,'false'为单击开机.");
			}
	 else
	    {
			CfgFile.IsHoldForPowerOn=(UserInput==UserInput_True)?true:false;
			UartPrintf("\r\n手电筒已被设置为%s开机.",CfgFile.IsHoldForPowerOn?"长按":"单击");
			}
	 }
 //没有检测到处理函数执行了功能
 if(!IsParameterOK)UartPrintCommandNoParam(8);
 //事务处理完毕，复位
 ClearRecvBuffer();//清除接收缓冲
 CmdHandle=Command_None;//命令执行完毕
 }
