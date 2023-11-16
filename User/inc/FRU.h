#ifndef _FRU_
#define _FRU_

#include <stdbool.h>
#include "FirmwareConf.h"

//处理UV模式的自动define
#ifdef Firmware_UV_Mode
 #define FRUVer 0x03 //使用通用的3V LED标准
#endif

//关于LED选择的自动define
#ifdef Using_SBT90Gen2_LED
  #ifdef Firmware_UV_Mode
	#error "UV Mode is Not compatible with Normal LED Defines! Please Comment 'Using_SBT90Gen2_LED' define in FirmwareConf.h!"
	#endif
#define FRUVer 0x08
#endif

#ifdef Using_SBT90R_LED
#define FRUVer 0x04
  #ifdef Firmware_UV_Mode
	#error "UV Mode is Not compatible with Normal LED Defines! Please Comment 'Using_SBT90R_LED' define in FirmwareConf.h!"
	#endif
#endif

#ifdef Using_SBT70G_LED
  #ifdef Firmware_UV_Mode
	#error "UV Mode is Not compatible with Normal LED Defines! Please Comment 'Using_SBT70G_LED' define in FirmwareConf.h!"
	#endif
#define FRUVer 0x05
#endif

#ifdef Using_SBT70B_LED
  #ifdef Firmware_UV_Mode
	#error "UV Mode is Not compatible with Normal LED Defines! Please Comment 'Using_SBT70B_LED' define in FirmwareConf.h!"
	#endif
#define FRUVer 0x07
#endif

#ifdef Using_Generic_3V_LED
  #ifdef Firmware_UV_Mode
	#error "UV Mode is Not compatible with Normal LED Defines! Please Comment 'Using_Generic_3V_LED' define in FirmwareConf.h!"
	#endif
#define FRUVer 0x03
#endif

#ifdef Using_Generic_6V_LED
  #ifdef Firmware_UV_Mode
	#error "UV Mode is Not compatible with Normal LED Defines! Please Comment 'Using_Generic_6V_LED' define in FirmwareConf.h!"
	#endif
#define FRUVer 0x06
#endif

//判断用户是否指定了FRU信息的自动define
#ifndef FRUVer
  #error "You need to define what type of LED you want to use!"
#endif

#ifndef HardwareMajorVer
  #error "You need to define the major version of the hardware!"
#endif

#ifndef HardwareMinorVer
  #error "You need to define the minor version of the hardware!"
#endif

//结构体和联合体声明
typedef struct
 {
 char SerialNumber[32];
 char GeneralLEDString[32];
 short NTCBValue;
 unsigned short CustomLEDIDCode;
 float MaxLEDCurrent;
 char FRUVersion[3];
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
char WriteFRU(FRUBlockUnion *FRU);
char ReadFRU(FRUBlockUnion *FRU);//读写FRU 
const char *DisplayLEDType(FRUBlockUnion *FRU);//识别LED类型并返回常量字符串
float QueryMaximumCurrentLimit(FRUBlockUnion *FRU);//识别LED类型并返回最大电流2
 
#endif
