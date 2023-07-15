#include "console.h"
#include "CfgFile.h"
#include "modelogic.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//外部字符串指针数组
extern const char *ModeSelectStr[];

//参数帮助entry
const char *customflashcfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return ModeSelectStr[2];
		case 2:
		case 3:return ModeSelectStr[3];
		case 4:
		case 5:return "设置自定义闪的闪烁序列字符串(需要用双引号括起来)";
	  case 6:
		case 7:return "设定自定义闪的最高闪烁频率(Hz,以'101010'这样的密集字符串来计算)";
		}
	return NULL;
	}

//命令处理
void customflashcfgHandler(void)
  {
	int modenum,maxmodenum,strresult;
	ModeGrpSelDef UserSelect;
	ModeConfStr *TargetMode;
	char *ParamPtr;
	float buf;
	bool IsCmdParamOK=false;
  char ParamOK;
	//没输入指定选项
	IsParameterExist("4567",25,&ParamOK);  
	if(!ParamOK)
	  {
		//命令执行完毕的后处理
    ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
		UartPrintCommandNoParam(25);//显示啥也没找到的信息 
		return;
		}
	//识别用户输入的模式组
  ParamPtr=IsParameterExist("01",25,NULL);  
	UserSelect=CheckUserInputForModeGroup(ParamPtr);
	if(UserSelect==ModeGrp_None)
	  {
	  ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
	  DisplayIllegalParam(ParamPtr,25,0);//显示用户输入了非法参数
	  DisplayCorrectModeGroup();//显示正确的模式组
	  return;
		}
	//识别用户输入的挡位编号
  if(UserSelect!=ModeGrp_DoubleClick)
	  {
	  ParamPtr=IsParameterExist("23",25,NULL);  
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
	//处理字符串的输入
	ParamPtr=IsParameterExist("45",25,NULL); 
	if(ParamPtr!=NULL)  
		{
		IsCmdParamOK=true;
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		strresult=CheckForCustomFlashStr(ParamPtr);
		if(strlen(ParamPtr)>31||strlen(ParamPtr)<1)
			UARTPuts("\r\n错误:自定义闪烁序列的字符串内容不得超过31字符且应至少包含1个字符.\r\n");
		else if(strresult!=-1)//字符串包含非法内容
		  {
			UARTPuts("\r\n错误:自定义闪烁序列的发送的字符串内容中仅能包含数字[0-9],'-'以及大写字母'A'和'WXYZ'.");
		  UartPrintf("\r\n您输入的字符串'%s'在第%d个字符的位置存在非法内容,请修正后再试.\r\n",ParamPtr,strresult+1);
			UARTPutc(' ',strresult+15);
			UARTPuts("^\r\n");
			}
		else
		  {
			strncpy(TargetMode->CustomFlashStr,ParamPtr,31);
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf("的自定义闪烁序列的字符串内容已被设置为'%s'\r\n",ParamPtr);
			if(TargetMode->Mode!=LightMode_CustomFlash)
			  UartPrintf((char *)ModeSelectStr[5],"自定义闪烁");
			}
		}
	//处理频闪速度的变更
	ParamPtr=IsParameterExist("67",25,NULL); 
	if(ParamPtr!=NULL)  
		{
		IsCmdParamOK=true;
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(buf==NAN||buf<2||buf>15) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,25,4);//显示用户输入了非法参数  
			UARTPuts("\r\n您设定的自定义闪最高闪烁频率应当在2Hz到15.0Hz之间的正数,负数值和0均为非法值.");
			}
		else //更改
		  {
			TargetMode->CustomFlashSpeed=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf("的自定义闪的最高闪烁频率已被设置为%.1fHz.\r\n",buf);
			if(TargetMode->Mode!=LightMode_CustomFlash)
			  UartPrintf((char *)ModeSelectStr[5],"自定义闪烁");
			}
		}
	if(!IsCmdParamOK)UartPrintCommandNoParam(25);//显示啥也没找到的信息 
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕
	}	
