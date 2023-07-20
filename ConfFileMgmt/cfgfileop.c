#include "cfgfile.h"
#include <string.h>
#include <stdio.h>
#include "AES256.h"
#include "delay.h"
#include "console.h"
#include "LEDMgmt.h"
#include "Xmodem.h"

ConfUnionDef CfgFileUnion;
const char ZeroConstBuf[32]={0};
const char *EEPModName="CfgEEP";
extern int DeepSleepTimer;  //外部休眠定时器

/*
SPS电流检测的非线性矫正曲线。这部分数值会因为驱动的电路
设计不同而变更，需要根据实物的误差情况进行调节。
*/
const float IMONGainSettings[SPSCompensateTableSize*2]=
{
     1,    2,    5,   8,10.037,   12,   16,   20,   25,MaxAllowedLEDCurrent,
0.8196,1.307,1.066,1.12,1.057 ,1.068,1.075,1.071,1.087,1.089	
};
/*
默认驱动的温控曲线，从50度开始缓缓降档到原始值的85%，
超过65度后开始迅速降档
当温度达到82度的时候降档至原始值的10%
*/
const float DefaultThermalThrCurve[10]=
{
50,55,65,75,82,  //温度阈值
100,95,85,50,10  //百分比
};
/*
被加密的管理员用户ADMIN和超级用户root的默认密码
密码使用AES-256加密，可以通过'cptextgen'命令生成，
然后替换掉下方数组的内容即可，密码长度需要小于或
等于16位。为了安全起见本固件的用户密码在单片机片
内RAM区域和配置ROM区域均使用AES256密文存储。
*/
const char RootPassword[16]=
{
/* Encrypted Raw text 'Nje7^8#a5Be&d' */
0x8D,0x09,0xEF,0x0B,0x63,0x12,0x85,0xA8,
0x05,0xC6,0x91,0x02,0x20,0xA6,0x9F,0xD7
};
const char AdminPassword[16]=
{
/* AES256加密的文本 'ADMIN' */
0x62,0xFE,0xF0,0xC7,0x38,0x0B,0x08,0x04,
0x8E,0xBF,0x67,0x0F,0x6B,0x09,0x4C,0xA3
};	
//加载默认的配置
void LoadDefaultConf(void)
 {
 int i;
 //系统基本设置
 CfgFile.USART_Baud=115200;
 CfgFile.EnableRunTimeLogging=true;
 CfgFile.IsDriverLockedAfterPOR=false; //上电不自锁
 CfgFile.PWMDIMFreq=20000;//20KHz调光频率
 CfgFile.DeepSleepTimeOut=8*DeepsleepDelay;//深度睡眠时间
 CfgFile.IdleTimeout=8*DefaultTimeOutSec; //定时器频率乘以超时时间得到超时值
 strncpy(CfgFile.AdminAccountname,"ADMIN",20);
 strncpy(CfgFile.HostName,"DDH-D8B-SBT90",20);
 /* 密码处理 */  	
 strncpy(CfgFile.RootAccountPassword,RootPassword,16);//root密码		 
 strncpy(CfgFile.AdminAccountPassword,AdminPassword,16);//管理员密码
 //恢复挡位配置	 
 RestoreFactoryModeCfg();	 
 //恢复电流矫正设置
 for(i=0;i<SPSCompensateTableSize;i++)
	 {
	 CfgFile.LEDIMONCalThreshold[i]=IMONGainSettings[i];
	 CfgFile.LEDIMONCalGain[i]=IMONGainSettings[i+SPSCompensateTableSize];
	 }
 //恢复温控设置
 CfgFile.LEDThermalStepWeight=50;//LED的降档设置权重为50%
 CfgFile.LEDThermalTripTemp=90;
 CfgFile.MOSFETThermalTripTemp=110; //LED热跳闸为110度，LED 90度
 for(i=0;i<5;i++)
	 {
	 CfgFile.LEDThermalStepThr[i]=DefaultThermalThrCurve[i];
	 CfgFile.SPSThermalStepThr[i]=DefaultThermalThrCurve[i]; //温度阈值
	 CfgFile.LEDThermalStepRatio[i]=DefaultThermalThrCurve[i+5];
	 CfgFile.SPSThermalStepRatio[i]=DefaultThermalThrCurve[i+5]; //温度曲线
	 }
 //恢复电池设置
 CfgFile.VoltageFull=4.0*BatteryCellCount;
 CfgFile.VoltageAlert=3.0*BatteryCellCount;
 CfgFile.VoltageTrip=2.8*BatteryCellCount;
 CfgFile.VoltageOverTrip=14.5;//过压保护值14.5V
 CfgFile.OverCurrentTrip=16;// 16A的电池端过流保护值
 } 
