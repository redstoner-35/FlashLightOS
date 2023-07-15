#include "console.h"
#include "CfgFile.h"
#include "modelogic.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//外部字符串指针数组
extern const char *ModeSelectStr[];

//参数帮助entry
const char *breathecfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return ModeSelectStr[2];
		case 2:
		case 3:return ModeSelectStr[3];
		case 4:
		case 5:return "配置指定挡位信标模式的电流爬升时间Trampup(电流从地板爬升到天花板所用的总时间,单位秒)";	
		case 6:
		case 7:return "配置指定挡位信标模式的电流下滑时间Trampdn(电流从天花板下滑到地板所用的总时间,单位秒)";		
		case 8:
		case 9:return "配置指定挡位信标模式的天花板电流保持时间Tholdup(电流爬升到天花板后,在下滑到地板前的等待时间,单位秒)";	
		case 10:
		case 11:return "配置指定挡位信标模式的地板电流保持时间Tholddn(电流下滑到地板后,在重新爬升前的等待时间,单位秒)";
	  }
	return NULL;
	}

//命令处理主函数
void breathecfghandler(void)
  {
	int modenum,maxmodenum;
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
  ParamPtr=IsParameterExist("01",21,NULL);  
	UserSelect=CheckUserInputForModeGroup(ParamPtr);
	if(UserSelect==ModeGrp_None)
	  {
	  ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
	  DisplayIllegalParam(ParamPtr,20,0);//显示用户输入了非法参数
	  DisplayCorrectModeGroup();//显示正确的模式组
	  return;
		}
	//识别用户输入的挡位编号
  if(UserSelect!=ModeGrp_DoubleClick)
	  {
	  ParamPtr=IsParameterExist("23",21,NULL);  
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
			UartPrintf("的电流爬升时间(信标模式)Trampup已被设置为%.1f秒.\r\n",buf);
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
			UartPrintf("的电流下滑时间(信标模式)Trampdn已被设置为%.1f秒.\r\n",buf);
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
			UartPrintf("的天花板电流保持时间(信标模式)Tholdup已被设置为%.1f秒.\r\n",buf);
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
			UartPrintf("的地板电流保持时间(信标模式)Tholddn已被设置为%.1f秒.\r\n",buf);
			if(TargetMode->Mode!=LightMode_Breath)
			   UartPrintf((char *)ModeSelectStr[5],"信标");
			}
		}
	if(!IsCmdParamOK)UartPrintCommandNoParam(21);//显示啥也没找到的信息 
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕
	}
