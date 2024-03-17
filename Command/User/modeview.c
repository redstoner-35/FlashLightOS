#include "console.h"
#include "CfgFile.h"
#include "modelogic.h"
#include "runtimelogger.h"
#include <stdlib.h>
#include <string.h>

//外部变量
extern const char *ModeSelectStr[];
static const char *ModeInfoStr="\r\n  挡位";
static const char *ModeViewStr=" 挡位信息查看器(%s视图) ";

//外部函数
const char *ConvertStrobeModeStr(ModeConfStr *TargetMode);

//显示挡位组信息
static void DisplayModeGroupBar(const char *GrpName)
  {
	UARTPuts("\r\n  ");
	UARTPutc('-',10);
	UartPrintf("  %s功能挡位组  ",GrpName);
	UARTPutc('-',10);
	UARTPuts("\r\n  | 挡位序号 |  挡位模式  |  挡位名称  ");
	}

//参数帮助entry
const char *modeviewArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return "指定要查看挡位所在的挡位组.";
		case 2:
		case 3:return "指定该挡位组中要查看数据的挡位序号.";	
		}
	return NULL;
	} 
//命令主函数	
void modeviewhandler(void)
  {
	char ParamOK;
  int modenum,i,len,totalstep,RampCfgIndex;
	ModeGrpSelDef UserSelect;
	ModeConfStr *TargetMode;
	//使用详细视图显示
  IsParameterExist("0123",26,&ParamOK);
	if(ParamOK)
	  {
		//识别用户输入的模式组
		if(!GetUserModeNum(26,&UserSelect,&modenum))return;		
		//获得挡位模式
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL) //挡位数据库为空
		   UARTPuts((char *)ModeSelectStr[4]);
		else //正常显示
		  {
		  switch(UserSelect)
	      {
		    case ModeGrp_Regular:RampCfgIndex=modenum;break;//常规挡位
		    case ModeGrp_DoubleClick:RampCfgIndex=8;break;//双击挡位
		    case ModeGrp_Special:RampCfgIndex=modenum+9;break;//特殊功能
	    	default : ClearRecvBuffer();CmdHandle=Command_None;return; //命令出错直接退出
			  } 
		  //显示数据
			UARTPuts("\r\n");
			UARTPutc('-',8);
			UartPrintf((char *)ModeViewStr,"详细");
			UARTPutc('-',8);
			UARTPuts("\r\n");
			UartPrintf("%s名称 : %s",(char *)ModeInfoStr,TargetMode->ModeName);
			UartPrintf("%s是否启用 : %s",(char *)ModeInfoStr,TargetMode->IsModeEnabled?"是":"否");
			UartPrintf("%s是否带记忆功能 : %s",(char *)ModeInfoStr,TargetMode->IsModeHasMemory?"是":"否");
			UartPrintf("%s是否带温控功能 : %s",(char *)ModeInfoStr,TargetMode->IsModeAffectedByStepDown?"是":"否");
			UartPrintf("%s温控值偏移 : -%.2fC",(char *)ModeInfoStr,TargetMode->ThermalControlOffset);
			if(TargetMode->MaxMomtTurboCount>0)
			  UartPrintf("%s鸡血最大次数 : %d",(char *)ModeInfoStr,TargetMode->MaxMomtTurboCount);
			UARTPuts("\r\n  定时关机 : ");
			if(TargetMode->PowerOffTimer>0)
				UartPrintf("%d分钟后自动关机",TargetMode->PowerOffTimer);
			else
				UARTPuts("已禁用");
			UartPrintf("%s模式 : %s模式",(char *)ModeInfoStr,LightModeString[(int)TargetMode->Mode]);
			//显示电流
			if(TargetMode->Mode!=LightMode_Ramp&&TargetMode->Mode!=LightMode_Breath)
				UartPrintf("%s额定LED电流 : %.2fA",(char *)ModeInfoStr,TargetMode->LEDCurrentHigh);
			else
			  {
				UartPrintf("%s天花板电流(最高电流) : %.2fA",(char *)ModeInfoStr,TargetMode->LEDCurrentHigh);
				UartPrintf("%s地板电流(最低电流) : %.2fA",(char *)ModeInfoStr,TargetMode->LEDCurrentLow);
				}
			//显示其他参数
			switch(TargetMode->Mode)
			  {
				case LightMode_On:break;//常亮
				case LightMode_BreathFlash:
				case LightMode_RandomFlash:
					UartPrintf("\r\n  %s上/下限频率 : %.1fHz/%.1fHz",ConvertStrobeModeStr(TargetMode),TargetMode->RandStrobeMinFreq,TargetMode->RandStrobeMaxFreq);
				   break;
				case LightMode_Flash://爆闪
					 UartPrintf("\r\n  爆闪频率 : %.1fHz",TargetMode->StrobeFrequency);
				   break;
			  case LightMode_MosTrans://摩尔斯代码发送
					 UartPrintf("\r\n  自定义发送内容 : '%s'",TargetMode->MosTransStr);
 				case LightMode_SOS://SOS
					 UartPrintf("\r\n  发送速度('.'的长度) : %.2f秒",TargetMode->MosTransferStep);
				   break; 
 				case LightMode_Breath://呼吸模式(参数调整正确时为信标模式)
					 UartPrintf("\r\n  亮度爬升时间 : %.1f秒",TargetMode->CurrentRampUpTime);
				   UartPrintf("\r\n  最高亮度保持时间 : %.1f秒",TargetMode->MaxCurrentHoldTime);
				   UartPrintf("\r\n  亮度下滑时间 : %.1f秒",TargetMode->CurrentRampDownTime);
				   UartPrintf("\r\n  亮度下滑时间 : %.1f秒",TargetMode->MinCurrentHoldTime);
				   break;
 				case LightMode_Ramp: //无极调光模式
					 UartPrintf("\r\n  默认亮度等级 : %.1f%%",CfgFile.DefaultLevel[RampCfgIndex]*100);
			     UartPrintf("\r\n  当前亮度等级 : %.1f%%",RunLogEntry.Data.DataSec.RampModeStor[RampCfgIndex].RampModeConf*100);
			     UartPrintf("\r\n  当前调光方向 : %s",!RunLogEntry.Data.DataSec.RampModeStor[RampCfgIndex].RampModeDirection?"向上":"向下");
			     UartPrintf("\r\n  亮度等级是否记忆 : %s",CfgFile.IsRememberBrightNess[RampCfgIndex]?"是":"否");	 
					 UartPrintf("\r\n  亮度爬升/下滑总时长 : %.1f秒",TargetMode->RampModeSpeed);
				   break;
 				case LightMode_CustomFlash: //自定义闪模式
				   UartPrintf("\r\n  自定义闪控制字符串 : '%s'",TargetMode->CustomFlashStr);
				   UartPrintf("\r\n  序列运行频率 : %.1fHz",TargetMode->CustomFlashSpeed);
				//其余的啥也不做
				default : break;
				}
			UARTPuts("\r\n\r\n");
			}
		}
	else //显示简略视图
	  {
		UARTPuts("\r\n");
		UARTPutc('-',8);
		UartPrintf((char *)ModeViewStr,"概览");
		UARTPutc('-',8);
		UARTPuts("\r\n");
		//统计常规功能挡位组内的挡位数量
		totalstep=0;
		for(i=0;i<8;i++)
			{
			TargetMode=GetSelectedModeConfig(ModeGrp_Regular,i);
			if(TargetMode==NULL||!TargetMode->IsModeEnabled)continue;
			totalstep++;
			}
		//显示常规挡位组内的挡位
		if(totalstep>0)
		  {			
      DisplayModeGroupBar("常规");
			for(i=0;i<8;i++)
				{
				TargetMode=GetSelectedModeConfig(ModeGrp_Regular,i);
				if(TargetMode==NULL||!TargetMode->IsModeEnabled)continue; //挡位数据库为空或者改挡位被禁用不显示
				UartPrintf("\r\n  |   NO.%d   |  ",i);
				len=UartPrintf("%s",LightModeString[(int)TargetMode->Mode]);
				UARTPutc(' ',10-len);
				UartPrintf("|  %s",TargetMode->ModeName);
				}
			UARTPuts("\r\n\r\n");
			}
		//显示双击功能挡位组的挡位
		TargetMode=GetSelectedModeConfig(ModeGrp_DoubleClick,0);
		if(TargetMode!=NULL&&TargetMode->IsModeEnabled)
		  {
      DisplayModeGroupBar("双击");
			UartPrintf("\r\n  |   ----   |  ");
			len=UartPrintf("%s",LightModeString[(int)TargetMode->Mode]);
			UARTPutc(' ',10-len);
			UartPrintf("|  %s",TargetMode->ModeName);			
			UARTPuts("\r\n\r\n");
			}
		//统计三击功能的挡位数目
		totalstep=0;
		for(i=0;i<4;i++)
			{
			TargetMode=GetSelectedModeConfig(ModeGrp_Special,i);
			if(TargetMode==NULL||!TargetMode->IsModeEnabled)continue;
			totalstep++;
			}
		//显示常规挡位组内的挡位
		if(totalstep>0)
		  {			
      DisplayModeGroupBar("特殊");
			for(i=0;i<4;i++)
				{
				TargetMode=GetSelectedModeConfig(ModeGrp_Special,i);
				if(TargetMode==NULL||!TargetMode->IsModeEnabled)continue; //挡位数据库为空或者改挡位被禁用不显示
				UartPrintf("\r\n  |   NO.%d   |  ",i);
				len=UartPrintf("%s",LightModeString[(int)TargetMode->Mode]);
				UARTPutc(' ',10-len);
				UartPrintf("|  %s",TargetMode->ModeName);
				}
			UARTPuts("\r\n\r\n");
			}
		UARTPuts("\r\n\r\n");
		}	
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕
	}