//检查ROM中的数据是否损坏
int CheckConfigurationInROM(cfgfiletype cfgtyp,unsigned int *CRCResultO)
 {
 int CfgsizePage,i,CRCDcount;
 int CfgFilebase,j,CRCResultindex;
 char BUF[16];
 char CRCResultptr[4];
 unsigned int ROMCRCResult,DATACRCResult;
 CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
 //计算出文件大小，然后算出flash base
 CfgsizePage=(sizeof(CfgFile)+4)/16;
 if((sizeof(CfgFile)+4)%16)CfgsizePage++;
 if(cfgtyp==Config_XmodemRX)CfgFilebase=XmodemConfRecvBase;
 else CfgFilebase=(cfgtyp==Config_Main)?0:CfgsizePage*16;
 //初始化CRC32   
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,ENABLE);//启用CRC-32时钟
 CRC_DeInit(HT_CRC);//清除配置
 HT_CRC->SDR = 0x0;//CRC-32 poly: 0x04C11DB7  
 HT_CRC->CR = CRC_32_POLY | CRC_BIT_RVS_WR | CRC_BIT_RVS_SUM | CRC_BYTE_RVS_SUM | CRC_CMPL_SUM;
 //开始校验
 i=0;
 CRCResultindex=0;
 while(i<CfgsizePage)
   {
	 //读取数据并解码
   if(M24C512_PageRead(BUF,CfgFilebase,16))return 1;
   if(AES_EncryptDecryptData(BUF,0))return 2;
   //计算出本次需要校验的数据数目然后丢入寄存器
   CRCDcount=sizeof(CfgFile)-(i*16);
	 if(CRCDcount>16)CRCDcount=16;//大于16的值限制为16
	 if(CRCDcount<0)CRCDcount=0;//小于0的值限制为0
	 if(CRCDcount>0)for(j=0;j<CRCDcount;j++)
		 wb(&HT_CRC->DR,BUF[j]);//将数据丢入寄存器内
	 //如果当前剩下的字节小于16字节，则说明配置文件已经读完了需要读取CRC数据
	 if(CRCDcount<16)while(CRCDcount<16&&CRCResultindex<4)
	   {
		 CRCResultptr[CRCResultindex]=BUF[CRCDcount];
		 CRCDcount++;
		 CRCResultindex++;
		 }
	 //指向下一个page
	 i++;
	 CfgFilebase+=16; 
	 }
 //生成结果，然后比较CRC缓冲的内容
 ROMCRCResult=0;
 for(i=0;i<4;i++)
	 {
	 ROMCRCResult<<=8;
	 ROMCRCResult&=0xFFFFFF00;
	 ROMCRCResult|=CRCResultptr[i];
	 }
 DATACRCResult=HT_CRC->CSR;//获取数据	 
 CRC_DeInit(HT_CRC);//清除配置
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,DISABLE);//禁用CRC-32时钟节省电力
 if(ROMCRCResult!=DATACRCResult)return 3;
 //操作完毕，返回0
 *CRCResultO=DATACRCResult;//输出CRC32结果
 return 0;
 }
//读取数据
int ReadConfigurationFromROM(cfgfiletype cfgtyp)
 {
	 int CurrentSize,TargetReadSize;
	 int CfgsizePage,CfgFilebase;
	 char Buf[16];
	 //计算初始地址
	 CfgsizePage=(sizeof(CfgFile)+4)/16;
   if((sizeof(CfgFile)+4)%16)CfgsizePage++;
   if(cfgtyp==Config_XmodemRX)CfgFilebase=XmodemConfRecvBase;
   else CfgFilebase=(cfgtyp==Config_Main)?0:CfgsizePage*16;
	 //读数据
	 CurrentSize=0;
	 while(CurrentSize<sizeof(CfgFile))
	   {
		 //计算需要读取的内容大小
		 TargetReadSize=sizeof(CfgFile)-CurrentSize;
		 if(TargetReadSize>16)TargetReadSize=16;
     //读取数据并解码
     if(M24C512_PageRead(Buf,CurrentSize+CfgFilebase,16))return 1;
     if(AES_EncryptDecryptData(Buf,0))return 2;
		 //开始写数据，写完数据后继续
		 memcpy(&CfgFileUnion.StrBUF[CurrentSize],Buf,TargetReadSize);
	   CurrentSize+=TargetReadSize;
		 }
   return 0;
 }
