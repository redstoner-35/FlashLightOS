#include "console.h"
#include "CfgFile.h"
#include "modelogic.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//外部字符串指针数组
extern const char *ModeSelectStr[];
static const char *CurrentIllegalInfo="\r\n错误:您指定的%s电流值无效.有效的值应该在%.1f-%.1f(A)之间且大于该挡位的额定电流%.1f(A).\r\n";
static const char *CurrentHasChanged="的%s电流已变更为%.2fA.\r\n";
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
		case 7:return "指定挡位的额定电流(作为无极调光和信标模式的天花板电流)";
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
	int modenum;
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
  if(!GetUserModeNum(18,&UserSelect,&modenum))return;		
	//处理用户输入的最小电流的检测
	ParamPtr=IsParameterExist("45",18,NULL);
  if(ParamPtr!=NULL)
	  {
		buf=atof(ParamPtr);
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL)
			UARTPuts((char *)ModeSelectStr[4]);
		else if(buf<0||buf==NAN||buf>FusedMaxCurrent||buf>=TargetMode->LEDCurrentHigh)
			UartPrintf((char *)CurrentIllegalInfo,"最小",0,FusedMaxCurrent,TargetMode->LEDCurrentHigh);
	  else if(TargetMode->Mode==LightMode_Ramp&&buf<MinimumLEDCurrent)
			UartPrintf("\r\n错误:对于无极调光模式而言,您指定的最小电流值(地板电流)应大于等于%.1fA.\r\n",MinimumLEDCurrent);
		else
			{
			TargetMode->LEDCurrentLow=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf((char *)CurrentHasChanged,"最小",TargetMode->LEDCurrentLow);
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
		else if(buf<MinimumLEDCurrent||buf==NAN||buf>FusedMaxCurrent||buf<=TargetMode->LEDCurrentLow)
			UartPrintf((char *)CurrentIllegalInfo,"额定",MinimumLEDCurrent,FusedMaxCurrent,TargetMode->LEDCurrentLow);
	  else
			{
			TargetMode->LEDCurrentHigh=buf;
			DisplayWhichModeSelected(UserSelect,modenum);
			UartPrintf((char *)CurrentHasChanged,"额定",TargetMode->LEDCurrentHigh);
		  if(AccountState!=Log_Perm_Root&&buf>ForceThermalControlCurrent) //电流调整后进行计算
			  {
				DisplayWhichModeSelected(UserSelect,modenum);
			  UARTPuts("的温控功能因为电流较高,为了保证手电安全已被强制启用.\r\n");
				TargetMode->IsModeAffectedByStepDown=true;
				}
			}
		IsCmdParamOK=true;
		}		
	if(!IsCmdParamOK)UartPrintCommandNoParam(18);//显示啥也没找到的信息 
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕	
	}
