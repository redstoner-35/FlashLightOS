#include "console.h"
#include "CfgFile.h"
#include "modelogic.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//外部字符串指针数组
extern const char *ModeSelectStr[];

//参数帮助entry
const char *modepofcfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return ModeSelectStr[2];
		case 2:
		case 3:return ModeSelectStr[3];
		case 4:
		case 5:return "设置挡位的自动关机计时器时间(单位分钟),若输入0则该挡位的自动关机功能禁用.";	
		case 6:
		case 7:return "设置挡位的短时鸡血次数(鸡血指温控触发后强制关闭温控跑满挡位亮度,通过双击+长按触发).";
	  }
	return NULL;
	}

void modepofcfghandler(void)
 {
  int modenum;
	ModeGrpSelDef UserSelect;
	ModeConfStr *TargetMode;
	char *ParamPtr;
	short buf;
	char ParamOK;
	//没输入指定选项
	IsParameterExist("4567",7,&ParamOK);  
	if(!ParamOK)
	  {
		//命令执行完毕的后处理
    ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
		UartPrintCommandNoParam(7);//显示啥也没找到的信息 
		return;
		}
	//识别用户输入的模式组
  if(!GetUserModeNum(7,&UserSelect,&modenum))return;		
	//处理爆闪频率的变更
  ParamPtr=IsParameterExist("45",7,NULL); 
	if(ParamPtr!=NULL)  
		{
		if(!CheckIfParamOnlyDigit(ParamPtr))buf=(short)atoi(ParamPtr);
		else buf=-1;//读取数值
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
	  if(TargetMode->PowerOffTimer>0)
   		UARTPuts("\r\n自动关机定时器和鸡血技能冲突,请将此挡位的鸡血技能禁用!");	
		else if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(buf<0||buf>360) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,7,4);//显示用户输入了非法参数  
			UARTPuts("\r\n您设置的自动关机时间应该在1-360分钟内.如果输入0则表示禁用此挡位的定时关机功能.");
			}
		else //更改
		  {
			TargetMode->PowerOffTimer=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			if(buf>0)UartPrintf("的定时关机时间已被设置为%d分钟.",buf);
			else UARTPuts("的定时关机功能已被禁用.");
			}
		}
	ParamPtr=IsParameterExist("45",7,NULL); 
	if(ParamPtr!=NULL)  
		{
		if(!CheckIfParamOnlyDigit(ParamPtr))buf=(short)atoi(ParamPtr);
		else buf=-1;//读取数值
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(buf<0||buf>360) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,7,4);//显示用户输入了非法参数  
			UARTPuts("\r\n您设置的自动关机时间应该在1-360分钟内.如果输入0则表示禁用此挡位的定时关机功能.");
			}
		else //更改
		  {
			TargetMode->PowerOffTimer=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			if(buf>0)UartPrintf("的定时关机时间已被设置为%d分钟.",buf);
			else UARTPuts("的定时关机功能已被禁用.");
			}
		}
	//设置短时鸡血次数
	ParamPtr=IsParameterExist("67",7,NULL); 
	if(ParamPtr!=NULL)  
		{
		if(!CheckIfParamOnlyDigit(ParamPtr))buf=(short)atoi(ParamPtr);
		else buf=-1;//读取数值
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode->PowerOffTimer>0)
   		UARTPuts("\r\n鸡血技能和自动关机定时器冲突,请将此挡位的自动关机定时器禁用!");	
		else if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(buf<0||buf>5) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,7,4);//显示用户输入了非法参数  
			UARTPuts("\r\n您设置的鸡血技能次数应在0-5次范围,0=禁用!");
			}
		else //更改
		  {
			TargetMode->MaxMomtTurboCount=(char)buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			if(buf>0)UartPrintf("的鸡血技能次数已被设置为%d次.",buf);
			else UARTPuts("的鸡血技能已被禁用.");
			}
		}
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕	
 }
