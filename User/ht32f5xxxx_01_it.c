/*********************************************************************************************************//**
 * @file    GPIO/InputOutput/ht32f5xxxx_01_it.c
 * @version $Rev:: 2970         $
 * @date    $Date:: 2018-08-03#$
 * @brief   This file provides all interrupt service routine.
 *************************************************************************************************************
 * @attention
 *
 * Firmware Disclaimer Information
 *
 * 1. The customer hereby acknowledges and agrees that the program technical documentation, including the
 *    code, which is supplied by Holtek Semiconductor Inc., (hereinafter referred to as "HOLTEK") is the
 *    proprietary and confidential intellectual property of HOLTEK, and is protected by copyright law and
 *    other intellectual property laws.
 *
 * 2. The customer hereby acknowledges and agrees that the program technical documentation, including the
 *    code, is confidential information belonging to HOLTEK, and must not be disclosed to any third parties
 *    other than HOLTEK and the customer.
 *
 * 3. The program technical documentation, including the code, is provided "as is" and for customer reference
 *    only. After delivery by HOLTEK, the customer shall use the program technical documentation, including
 *    the code, at their own risk. HOLTEK disclaims any expressed, implied or statutory warranties, including
 *    the warranties of merchantability, satisfactory quality and fitness for a particular purpose.
 *
 * <h2><center>Copyright (C) Holtek Semiconductor Inc. All rights reserved</center></h2>
 ************************************************************************************************************/

/* Includes ------------------------------------------------------------------------------------------------*/
#include "ht32.h"
#include "LEDMgmt.h"
#include "SideKey.h"

/** @addtogroup HT32_Series_Peripheral_Examples HT32 Peripheral Examples
  * @{
  */

/** @addtogroup GPIO_Examples GPIO
  * @{
  */

/** @addtogroup InputOutput
  * @{
  */


/* Global functions ----------------------------------------------------------------------------------------*/
/*********************************************************************************************************//**
 * @brief   This function handles NMI exception.
 * @retval  None
 ************************************************************************************************************/
void NMI_Handler(void)
{
}

/*********************************************************************************************************//**
 * @brief   This function handles Hard Fault exception.
 * @retval  None
 ************************************************************************************************************/
void HardFault_Handler(void)
{
  #if 1

  static vu32 gIsContinue = 0;
  /*--------------------------------------------------------------------------------------------------------*/
  /* For development FW, MCU run into the while loop when the hardfault occurred.                           */
  /* 1. Stack Checking                                                                                      */
  /*    When a hard fault exception occurs, MCU push following register into the stack (main or process     */
  /*    stack). Confirm R13(SP) value in the Register Window and typing it to the Memory Windows, you can   */
  /*    check following register, especially the PC value (indicate the last instruction before hard fault).*/
  /*    SP + 0x00    0x04    0x08    0x0C    0x10    0x14    0x18    0x1C                                   */
  /*           R0      R1      R2      R3     R12      LR      PC    xPSR                                   */
  while (gIsContinue == 0)
  {
  }
  /* 2. Step Out to Find the Clue                                                                           */
  /*    Change the variable "gIsContinue" to any other value than zero in a Local or Watch Window, then     */
  /*    step out the HardFault_Handler to reach the first instruction after the instruction which caused    */
  /*    the hard fault.                                                                                     */
  /*--------------------------------------------------------------------------------------------------------*/

  #else

  /*--------------------------------------------------------------------------------------------------------*/
  /* For production FW, you shall consider to reboot the system when hardfault occurred.                    */
  /*--------------------------------------------------------------------------------------------------------*/
  NVIC_SystemReset();

  #endif
}

/*********************************************************************************************************//**
 * @brief   This function handles SVCall exception.
 * @retval  None
 ************************************************************************************************************/
void SVC_Handler(void)
{
}

/*********************************************************************************************************//**
 * @brief   This function handles PendSVC exception.
 * @retval  None
 ************************************************************************************************************/
