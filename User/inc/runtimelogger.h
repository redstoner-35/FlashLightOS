#ifndef _RunTimeLogger_
#define _RunTimeLogger_

#include "logger.h"

//结构体
typedef struct
 {
 signed char LogIncrementCode;//日志的递增码
 unsigned int TotalLogCount;//总日志条数
 bool IsRunlogHasContent; //运行日志是否有内容
 BatteryCapacityGaugeStrDef BattUsage;//库仑计设置
 char LogKey[4];  //用于检查log输入的key
 double LEDRunTime;//LED运行时间
 float ThermalStepDownValue;//温度降档数值
 float AverageLEDVf; 
 float MaximumLEDVf; //平均和最大LEDVf
 float AverageLEDIf; 
 float MaximumLEDIf; //平均和最大LEDIf
 float AverageLEDTemp;
 float MaximumLEDTemp; //平均和最大LED温度
 float AverageSPSTemp;
 float MaximumSPSTemp; //平均和最大MOS温度
 float MinimumBatteryVoltage; 
 float MaximumBatteryVoltage; 
 float AverageBatteryVoltage; //最小、平均和最大的电池电压
 float AverageBatteryCurrent; 
 float MaximumBatteryCurrent; //平均和最大电池电流
 float AverageBatteryPower;  
 float MaximumBatteryPower;  //平均和最大电池输出功率
 float AverageDriverEfficiency;  //平均驱动运行效率
 float MaximumEfficiency;//峰值效率
 double TotalBatteryCapDischarged;  //总共电池放电的mAH数
 float RampModeConf; //无极调光模式的目前挡位
 bool RampModeDirection; //无极调光模式的方向
 bool IsFlashLightLocked; //手电筒是否锁定
 bool IsLowVoltageAlert; //是否低电压报警
 unsigned short TotalMomtTurboCount; //总计强制极亮次数
 unsigned short LowVoltageShutDownCount;//低电压关机次数统计
 unsigned short DriverThermalFaultCount;//驱动因为严重过热关机的次数统计
 unsigned short LEDThermalFaultCount;//驱动因为严重过热关机的次数统计
 unsigned short OCPFaultCount;//电池和LED过流保护的次数统计
 unsigned short OtherFaultCount;//其他硬件错误的次数统计
 unsigned short LEDOpenShortCount;//LED开短路保护
 float MaximumThermalStepDown;//历史记录的最大温度降档数值
 }RunLogDataStrDef;

typedef union
 {
 RunLogDataStrDef DataSec;
 char DataCbuf[sizeof(RunLogDataStrDef)];
 }RunLogDataUnionDef;	//使得数据域部分可以按字节操作的Union
 
typedef struct
 {
 RunLogDataUnionDef Data;
 unsigned int CurrentDataCRC;
 unsigned int LastDataCRC; //CRC结果
 char ProgrammedEntry; //目标编程的entry
 }RunLogEntryStrDef;	//运行日志结构体的定义
 
//定义
#define RunTimeLoggerDepth 64  //运行日志的深度
#define RunTimeLogBase LoggerAreaSize+LoggerBase //运行日志的位置
#define RunTimeLogSize RunTimeLoggerDepth*sizeof(RunLogDataStrDef)  //运行日志的大小
#define RunTimeLogKey "RLoG" //运行log的内容检查Key

//函数
void RunTimeDataLogging(void);//记录模块
void CalcLastLogCRCBeforePO(void);//在自检前记录CRC结果的函数
unsigned int CalcRunLogCRC32(RunLogDataUnionDef *DIN);//计算log区域的CRC32
void LogDataSectionInit(RunLogDataUnionDef *DIN);//使用空的结构体填充整个数据部分
int FindLatestEntryViaIncCode(signed char *CodeIN);//从增量码中找到最新的记录 
bool LoadRunLogDataFromROM(RunLogDataUnionDef *DataOut,int LogEntryNum);
bool SaveRunLogDataToROM(RunLogDataUnionDef *DataIn,int LogEntryNum);//在ROM内读写指定的数据域
void RunLogModule_POR(void);//上电初始化
void WriteRuntimeLogToROM(void);//将运行日志写入到ROM中
void ForceWriteRuntimelog(void);//强制将运行日志写入到ROM内
bool ResetRunTimeLogArea(void);//复位运行日志区域
bool IsRunLogAreaOK(int RunLogAreaOffset);//检查运行日志区域是否损坏

//外部参考
extern bool IsRunTimeLoggingEnabled;
extern RunLogEntryStrDef RunLogEntry;

#endif
