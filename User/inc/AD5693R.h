#ifndef _AD5693R_
#define _AD5693R_

#include <stdbool.h>

//类型定义
typedef enum
 {
 DAC_Normal_Mode,   //DAC正常运行并转换输出
 DAC_Disable_1KRo,
 DAC_Disable_100KRo,  
 DAC_Disable_HiZ    //DAC关闭输出且分别处于1K低阻，100K和高阻状态
 }DACPStateDef;

typedef enum
 {
 DAC_Output_REF,
 DAC_Output_2xREF
 }DACOutRangeDef; //DAC输出范围选择	0-Vref或者0-2xVref
 
typedef struct
 {
 DACOutRangeDef DACRange;
 DACPStateDef DACPState; 
 bool IsOnchipRefEnabled;
 }DACInitStrDef;	

//地址和DAC电压的定义
#define DACVref 2.5  //DAC的基准电压，对于AD5693R而言使用的是内置基准，为2.5V
#define DACBit 16 //DAC位数，对于AD5693R而言是16bit
 
//函数
bool AD5693R_Detect(void);//检测DAC是否就绪,true表示就绪
bool AD5693R_SetChipConfig(DACInitStrDef *DACInitStruct);//根据传入的结构体配置DAC的参数	
bool AD5693R_SetOutput(float VOUT);//设置DAC的输出电压 

//上电自校准
void LinearDIM_POR(void);
 
#endif
