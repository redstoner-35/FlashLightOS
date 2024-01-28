#include "delay.h"
#include "LinearDIM.h"
#include "modelogic.h"
#include "console.h"
#include "Cfgfile.h"
#include "runtimelogger.h"

#ifdef FlashLightOS_Debug_Mode	
//变量声明
bool IsTimeExceeded; //测试变量，测试时间是否到达
#endif

//函数声明
void ExitLowPowerMode(void);

//设置RTC时钟
void SetupRTCForCounter(bool IsRTCStartCount)
  {
  //关闭RTC
	if(!IsRTCStartCount)
	  {
	  if(!(HT_RTC->CR&0x01))	return; //RTC已经被关闭，不需要再次关闭
		NVIC_DisableIRQ(RTC_IRQn); //禁用RTC中断
    RTC_DeInit();//禁用RTC
		}
	#ifdef FlashLightOS_Debug_Mode
	else if(!(HT_RTC->CR&0x01)) //RTC未启用,启用RTC
	  {
		NVIC_EnableIRQ(RTC_IRQn); //启用RTC中断
		HT_RTC->CR=0xF14; //使用LSI作为时钟，CMPCLR=1，当RTC溢出后自动清除CNT值 
		HT_RTC->CMP=5; //设置比较值为5，5秒后动作
		HT_RTC->IWEN=0x02; //比较中断使能
	  HT_RTC->CR|=0x01; //启动RTC开始计时
		}			
  #else
	else if(RunLogEntry.Data.DataSec.IsFlashLightLocked)return; //手电已经是上锁的了，不需要再启动RTC
	else if(!(HT_RTC->CR&0x01)&&CfgFile.AutoLockTimeOut>0) //RTC未启用且自动上锁时间大于0，启用RTC
	  {
		NVIC_EnableIRQ(RTC_IRQn); //启用RTC中断
		HT_RTC->CR=0xF14; //使用LSI作为时钟，CMPCLR=1，当RTC溢出后自动清除CNT值 
		HT_RTC->CMP=CfgFile.AutoLockTimeOut; //设置比较值
		HT_RTC->IWEN=0x02; //比较中断使能
	  HT_RTC->CR|=0x01; //启动RTC开始计时
		}
	#endif
	}
//RTC比较中断触发的处理函数
void RTCCMIntHandler(void)
  {
	#ifdef FlashLightOS_Debug_Mode	
  //时间到，设置flag为真然后关闭RTC
	IsTimeExceeded=true;	
	SetupRTCForCounter(false);
	#else	
	//首先检测电源状态，如果电源状态已经进入深度睡眠则首先退出低功耗模式，然后在结束写入后立刻重新进入
  if(SysPstatebuf.Pstate==PState_DeepSleep)
	  {
		DeepSleepTimer=0;  //立即重新进入睡眠
	  ExitLowPowerMode();
		}
	//关闭RTC并复位
	SetupRTCForCounter(false);
	//设置运行日志中的锁定位为1然后写入日志
	CalcLastLogCRCBeforePO();//计算CRC32
	RunLogEntry.Data.DataSec.IsFlashLightLocked=true;//指示手电已锁定
	RunLogEntry.CurrentDataCRC=CalcRunLogCRC32(&RunLogEntry.Data);
	WriteRuntimeLogToROM();	//计算新的CRC32然后写入运行日志
  SysPstatebuf.Pstate=PState_Locked; //进入锁定阶段
	#endif
	}
