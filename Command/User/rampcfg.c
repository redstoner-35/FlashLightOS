#include "console.h"
#include "CfgFile.h"
#include "runtimelogger.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

//外部字符串指针数组
extern const char *ModeSelectStr[];
extern const char *FuncOperationError;
const char *CantSetSeeMode="您选择的挡位并非无极调光模式!无法%s该挡位的%s.";
const char *BrightnessNotRight="您设定的%s亮度等级应在0-100内.\r\n";
const char *BrightnessHasbeenSet="的%s亮度等级已被设置为%.1f.\r\n";

//参数帮助entry
const char *rampcfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
	  case 0:
		case 1:return ModeSelectStr[2];
		case 2:
		case 3:return ModeSelectStr[3];
		case 4:
		case 5:return "设置从最低爬升到最高亮度(反之亦然)的总耗时";
		case 6:
		case 7:return "设置关闭手电后亮度等级是否记忆";
		case 8:
	  case 9:return "设置该挡位的当前亮度等级";
		case 10:
	  case 11:return "设置该挡位的默认亮度等级";
		case 12:
			case 13:return "设置调光方向提示是否启用";
		}
	return NULL;
	}

//命令处理主函数
void rampcfghandler(void)
  {
	#ifndef FlashLightOS_Init_Mode
	int modenum,RampCfgIndex;	
	char *ParamPtr;
	char ParamO;
	ModeConfStr *TargetMode;
	float buf;
	bool IsCmdParamOK=false;
  ModeGrpSelDef UserSelect;
	bool IsUserWantToEnable,UserInputOK;
	//设置无极调光方向提示是否启用
	ParamPtr=IsParameterExist("CD",29,NULL);	 
	UserInputOK=true;
	if(ParamPtr!=NULL)
	  {
	  switch(CheckUserInputIsTrue(ParamPtr))
     {
	   case UserInput_True:IsUserWantToEnable=true;break;
	   case UserInput_False: IsUserWantToEnable=false;break; //true或者false
	   case UserInput_Nothing:UserInputOK=false; break;	 //其余情况
	   }
  	if(!UserInputOK) //用户输入非法内容
	   {
		 IsCmdParamOK=true;
		 DisplayIllegalParam(ParamPtr,29,6);//显示用户输入了非法参数
			 UartPrintf((char *)FuncOperationError,"调光方向提示");
		 }
	  else  //用户内容正确
	   {
		 IsCmdParamOK=true;	 
		 CfgFile.IsNoteLEDEnabled=IsUserWantToEnable;
		 UartPrintf("调光方向提示已被成功%s.\r\n",IsUserWantToEnable?"启用":"禁用");
		 }	
	  }
	//检测后面的参数是否有被用到
	IsParameterExist("456789AB",29,&ParamO);
	if(!ParamO) //没有任何有效参数
	  {
		if(!IsCmdParamOK)UartPrintCommandNoParam(29);//显示啥也没找到的信息 
	  ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
		return;
		}
	//识别用户输入的模式组
  if(!GetUserModeNum(29,&UserSelect,&modenum))return;	
	//将模式组转换为index 
	switch(UserSelect)
	  {
		case ModeGrp_Regular:RampCfgIndex=modenum;break;//常规挡位
		case ModeGrp_DoubleClick:RampCfgIndex=8;break;//双击挡位
		case ModeGrp_Special:RampCfgIndex=modenum+9;break;//特殊功能
		default : ClearRecvBuffer();CmdHandle=Command_None;return; //命令出错直接退出
		}
  //处理无极调光模式下电流爬升的速度
	ParamPtr=IsParameterExist("45",29,NULL); 
	if(ParamPtr!=NULL)  
		{
		IsCmdParamOK=true;
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(buf==NAN||buf<1||buf>15) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,29,4);//显示用户输入了非法参数  
			UARTPuts("您设定的亮度变化总耗时应在1-15秒内.\r\n");
			}
		else //更改
		  {
			TargetMode->RampModeSpeed=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf("亮度变化总耗时已被设置为%.1f秒.\r\n",buf);
			if(TargetMode->Mode!=LightMode_Ramp)
			   UartPrintf((char *)ModeSelectStr[5],"无极调光");
			}
		}
	//处理无极调光的亮度等级是否记忆
	ParamPtr=IsParameterExist("67",29,NULL);	 
	UserInputOK=true;
	if(ParamPtr!=NULL)
	  {
	  switch(CheckUserInputIsTrue(ParamPtr))
     {
	   case UserInput_True:IsUserWantToEnable=true;break;
	   case UserInput_False: IsUserWantToEnable=false;break; //true或者false
	   case UserInput_Nothing:UserInputOK=false; break;	 //其余情况
	   }
  	if(!UserInputOK) //用户输入非法内容
	   {
		 IsCmdParamOK=true;
		 DisplayIllegalParam(ParamPtr,29,6);//显示用户输入了非法参数
		 UartPrintf((char *)FuncOperationError,"亮度等级记忆");
		 }
	  else  //用户内容正确
	   {
		 IsCmdParamOK=true;	 
		 CfgFile.IsRememberBrightNess[RampCfgIndex]=IsUserWantToEnable;
		 DisplayWhichModeSelected(UserSelect,modenum);
		 UartPrintf("的亮度等级记忆功能已被成功%s.\r\n",IsUserWantToEnable?"启用":"禁用");
		 }	
	  }
	//设置该挡位的当前亮度等级
	ParamPtr=IsParameterExist("89",29,NULL); 
	if(ParamPtr!=NULL)  
		{
		IsCmdParamOK=true;
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(TargetMode->Mode!=LightMode_Ramp)
			 UartPrintf((char *)CantSetSeeMode,"设置","当前亮度");
		else if(buf==NAN||buf<0||buf>100) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,29,8);//显示用户输入了非法参数  
			UartPrintf((char *)BrightnessNotRight,"当前");
			}
		else //更改
		  {
			RunLogEntry.Data.DataSec.RampModeStor[RampCfgIndex].RampModeConf=buf/(float)100;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf((char *)BrightnessHasbeenSet,"当前",buf);
			}
		}	
	//设置该挡位的默认亮度等级
	ParamPtr=IsParameterExist("AB",29,NULL); 
	if(ParamPtr!=NULL)  
		{
		IsCmdParamOK=true;
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(buf==NAN||buf<0||buf>100) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,29,10);//显示用户输入了非法参数  
			UartPrintf((char *)BrightnessNotRight,"默认");
			}
		else //更改
		  {
			CfgFile.DefaultLevel[RampCfgIndex]=buf/(float)100;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf((char *)BrightnessHasbeenSet,"默认",buf);
			if(TargetMode->Mode!=LightMode_Ramp)
			   UartPrintf((char *)ModeSelectStr[5],"无极调光");
			}
		}
	if(!IsCmdParamOK)UartPrintCommandNoParam(29);//显示啥也没找到的信息 
	#endif
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕		
	}
