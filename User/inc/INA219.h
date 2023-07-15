#ifndef _INA219_
#define _INA219_

//宏定义
#define BusVoltLSB 4 //总线电压的1LSB数值，单位mV
#define MaxmiumCurrent 50 //预期的最大电流（50A）
#define CurrentLSB (double)MaxmiumCurrent/PMBUS_2NPowCalc(15) //LSB=总线最大电流除以2的15次方
#define PowerLSB (float)20*CurrentLSB //功率读数的LSB，为20倍的电流LSB

#include <stdbool.h>

//模式枚举
typedef enum
{
INA219_PowerDown,
INA219_Trig_BCurrent,
INA219_Trig_BVoltage,
INA219_Trig_Both,	
INA219_ADC_Off,
INA219_Cont_BCurrent,
INA219_Cont_BVoltage,
INA219_Cont_Both	
}INA219ModeDef;
//获取参数用的结构体
typedef struct
{
char TargetSensorADDR;//目标的传感器地址
float BusVolt;
float BusCurrent;
float BusPower;
}INADoutSreDef;
//初始化219用的结构体
typedef struct
{
char TargetSensorADDR;//目标的传感器地址
char VoltageFullScale;//目标的总线电压满量程范围
int ShuntVoltageFullScale;//分流器的电压满量程范围，单位mV
int ADCAvrageCount;//电流和电压采集的平均次数
float ShuntValue;//检流电阻的阻值，单位为毫欧（mR）
INA219ModeDef ConvMode;//转换模式
}INAinitStrdef;

//可用函数
char INA219_INIT(INAinitStrdef * INAConf);//初始化
bool INA219_GetBusInformation(INADoutSreDef *INADout);//读取数值
bool INA219_SetConvMode(INA219ModeDef ConvMode,char SensorADDR);//设置转换模式
void INA219_POR(void);//上电初始化

#endif
