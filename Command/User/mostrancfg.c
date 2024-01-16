#include "console.h"
#include "CfgFile.h"
#include "modelogic.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>


//外部字符串指针数组
extern const char *ModeSelectStr[];
#ifndef FlashLightOS_Debug_Mode
static const char *IllegalMosStr="\r\n错误:自定义摩尔斯码发送的字符串内容%s.\r\n";
#endif

//参数帮助entry
const char *mostranscfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return ModeSelectStr[2];
		case 2:
		case 3:return ModeSelectStr[3];
		case 4:
		case 5:return "当挡位是SOS/自定义摩尔斯码发送时,指定发送时'.'的长度(单位为秒,越短速度越快)";	
		case 6:
		case 7:return "当挡位是自定义摩尔斯码发送模式时指定发送内容";	
	  }
	return NULL;
	}
//命令处理函数	
void mostranscfghandler(void)
 {
	#ifndef FlashLightOS_Debug_Mode
  int modenum,strresult;
	ModeGrpSelDef UserSelect;
	ModeConfStr *TargetMode;
	char *ParamPtr;
	float buf;
	bool IsCmdParamOK=false;
	char ParamOK;
	//没输入指定选项
	IsParameterExist("4567",20,&ParamOK);  
	if(!ParamOK)
	  {
		//命令执行完毕的后处理
    ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
		UartPrintCommandNoParam(20);//显示啥也没找到的信息 
		return;
		}
	//识别用户输入的模式组
  if(!GetUserModeNum(20,&UserSelect,&modenum))return;		
	//处理莫尔斯码发送速度的变更 
	ParamPtr=IsParameterExist("45",20,NULL); 
	if(ParamPtr!=NULL)  
		{
		IsCmdParamOK=true;
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(buf==NAN||buf<0.05||buf>2.0) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,20,4);//显示用户输入了非法参数  
			UARTPuts("\r\n您设定的摩尔斯代码中'.'的长度应当在0.05到2.0秒之间.");
			}
		else //更改
		  {
			TargetMode->MosTransferStep=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf("的摩尔斯代码发送速度('.'的长度)已被设置为%.2f秒.\r\n",buf);
			if(TargetMode->Mode!=LightMode_MosTrans&&TargetMode->Mode!=LightMode_SOS)
			   UartPrintf((char *)ModeSelectStr[5],"摩尔斯代码发送/SOS");
			}
		}
  //处理莫尔斯码的自定义字符串
	ParamPtr=IsParameterExist("67",20,NULL); 
	if(ParamPtr!=NULL)  
		{
		IsCmdParamOK=true;
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		strresult=CheckForStringCanBeSentViaMorse(ParamPtr);
		if(strlen(ParamPtr)>31||strlen(ParamPtr)<1)
			UartPrintf((char *)IllegalMosStr,"不得超过31字符且应至少包含1个字符");
		else if(strresult!=-1)//字符串包含非法内容
		  {
			UartPrintf((char *)IllegalMosStr,"仅能包含大小写字母[A-Z,a-z]数字[0-9]和部分特殊符号[? / ( ) - _ .]");
		  UartPrintf("\r\n您输入的字符串'%s'在第%d个字符的位置存在非法内容,请修正后再试.\r\n",ParamPtr,strresult+1);
			UARTPutc(' ',strresult+15);
			UARTPuts("^\r\n");
			}
		else
		  {
			strncpy(TargetMode->MosTransStr,ParamPtr,31);
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf("的自定义摩尔斯码发送的字符串内容已被设置为'%s'\r\n",ParamPtr);
			if(TargetMode->Mode!=LightMode_MosTrans)
			  UartPrintf((char *)ModeSelectStr[5],"摩尔斯代码发送");
			}
		}
  if(!IsCmdParamOK)UartPrintCommandNoParam(20);//显示啥也没找到的信息 
	#endif
 	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕
  }
