#include "console.h"
#include "CfgFile.h"
#include "modelogic.h"
#include <stdlib.h>
#include <string.h>

//外部变量
extern const char *ModeSelectStr[];

//参数帮助entry
const char *modeviewArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return "指定要查看详细数据的挡位所在的挡位组.";
		case 2:
		case 3:return "指定该挡位组中要被编辑的挡位序号.";	
		}
	return NULL;
	} 

//命令主函数	
void modeviewhandler(void)
  {
	char ParamOK;
  int modenum,maxmodenum,i,len,totalstep;
	ModeGrpSelDef UserSelect;
	ModeConfStr *TargetMode;
	char *ParamPtr;
	//使用详细视图显示
  IsParameterExist("0123",26,&ParamOK);
	if(ParamOK)
	  {
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
		//获得挡位模式
		TargetMode=GetSelectedModeConfig(UserSelect,modenum);
		if(TargetMode==NULL) //挡位数据库为空
		   UARTPuts((char *)ModeSelectStr[4]);
		else //正常显示
		  {
			UARTPuts("\r\n");
			UARTPutc('-',8);
			UARTPuts(" 挡位信息查看器(详细视图) ");
			UARTPutc('-',8);
			UARTPuts("\r\n");
			UartPrintf("\r\n  挡位名称 : %s",TargetMode->ModeName);
			UartPrintf("\r\n  挡位是否启用 : %s",TargetMode->IsModeEnabled?"是":"否");
			UartPrintf("\r\n  挡位是否带记忆功能 : %s",TargetMode->IsModeHasMemory?"是":"否");
			UartPrintf("\r\n  挡位是否带温控功能 : %s",TargetMode->IsModeAffectedByStepDown?"是":"否");
			UartPrintf("\r\n  挡位模式 : %s模式",LightModeString[(int)TargetMode->Mode]);
			//显示电流
			if(TargetMode->Mode!=LightMode_Ramp&&TargetMode->Mode!=LightMode_Breath)
				UartPrintf("\r\n  挡位额定LED电流 : %.2fA",TargetMode->LEDCurrentHigh);
			else
			  {
				UartPrintf("\r\n  挡位天花板电流(最高电流) : %.2fA",TargetMode->LEDCurrentHigh);
				UartPrintf("\r\n  挡位地板电流(最低电流) : %.2fA",TargetMode->LEDCurrentLow);
				}
			//显示其他参数
			switch(TargetMode->Mode)
			  {
				case LightMode_On:break;//常亮
				case LightMode_Flash://爆闪
					 UartPrintf("\r\n  挡位爆闪频率 : %.1fHz",TargetMode->StrobeFrequency);
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
					 UartPrintf("\r\n  亮度爬升/下滑总时长 : %.1f秒",TargetMode->RampModeSpeed);
				   break;
 				case LightMode_CustomFlash: //自定义闪模式
				   UartPrintf("\r\n  自定义闪控制字符串 : '%s'",TargetMode->CustomFlashStr);
				   UartPrintf("\r\n  最高频闪频率 : %.1fHz",TargetMode->CustomFlashSpeed);
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
		UARTPuts(" 挡位信息查看器(简略视图) ");
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
			UartPrintf("\r\n  ----------  常规功能挡位组 -----------");
			UartPrintf("\r\n  | 挡位序号 |  挡位模式  |  挡位名称  ");
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
		  UartPrintf("\r\n  ----------  双击功能挡位组 -----------");
		  UartPrintf("\r\n  | 挡位序号 |  挡位模式  |  挡位名称  ");
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
			UartPrintf("\r\n  ----------  特殊功能挡位组 -----------");
			UartPrintf("\r\n  | 挡位序号 |  挡位模式  |  挡位名称  ");
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
