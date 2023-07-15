#include "console.h"
#include "CfgFile.h"
#include "modelogic.h"
#include <stdlib.h>
#include <string.h>

//外部变量
extern const char *ModeGrpString[3];

//常量（包含通用的字符串）
const char *ModeSelectStr[]=
{
"\r\n错误:您需要指定您要启用/禁用的挡位在挡位组内的编号!您可使用'help'命令获得帮助.\r\n",
"\r\n错误:您指定的挡位编号在该挡位组内不存在!该挡位组的编号范围为0-%d.\r\n",
"指定要编辑的挡位所在的挡位组.",
"指定该挡位组中要被编辑的挡位序号",
"\r\n错误:选择的挡位组数据库为空,无法操作!\r\n",
"\r\n由于此挡位目前模式并非%s,因此您需要将该挡位配置为对应模式方可察觉您所做的更改.",
"\r\n您设定的%s应当取0.5到7200秒之间的正数.", //6
"\r\n您设定的%s应当取0到7200秒之间的任意数值.", //7
"\r\n驱动在上电自检结束后的状态已被定义为%s,%s" //8
};

//参数帮助entry
const char *modeenacfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return ModeSelectStr[2];
		case 2:
		case 3:return ModeSelectStr[3];
		case 4:
		case 5:return "编辑被选中的挡位是否启用";		
		}
	return NULL;
	} 
//命令处理函数
void ModeEnacfghandler(void)	
  {
	ModeGrpSelDef UserSelect;
	ModeConfStr *TargetMode;
	int modenum,maxmodenum,i,EnableCount;
	char *ParamPtr;
	bool IsUserWantToEnable;
	bool IsCollision;
	char ParamOK;
	//没输入指定选项
	IsParameterExist("45",16,&ParamOK);  
	if(!ParamOK)
	  {
		//命令执行完毕的后处理
    ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
		UartPrintCommandNoParam(16);//显示啥也没找到的信息 
		return;
		}
	//识别用户输入的模式组
  ParamPtr=IsParameterExist("01",16,NULL);  
	UserSelect=CheckUserInputForModeGroup(ParamPtr);
	if(UserSelect==ModeGrp_None)
	  {
	  ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
	  DisplayIllegalParam(ParamPtr,16,0);//显示用户输入了非法参数
	  DisplayCorrectModeGroup();//显示正确的模式组
	  return;
		}
	//识别用户输入的挡位编号
  if(UserSelect!=ModeGrp_DoubleClick)
	  {
	  ParamPtr=IsParameterExist("23",16,NULL);  
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
	//识别用户Enable与否
	ParamPtr=IsParameterExist("45",16,NULL); 
	if(ParamPtr!=NULL)switch(CheckUserInputIsTrue(ParamPtr))
   {
	 case UserInput_True:IsUserWantToEnable=true;break;
	 case UserInput_False:IsUserWantToEnable=false;break; 
	 case UserInput_Nothing:	 //其余情况
	   {
		 DisplayIllegalParam(ParamPtr,14,2);//显示用户输入了非法参数
		 UARTPuts("\r\n如您需要禁用该挡位,请输入'false'参数.对于启用,则输入'true'参数.\r\n");
		 ClearRecvBuffer();//清除接收缓冲
     CmdHandle=Command_None;//命令执行完毕	
		 return;
		 }
	 }
  //使能挡位前需要检查冲突
	if(IsUserWantToEnable)IsCollision=false;//并不是想禁用该挡位
	else if(UserSelect==CfgFile.BootupModeGroup&&modenum==CfgFile.BootupModeNum)IsCollision=true;
	else if(CfgFile.BootupModeGroup==ModeGrp_DoubleClick&&UserSelect==ModeGrp_DoubleClick)IsCollision=true;
	else IsCollision=false;//试图关闭的挡位作为无记忆跳档时默认的挡位，不允许操作
	if(IsCollision)   
	   {
		 UARTPuts("\r\n错误:您所指定的挡位作为系统启动和无记忆挡位跳回时的默认挡位,因此您无法禁用该挡位!\r\n");
		 ClearRecvBuffer();//清除接收缓冲
     CmdHandle=Command_None;//命令执行完毕	
		 return;
		 }	
	EnableCount=0;
	if(UserSelect==ModeGrp_Regular)for(i=0;i<maxmodenum;i++)
		 {
		 TargetMode=GetSelectedModeConfig(UserSelect,i);
		 if(TargetMode!=NULL&&TargetMode->IsModeEnabled)EnableCount++;
		 }
	if(EnableCount<=1&&!IsUserWantToEnable)
	   {
		 UARTPuts("\r\n错误:对于常规挡位组,应至少存在1个启用的挡位!\r\n");
		 ClearRecvBuffer();//清除接收缓冲
     CmdHandle=Command_None;//命令执行完毕	
		 return;
		 }		
	//开始使能和禁止挡位的操作
	TargetMode=GetSelectedModeConfig(UserSelect,modenum);
	if(TargetMode==NULL)
		UARTPuts((char *)ModeSelectStr[4]);
	else if(TargetMode->IsModeEnabled==IsUserWantToEnable)
	  {
		DisplayWhichModeSelected(UserSelect,modenum);
		UartPrintf("已经是%s状态,无需操作.\r\n",TargetMode->IsModeEnabled?"启用":"禁用");
		}
	else
	  {
		TargetMode->IsModeEnabled=IsUserWantToEnable;
		DisplayWhichModeSelected(UserSelect,modenum);
		UartPrintf("已被成功%s.\r\n",IsUserWantToEnable?"启用":"禁用");
		}
  //命令执行完毕的后处理
  ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕	
	}
