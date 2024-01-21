#ifndef _Ccomp_
#define _Ccomp_

//内部包含
#include "selftestlogger.h" //为了获取FRU的地址偏移

//枚举定义
typedef enum
 {
 Database_No_Error,
 Database_EEPROM_Read_Error,
 Database_Integrity_Error,
 Database_Value_Error
 }CalibrationDBErrorDef;

//结构体定义
typedef struct
 {
 //辅助Buck调光参数
 float AuxBuckDimmingThreshold[50];
 float AuxBuckDimmingValue[50]; 
 //主Buck调光参数
 float MainBuckDimmingThreshold[50];  
 float MainBuckDimmingValue[50]; 
 //辅助Buck电流反馈参数
 float AuxBuckIFBThreshold[50];
 float AuxBuckIFBValue[50];
 //主Buck电流反馈参数
 float MainBuckIFBThreshold[50];
 float MainBuckIFBValue[50]; 
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
CalibrationDBErrorDef CheckCompData(void);//计算校验和

#endif
