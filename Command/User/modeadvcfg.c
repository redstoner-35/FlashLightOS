#include "console.h"
#include "CfgFile.h"
#include "modelogic.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//外部字符串指针数组
extern const char *ModeSelectStr[];

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
		case 7:return "指定挡位的名称";
		case 8:
		case 9:return "指定挡位是否带记忆功能(手电筒关闭后保持在该挡位)";
		case 10:
		case 11:return "指定挡位是否带温控功能(手电筒温度过高时驱动自动降低输出电流)";		
		case 12:
		case 13:return "无极调光模式下,指定亮度从0爬升到100%以及从100%下降到0%的总耗时(秒).";
	  case 14:
		case 15:return "调整驱动在<1A的电流月光档时所使用的PWM调光频率(频率越高越费电但是频闪越小)";
		}
	return NULL;
	}
//命令处理主函数
void modeadvcfgHandler(void)
  {
  int modenum,maxmodenum;	
	char ParamO;
  ModeGrpSelDef UserSelect;
	ModeConfStr *TargetMode;
	char *ParamPtr;
	bool IsCmdParamOK=false;
	float buf;
	LightModeDef LightMode;
	bool IsUserWantToEnable,UserInputOK;
  //处理PWM调光频率的调整
	ParamPtr=IsParameterExist("EF",17,NULL); 
	if(ParamPtr!=NULL)  
		{
		IsCmdParamOK=true;
		buf=atof(ParamPtr);
		if(buf==NAN||buf<2||buf>20) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,17,12);//显示用户输入了非法参数  
			UARTPuts("您设定的低电流下PWM调光的频率应在2-20(Khz)范围内.\r\n");
			}
		else //更改
		  {
			CfgFile.PWMDIMFreq=(unsigned short)(buf*1000);
			UartPrintf("\r\n驱动的PWM调光频率已被设置为%.2fKHz.",buf);
			UARTPuts("\r\n注意,此设定需要重启驱动后方可生效,您首先需要使用'cfgmgmt -s'命令");
			UARTPuts("\r\n保存您的驱动配置,然后通过'reboot'命令重启驱动的固件.\r\n");
			}
		}
	//检测后面的参数是否有被用到
	IsParameterExist("456789ABCD",17,&ParamO);
	if(!ParamO)
	  {
		if(!IsCmdParamOK)UartPrintCommandNoParam(17);//显示啥也没找到的信息 
	  //命令处理完毕
	  ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
		return;
		}
	//识别用户输入的模式组
  ParamPtr=IsParameterExist("01",17,NULL);  
	UserSelect=CheckUserInputForModeGroup(ParamPtr);
	if(UserSelect==ModeGrp_None)
	  {
	  ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
	  DisplayIllegalParam(ParamPtr,17,0);//显示用户输入了非法参数
	  DisplayCorrectModeGroup();//显示正确的模式组
	  return;
		}
	//识别用户输入的挡位编号
  if(UserSelect!=ModeGrp_DoubleClick)
	  {
	  ParamPtr=IsParameterExist("23",17,NULL);  
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
	//处理挡位类型识别
	ParamPtr=IsParameterExist("45",17,NULL); 
	LightMode=CheckUserInputForLightmode(ParamPtr);
	if(LightMode!=LightMode_None)
	  {
		IsCmdParamOK=true;
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
	  if(TargetMode==NULL)
		  UARTPuts((char *)ModeSelectStr[4]);
	   else if(TargetMode->Mode==LightMode)
			UartPrintf("\r\n%s挡位组中的第%d挡位已经配置为%s模式,无需操作.\r\n",ModeGrpString[(int)UserSelect],modenum,LightModeString[(int)LightMode]);
		 else
		  {
			TargetMode->Mode=LightMode;
			UartPrintf("\r\n%s挡位组中的第%d挡位成功被配置为%s模式.\r\n",ModeGrpString[(int)UserSelect],modenum,LightModeString[(int)LightMode]);
			if(TargetMode->Mode==LightMode_Ramp&&TargetMode->LEDCurrentLow<0.5)//检查电流值
			  {
				TargetMode->LEDCurrentLow=0.5;
				UARTPuts("\033[40;33m警告:您选择将该挡位更改为无极调光模式.对于该模式,指定的最小\r\n电流值(地板电流)应大于等于0.5A,程序已为您自动修正此错误.\r\n");
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
			UARTPuts("\r\n错误:您指定的挡位名称的长度不得超过5个汉字字符或15个英文字符且应大于1个字符.\r\n");
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
	if(ParamPtr!=NULL)switch(CheckUserInputIsTrue(ParamPtr))
     {
	   case UserInput_True:IsUserWantToEnable=true;break;
	   case UserInput_False: IsUserWantToEnable=false;break; //true或者false
	   case UserInput_Nothing:UserInputOK=false; break;	 //其余情况
	   }
	if(ParamPtr!=NULL&&!UserInputOK) //用户输入非法内容
	   {
		 IsCmdParamOK=true;
		 DisplayIllegalParam(ParamPtr,17,8);//显示用户输入了非法参数
		 UARTPuts("\r\n如您需要禁用该挡位的记忆功能,请输入'false'参数.对于启用,则输入'true'参数.\r\n");
		 }
	else if(ParamPtr!=NULL) //用户内容正确
	   {
		 IsCmdParamOK=true;
		 TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		 if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		 else if(TargetMode->IsModeHasMemory==IsUserWantToEnable)
		   {
		   DisplayWhichModeSelected(UserSelect,modenum);
			 UartPrintf("的记忆功能已经是%s状态,无需操作.\r\n",TargetMode->IsModeHasMemory?"启用":"禁用");
			 }
		 else
		   {
			 TargetMode->IsModeHasMemory=IsUserWantToEnable;
			 DisplayWhichModeSelected(UserSelect,modenum);
			 UartPrintf("的记忆功能已被成功%s.\r\n",IsUserWantToEnable?"启用":"禁用");
			 }
		 }
	//处理是否带温控
	ParamPtr=IsParameterExist("AB",17,NULL);	 
	UserInputOK=true;
	if(ParamPtr!=NULL)switch(CheckUserInputIsTrue(ParamPtr))
     {
	   case UserInput_True:IsUserWantToEnable=true;break;
	   case UserInput_False: IsUserWantToEnable=false;break; //true或者false
	   case UserInput_Nothing:UserInputOK=false; break;	 //其余情况
	   }
	if(ParamPtr!=NULL&&!UserInputOK) //用户输入非法内容
	   {
		 IsCmdParamOK=true;
		 DisplayIllegalParam(ParamPtr,17,10);//显示用户输入了非法参数
		 UARTPuts("\r\n如您需要禁用该挡位的温控功能,请输入'false'参数.对于启用,则输入'true'参数.\r\n");
		 }
	else if(ParamPtr!=NULL) //用户内容正确
	   {
		 IsCmdParamOK=true;
		 TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		 if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		 else if(TargetMode->IsModeAffectedByStepDown==IsUserWantToEnable)
		   {
			 DisplayWhichModeSelected(UserSelect,modenum);
		   UartPrintf("的温控功能已经是%s状态,无需操作.\r\n",TargetMode->IsModeAffectedByStepDown?"启用":"禁用");
			 }
		 else
		   {
			 TargetMode->IsModeAffectedByStepDown=IsUserWantToEnable;
			 DisplayWhichModeSelected(UserSelect,modenum);
			 UartPrintf("的温控功能已被成功%s.\r\n",IsUserWantToEnable?"启用":"禁用");
			 }
		 }
	//处理无极调光模式下电流爬升的速度
	ParamPtr=IsParameterExist("CD",17,NULL); 
	if(ParamPtr!=NULL)  
		{
		IsCmdParamOK=true;
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
		   UARTPuts((char *)ModeSelectStr[4]);
		else if(buf==NAN||buf<1||buf>15) //数值非法
		  {
			DisplayIllegalParam(ParamPtr,17,12);//显示用户输入了非法参数  
			UARTPuts("您设定的无极调光模式下电流爬升的总耗时应在1-15秒内.\r\n");
			}
		else //更改
		  {
			TargetMode->RampModeSpeed=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf("的无极调光模式下电流爬升的总耗时已被设置为%.1f秒.\r\n",buf);
			if(TargetMode->Mode!=LightMode_Ramp)
			   UartPrintf((char *)ModeSelectStr[5],"无极调光");
			}
		}
	if(!IsCmdParamOK)UartPrintCommandNoParam(17);//显示啥也没找到的信息 
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕	
	}
