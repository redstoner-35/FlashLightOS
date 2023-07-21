#include "I2C.h"
#include "FirmwareConf.h"
#include "FRU.h"
#include "selftestlogger.h"
#include "console.h"
#include "Xmodem.h"
#include "LEDMgmt.h"
#include <string.h>

//全局变量和常量
float FusedMaxCurrent=5;//安全电流设置
extern const char *LEDInfoStr[];//外部的LED类型常量

#ifdef UsingLED_3V
   #ifdef Using_SBT90R
	 const char FRUVersion[3]={0x04,0x01,0x02}; //分别表示适用SBT90-R LED，硬件版本1.2
   #else
   const char FRUVersion[3]={0x03,0x01,0x00}; //分别表示适用LED电压3V，硬件版本1.0
	 #endif
#else
const char FRUVersion[3]={0x06,0x01,0x01}; //分别表示适用LED电压6V，硬件版本1.1
#endif 
/**************************************************************
这个函数负责接收传入的FRU结构体指针，解析出LED类型后返回最大电流
限制的合法值
**************************************************************/
float QueryMaximumCurrentLimit(FRUBlockUnion *FRU)
	{
  float result=0;
  switch(FRU->FRUBlock.Data.Data.FRUVersion[0]) //显示LED型号
		{
		case 0x03:result=50;break;
		case 0x04:result=18;break;
		case 0x05:result=14;break;
		case 0x06:result=30;break;
		case 0x07:result=14;break;
		case 0x08:result=32;break;
		}
  if(result==0||result>MaxAllowedLEDCurrent)return MaxAllowedLEDCurrent;//数值不合法
	return result;
	} 
	 
/**************************************************************
这个函数负责接收传入的FRU结构体并解析出LED类型然后返回LED类型的
常量指针字符串。
**************************************************************/	
const char *DisplayLEDType(FRUBlockUnion *FRU)
  {
  switch(FRU->FRUBlock.Data.Data.FRUVersion[0]) //显示LED型号
		{
		case 0x03:return "Generic 3V";
		case 0x04:return LEDInfoStr[1];
		case 0x05:return LEDInfoStr[2];
		case 0x06:return "Generic 6V";
		case 0x07:return LEDInfoStr[4];
		case 0x08:return LEDInfoStr[5];
		}
	return "Unknown";
	}
/**************************************************************
这个函数主要用于从ROM内读取FRU的数据，如果成功，会返回0，否则返
回1
**************************************************************/
char ReadFRU(FRUBlockUnion *FRU)
 {
 
 #ifndef EnableSecureStor
 if(M24C512_PageRead(FRU.FRUBUF,SelftestLogEnd,sizeof(FRUBlockUnion)))return 1;
 #else
 if(M24C512_ReadSecuSct(FRU->FRUBUF,0,sizeof(FRUBlockUnion)))return 1; 
 #endif	 
 //读取完毕，返回0
 return 0;
 }
/**************************************************************
这个函数主要用于向ROM内写入FRU的数据，如果成功，会返回0，否则返
回1
**************************************************************/
char WriteFRU(FRUBlockUnion *FRU)
 {
 #ifndef EnableSecureStor
 CalcFRUCRC(FRUBlockUnion *FRU);
 if(M24C512_PageWrite(FRU.FRUBUF,SelftestLogEnd,sizeof(FRUBlockUnion)))return 1; //计算校验和然后写入
 #else
 if(!CalcFRUCRC(FRU))return 1;//CRC计算失败
 if(M24C512_WriteSecuSct(FRU->FRUBUF,0,sizeof(FRUBlockUnion)))return 1; //写入失败
 #endif
 //写入完毕，返回0
 return 0;
 }
