#include "console.h"
#include "modelogic.h"
#include "cfgfile.h"
#include "SideKey.h"
#include "LEDMgmt.h"
#include "logger.h"
#include "runtimelogger.h"

//内部和外部变量
volatile SYSPStateStrDef SysPstatebuf;
extern int AutoOffTimer;
const char *ErrorStrDuringPost="上电自检期间";

/*  辅助电源引脚的自动define,不允许修改！  */
#define AUXPWR_EN_IOB STRCAT2(GPIO_P,AUXPWR_EN_IOBank)
#define AUXPWR_EN_IOG STRCAT2(HT_GPIO,AUXPWR_EN_IOBank)
#define AUXPWR_EN_IOP STRCAT2(GPIO_PIN_,AUXPWR_EN_IOPinNum)

//初始化系统的电源状态的状态机
void PStateInit(void)
  {
	//初始化辅助电源管理的IO
	AFIO_GPxConfig(AUXPWR_EN_IOB,AUXPWR_EN_IOP, AFIO_FUN_GPIO);//配置为GPIO
  GPIO_DirectionConfig(AUXPWR_EN_IOG,AUXPWR_EN_IOP,GPIO_DIR_OUT);//输出
  GPIO_ClearOutBits(AUXPWR_EN_IOG,AUXPWR_EN_IOP);//默认输出0
	//初始化电源状态管理状态机的相关变量
	SysPstatebuf.ErrorCode=Error_None;//无错误
	SysPstatebuf.Pstate=CfgFile.IsDriverLockedAfterPOR?PState_Locked:PState_Standby;//根据配置文件配置为locked或者状态
  SysPstatebuf.ToggledFlash=true;//LED点亮
	SysPstatebuf.Duty=30;//默认PWM模式占空比从30%开始
  SysPstatebuf.IsLinearDim=true;//使用线性调光
	SysPstatebuf.CurrentThrottleLevel=0;
  SysPstatebuf.CurrentDACVID=0;
  //初始化电池遥测模块
	RunTimeBattTelemResult.TargetSensorADDR=INA219ADDR;//设置地址
	RunTimeBattTelemResult.BusCurrent=0;
	RunTimeBattTelemResult.BusPower=0;
	RunTimeBattTelemResult.BusVolt=0;
	//复位状态机
	ResetRampMode();//重置无极调光模块
	ResetBreathStateMachine();
  ResetCustomFlashControl();//复位自定义闪控制
  MorseSenderReset();//关灯后重置呼吸和摩尔斯电码发送的状态机	
	}
//驱动锁定状态的上电恢复处理
void DriverLockPOR(void)
  {
	bool IsLightNeedtolock;
	//判断手电筒是否需要锁定
	if(CfgFile.IsDriverLockedAfterPOR)IsLightNeedtolock=true; 
	else IsLightNeedtolock=RunLogEntry.Data.DataSec.IsFlashLightLocked; 
	SysPstatebuf.Pstate=IsLightNeedtolock?PState_Locked:PState_Standby;//根据配置文件配置为locked或者状态
	UartPost(Msg_info,"PORLock","Flash light will be set to %s state after selftest is done.",IsLightNeedtolock?"locked":"unlocked");
	//将变更后的手电锁定状态写入到ROM
	if(!IsRunTimeLoggingEnabled)return;
	if(IsRunTimeLoggingEnabled)CalcLastLogCRCBeforePO();//填写数据前更新运行log的CRC32
  RunLogEntry.Data.DataSec.IsFlashLightLocked=IsLightNeedtolock;
	RunLogEntry.CurrentDataCRC=CalcRunLogCRC32(&RunLogEntry.Data);//填写数据后更新CRC-32
	WriteRuntimeLogToROM();//尝试写ROM
	}
	
//控制驱动上的3.3V辅助电源(给电流检测和SPS供电)
void SetAUXPWR(bool IsEnabled)
  {
	//根据输入控制IO
	if(!IsEnabled)GPIO_ClearOutBits(AUXPWR_EN_IOG,AUXPWR_EN_IOP);
	else GPIO_SetOutBits(AUXPWR_EN_IOG,AUXPWR_EN_IOP);
	}
