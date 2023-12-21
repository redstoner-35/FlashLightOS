#ifndef _selftestLOG
#define _selftestLOG

//内部包含
#include "runtimelogger.h" //实际上没有使用到里面的函数，主要是为了获取地址偏移
#include "console.h"

//地址和log区域定义
#define SelfTestLogBase RunTimeLogBase+(2*RunTimeLogSize)  //自检日志写入的基地址
#define SelftestLogDepth 64 //64条收录深度
#define SelftestLogControlByte (sizeof(SelfTestLogEntryStr)*SelftestLogDepth)+SelfTestLogBase //自检日志控制位结束点
#define SelftestLogEnd 1+(sizeof(SelfTestLogEntryStr)*SelftestLogDepth)+SelfTestLogBase //自检日志存储空间结束点
//结构体定义
typedef struct
{
Postmessagelevel Level;
unsigned char ModuleNameChecksum; 
unsigned char MessageCheckSum;  //发出消息的模块名和消息内容的checksum
char LogContentCheckByte; //日志内容检查byte 读到0x35表示日志有内容
char ModuleName[16];
char Message[236]; //模块名和消息	
}SelfTestLogEntryStr;

//允许按字节存储的东西
typedef union
{
SelfTestLogEntryStr Log;
char ByteBuf[sizeof(SelfTestLogEntryStr)];
}SelfTestLogUnion;

//函数
bool DisplayLastTraceBackMessage(void);//显示最后的一条trace信息
void CheckLastStartUpIsOK(void); //检查上次启动是否完成
void ResetLogEnableAfterPOST(void); //当自检通过后重置记录器的使能位
#endif
