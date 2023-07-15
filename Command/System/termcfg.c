#include "console.h"
#include "CfgFile.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const char *termcfgArgument(int ArgCount)
 {
	switch(ArgCount)
	 {
		case 0:
		case 1:return "设置终端的超时时间(单位:秒),如果输入0则终端登录永不超时.";
	  case 2:
		case 3:return "设置终端的波特率(需要重启方可生效).";
	  case 4:
		case 5:return "设置驱动的自动省电睡眠延时(秒),";
	 }
 return NULL;
 }


void termcfgHandler(void)
 {
 bool IsParameterOK=false;
 char *Param;
 int Value;
 //更改超时时间
 if(IsParameterExist("01",8,NULL)!=NULL)
   {
	 Value=-1;
	 Param=IsParameterExist("01",8,NULL);
	 if(!CheckIfParamOnlyDigit(Param))Value=atoi(Param);
	 if(CheckIfParamOnlyDigit(Param)||Value<0||Value>2040)
	    {
      DisplayIllegalParam(Param,8,0);//显示用户输入了非法参数
			UARTPuts("\r\n该选项的参数仅接受范围从0-2040(单位为秒)的纯数字输入.");
			UARTPuts("\r\n如果该参数数值为0，则终端的登录永不超时.");	
			}
	 else //更新数值
	    {
      IsParameterOK=true;
      CfgFile.IdleTimeout=Value*8;
			if(CfgFile.IdleTimeout==0)UARTPuts("\r\n终端登录的超时时间已被禁用.");
			else UartPrintf("\r\n终端登录的超时时间已更新为%d秒.",Value);
			}
	 }
 //更改波特率
  if(IsParameterExist("23",8,NULL)!=NULL)
   {
	 Value=-1;
	 Param=IsParameterExist("23",8,NULL);
	 if(!CheckIfParamOnlyDigit(Param))Value=atoi(Param);
	 if(CheckIfParamOnlyDigit(Param)||Value<9600||Value>576000)
	    {
      DisplayIllegalParam(Param,8,2);//显示用户输入了非法参数
			UARTPuts("\r\n该选项的参数仅接受范围从9600-576000(单位为bps)的纯数字输入.");
			}
	 else //更新数值
	    {
      IsParameterOK=true;
      CfgFile.USART_Baud=Value;
			UartPrintf("\r\n终端的波特率已更新为%dbps.",Value);
			UARTPuts("您需要使用'cfgmgmt -s'\r\n将配置保存到ROM后,使用'reboot'命令重启系统方可使配置生效。");
			}
	 }
 //更改睡眠时间
 if(IsParameterExist("45",8,NULL)!=NULL)
   {
	 Value=-1;
	 Param=IsParameterExist("45",8,NULL);
	 if(!CheckIfParamOnlyDigit(Param))Value=atoi(Param);
	 if(CheckIfParamOnlyDigit(Param)||Value<0||Value>2040)
	    {
      DisplayIllegalParam(Param,8,4);//显示用户输入了非法参数
			UARTPuts("\r\n该选项的参数仅接受范围从0-2040(单位为秒)的纯数字输入.");
			UARTPuts("\r\n如果该参数数值为0，则驱动将永远保持在较高功耗的待机模式.");	
			}
	 else //更新数值
	    {
      IsParameterOK=true;
      CfgFile.DeepSleepTimeOut=Value;
			if(CfgFile.DeepSleepTimeOut==0)
				 {
				 UARTPuts("\r\n驱动的自动省电睡眠功能已被禁用.注意:一直保持在常规待机模式下");
				 UARTPuts("\r\n虽然可提高响应速度,但是驱动每小时将消耗15mAH的电池容量,若手电\r\n长期不用请取出电池否则电池将过度放电.");
				 }
			else UartPrintf("\r\n驱动的自动省电睡眠延时已更新为%d秒.",Value);
			}
	 }
 //没有检测到处理函数执行了功能
 if(!IsParameterOK)UartPrintCommandNoParam(8);
 //事务处理完毕，复位
 ClearRecvBuffer();//清除接收缓冲
 CmdHandle=Command_None;//命令执行完毕
 }