/**************************************************************
这个函数主要用于检查读出来的FRU数据是否匹配CRC32校验和。如果匹配
则返回true 否则返回false
**************************************************************/
bool CheckFRUInfoCRC(FRUBlockUnion *FRU)
 {
 unsigned int DATACRCResult;
 unsigned int UID;
 int i;
 char UIDbuf[4];
 CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
 //初始化CRC32      
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,ENABLE);//启用CRC-32时钟  
 CRC_DeInit(HT_CRC);//清除配置
 HT_CRC->SDR = 0x0;//CRC-32 poly: 0x04C11DB7  
 HT_CRC->CR = CRC_32_POLY | CRC_BIT_RVS_WR | CRC_BIT_RVS_SUM | CRC_BYTE_RVS_SUM | CRC_CMPL_SUM;
 //开始校验
 for(i=0;i<sizeof(CfgFile);i++)
	 wb(&HT_CRC->DR,FRU->FRUBlock.Data.FRUDbuf[i]);//将内容写入到CRC寄存器内
 //校验完毕计算结果
 DATACRCResult=HT_CRC->CSR;
 CRC_DeInit(HT_CRC);//清除CRC结果
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,DISABLE);//禁用CRC-32时钟节省电力
 //返回比对结果
 #ifndef EnableSecureStor
 DATACRCResult^=0xAC672833;
 #else
 UID=0;
 if(M24C512_ReadUID(UIDbuf,4))return false; //读取失败
 for(i=0;i<4;i++)
  {
	UID<<=8;
	UID|=UIDbuf[i];//将读取到的UID拼接一下
	}
 DATACRCResult^=UID;
 DATACRCResult^=0x3C6AF8E3;
 #endif
 if(DATACRCResult!=FRU->FRUBlock.CRC32Val)return false;
 return true;
 }
/**************************************************************
这个函数主要用于计算并自动填写FRU区域的CRC32校验和。程序会把FRU
数据区域的校验和给计算出来然后填写到指定区域
**************************************************************/
bool CalcFRUCRC(FRUBlockUnion *FRU)
 {
 unsigned int DATACRCResult;
 unsigned int UID;
 int i;
 char UIDbuf[4];
 CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
 //初始化CRC32      
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,ENABLE);//启用CRC-32时钟  
 CRC_DeInit(HT_CRC);//清除配置
 HT_CRC->SDR = 0x0;//CRC-32 poly: 0x04C11DB7  
 HT_CRC->CR = CRC_32_POLY | CRC_BIT_RVS_WR | CRC_BIT_RVS_SUM | CRC_BYTE_RVS_SUM | CRC_CMPL_SUM;
 //开始校验
 for(i=0;i<sizeof(CfgFile);i++)
	 wb(&HT_CRC->DR,FRU->FRUBlock.Data.FRUDbuf[i]);//将内容写入到CRC寄存器内
 //校验完毕计算结果
 DATACRCResult=HT_CRC->CSR;
 CRC_DeInit(HT_CRC);//清除CRC结果
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,DISABLE);//禁用CRC-32时钟节省电力
 //将CRC32值进行加盐
 #ifndef EnableSecureStor
 DATACRCResult^=0xAC672833;
 #else
 UID=0;
 if(M24C512_ReadUID(UIDbuf,4))return false; //读取失败
 for(i=0;i<4;i++)
  {
	UID<<=8;
	UID|=UIDbuf[i];//将读取到的UID拼接一下
	}
 DATACRCResult^=UID;
 DATACRCResult^=0x3C6AF8E3;
 #endif
 FRU->FRUBlock.CRC32Val=DATACRCResult;//填写一下
 return true;
 }
/**************************************************************
这个函数负责在系统检测到FRU损坏时，且debug模式激活时向EEPROM内
写入默认的FRU信息。这个函数平时不会编译到ROM中，只有debug模式开
启才会编译到ROM内。
**************************************************************/
#ifdef FlashLightOS_Debug_Mode
static void WriteNewFRU(const char *reason)
 {
 FRUBlockUnion FRU;
 const char *LEDStrPtr;
 //生成新的FRU数据并写入到EEPROM
 UartPost(Msg_info,"FRUChk","Due to %s,old FRU record will overwrite with new data.",reason);
 FRU.FRUBlock.Data.Data.MaxLEDCurrent=MaxAllowedLEDCurrent;//设置电流信息
 strncpy(FRU.FRUBlock.Data.Data.SerialNumber,"Serial Undefined",32);	//复制序列号信息
 memcpy(FRU.FRUBlock.Data.Data.FRUVersion,FRUVersion,3);//复制FRU信息		
 if(!WriteFRU(&FRU))
   {
	 LEDStrPtr=DisplayLEDType(&FRU);
	 UartPost(Msg_info,"FRUChk","FRU Record has been refreshed.");
   UartPost(Msg_info,"FRUChk","New record is for %s LED,Hardware Rev. V%d.%d.",LEDStrPtr,FRU.FRUBlock.Data.Data.FRUVersion[1],FRU.FRUBlock.Data.Data.FRUVersion[2]);
	 }
 else
	 UartPost(msg_error,"FRUChk","Failed to overwrite FRU information.");
 }	
