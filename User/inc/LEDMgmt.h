#ifndef LEDMgmt_
#define LEDMgmt_

#include "Pindefs.h"

//处理LED GPIO的自动define
#define LED_Green_IOP STRCAT2(GPIO_PIN_,LED_Green_IOPinNum)
#define LED_Green_IOB STRCAT2(GPIO_P,LED_Green_IOBank)
#define LED_Green_IOG STRCAT2(HT_GPIO,LED_Green_IOBank)

#define LED_Red_IOB STRCAT2(GPIO_PIN_,LED_Red_IOPinNum)
#define LED_Red_IOG STRCAT2(HT_GPIO,LED_Red_IOBank)
#define LED_Red_IOP STRCAT2(GPIO_P,LED_Red_IOBank)

//函数
void LED_Init(void);
void LEDMgmt_CallBack(void);
void LED_Reset(void);

//外部变量
extern int CurrentLEDIndex;//给外部函数设置LED状态
extern char *ExtLEDIndex; //用于传入的外部序列

#endif
