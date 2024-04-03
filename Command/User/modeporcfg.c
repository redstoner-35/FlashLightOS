#include "console.h"
#include "CfgFile.h"
#include "modelogic.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//外部字符串指针数组
extern const char *ModeSelectStr[];

//参数帮助entry
const char *modeporcfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return "指定驱动的默认挡位所在的挡位组.";
		case 2:
		case 3:return "指定驱动的默认挡位在挡位组内的序号.";
    case 4:
		case 5:return "设置驱动是否在每次上电后进入锁定状态.";
	  }
	return NULL;
	}

//命令处理主函数
void modeporcfghandler(void)
  {
  #ifndef FlashLightOS_Init_Mode
	int modenum,maxmodenum;	
  ModeGrpSelDef UserSelect;
	ModeConfStr *TargetMode;
	char *ParamPtr;
	char Param;
	bool IsCmdParamOK;
	//处理自检后驱动行为是否锁定的变更参数
	ParamPtr=IsParameterExist("45",23,NULL);
	IsCmdParamOK=(ParamPtr==NULL)?false:true; //检查参数
	if(ParamPtr!=NULL)switch(CheckUserInputIsTrue(ParamPtr))
	  {
		case UserInput_Nothing:
			DisplayIllegalParam(ParamPtr,23,4);//显示用户输入了非法参数
		  UARTPuts("\r\n如您希望驱动在上电后自动进入锁定状态,请输入'true',否则请输入'false'.");
		  break;
		case UserInput_False: //进入解锁
			 CfgFile.IsDriverLockedAfterPOR=false;
			 UartPrintf((char *)ModeSelectStr[8],"解锁");
			 break;
		case UserInput_True: //进入锁定
			 CfgFile.IsDriverLockedAfterPOR=true;
			 UartPrintf((char *)ModeSelectStr[8],"锁定");
       break;		
		 }
	//检查是否有改挡位的配置输入
	IsParameterExist("0123",23,&Param);
	if(Param)//存在配置，开始处理
	  {
		IsCmdParamOK=true;
		//识别用户输入的模式组
    ParamPtr=IsParameterExist("01",23,NULL);  
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
	    ParamPtr=IsParameterExist("23",23,NULL);  
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
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(!TargetMode->IsModeEnabled)
			UARTPuts("\r\n错误:您不能指定一个已被禁用的挡位作为默认挡位!\r\n");
		else
		  {
			CfgFile.BootupModeGroup=UserSelect;
			CfgFile.BootupModeNum=modenum; //更新参数
			DisplayWhichModeSelected(UserSelect,modenum);
			UARTPuts("已被设置为系统的默认挡位.\r\n");
			}
		}
	if(!IsCmdParamOK)UartPrintCommandNoParam(23);//显示啥也没找到的信息 
	#endif
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕	
	}
