#include "console.h"
#include "CfgFile.h"
#include "modelogic.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//外部字符串指针数组
extern const char *ModeSelectStr[];
#ifndef FlashLightOS_Init_Mode
static const char *TimeParamHasSet="的%s时间(信标模式)%s已被设置为%.1f秒.\r\n";
#endif

//参数帮助entry
const char *breathecfgArgument(int ArgCount)
  {
	#ifndef FlashLightOS_Init_Mode
	switch(ArgCount)
	  {
		case 0:
		case 1:return ModeSelectStr[2];
		case 2:
		case 3:return ModeSelectStr[3];
		case 4:
		case 5:return "配置电流爬升时间Trampup(秒)";	
		case 6:
		case 7:return "配置电流下滑时间Trampdn(秒)";		
		case 8:
		case 9:return "配置天花板电流保持时间Tholdup(秒)";	
		case 10:
		case 11:return "配置地板电流保持时间Tholddn(秒)";
	  }
	return NULL;
	#else
	return "No Param";
	#endif
	}

//命令处理主函数
void breathecfghandler(void)
  {
	#ifndef FlashLightOS_Init_Mode
	int modenum;
	ModeGrpSelDef UserSelect;
	ModeConfStr *TargetMode;
	char *ParamPtr;
	float buf;
	bool IsCmdParamOK=false;
	char ParamOK;
	//没输入指定选项
	IsParameterExist("456789AB",21,&ParamOK);  
	if(!ParamOK)
	  {
		//命令执行完毕的后处理
    ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
		UartPrintCommandNoParam(21);//显示啥也没找到的信息 
		return;
		}
	//识别用户输入的模式组
  if(!GetUserModeNum(21,&UserSelect,&modenum))return;		
	//处理爬升速度的变更 
	ParamPtr=IsParameterExist("45",21,NULL); 
	if(ParamPtr!=NULL)  
		{
		IsCmdParamOK=true;
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(buf==NAN||buf<0.5||buf>7200) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,21,4);//显示用户输入了非法参数  
			UartPrintf((char *)ModeSelectStr[6],"电流爬升时间Trampup");
			}
		else //更改
		  {
			TargetMode->CurrentRampUpTime=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf((char *)TimeParamHasSet,"电流爬升","Trampup",buf);
			if(TargetMode->Mode!=LightMode_Breath)
			   UartPrintf((char *)ModeSelectStr[5],"信标");
			}
		}
	//处理下滑速度的变更 
	ParamPtr=IsParameterExist("67",21,NULL); 
	if(ParamPtr!=NULL)  
		{
		IsCmdParamOK=true;
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(buf==NAN||buf<0.5||buf>7200) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,21,6);//显示用户输入了非法参数  
			UartPrintf((char *)ModeSelectStr[6],"电流下滑时间Trampdn");
			}
		else //更改
		  {
			TargetMode->CurrentRampDownTime=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf((char *)TimeParamHasSet,"电流下滑","Trampdn",buf);
			if(TargetMode->Mode!=LightMode_Breath)
			   UartPrintf((char *)ModeSelectStr[5],"信标");
			}
		}
	//处理天花板保持电流的变更 
	ParamPtr=IsParameterExist("89",21,NULL); 
	if(ParamPtr!=NULL)  
		{
		IsCmdParamOK=true;
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(buf==NAN||buf<0||buf>7200) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,21,8);//显示用户输入了非法参数  
			UartPrintf((char *)ModeSelectStr[7],"天花板电流保持时间Tholdup");
			}
		else //更改
		  {
			TargetMode->MaxCurrentHoldTime=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf((char *)TimeParamHasSet,"天花板电流保持","Tholdup",buf);
			if(TargetMode->Mode!=LightMode_Breath)
			   UartPrintf((char *)ModeSelectStr[5],"信标");
			}
		}
	//处理地板保持电流的变更 
	ParamPtr=IsParameterExist("AB",21,NULL); 
	if(ParamPtr!=NULL)  
		{
		IsCmdParamOK=true;
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(buf==NAN||buf<0||buf>7200) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,21,10);//显示用户输入了非法参数  
			UartPrintf((char *)ModeSelectStr[7],"地板电流保持时间Tholddn");
			}
		else //更改
		  {
			TargetMode->MaxCurrentHoldTime=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf((char *)TimeParamHasSet,"地板电流保持","Tholddn",buf);
			if(TargetMode->Mode!=LightMode_Breath)
			   UartPrintf((char *)ModeSelectStr[5],"信标");
			}
		}
	if(!IsCmdParamOK)UartPrintCommandNoParam(21);//显示啥也没找到的信息 
	#endif
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕
	}
