#ifndef LEDMgmt_
#define LEDMgmt_

#include "Pindefs.h"

//函数
void LED_Init(void);
void LEDMgmt_CallBack(void);
void LED_Reset(void);

//外部变量
extern int CurrentLEDIndex;//给外部函数设置LED状态
extern char *ExtLEDIndex; //用于传入的外部序列

#endif
