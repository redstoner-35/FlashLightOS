#include "console.h"
#include "CfgFile.h"
#include "modelogic.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//外部字符串指针数组
extern const char *ModeSelectStr[];
static const char *RandStrobeString="\r\n您设定的%s频率应当在%.1fHz到%.1fHz之间的正数,负数值和0均为非法值.";
static const char *RandStrobeSetDoneString="的随机爆闪%s频率已被设置为%.1fHz.\r\n";

//参数帮助entry
const char *strobecfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return ModeSelectStr[2];
		case 2:
		case 3:return ModeSelectStr[3];
		case 4:
	  case 5:return "当挡位是爆闪模式时,指定挡位的爆闪频率(单位Hz)";	
		case 6:
		case 7:return "指定随机爆闪模式下,对应挡位的下限频率(单位Hz)";	
		case 8:
		case 9:return "指定随机爆闪模式下,对应挡位的上限频率(单位Hz)";	
	  }
	return NULL;
	}

void strobecfghandler(void)
 {
  int modenum;
	ModeGrpSelDef UserSelect;
	ModeConfStr *TargetMode;
	char *ParamPtr;
	float buf;
	char ParamOK;
	//没输入指定选项
	IsParameterExist("456789",19,&ParamOK);  
	if(!ParamOK)
	  {
		//命令执行完毕的后处理
    ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
		UartPrintCommandNoParam(19);//显示啥也没找到的信息 
		return;
		}
	//识别用户输入的模式组
  if(!GetUserModeNum(19,&UserSelect,&modenum))return;		
	//处理爆闪频率的变更
  ParamPtr=IsParameterExist("45",19,NULL); 
	if(ParamPtr!=NULL)  
		{
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(buf==NAN||buf<0.5||buf>16) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,19,4);//显示用户输入了非法参数  
			UartPrintf((char *)RandStrobeString,"爆闪",0.5,16);
			}
		else //更改
		  {
			TargetMode->StrobeFrequency=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf("的爆闪频率已被设置为%.1fHz.\r\n",buf);
			if(TargetMode->Mode!=LightMode_Flash)
			  UartPrintf((char *)ModeSelectStr[5],"爆闪");
			}
		}
	//处理随机爆闪下限频率的变更
	ParamPtr=IsParameterExist("67",19,NULL); 
	if(ParamPtr!=NULL)  
		{
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(buf==NAN||buf<4||buf>=TargetMode->RandStrobeMaxFreq) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,19,6);//显示用户输入了非法参数  
				UartPrintf((char *)RandStrobeString,"随机爆闪下限",4,TargetMode->RandStrobeMaxFreq-0.05);
			}
		else //更改
		  {
			TargetMode->RandStrobeMinFreq=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf((char *)RandStrobeSetDoneString,"下限频率",buf);
			if(TargetMode->Mode!=LightMode_RandomFlash)
			  UartPrintf((char *)ModeSelectStr[5],"随机爆闪");
			}
		}
  //处理随机爆闪上限频率的变更
	ParamPtr=IsParameterExist("89",19,NULL); 
	if(ParamPtr!=NULL)  
		{
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(buf==NAN||buf<TargetMode->RandStrobeMinFreq||buf>16) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,19,8);//显示用户输入了非法参数  
			UartPrintf((char *)RandStrobeString,"随机爆闪上限",TargetMode->RandStrobeMinFreq+0.05,16);
			}
		else //更改
		  {
			TargetMode->RandStrobeMaxFreq=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf((char *)RandStrobeSetDoneString,"上限频率",buf);
			if(TargetMode->Mode!=LightMode_RandomFlash)
			  UartPrintf((char *)ModeSelectStr[5],"随机爆闪");
			}
		}
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕	
 }