#endif

/**************************************************************
固件启动时检查EEPROM内的FRU信息，如果版本信息不匹配则拒绝启动。
这是因为3V和6V版本的驱动固件和硬件之间互不兼容，会存在用户OTA更
新时意外将3V固件刷入6V驱动内的情况，这种情况就会导致不应出现的
LED开路和短路故障。
**************************************************************/
void FirmwareVersionCheck(void)
 {
 char checksumbuf[4];
 FRUBlockUnion FRU;
 const char *LEDStrPtr;
 //加载固件信息
 memcpy(checksumbuf,FRUVersion,3);
 checksumbuf[3]=Checksum8(checksumbuf,3)^0xAF; //计算校验和
 //读取	 
 if(ReadFRU(&FRU))
 	 {
	 CurrentLEDIndex=6;//EEPROM不工作
	 UartPost(Msg_critical,"FRUChk","Failed to check Hardware FRU info in EEPROM.");
	 SelfTestErrorHandler();//EEPROM掉线
	 }
 //检查FRU数据是否有效
 if(!CheckFRUInfoCRC(&FRU))
   {
	 #ifndef FlashLightOS_Debug_Mode
	 CurrentLEDIndex=3;//红灯常亮表示固件EEPROM损坏
	 UartPost(Msg_critical,"FRUChk","FRU information corrupted and not usable.System halted!");
	 SelfTestErrorHandler();//FRU信息损坏
	 #else
	 WriteNewFRU("information corrupted");//重写FRU
	 #endif
	 }
 //检查平台版本是否匹配
 else if(memcmp(FRU.FRUBlock.Data.Data.FRUVersion,checksumbuf,3)) //信息不匹配
	  {
		#ifndef FlashLightOS_Debug_Mode
		CurrentLEDIndex=3;//红灯常亮
		FRU.FRUBlock.Data.Data.FRUVersion[0]=FRUVersion[0];//写入固件编译时指定的LED类型
		LEDStrPtr=DisplayLEDType(&FRU);
		UartPost(Msg_critical,"FRUChk","hardware platform mismatch!This firmware is for %s LED and Hardware Rev. of V%d.%d.System halted!",LEDStrPtr,FRUVersion[1],FRUVersion[2]);
		SelfTestErrorHandler();//固件版本不匹配
	  #else
		WriteNewFRU("hardware platform mismatch");//重写FRU
	  #endif	
		}
 //信息匹配，加载数据，显示信息
 else
    {
		UartPost(Msg_info,"FRUChk","FRU Information has been loaded.");
		if(FRU.FRUBlock.Data.Data.MaxLEDCurrent>QueryMaximumCurrentLimit(&FRU))//非法的电流设置
		  {
			UartPost(msg_error,"FRUChk","Maximum Current value %.2fA is illegal for %s LED and will be trim to %.2fA.",
				FRU.FRUBlock.Data.Data.MaxLEDCurrent,
				DisplayLEDType(&FRU),
				QueryMaximumCurrentLimit(&FRU));
			FusedMaxCurrent=QueryMaximumCurrentLimit(&FRU);
			FRU.FRUBlock.Data.Data.MaxLEDCurrent=FusedMaxCurrent;//修正数值
		  if(!WriteFRU(&FRU))UartPost(Msg_info,"FRUChk","FRU Record has been corrected.");
			}
		else FusedMaxCurrent=FRU.FRUBlock.Data.Data.MaxLEDCurrent;//加载最大LED电流
		}
 }