//写入数据
int WriteConfigurationToROM(cfgfiletype cfgtyp)
 {
 int CfgsizePage,i,CRCDcount;
 int CfgFilebase,j,CRCResultindex,k;
 char BUF[16];
 char *StrPtr;
 char CRCResult[4];
 unsigned int DATACRCResult;
 CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
 //计算出文件大小，然后算出flash base
 CfgsizePage=(sizeof(CfgFile)+4)/16;
 if((sizeof(CfgFile)+4)%16)CfgsizePage++;
 if(cfgtyp==Config_XmodemRX)CfgFilebase=XmodemConfRecvBase;
 else CfgFilebase=(cfgtyp==Config_Main)?0:CfgsizePage*16;
 //初始化CRC32        
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,ENABLE);//启用CRC-32时钟 
 CRC_DeInit(HT_CRC);//清除配置
 HT_CRC->SDR = 0x0;//CRC-32 poly: 0x04C11DB7  
 HT_CRC->CR = CRC_32_POLY | CRC_BIT_RVS_WR | CRC_BIT_RVS_SUM | CRC_BYTE_RVS_SUM | CRC_CMPL_SUM;
 //开始写数据
 i=0;
 k=0;
 CRCResultindex=0;
 while(i<CfgsizePage)
   {
   //计算出本次需要校验的数据数目然后丢入寄存器
   CRCDcount=sizeof(CfgFile)-(i*16);
	 if(CRCDcount>16)CRCDcount=16;//大于16的值限制为16
	 if(CRCDcount<0)CRCDcount=0;
	 if(CRCDcount>0)//需要写入的数量大于0，复制内容
	   {
		 memset(BUF,0,sizeof(BUF));//清零缓存
		 StrPtr=&CfgFileUnion.StrBUF[i*16];
		 memcpy(BUF,StrPtr,CRCDcount);
	   for(j=0;j<CRCDcount;j++)wb(&HT_CRC->DR,BUF[j]);//将数据丢入寄存器内 
		 }
	 //如果当前剩下的字节小于16字节，则说明配置文件已经写完了需要读取CRC数据
	 if(CRCDcount<16)while(CRCDcount<16&&CRCResultindex<4)
	   {
		 //从CRC寄存器内读取结果
		 if(!k)
		   {
			 DATACRCResult=HT_CRC->CSR;
			 CRC_DeInit(HT_CRC);
			 k=1;//只允许一次
			 for(j=0;j<4;j++)
				 {
			   CRCResult[j]=(DATACRCResult>>24)&0xFF;//取最高位的数据
				 DATACRCResult<<=8;//将数据往高位移动
				 }					 
			 }
		 BUF[CRCDcount]=CRCResult[CRCResultindex];
		 CRCDcount++;
		 CRCResultindex++;
		 }
	 //将数据加密后写入
   if(AES_EncryptDecryptData(BUF,1))return 1;
   if(M24C512_PageWrite(BUF,CfgFilebase,16))return 2;	 
	 //指向下一个page
	 i++;
	 CfgFilebase+=16; 
	 }
 //操作完毕，返回0
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,DISABLE);//禁用CRC-32时钟节省电力
 return 0;
 }
//校验内存中的配置并生成crc
unsigned int ActiveConfigurationCRC(void)
 {
 unsigned int DATACRCResult;
 int i;
 CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
 //初始化CRC32      
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,ENABLE);//启用CRC-32时钟  
 CRC_DeInit(HT_CRC);//清除配置
 HT_CRC->SDR = 0x0;//CRC-32 poly: 0x04C11DB7  
 HT_CRC->CR = CRC_32_POLY | CRC_BIT_RVS_WR | CRC_BIT_RVS_SUM | CRC_BYTE_RVS_SUM | CRC_CMPL_SUM;
 //开始校验
 for(i=0;i<sizeof(CfgFile);i++)
	 wb(&HT_CRC->DR,CfgFileUnion.StrBUF[i]);//将内容写入到CRC寄存器内
 //校验完毕计算结果
 DATACRCResult=HT_CRC->CSR;
 CRC_DeInit(HT_CRC);//清除CRC结果
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,DISABLE);//禁用CRC-32时钟节省电力
 return DATACRCResult;
 }

