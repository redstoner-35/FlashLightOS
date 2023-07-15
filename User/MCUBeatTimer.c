#include "HT32.h"
#include "console.h"
#include "delay.h"

const char *CheckHBTime="SYSHBTIM";
extern bool SensorRefreshFlag;

void CheckHBTimerIsEnabled(void)
 {
 int retry=0;
 UartPost(Msg_info,CheckHBTime,"Checking System heartbeat timer statu..."); 
 SensorRefreshFlag=false;
 while(!SensorRefreshFlag)
   {
	 delay_ms(10);
	 retry++;
	 if(retry==300)
	   {
		 UartPost(Msg_critical,CheckHBTime,"System 4Hz heartbeat timer did not start."); 
		 SelfTestErrorHandler();
		 }
	 }
 UartPost(Msg_info,CheckHBTime,"System heartbeat timer is working."); 
 }
//启用系统内的0.25秒心跳定时器
void EnableHBTimer(void)
 {
 CKCU_PeripClockConfig_TypeDef CKCUClock = {{ 0 }};  
 TM_TimeBaseInitTypeDef TimeBaseInit;
 //重新配置定时器用于产生0.25秒的定时中断
 CKCUClock.Bit.GPTM0 = 1;
 CKCU_PeripClockConfig(CKCUClock, ENABLE);
 TimeBaseInit.Prescaler = 4799;                         // 48MHz->10KHz
 TimeBaseInit.CounterReload = 1249;                  // 10KHz->8Hz
 TimeBaseInit.RepetitionCounter = 0;
 TimeBaseInit.CounterMode = TM_CNT_MODE_UP;
 TimeBaseInit.PSCReloadTime = TM_PSC_RLD_IMMEDIATE;
 TM_TimeBaseInit(HT_GPTM0, &TimeBaseInit);
 TM_ClearFlag(HT_GPTM0, TM_FLAG_UEV);
 //配置好中断然后让定时器运行起来
 NVIC_EnableIRQ(GPTM0_IRQn);
 TM_IntConfig(HT_GPTM0,TM_INT_UEV,ENABLE);
 TM_Cmd(HT_GPTM0, ENABLE);
 }

//关闭系统内的0.25秒心跳定时器
void DisableHBTimer(void)
 {
  CKCU_PeripClockConfig_TypeDef CKCUClock = {{ 0 }};  
 //禁用定时器并关闭中断
 TM_Cmd(HT_GPTM0, DISABLE);
 TM_ClearFlag(HT_GPTM0, TM_FLAG_UEV);
 NVIC_DisableIRQ(GPTM0_IRQn);
 //关闭定时器时钟
 CKCUClock.Bit.GPTM0 = 1;
 CKCU_PeripClockConfig(CKCUClock, DISABLE);	 
 }	
