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
		case 0:return "设置终端的超时时间(秒)";	
	  case 1:return "设置终端的波特率(bps)";
		case 2:return "设置驱动的自动省电睡眠延时(秒)";
		case 3:return "设置侧按的定位LED功能是否激活";
		case 4:return "设置手电筒的开机操作方式";
		case 5:return "设置反向战术功能的模式";
		case 6:return "设置驱动的自动锁定延时(秒)";
	 }
 return NULL;
 }
//显示延迟已被更新
void DisplayValueHasUpgraded(int Value)
 {
 if(Value==0)
		UARTPuts("功能已被禁用.");
 else 
		UartPrintf("延时已更新为%d秒.",Value);
 }	
//处理主函数
void termcfgHandler(void)
 {
 bool IsParameterOK=false;
 char *Param;
 char TacString[32];
 int Value;
 UserInputTrueFalseDef UserInput;
 ReverseTacModeDef TacSettings;
 //更改超时时间
 Param=IsParameterExist("0",8,NULL);
 if(Param!=NULL)
   {
	 Value=-1;
	 IsParameterOK=true;
	 if(!CheckIfParamOnlyDigit(Param))Value=atoi(Param);
	 if(CheckIfParamOnlyDigit(Param)||Value<0||Value>2040)
	    {
      DisplayIllegalParam(Param,8,0);//显示用户输入了非法参数
			UartPrintf((char *)OptionsOnlyAcceptNumber,1,2040,"秒","为0则终端永不超时");
			}
	 else //更新数值
	    {      
      CfgFile.IdleTimeout=Value*8;
		  UARTPuts("\r\n终端登录的超时时间");
			if(CfgFile.IdleTimeout==0)
				 UARTPuts("已被禁用.");
			else 
				 UartPrintf("\r\n已更新为%d秒.",Value);
			}
	 }
 //更改波特率
 Param=IsParameterExist("1",8,NULL);
 if(Param!=NULL)
   {
	 Value=-1;
	 IsParameterOK=true;
	 if(!CheckIfParamOnlyDigit(Param))Value=atoi(Param);
	 if(CheckIfParamOnlyDigit(Param)||Value<9600||Value>576000)
	    {
      DisplayIllegalParam(Param,8,1);//显示用户输入了非法参数
			UartPrintf((char *)OptionsOnlyAcceptNumber,9600,57600,"bps","");
			}
	 else //更新数值
	    {    
      CfgFile.USART_Baud=Value;
			UartPrintf("\r\n终端的波特率已更新为%dbps.",Value);
			}
	 }
 //更改睡眠时间
 Param=IsParameterExist("2",8,NULL);
 if(Param!=NULL)
   {
	 Value=-1;	
	 IsParameterOK=true; 
	 if(!CheckIfParamOnlyDigit(Param))Value=atoi(Param);
	 if(CheckIfParamOnlyDigit(Param)||Value<0||Value>2040)
	    {
      DisplayIllegalParam(Param,8,2);//显示用户输入了非法参数
			UartPrintf((char *)OptionsOnlyAcceptNumber,1,2040,"秒","为0则驱动永不睡眠");
			}
	 else //更新数值
	    {
      CfgFile.DeepSleepTimeOut=Value;
		  UARTPuts("\r\n驱动的自动省电睡眠");
      DisplayValueHasUpgraded(Value);
			}
	 }
 //更改侧按LED的定位灯
 Param=IsParameterExist("3",8,NULL);
 if(Param!=NULL)
   {
	 UserInput=CheckUserInputIsTrue(Param);
	 IsParameterOK=true; 
	 if(UserInput==UserInput_Nothing)
	    {
      DisplayIllegalParam(Param,8,3);//显示用户输入了非法参数
      UARTPuts("\r\n请输入'true'以启用侧按定位LED,'false'以禁用.");
			}
	 else
	    {
			CfgFile.EnableLocatorLED=(UserInput==UserInput_True)?true:false;
			UartPrintf("\r\n驱动的侧按LED已被成功%s用.",CfgFile.EnableLocatorLED?"启":"禁");
			}
	 }
 //设置开机模式
 Param=IsParameterExist("4",8,NULL);
 if(Param!=NULL)
   {
	 UserInput=CheckUserInputIsTrue(Param);
	 IsParameterOK=true; 
	 if(UserInput==UserInput_Nothing)
	    {
      DisplayIllegalParam(Param,8,4);//显示用户输入了非法参数
			UARTPuts("\r\n输入'true'为长按开机,'false'为单击开机.");
			}
	 else
	    {
			CfgFile.IsHoldForPowerOn=(UserInput==UserInput_True)?true:false;
			UartPrintf("\r\n手电筒已被设置为%s开机.",CfgFile.IsHoldForPowerOn?"长按":"单击");
			}
	 }
 Param=IsParameterExist("5",8,NULL);
 if(Param!=NULL)
   {
	 TacSettings=getReverseTacModeFromUserInput(Param);
	 IsParameterOK=true; 
	 if(TacSettings==RevTactical_InputError)
	    {
      DisplayIllegalParam(Param,8,5);//显示用户输入了非法参数
			UARTPuts("\r\n可用的参数如下:\r\n'turbo' : 瞬时极亮");
			UARTPuts("\r\n'off' : 关闭手电");	
			UARTPuts("\r\n'disable' : 无动作");	
			for(Value=30;Value<=70;Value+=20)UartPrintf("\r\n'dim%d%%' : 设置亮度为%d%%",Value,Value);		
			}
	 else
	    {
			CfgFile.RevTactalSettings=TacSettings;
			DisplayReverseTacModeName(TacString,32,TacSettings);
			UartPrintf("\r\n手电筒的反向战术模式已更新为%s.",TacString);
			}
	 }
//更改自动锁定时间
 Param=IsParameterExist("6",8,NULL);
 if(Param!=NULL)
   {
	 Value=-1;	
	 IsParameterOK=true; 
	 if(!CheckIfParamOnlyDigit(Param))Value=atoi(Param);
	 if(CheckIfParamOnlyDigit(Param)||Value<0||(Value>0&&Value<60)||Value>2040)
	    {
      DisplayIllegalParam(Param,8,2);//显示用户输入了非法参数
			UartPrintf((char *)OptionsOnlyAcceptNumber,60,2040,"秒","为0则永不自动锁定");
			}
	 else //更新数值
	    {
      CfgFile.AutoLockTimeOut=Value;
		  UARTPuts("\r\n驱动的自动锁定");
      DisplayValueHasUpgraded(Value);
			}
	 }
 //没有检测到处理函数执行了功能
 if(!IsParameterOK)UartPrintCommandNoParam(8);
 //事务处理完毕，复位
 ClearRecvBuffer();//清除接收缓冲
 CmdHandle=Command_None;//命令执行完毕
 }
