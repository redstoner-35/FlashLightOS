#ifndef _MCP3421_
#define _MCP3421_

#include <stdbool.h>

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
 
//函数
bool MCP3421_SetChip(PGAGainDef Gain,SampleRateDef Rate,bool IsPowerDown); //初始化
bool MCP3421_ReadVoltage(float *VOUT); //MCP3421读取电压
 
#endif