void PendSV_Handler(void)
{
}

/*********************************************************************************************************//**
 * @brief   This function handles SysTick Handler.
 * @retval  None
 ************************************************************************************************************/
extern char DelaySeconds;

void SysTick_Handler(void)
{
	//秒数倒计时
  if(DelaySeconds)DelaySeconds--;
}
/*********************************************************************************************************//**
 * @brief   This function handles EXTI 4~15 interrupt.
 * @retval  None
 ************************************************************************************************************/
void EXTI4_15_IRQHandler(void)
{
  if (EXTI_GetEdgeFlag(ExtKey_EXTI_CHANNEL))
  {
    EXTI_ClearEdgeFlag(ExtKey_EXTI_CHANNEL);
    SideKey_Callback();//进入回调处理
  }
}

/*********************************************************************************************************//**
 * @brief   This function handles ADC interrupt.
 * @retval  None
 ************************************************************************************************************/
void ADC_EOC_interrupt_Callback(void);

void ADC_IRQHandler(void)
{
  if(ADC_GetIntStatus(HT_ADC0, ADC_INT_CYCLE_EOC) == SET)
    ADC_EOC_interrupt_Callback();//进行中断处理
}
/*********************************************************************************************************//**
 * @brief   This function handles PDMA channel 2 to 5 interrupt.
 * @retval  None
 ************************************************************************************************************/
void TXDMAIntCallBack(void);//发送DMA 

void PDMA_CH2_5_IRQHandler(void)
 {
 //USART TX 发送完毕中断发出
 if(PDMA_GetFlagStatus(PDMA_USART1_TX,PDMA_FLAG_TC))
  {
	PDMA_ClearFlag(PDMA_USART1_TX,PDMA_FLAG_TC);//清除flag
	TXDMAIntCallBack();//处理发送callback
	}
 }
/*********************************************************************************************************//**
 * @brief   This function handles GPTM0 interrupt.
 * @retval  None
 ************************************************************************************************************/
extern bool SensorRefreshFlag;
extern unsigned int LogErrTimer;
extern bool EnteredMainApp;
extern bool RXTimerFilpFlag;
void IdleTimerCallback(void); 
void reboot_TIM_Callback(void);
extern bool DeepSleepTimerFlag;
extern unsigned char PSUState;
 
void GPTM0_IRQHandler(void)
 {
	 unsigned char buf;
   //更新事件标志位使能
	 if(TM_GetFlagStatus(HT_GPTM0,TM_INT_UEV))
	  {
		TM_ClearFlag(HT_GPTM0,TM_INT_UEV);
		if(!EnteredMainApp)LEDMgmt_CallBack();//LED管理器(POST阶段)
		SensorRefreshFlag=true;//请求传感器刷新
		RXTimerFilpFlag=true;//请求Xmodem刷新
		DeepSleepTimerFlag=true;//处理深度睡眠定时器
		//登录锁定延时和超时计时器
		reboot_TIM_Callback();//重启计时器
    if(LogErrTimer>0)LogErrTimer--;			
	  IdleTimerCallback();//超时计时器
	  SideKey_TIM_Callback();//侧按按键计时器
	  //主电源延时关闭计时器
		if(PSUState&0x80)	
		  {
			buf=PSUState&0x7F;
			if(buf<0x0A)buf++;
		  PSUState&=0x80; 
		  PSUState|=buf;  //计时器被使能，开始累加
			}
		}
 }
 
/*********************************************************************************************************//**
 * @brief   This function handles GPTM1 interrupt.
 * @retval  None
 ************************************************************************************************************/ 
void FlashTimerCallbackRoutine(void); 
 
void GPTM1_IRQHandler(void)
 {
 if(TM_GetFlagStatus(HT_GPTM1,TM_INT_UEV))
	  {
		TM_ClearFlag(HT_GPTM1,TM_INT_UEV);
		FlashTimerCallbackRoutine();
		}
 }	
