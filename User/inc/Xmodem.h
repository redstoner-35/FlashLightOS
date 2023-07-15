#ifndef _Xmodem_
#define _Xmodem_

//内部包含
#include "runtimelogger.h" //实际上没有使用到里面的函数，主要是为了获取地址偏移

//地址偏移和参数配置
#define ForceUseCRC16 false //是否强制使用CRC-16进行传输(强制使用CRC-16则一些老终端不兼容)
#define MaximumRxWait 20 //最大的RX等待时间(秒)
#define MaximumPackRetry 10 //最大的重传尝试次数
#define XmodemConfRecvBase RunTimeLogBase+RunTimeLogSize  //Xmodem EEPROM接收缓冲区的基地址
#define XmodemTxCtrlByte XmodemTransferCtrl.XmodemRXBuf[133] //当Xmodem模块作为发送模式时收到的控制字节所在的区域

//Xmodem协议需要的一些控制字节
#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NACK 0x15
#define CAN 0x18
#define CTRLZ 0x1A

//发送状态机使用的枚举
typedef enum
 {
 Xmodem_TxWaitFirstPack, //Xmodem开始发送，等待主机发出'C'启动接收的阶段
 Xmodem_PreparePacket, //开始准备要发出去的数据包，准备完毕后发出
 Xmodem_WaitForHostRespond, //发送完毕，等待主机的ACK
 //错误和传输完毕状态
 Xmodem_Error_TxTimeOut,//发送超时
 Xmodem_Error_TxManualStop,   //用户在发送时手动输入Ctrl+C停止
 Xmodem_Error_TxRetryTooMuch, //包裹出错的错误次数超过允许值
 Xmodem_Error_PreparePacket, //数据处理过程(准备数据)出错
 Xmodem_TxTransferDone //传输完毕 
 }XmodemTxStateDef;

//接收状态机使用的枚举
typedef enum
 {
 Xmodem_RxStartWait,//Xmodem开始接收第一帧前面发送C的阶段
 Xmodem_PacketRxDone,//第一个包裹完毕
 Xmodem_PacketNACK, //包裹检查出错，NACK要求重发并等待
 Xmodem_SlaveProcessData,  //收到了合法数据，此时交给从机处理
 Xmodem_SlaveWaitForNextFrame, //从机处理完毕数据，发出ACK并等待下一帧
 //错误和传输完毕状态
 Xmodem_Error_RxTimeOut,//接收超时
 Xmodem_Error_PacketOrder,//包裹顺序错误
 Xmodem_Error_TransferSize, //传输大小出错
 Xmodem_Error_PacketData, //包裹出错的错误次数超过允许值
 Xmodem_Error_DataProcess, //数据处理过程(EEPROM编程)出错
 Xmodem_Error_ManualStop,  //用户输入Ctrl+C强制结束传输
 Xmodem_TransferDone //传输完毕  
 }XmodemRxStateDef;

//XModem传输的状态
typedef enum
 {
 Xmodem_Mode_NoTransfer,
 Xmodem_Mode_Rx,
 Xmodem_Mode_Tx
 }XmodemTransferCtrlDef;
//Xmodem传输操作的结构体
typedef struct
 {
 int MaxTransferSize;  //最大传输大小
 int ReadWriteBase;    //在EEPROM内读写的base
 char XmodemRXBuf[134]; //接收缓冲区
 char CurrentPacketNum; //当前的包数
 short CurrentBUFRXPtr; //当前RX缓冲的指针
 XmodemTransferCtrlDef XmodemTransferMode; //Xmodem传输模式
 short RXTimer; //收取操作中需要延时的计时器
 char RxRetryCount;//收取/发送重试操作的次数
 bool IsUseCRC16;//使用CRC-16方式接收
 XmodemRxStateDef	XmodemRxState;//RX状态机控制变量
 XmodemTxStateDef XmodemTxState;//Xmodem发送的状态机
 }XmodemTransferCtrlStrDef; 

//内部使用的函数
int PEC16CheckXModem(char *DIN,long Len);//进行CRC16检查
int Checksum8(char *DIN,long Len);//进行Checksum16的检查
void XmodemDMARXHandler(void); //DMA RX处理
void XmodemTxStateMachine(void);
void XmodemRXStateMachine(void);//发送状态机和接收状态机
 
//引用到外部的函数
void XmodemInitTxModule(int MaximumTxSize,int ROMAddr);//启动Xmodem发送模块
void XmodemInitRXModule(int MaximumRxSize,int ROMAddr);//启动Xmodem接收模块
void XmodemTransferReset(void); //重置传输结构体
 
//引用到外部的显示函数 
void DisplayXmodemBackUp(const char *Operation,bool IsRestore);//在开始操作前显示提示信息
void XmodemTxDisplayHandler(const char *FinishContentString,void (*ThingsToDoInError)(void),void (*ThingsToDoWhenDone)(void));//发送期间根据状态显示字符串
void XmodemRxDisplayHandler(const char *FinishContentString,void (*ThingsToDoInError)(void),void (*ThingsToDoWhenDone)(void));//接收期间根据状态显示字符串
 
//外部结构体(可以给其余程序获取Xmodem模块的状态) 
extern XmodemTransferCtrlStrDef XmodemTransferCtrl;
extern const char *XmodemStatuString[];
 
#endif
