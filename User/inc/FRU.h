#ifndef _FRU_
#define _FRU_

#include <stdbool.h>

//结构体和联合体声明
typedef struct
 {
 char SerialNumber[32];
 char FRUVersion[3];
 float MaxLEDCurrent;
 }FRUDataSection;

typedef union
 {
 FRUDataSection Data;
 char FRUDbuf[sizeof(FRUDataSection)];
 }FRUDataUnion;	
 
typedef struct
 {
 FRUDataUnion Data;
 unsigned int CRC32Val;
 }FRUBlockStor;	

typedef union
 {
 FRUBlockStor FRUBlock;
 char FRUBUF[sizeof(FRUBlockStor)];
 }FRUBlockUnion;

//函数
bool CheckFRUInfoCRC(FRUBlockUnion *FRU);//检查FRU CRC
bool CalcFRUCRC(FRUBlockUnion *FRU);//计算CRC
 
#endif
