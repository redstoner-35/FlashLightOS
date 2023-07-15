#include "console.h"
#include "CfgFile.h"
#include "modelogic.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//外部字符串指针数组
extern const char *ModeSelectStr[];

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
	  case 5:return "当挡位是爆闪模式时，指定挡位的爆闪频率(单位Hz)";	
	  }
	return NULL;
	}

void strobecfghandler(void)
 {
  int modenum,maxmodenum;
	ModeGrpSelDef UserSelect;
	ModeConfStr *TargetMode;
	char *ParamPtr;
	float buf;
	char ParamOK;
	//没输入指定选项
	IsParameterExist("45",19,&ParamOK);  
	if(!ParamOK)
	  {
		//命令执行完毕的后处理
    ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
		UartPrintCommandNoParam(19);//显示啥也没找到的信息 
		return;
		}
	//识别用户输入的模式组
  ParamPtr=IsParameterExist("01",19,NULL);  
	UserSelect=CheckUserInputForModeGroup(ParamPtr);
	if(UserSelect==ModeGrp_None)
	  {
	  ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
	  DisplayIllegalParam(ParamPtr,19,0);//显示用户输入了非法参数
	  DisplayCorrectModeGroup();//显示正确的模式组
	  return;
		}
	//识别用户输入的挡位编号
  if(UserSelect!=ModeGrp_DoubleClick)
	  {
	  ParamPtr=IsParameterExist("23",19,NULL);  
    if(!CheckIfParamOnlyDigit(ParamPtr))modenum=atoi(ParamPtr);
    else modenum=-1;
    switch(UserSelect)
	    {
			case ModeGrp_Regular:maxmodenum=8;break;
		  case ModeGrp_Special:maxmodenum=4;break;
		  default:maxmodenum=0;break;
			}
		if(modenum==-1||modenum>=maxmodenum)
		  {
			if(modenum==-1)
				UARTPuts((char *)ModeSelectStr[0]);
			else
				UartPrintf((char *)ModeSelectStr[1],maxmodenum-1);
			ClearRecvBuffer();//清除接收缓冲
      CmdHandle=Command_None;//命令执行完毕	
			return;
			}
		}
	else modenum=0;//双击挡位组编号为0
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
			UARTPuts("\r\n您设定的爆闪频率应当在0.5Hz到16.0Hz之间的正数,负数值和0均为非法值.");
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
	else UartPrintCommandNoParam(19);//显示啥也没找到的信息 
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕	
 }
