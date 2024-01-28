#ifndef _MCP3421_
#define _MCP3421_

#include <stdbool.h>
#include "FirmwareConf.h"

//枚举
typedef enum
 {
 PGA_Gain1to1,
 PGA_Gain2to1,
 PGA_Gain4to1,
 PGA_Gain8to1
 }PGAGainDef;

typedef enum
 {
 Sample_12bit_240SPS,
 Sample_14bit_60SPS,
 Sample_16bit_15SPS
 }SampleRateDef;	
 
/*程序里面对于ADC等待延时,采样率的自动定义*/
#ifdef AuxBuckADC_14BitMode
  #define AuxBuckIsenADCRes Sample_14bit_60SPS
  #define MCP_SampleFreq 60  //14bit 60Hz
#endif
 
#ifdef AuxBuckADC_16BitMode
  #define AuxBuckIsenADCRes Sample_16bit_15SPS
  #define MCP_SampleFreq 15  //16bit 15Hz
#endif 
 
#ifdef AuxBuckADC_12BitMode
  #define AuxBuckIsenADCRes Sample_12bit_240SPS
  #define MCP_SampleFreq 240  //12bit 240Hz
#endif 
 
#ifdef MCP_SampleFreq
  #define MCP_waitTime (1000/MCP_SampleFreq)+1 //自动定义的等待时间(单位ms)
#else
  #error "You Should Define what ADC resolution that you want to use!"
#endif
//函数
bool MCP3421_SetChip(PGAGainDef Gain,SampleRateDef Rate,bool IsPowerDown); //初始化
bool MCP3421_ReadVoltage(float *VOUT); //MCP3421读取电压
 
#endif
