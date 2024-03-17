#include "console.h"
#include "CfgFile.h"
#include "modelogic.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "FRU.h"

//外部字符串指针数组
extern const char *ModeSelectStr[];
static const char *ModeHasBeenSet="\r\n%s挡位组中的第%d挡位已经配置为%s模式%s\r\n";
const char *FuncOperationError="\r\n如您需要禁用该挡位的%s功能,请输入'false'参数.对于启用,则输入'true'参数.\r\n";

//参数帮助entry
const char *modeadvcfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return ModeSelectStr[2];
		case 2:
		case 3:return ModeSelectStr[3];
		case 4:
	  case 5:return "指定挡位的运行模式(常亮/爆闪/SOS等)";
		case 6:
		case 7:return "指定挡位名称";
		case 8:
		case 9:return "指定挡位是否带记忆功能";
		case 10:
		case 11:return "指定挡位是否带过热降档功能";		
	  case 12:
		case 13:return "指定此挡位温控数值的负偏移.";
		}
	return NULL;
	}
//命令处理主函数
void modeadvcfgHandler(void)
  {
  int modenum;	
  ModeGrpSelDef UserSelect;
	ModeConfStr *TargetMode;
	char *ParamPtr;
  float Buf;
	bool IsCmdParamOK=false;
	LightModeDef LightMode;
	bool IsUserWantToEnable,UserInputOK;
	//识别用户输入的模式组
  if(!GetUserModeNum(17,&UserSelect,&modenum))return;		
	//处理挡位类型识别
	ParamPtr=IsParameterExist("45",17,NULL); 
	LightMode=CheckUserInputForLightmode(ParamPtr);
	if(LightMode!=LightMode_None)
	  {
		IsCmdParamOK=true;
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
	  if(TargetMode==NULL)
		  UARTPuts((char *)ModeSelectStr[4]);
		 else
		  {
			TargetMode->Mode=LightMode;
			UartPrintf((char *)ModeHasBeenSet,ModeGrpString[(int)UserSelect],modenum,LightModeString[(int)LightMode],".");
			if(TargetMode->Mode==LightMode_Ramp&&TargetMode->LEDCurrentLow<MinimumLEDCurrent)//检查电流值
			  {
				TargetMode->LEDCurrentLow=MinimumLEDCurrent;
				UartPrintf("\033[40;33m警告:配置为无极调光模式时挡位最小电流值(地板电流)应大于等于%.2fA!\033[0m\r\n",MinimumLEDCurrent);
				}	
			}
		}
	else if(ParamPtr!=NULL)//用户输入的挡位组参数有误
	  {
		DisplayIllegalParam(ParamPtr,17,4);//显示用户输入了非法参数
		DisplayCorrectMode();
		}
	//处理挡位名称变更
	ParamPtr=IsParameterExist("67",17,NULL);	
	if(ParamPtr!=NULL)
	  {
		IsCmdParamOK=true;
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
	  if(strlen(ParamPtr)>15||strlen(ParamPtr)<1)
			UARTPuts("\r\n错误:您指定的挡位名称的长度应在1-5个汉字/15个英文字符之间!\r\n");
		else if(TargetMode==NULL)
		  UARTPuts((char *)ModeSelectStr[4]);
		else
		  {
		  strncpy(TargetMode->ModeName,ParamPtr,15);
			UartPrintf("\r\n%s挡位组中的第%d挡位的名称已变更为'%s'.\r\n",ModeGrpString[(int)UserSelect],modenum,TargetMode->ModeName);
		  }
		}
	//处理是否带记忆
	ParamPtr=IsParameterExist("89",17,NULL);	 
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
		 DisplayIllegalParam(ParamPtr,17,8);//显示用户输入了非法参数
		 UartPrintf((char *)FuncOperationError,"记忆");
		 }
	  else //用户内容正确
	   {
		 IsCmdParamOK=true;
		 TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		 if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		 else
		   {
			 TargetMode->IsModeHasMemory=IsUserWantToEnable;
			 DisplayWhichModeSelected(UserSelect,modenum);
			 UartPrintf("的记忆功能已被成功%s.\r\n",IsUserWantToEnable?"启用":"禁用");
			 }
		 }
	 }
	//处理是否带温控
	ParamPtr=IsParameterExist("AB",17,NULL);	 
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
		 DisplayIllegalParam(ParamPtr,17,10);//显示用户输入了非法参数
		 UartPrintf((char *)FuncOperationError,"温控");
		 }
	  else  //用户内容正确
	   {
		 IsCmdParamOK=true;
		 TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		 if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		 //非root登录状态下当挡位电流超过某个值时禁止温控被关闭
		 else if(AccountState!=Log_Perm_Root&&TargetMode->LEDCurrentHigh>ForceThermalControlCurrent&&!IsUserWantToEnable)
			 {
			 DisplayWhichModeSelected(UserSelect,modenum);
			 UARTPuts("的温控功能因为电流较高,不允许关闭!\r\n");
			 }
		 else
		   {
			 #ifndef FlashLightOS_Init_Mode
			 if(TargetMode->LEDCurrentHigh>ForceThermalControlCurrent&&!IsUserWantToEnable)
			   {
				 UARTPuts("\033[40;33m警告:通过超级用户权限强制关闭温度保护将会使此设备永久失去保修!\033[0m\r\n");
				 ProgramWarrantySign(Void_ForceDisableThermalLimit); //用户通过root权限强制关闭温控,保修作废
				 }
			 #endif
			 TargetMode->IsModeAffectedByStepDown=IsUserWantToEnable;
			 DisplayWhichModeSelected(UserSelect,modenum);
			 UartPrintf("的温控功能已被成功%s.\r\n",IsUserWantToEnable?"启用":"禁用");
			 }
		 }
	  }
	//处理温度偏移
  ParamPtr=IsParameterExist("CD",17,NULL);
	if(ParamPtr!=NULL)
	  {
		Buf=atof(ParamPtr);
	  TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(Buf==NAN||Buf<0||Buf>20)
			UARTPuts("\r\n您输入的温度负偏移数值无效.允许的输入范围为0-20℃.");
		else
		  {			
		  TargetMode->ThermalControlOffset=Buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf("的温控负偏移已被设置为%.2f℃.\r\n\r\n此挡位的温控设置如下:",Buf);
			UartPrintf("\r\n启动温度:%.2f℃",CfgFile.PIDTriggerTemp-TargetMode->ThermalControlOffset);
			UartPrintf("\r\n目标维持温度:%.2f℃",CfgFile.PIDTargetTemp-TargetMode->ThermalControlOffset);
			UartPrintf("\r\n释放温度:%.2f℃\r\n",CfgFile.PIDRelease-TargetMode->ThermalControlOffset);
			}
		IsCmdParamOK=true;
		}				
	if(!IsCmdParamOK)UartPrintCommandNoParam(17);//显示啥也没找到的信息 
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕	
	}
