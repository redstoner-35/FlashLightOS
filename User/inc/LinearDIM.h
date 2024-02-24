#ifndef _LinearDIM_
#define _LinearDIM_

#include "Pindefs.h" //引脚定义
#include <stdbool.h>
#include "ht32.h"

//PWM引脚自动定义
#define ToggleFlash_IOG STRCAT2(HT_GPIO,ToggleFlash_IOBank)
#define ToggleFlash_IOB STRCAT2(GPIO_P,ToggleFlash_IOBank)
#define ToggleFlash_IOP STRCAT2(GPIO_PIN_,ToggleFlash_IOPinNum)

/*  主副buck切换引脚的自动define,不允许修改！  */
#define BUCKSEL_IOB STRCAT2(GPIO_P,BUCKSEL_IOBank)
#define BUCKSEL_IOG STRCAT2(HT_GPIO,BUCKSEL_IOBank)
#define BUCKSEL_IOP STRCAT2(GPIO_PIN_,BUCKSEL_IOPinNum)

/*  3.3V辅助DCDC引脚的自动define,不允许修改！  */
#define AUXV33_IOB STRCAT2(GPIO_P,AUXV33_IOBank)
#define AUXV33_IOG STRCAT2(HT_GPIO,AUXV33_IOBank)
#define AUXV33_IOP STRCAT2(GPIO_PIN_,AUXV33_IOPinNum)

//函数
FlagStatus IsHostConnectedViaUSB(void);//检测USB是否连接
void RuntimeModeCurrentHandler(void);//正常运行时处理挡位控制
void SetTogglePin(bool IsPowerEnabled);//设置爆闪控制引脚
void LinearDIM_POR(void); //初始化调光器
void DoLinearDimControl(float Current,bool IsMainLEDEnabled);//线性调光控制
void GetAuxBuckCurrent(void);//获取辅助buck的电流
void NotifyUserForGearChgHandler(void);//用于处理当挡位电流切换小于20%的时候提示用户的功能

#endif
