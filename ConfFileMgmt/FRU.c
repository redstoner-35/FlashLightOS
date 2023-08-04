#include "I2C.h"
#include "FirmwareConf.h"
#include "FRU.h"
#include "selftestlogger.h"
#include "console.h"
#include "Xmodem.h"
#include "LEDMgmt.h"
#include <string.h>

//全局变量和常量
float FusedMaxCurrent;//安全电流设置
float LEDVfMin;
float LEDVfMax; //LEDVf限制
const char FRUVersion[3]={FRUVer,HardwareMajorVer,HardwareMinorVer}; //版本信息
extern const char *LEDInfoStr[];//外部的LED类型常量

/**************************************************************
这个函数负责根据传入的LED类型设置LED的最低和最高Vf限制。
**************************************************************/
void SetLEDVfMinMax(FRUBlockUnion *FRU)
	{
  switch(FRU->FRUBlock.Data.Data.FRUVersion[0]) //显示LED型号
		{
		case 0x08:LEDVfMin=2.3;LEDVfMax=4.2;break; //SBT90.2
		case 0x07:
		case 0x03:LEDVfMin=1.95;LEDVfMax=4.2;break; //通用3V LED、蓝色SBT70
		case 0x04:LEDVfMin=1.4;LEDVfMax=3.2;;break;//红色SBT90
		case 0x05:LEDVfMin=1.85;LEDVfMax=4.9;break; //绿色SBT70
		case 0x06:LEDVfMin=4.5;LEDVfMax=6.8;break; //6V LED
	  //其他数值则使用默认值
		default:LEDVfMin=1.0;LEDVfMax=6.8;
		}
	}
/**************************************************************
这个函数负责接收传入的FRU结构体指针，解析出LED类型后返回最大电流
限制的合法值
**************************************************************/
float QueryMaximumCurrentLimit(FRUBlockUnion *FRU)
	{
  float result;
  switch(FRU->FRUBlock.Data.Data.FRUVersion[0]) //显示LED型号
		{
		case 0x03:result=50;break;
		case 0x04:result=25;break;
		case 0x05:result=14;break;
		case 0x06:result=30;break;
		case 0x07:result=14;break;
		case 0x08:result=33;break;
		//其他数值则使用默认值
		default:result=8;
		}
  if(result>MaxAllowedLEDCurrent)return MaxAllowedLEDCurrent;//数值不合法，大于固件编译时指定的最高电流
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
 for(i=0;i<sizeof(FRUDataUnion);i++)
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
 for(i=0;i<sizeof(FRUDataUnion);i++)
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
固件启动时加载EEPROM内的FRU信息,然后根据FRU信息加载熔断电流限制
最低和最高Vf值等运行信息到RAM内。
**************************************************************/
void FirmwareVersionCheck(void)
 {
 FRUBlockUnion FRU;
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
	 CurrentLEDIndex=3;//红灯常亮表示FRU验证不通过
	 UartPost(Msg_critical,"FRUChk","FRU information corrupted and not usable.System halted!");
	 SelfTestErrorHandler();//FRU信息损坏
	 #else
	 WriteNewFRU("information corrupted");//重写FRU
	 #endif
	 }
 //检查硬件版本是否匹配
 else if(memcmp(&FRU.FRUBlock.Data.Data.FRUVersion[1],&FRUVersion[1],2))
   {
	 #ifndef FlashLightOS_Debug_Mode
	 CurrentLEDIndex=3;//红灯常亮表示FRU验证不通过
	 UartPost(Msg_critical,"FRUChk","This firmware is targeted for Hardware version V%d.%d and not for current hardware.system halted!",HardwareMajorVer,HardwareMinorVer);
	 SelfTestErrorHandler();//FRU信息损坏
	 #else
	 WriteNewFRU("Hardware Mismatch");//重写FRU
	 #endif
	 }
 //信息匹配，加载数据，显示信息
 else
    {
		UartPost(Msg_info,"FRUChk","FRU Information has been loaded.");
		SetLEDVfMinMax(&FRU);//设置Vmin和Vmax
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
