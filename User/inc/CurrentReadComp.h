#ifndef _Ccomp_
#define _Ccomp_

//内部包含
#include "selftestlogger.h" //为了获取FRU的地址偏移

//结构体定义
typedef struct
 {
 float CurrentCompThershold[51];
 float CurrentCompValue[51];
 float DimmingCompThreshold[50];
 float DimmingCompValue[50];
 }CompDataStrDef; //补偿阈值和数据的结构体 

typedef union
 {
 CompDataStrDef Data; //补偿数据
 char ByteBuf[sizeof(CompDataStrDef)];
 }CompDataUnion;	//用于按字节计算数值的补偿结构体
 
typedef struct
 {
 CompDataUnion CompData; //补偿数据
 unsigned int Checksum; //CRC32校验值
 }CompDataStorStrDef;	

typedef union
 {
 CompDataStorStrDef CompDataEntry;
 char ByteBuf[sizeof(CompDataStorStrDef)]; 
 }CompDataStorUnion;	

//地址定义和外部声明
#define CalibrationRecordEnd sizeof(CompDataStorUnion)+SelftestLogEnd //自检记录结束
extern CompDataStorUnion CompData; //补偿数据
 
//函数
void LoadCalibrationConfig(void);//加载配置
char WriteCompDataToROM(void); //保存配置
void DoSelfCalibration(void); //自动校准

#endif
