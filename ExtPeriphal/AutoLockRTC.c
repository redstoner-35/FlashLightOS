#include "delay.h"
#include "LinearDIM.h"
#include "modelogic.h"
#include "console.h"
#include "Cfgfile.h"
#include "runtimelogger.h"

//函数声明
void ExitLowPowerMode(void);

//设置RTC时钟
void SetupRTCForCounter(bool IsRTCStartCount)
  {
	//启用BKP时钟才能访问BKP域
	CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
  CLKConfig.Bit.BKP = 1;
  CKCU_PeripClockConfig(CLKConfig,ENABLE);
  if((PWRCU_CheckReadyAccessed() != PWRCU_OK))return; //等待BKP可以访问
  //关闭RTC
	if(!IsRTCStartCount)
	  {
	  if(!(HT_RTC->CR&0x01))
		  {
			CKCU_PeripClockConfig(CLKConfig,DISABLE);
			return; //RTC已经被关闭，不需要再次关闭
			}
		NVIC_DisableIRQ(RTC_IRQn); //禁用RTC中断
		RTC_DeInit(); //复位RTC的所有寄存器
		}
	else if(RunLogEntry.Data.DataSec.IsFlashLightLocked) //手电已经是上锁的了，不需要再启动RTC
	  {
		CKCU_PeripClockConfig(CLKConfig,DISABLE); //关闭BKP访问
		return;
		}
	else if(!(HT_RTC->CR&0x01)&&CfgFile.AutoLockTimeOut>0) //RTC未启用且自动上锁时间大于0，启用RTC
	  {
		HT_RTC->CR|=0x10; //CMPCLR=1，当RTC溢出后自动清除CNT值
		HT_RTC->CMP=CfgFile.AutoLockTimeOut-1; //设置比较值
		HT_RTC->IWEN=0x202; //比较唤醒和比较中断使能
	  NVIC_EnableIRQ(RTC_IRQn); //启用RTC中断
	  HT_RTC->CR|=0x01; //启动RTC开始计时
		}
	CKCU_PeripClockConfig(CLKConfig,DISABLE); //关闭BKP访问
	}
//RTC比较中断触发的处理函数
void RTCCMIntHandler(void)
  {
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
	}
