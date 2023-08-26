#ifndef _SideKey_
#define _SideKey_

#include "Pindefs.h" //引脚定义
#include <stdbool.h>

//按键检测延时(每个单位=0.125秒)
#define LongPressTime 4 //长按按键检测延时(按下时间超过这个数值则判定为长按)
#define ContShortPressWindow 2 //连续多次按下时侧按的检测释抑时间(在该时间以内按下的短按才算入短按次数内)

/*负责处理外部中断的自动Define，不允许修改！*/
#define ExtKey_IOB STRCAT2(GPIO_P,ExtKey_IOBank)
#define ExtKey_IOG STRCAT2(HT_GPIO,ExtKey_IOBank)
#define ExtKey_IOP STRCAT2(GPIO_PIN_,ExtKey_IOPN) 
#define ExtKey_EXTI_CHANNEL  STRCAT2(EXTI_CHANNEL_,ExtKey_IOPN)
#define _ExtKey_EXTI_IRQn STRCAT2(EXTI,ExtKey_IOPN)
#define ExtKey_EXTI_IRQn  STRCAT2(_ExtKey_EXTI_IRQn,_IRQn)

//按键事件结构体定义
typedef struct
{
bool LongPressDetected;
bool LongPressEvent;
short ShortPressCount;
bool ShortPressEvent;
bool PressAndHoldEvent;
bool DoubleClickAndHoldEvent;
bool TripleClickAndHold;
}KeyEventStrDef;

//函数
void SideKeyInit(void);
int getSideKeyShortPressCount(bool IsRemoveResult);//获取侧按按键的单击和连击次数
bool getSideKeyLongPressEvent(void);//获得侧按按钮长按的事件
bool getSideKeyHoldEvent(void);//获得侧按按钮一直按住的事件
bool getSideKeyClickAndHoldEvent(void);//获得侧按按钮短按一下立即长按的事件
bool getSideKeyDoubleClickAndHoldEvent(void);//获取侧按按键是否有双击并长按的事件
bool getSideKeyTripleClickAndHoldEvent(void);//获取侧按按键是否有三击并长按的事件

//回调处理
void SideKey_Callback(void);//中断回调处理
void SideKey_TIM_Callback(void);//连按检测计时的回调处理
void SideKey_LogicHandler(void);//逻辑处理

#endif
