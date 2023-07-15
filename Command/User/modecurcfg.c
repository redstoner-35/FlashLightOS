#include "console.h"
#include "CfgFile.h"
#include "modelogic.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//外部字符串指针数组
extern const char *ModeSelectStr[];

//参数帮助entry
const char *modecurcfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return ModeSelectStr[2];
		case 2:
		case 3:return ModeSelectStr[3];
		case 4:
	  case 5:return "指定挡位的最小电流(作为无极调光和信标模式的地板电流)";
		case 6:
		case 7:return "指定挡位的额定电流(同时作为无极调光和信标模式的天花板电流)";
	  }
	return NULL;
	}

//命令处理主函数
void modecurcfghandler(void)
  {
	ModeGrpSelDef UserSelect;
	ModeConfStr *TargetMode;
	char *ParamPtr;
	bool IsCmdParamOK=false;
	int modenum,maxmodenum;
	float buf;
	char ParamOK;
	//没输入指定选项
	IsParameterExist("4567",18,&ParamOK);  
	if(!ParamOK)
	  {
		//命令执行完毕的后处理
    ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
		UartPrintCommandNoParam(18);//显示啥也没找到的信息 
		return;
		}
	//识别用户输入的模式组
  ParamPtr=IsParameterExist("01",18,NULL);  
	UserSelect=CheckUserInputForModeGroup(ParamPtr);
	if(UserSelect==ModeGrp_None)
	  {
	  ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
	  DisplayIllegalParam(ParamPtr,18,0);//显示用户输入了非法参数
	  DisplayCorrectModeGroup();//显示正确的模式组
	  return;
		}
	//识别用户输入的挡位编号
  if(UserSelect!=ModeGrp_DoubleClick)
	  {
	  ParamPtr=IsParameterExist("23",18,NULL);  
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
	//处理用户输入的最小电流的检测
	ParamPtr=IsParameterExist("45",18,NULL);
  if(ParamPtr!=NULL)
	  {
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
			UARTPuts((char *)ModeSelectStr[4]);
		else if(buf<0||buf==NAN||buf>FusedMaxCurrent||buf>=TargetMode->LEDCurrentHigh)
			UartPrintf("\r\n错误:您指定的最小电流值无效.有效的值应该在0-%d(A)之间且小于该挡位的额定电流%.2f(A).\r\n",FusedMaxCurrent,TargetMode->LEDCurrentHigh);
	  else if(TargetMode->Mode==LightMode_Ramp&&buf<0.5)
			UartPrintf("\r\n错误:对于无极调光模式而言,您指定的最小电流值(地板电流)应大于等于0.5A.\r\n");
		else
			{
			TargetMode->LEDCurrentLow=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf("的最小电流已变更为%.2fA.\r\n",TargetMode->LEDCurrentLow);
			}
		IsCmdParamOK=true;
		}
	//处理用户输入的额定电流的检测
	ParamPtr=IsParameterExist("67",18,NULL);
  if(ParamPtr!=NULL)
	  {
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
			UARTPuts((char *)ModeSelectStr[4]);
		else if(buf<0.5||buf==NAN||buf>FusedMaxCurrent||buf<=TargetMode->LEDCurrentLow)
			UartPrintf("\r\n错误:您指定的额定电流值无效.有效的值应该在0.5-%d(A)之间且大于该挡位的额定电流%.2f(A).\r\n",FusedMaxCurrent,TargetMode->LEDCurrentLow);
	  else
			{
			TargetMode->LEDCurrentHigh=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf("的额定电流已变更为%.2fA.\r\n",TargetMode->LEDCurrentHigh);
			}
		IsCmdParamOK=true;
		}		
		
	if(!IsCmdParamOK)UartPrintCommandNoParam(18);//显示啥也没找到的信息 
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕	
	}