/*
系统运行时出现错误的报错函数，负责生成日志并向电
源管理状态机提交错误代码和新的电源状态。
*/
void RunTimeErrorReportHandler(SystemErrorCodeDef ErrorCode)
  {
  SysPstatebuf.ErrorCode=ErrorCode;
	CollectLoginfo("正常运行中",&RunTimeBattTelemResult);
	SysPstatebuf.Pstate=PState_Error;
	TurnLightOFFLogic();
	return;
	}
/*
系统的状态机处理函数。该函数主要负责根据用户操作
实现各种状态(开关机,战术模式,锁定,待机等等)的跳转。	
*/
void PStateStateMachine(void)
  {
	bool LongPressOnce,LongPressHold,DoubleClickHold;
	int ShortPress;
	INADoutSreDef BattO;
  //获取侧按按钮的操作(按下并按住，短按，长按等等)
	LongPressHold=getSideKeyHoldEvent(); 
	LongPressOnce=getSideKeyLongPressEvent(); 
	ShortPress=getSideKeyShortPressCount(true); 
	DoubleClickHold=getSideKeyDoubleClickAndHoldEvent();
	//电源状态的状态机
	switch(SysPstatebuf.Pstate)
	  {
		/*******************************************************************************************
		手电处于锁定状态，此时任何操作无效。*/
		case PState_Locked:
		  {
			//五击侧按，解锁手电
		  if(ShortPress==5)
			  {
				RunLogEntry.Data.DataSec.IsFlashLightLocked=false;//指示手电已解锁
				ForceWriteRuntimelog();//尝试写ROM
				CurrentLEDIndex=25;
				SysPstatebuf.Pstate=PState_Standby; 
				}
			//其余任何按键情况，手电红色灯闪烁三次表示已锁定
			else if(ShortPress||LongPressOnce||DoubleClickHold||LongPressHold||getSideKeyTripleClickAndHoldEvent())
				CurrentLEDIndex=27;
			//时间到，深度睡眠
			else if(DeepSleepTimer==0)
				SysPstatebuf.Pstate=PState_DeepSleep; 
			//执行休眠定时器的判断
			else 
				DeepSleepTimerCallBack();
			break;
			}
		/*******************************************************************************************
		普通模式,待机*/
		case PState_Standby:
		  {
			//五击侧按，锁定手电
		  if(ShortPress==5)
			  {
				RunLogEntry.Data.DataSec.IsFlashLightLocked=true;//指示手电已锁定
				ForceWriteRuntimelog();//尝试写ROM
				CurrentLEDIndex=26;
				SysPstatebuf.Pstate=PState_Locked; 
				}
			//长按3秒开启手电
		  else if(LongPressOnce)
			  {
			  if(IsRunTimeLoggingEnabled)CalcLastLogCRCBeforePO();//计算运行log的CRC32
			  SysPstatebuf.ErrorCode=TurnLightONLogic(&BattO);//执行自检逻辑
			  if(SysPstatebuf.ErrorCode==Error_None)SysPstatebuf.Pstate=PState_LEDOn;
				else //如果自检成功则跳转到开灯状态，否则跳转到错误状态并写入日志。
				  {	
				  if(SysPstatebuf.ErrorCode!=Error_Input_UVP)CollectLoginfo(ErrorStrDuringPost,&BattO);
					else WriteRunLogWithLVAlert();//标记低压告警次数+1
					SysPstatebuf.Pstate=PState_Error; 
					TurnLightOFFLogic();
					ResetRampMode();//重置无极调光模块
		      ResetBreathStateMachine();
				  ResetCustomFlashControl();//复位自定义闪控制
					MorseSenderReset();//关灯后重置呼吸和摩尔斯电码发送的状态机
				  }
				}
			//快速短按四次，进入战术模式(长按并按住开灯,松手就灭)
		  else if(ShortPress==4)
			  {
				CurrentLEDIndex=18;
				SysPstatebuf.Pstate=PState_NonHoldStandBy;
				}
			//时间到，深度睡眠
			else if(DeepSleepTimer==0)
				SysPstatebuf.Pstate=PState_DeepSleep; 
			//执行休眠定时器的判断
			else 
				DeepSleepTimerCallBack();
		  break;
			}
		/*******************************************************************************************
		战术模式,待机*/
		case PState_NonHoldStandBy:
		  {
			//五击侧按，锁定手电
		  if(ShortPress==5)
			  {
				RunLogEntry.Data.DataSec.IsFlashLightLocked=true;//指示手电已锁定
				ForceWriteRuntimelog();//尝试写ROM
				CurrentLEDIndex=26;
				SysPstatebuf.Pstate=PState_Locked; 
				}
			//长按3秒开启手电
		  else if(LongPressHold)
			  {
			  if(IsRunTimeLoggingEnabled)CalcLastLogCRCBeforePO();//计算运行log的CRC32
				SysPstatebuf.ErrorCode=TurnLightONLogic(&BattO);//执行自检逻辑
			  if(SysPstatebuf.ErrorCode==Error_None)SysPstatebuf.Pstate=PState_LEDOnNonHold;
				else //如果自检成功则跳转到开灯状态，否则跳转到错误状态并写入日志。
				  {		
					if(SysPstatebuf.ErrorCode!=Error_Input_UVP)CollectLoginfo(ErrorStrDuringPost,&BattO);
					else WriteRunLogWithLVAlert();//标记低压告警次数+1
					SysPstatebuf.Pstate=PState_Error; 
					TurnLightOFFLogic();
		      ResetBreathStateMachine();
					ResetCustomFlashControl();//复位自定义闪控制
					ResetRampMode();//重置无极调光模块
          MorseSenderReset();//关灯后重置呼吸和摩尔斯电码发送的状态机
				  }
				}
			//快速短按四次，退出战术模式回到普通模式(长按开灯,再次长按关灯)
		  else if(ShortPress==4)
			  {
				CurrentLEDIndex=19;
			  SysPstatebuf.Pstate=PState_Standby;
				}
			//时间到，深度睡眠
			else if(DeepSleepTimer==0)
				SysPstatebuf.Pstate=PState_DeepSleep; 
			//执行休眠定时器的判断
			else 
				DeepSleepTimerCallBack();
		  break;
			}		  
		/*******************************************************************************************
		驱动检测到异常进入保护模式*/
		case PState_Error: 
		  {
			switch(SysPstatebuf.ErrorCode)//根据错误代码驱动侧按LED闪烁对应的提示码
			  {
				case Error_None:CurrentLEDIndex=0;break; //无错误
				case Error_Mode_Logic:CurrentLEDIndex=21;break;//内部模式逻辑错误
			  case Error_Input_OVP:  CurrentLEDIndex=11;break;//输入过压保护
				case Error_Input_OCP:  CurrentLEDIndex=22;break;//输入电流过流保护
			  case Error_SPS_TMON_Offline:
			  case Error_Thremal_Logic: //温控逻辑异常
				case Error_PWM_Logic:  CurrentLEDIndex=17;break;//PWM逻辑异常
				case Error_SPS_CATERR:CurrentLEDIndex=24;break; //智能功率级反馈灾难性错误
				case Error_SPS_ThermTrip:  CurrentLEDIndex=10;break; //智能功率级过热保护
				case Error_LED_Open:  CurrentLEDIndex=14;break; //灯珠开路
				case Error_LED_Short:  CurrentLEDIndex=15;break; //灯珠短路
				case Error_LED_OverCurrent:  CurrentLEDIndex=12;break; //灯珠过流保护
				case Error_LED_ThermTrip: CurrentLEDIndex=16;break; //灯珠过热保护
				case Error_Input_UVP: CurrentLEDIndex=9;break; //输入欠压保护
				case Error_DAC_Logic: CurrentLEDIndex=20;break; //DAC异常
				case Error_ADC_Logic: CurrentLEDIndex=13;break; //ADC异常
				}				
			//在不是模式逻辑错误的情况下长按3秒或者到达休眠状态,重置错误并退回到待机模式
			if((LongPressOnce||DeepSleepTimer==0)&&SysPstatebuf.ErrorCode!=Error_Mode_Logic)
			  {
				CurrentLEDIndex=0;
				LED_Reset();//重置侧按LED闪烁控制器为熄灭状态
				ResetRampMode();//重置无极调光模块
			  ResetBreathStateMachine();
				ResetCustomFlashControl();//复位自定义闪控制
				MorseSenderReset();//复位所有的特殊功能状态机
				if(LongPressOnce)
				  {
					SysPstatebuf.Pstate=PState_Standby;
					SysPstatebuf.ErrorCode=Error_None; //手动清除错误码才回到待机状态
					}
				else
					SysPstatebuf.Pstate=PState_DeepSleep;//进入深度睡眠
				RunLogEntry.Data.DataSec.IsLowVoltageAlert=false;//清除低电压警报
				ModeNoMemoryRollBackHandler();//关闭主灯后检查挡位是否带记忆，不带的就自动复位
				WriteRuntimeLogToROM();//尝试写ROM
				}
			//执行休眠定时器的判断
			else 
				DeepSleepTimerCallBack();
			break;
			}
		/*******************************************************************************************
		正常模式下，自检通过后主LED点亮的模式*/
		case PState_LEDOn:
		  {
			//执行运行过程中的故障检测,以及挡位逻辑
			FlashTimerInitHandler();                  //定时器配置
	    RuntimeModeCurrentHandler();              //运行过程中的电流管理
			//长按3秒或者定时器已经到时间了,关闭LED回到待机状态
		  if(LongPressOnce||AutoOffTimer==0)
			 {
			 SysPstatebuf.Pstate=PState_Standby;//返回到待机状态
			 TurnLightOFFLogic();
			 ModeNoMemoryRollBackHandler();//关闭主灯后检查挡位是否带记忆，不带的就自动复位
			 ResetPowerOffTimerForPoff();//重置定时器
		   ResetBreathStateMachine();
			 ResetRampMode();//重置无极调光模块
			 ResetCustomFlashControl();//复位自定义闪控制
       MorseSenderReset();//关灯后重置呼吸和摩尔斯电码发送的状态机
			 WriteRuntimeLogToROM();//尝试写ROM
			 RunLogEntry.Data.DataSec.IsLowVoltageAlert=false;//清除低电压警报
			 }				 
			break;
			}
		/*******************************************************************************************
		战术模式下，自检通过后主LED点亮的模式*/
		case PState_LEDOnNonHold:
		  {
			//执行运行过程中的故障检测,以及挡位逻辑
			FlashTimerInitHandler();                   //定时器配置
	    RuntimeModeCurrentHandler();               //运行过程中的电流管理
			//当用户放开侧按或者定时器已经到时间后,回到战术模式的待机状态
		  if(!LongPressHold||AutoOffTimer==0)
			  {
				SysPstatebuf.Pstate=PState_NonHoldStandBy;//回到战术模式的待机状态
			  TurnLightOFFLogic();
				ResetPowerOffTimerForPoff();//重置定时器
				ResetRampMode();//重置无极调光模块
			  ResetBreathStateMachine();
				ResetCustomFlashControl();//复位自定义闪控制
        MorseSenderReset();//关灯后重置呼吸和摩尔斯电码发送的状态机
			  WriteRuntimeLogToROM();//尝试写ROM
				RunLogEntry.Data.DataSec.IsLowVoltageAlert=false;//清除低电压警报
				}					
			break;
			}
	  /*******************************************************************************************
		当用户长时间没有操作后,关闭所有外设省电的模式*/
		case PState_DeepSleep:
		  {
			EnteredLowPowerMode();//这个函数是阻塞执行因此进来之后就不会出去了。所以下面啥也不用
			break;
			}
		}
	}
