#ifndef _ADC_
#define _ADC_

#include <stdbool.h>
#include "Pindefs.h"
#include "FirmwareConf.h"

/*下面的自动Define负责处理ADC的IO，ADC通道定义，不允许修改！*/
#define LED_Vf_IOP STRCAT2(AFIO_PIN_,LED_Vf_ADC_Ch)
#define NTC_IOP STRCAT2(AFIO_PIN_,NTC_ADC_Ch) 
#define LED_If_IOP STRCAT2(AFIO_PIN_,LED_If_Ch) 
#define SPS_Temp_IOP STRCAT2(AFIO_PIN_,SPS_Temp_Ch) 
#define _LED_Vf_ADC_Ch STRCAT2(ADC_CH_,LED_Vf_ADC_Ch)
#define _NTC_ADC_Ch STRCAT2(ADC_CH_,NTC_ADC_Ch)
#define _LED_If_Ch STRCAT2(ADC_CH_,LED_If_Ch)
#define _SPS_Temp_Ch STRCAT2(ADC_CH_,SPS_Temp_Ch)

//ADC的平均次数
#define ADCAvg 5  

//温度传感器状态Enum
typedef enum
{
SPS_TMON_OK,
SPS_TMON_Disconnect,
SPS_TMON_CriticalFault,
}SPSTMONStatDef;
typedef enum
{
LED_NTC_OK, //NTC就绪
LED_NTC_Short,//NTC短路
LED_NTC_Open //NTC开路
}NTCSensorStatDef;
//ADC结果输出
typedef struct
{
float LEDVf;
float LEDIf;
float LEDIfNonComp;//未补偿的LED If
float LEDTemp;//LED状态
float SPSTemp;//SPS温度
NTCSensorStatDef NTCState;//LED基板温度状态
SPSTMONStatDef SPSTMONState;//SPS温度状态
}ADCOutTypeDef;//ADC

//函数
float QueueLinearTable(int TableSize,float Input,float *Thr,float *Value);//线性表插值
void InternalADC_Init(void);//初始化内部ADC
bool ADC_GetLEDIfPinVoltage(float *VOUT);//ADC获取LED电流测量引脚的电压输入值
bool ADC_GetResult(ADCOutTypeDef *ADCOut);//获取温度和LED Vf
void OnChipADC_FaultHandler(void);//ADC异常处理
void ADC_EOC_interrupt_Callback(void);//ADC结束转换处理
void InternalADC_QuickInit(void);//内部ADC快速初始化

//外部ref
extern const char *NTCStateString[3];
extern const char *SPSTMONString[3];

#endif
