#ifndef _Logger_
#define _Logger_

//内部包含
#include "cfgfile.h"
#include "modelogic.h"
#include "ADC.h"
#include "INA219.h"

//logger定义
#define MaximumLoggerDepth 20 //logger深度20级
#define LoggerHeaderKey "DD35" //记录器头部内容的校验key
#define LoggerAutoHeaderSaveTime 10 //记录器头部的内容检测到变更，且没有新的log entry写入后，后会在多少秒内完成写入

//log区域的头部
typedef struct
 {
 char CurrentLoggerIndex;
 bool IsEntryHasError[MaximumLoggerDepth];//对应的log entry是否出错
 unsigned int EntryCRC[MaximumLoggerDepth];//入口的CRC值
 char CheckKey[5];//检查logger区域的特征值
 }LoggerHeaderStrdef;

typedef union
 {
 LoggerHeaderStrdef LoggerHeader;
 char LoggerHeaderbuf[sizeof(LoggerHeaderStrdef)];
 }LoggerHeaderUnion;

//logger区域的数据域
typedef struct
 {
 SystemErrorCodeDef ErrorCode;
 DrivertelemStr DriverTelem;
 PStateDef SystemPstate;
 CurrentModeStr CurrentModeSel;
 bool IsBattTelemWorking;//电池的调光器是否运行
 float CurrentDACVID;//当前DAC的VID
 float CurrentThrottleLevel;//当前降档的级别
 NTCSensorStatDef LEDBaseNTCStatu;//LED基板NTC状态
 SPSTMONStatDef SPSTMONStatu;//SPSTMON状态
 char ErrorStageText[32];//记录出错的时候驱动是哪个阶段
 }LoggerDateStrDef;	
 
typedef union
 {
 LoggerDateStrDef LoggerDateSection;
 char LoggerDatabuf[sizeof(LoggerDateStrDef)];
 }LoggerDataUnion; 
 
//logger区域的地址定义
#define LoggerBase CfgFileSize*2 //logger对应RAM空间
#define LoggerDatabase LoggerBase+sizeof(LoggerHdr)  //logger对应的数据库的RAM空间
#define LoggerAreaSize sizeof(LoggerHdr)+(MaximumLoggerDepth*sizeof(LoggerDataUnion))  //错误日志记录器所占空间

//外部引用
extern LoggerHeaderUnion LoggerHdr;

//函数
bool CheckForErrLogStatu(int ROMAddr);//恢复日志的时候检查指定的log区域里面的内容是否合法
bool fetchloggerheader(LoggerHeaderUnion *LogHdrOut);
bool WriteLoggerHeader(LoggerHeaderUnion *LogHdrIn); //读写logger头部的函数
bool FetchLoggerData(LoggerDataUnion *LogDataOut,int LogEntryNum);
bool WriteLoggerData(LoggerDataUnion *LogDataIn,int LogEntryNum);//读写logger区域的函数
unsigned int calculateLogEntryCRC32(LoggerDataUnion *LogDataIN);//计算log数据域的CRC32
void PushDefaultInfoInsideLog(LoggerDataUnion *LogDataIN);//给log entry替换入默认的数据
bool ReInitLogArea(void);//清空并重新初始化log区域
void LoggerHeader_POR(void);//上电时初始化log区域
void CollectLoginfo(const char *ErrorArea,INADoutSreDef *BattStat);//错误发生时保存log信息
void LoggerHeader_AutoUpdateHandler(void);//写日志头部
void ResetLoggerHeader_AutoUpdateTIM(void);//复位定时器

#endif