//自检校验handler
void PORConfHandler(void)
 {
 #ifndef FlashLightOS_Debug_Mode
 unsigned int MainCRC,BackupCRC;
 int checkresult,backupcheckresult;
 UartPost(Msg_info,EEPModName,"Checking main configuration file integrity...");
 checkresult=CheckConfigurationInROM(Config_Main,&MainCRC);
 switch(checkresult)
   {
		 case 1:UartPost(Msg_critical,EEPModName,"Configuration EEPROM offline.");CurrentLEDIndex=6;SelfTestErrorHandler();
		 case 2:UartPost(Msg_critical,EEPModName,"AES-256 Decrypt routine execution error.");CurrentLEDIndex=7;SelfTestErrorHandler();
	   case 3:UartPost(Msg_warning,EEPModName,"Main Configuration file Corrupted.");break;
	 }
 UartPost(Msg_info,EEPModName,"Checking backup configuration file integrity...");
 backupcheckresult=CheckConfigurationInROM(Config_Backup,&BackupCRC);
 switch(backupcheckresult)
   {
		 case 1:UartPost(Msg_critical,EEPModName,"Configuration EEPROM offline.");CurrentLEDIndex=6;SelfTestErrorHandler();
		 case 2:UartPost(Msg_critical,EEPModName,"AES-256 Decrypt routine execution error.");CurrentLEDIndex=7;SelfTestErrorHandler();
	   case 3:UartPost(Msg_warning,EEPModName,"Main Configuration file Corrupted.");break;
	 }
 //主文件OK
 if(!checkresult)
   {
	 UartPost(Msg_info,EEPModName,"Loading main configuration file into RAM..");
	 ReadConfigurationFromROM(Config_Main);
	 UartPost(Msg_info,EEPModName,"Main configuration has been loaded,file CRC-32 value:0x%8X.",MainCRC);		 
	 //更新备份文件
	 if(backupcheckresult)
	   {
		 UartPost(Msg_info,EEPModName,"fixing corrupted backup config file...");
		 WriteConfigurationToROM(Config_Backup);
		 UartPost(Msg_info,EEPModName,"Backup config file has been re-writed with main config.");
		 }
	 }
 //主文件不OK，检查备份文件是否OK
 else if(!backupcheckresult)
   {
	 UartPost(Msg_info,EEPModName,"Load Backup Conf file to RAM..");
	 ReadConfigurationFromROM(Config_Backup);
	 UartPost(Msg_info,EEPModName,"Backup configuration has been loaded,file CRC-32 value:0x%8X",BackupCRC);	
	 //更新备份文件
	 if(checkresult)
	   {
		 UartPost(Msg_info,EEPModName,"fixing corrupted main config file...");
		 WriteConfigurationToROM(Config_Main);
		 UartPost(Msg_info,EEPModName,"Main config file has been re-writed with backup config.");
		 }
	 }
 //全坏了
 else 
   {
	 LoadDefaultConf();
	 UartPost(msg_error,EEPModName,"No usable Conig in EEPROM.");
	 UartPost(Msg_info,EEPModName,"driver will use factory default and attempt to fix broken config.");
	 UartPost(Msg_info,EEPModName,"Programming main config file.");	 
	 backupcheckresult=WriteConfigurationToROM(Config_Main);
	 switch(backupcheckresult)
     {
		 case 1:UartPost(Msg_critical,EEPModName,"Configuration EEPROM offline.");CurrentLEDIndex=6;SelfTestErrorHandler();
		 case 2:UartPost(Msg_critical,EEPModName,"AES-256 Encrypt routine execution error.");CurrentLEDIndex=7;SelfTestErrorHandler();
	   }
	 UartPost(Msg_info,EEPModName,"Programming backup config file."); 
	 WriteConfigurationToROM(Config_Backup);
	 UartPost(Msg_info,EEPModName,"Programming completed,driver will restart automatically."); 
	 delay_Second(2);		
   NVIC_SystemReset();//硬重启
	 }
 #else
   UartPost(Msg_warning,EEPModName,"Firmware Debug Mode Enabled,Using Factory settings.");
   LoadDefaultConf();  
 #endif
 //显示警告和重新初始化休眠定时器
 if(CfgFile.DeepSleepTimeOut>0)DeepSleepTimer=CfgFile.DeepSleepTimeOut;
 else DeepSleepTimer=-1;
 if(CfgFile.IdleTimeout==0)
	 UartPost(Msg_warning,EEPModName,"Admin account time out has been disabled\r\nand might cause security issue.");
 }
