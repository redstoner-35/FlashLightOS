#include "cfgfile.h"
#include <string.h>
#include <stdio.h>
#include "AES256.h"
#include "delay.h"
#include "console.h"
#include "LEDMgmt.h"
#include "Xmodem.h"
#include "FRU.h"

ConfUnionDef CfgFileUnion;
const char ZeroConstBuf[32]={0};

//字符串
const char *EEPModName="CfgEEP";
static const char *CheckingFileInt="Checking %s configuration file integrity...";
static const char *LoadFileInfo="Loading %s configuration file into RAM..";
static const char *ConfigHasBeenLoaded="%s configuration has been loaded,file CRC-32 value:0x%8X.";
static const char *ConfigHasRestored="%s config file has been re-writed with %s config.";
static const char *FixingConfigFile="fixing corrupted %s config file...";
static const char *RestoreCfg="Restoring %s config file...";

//外部变量
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
 FRUBlockUnion FRU;
 bool IsUsingHighTemp;
 //系统基本设置
 CfgFile.USART_Baud=115200;
 CfgFile.EnableRunTimeLogging=true;
 CfgFile.IsHoldForPowerOn=false;//使用阿木的操控逻辑，单击开机长按关机
 CfgFile.EnableLocatorLED=true;//启用侧按定位LED
 CfgFile.IsDriverLockedAfterPOR=false; //上电不自锁
 CfgFile.PWMDIMFreq=20000;//20KHz调光频率
 CfgFile.DeepSleepTimeOut=8*DeepsleepDelay;//深度睡眠时间
 CfgFile.IdleTimeout=8*DefaultTimeOutSec; //定时器频率乘以超时时间得到超时值
 strncpy(CfgFile.AdminAccountname,"ADMIN",20);
 strncpy(CfgFile.HostName,"Xtern-Ripper",20);
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
 CfgFile.LEDThermalTripTemp=90;
 CfgFile.MOSFETThermalTripTemp=110; //LED热跳闸为110度，LED 90度
 //根据FRU信息决定是否使用更高的温度墙
 if(ReadFRU(&FRU))IsUsingHighTemp=false;
 else if(!CheckFRUInfoCRC(&FRU))IsUsingHighTemp=false;
 else if(FRU.FRUBlock.Data.Data.FRUVersion[0]==0x08)IsUsingHighTemp=true; //SBT90.2 LED，使用较高温度墙
 else IsUsingHighTemp=false;	 
 CfgFile.PIDTriggerTemp=IsUsingHighTemp?70:65; //当MOS和LED的平均温度等于指定温度时温控接入
 CfgFile.PIDTargetTemp=IsUsingHighTemp?57:55; //PID目标温度
 CfgFile.PIDRelease=50; //当温度低于50度时，PID不调节 
 CfgFile.ThermalPIDKp=0.28;
 CfgFile.ThermalPIDKi=0.8;
 CfgFile.ThermalPIDKd=1.10; //PID温控的P I D
 CfgFile.LEDThermalWeight=60;//LED温度加权值
 //恢复电池设置
 CfgFile.VoltageFull=4.0*BatteryCellCount;
 CfgFile.VoltageAlert=3.0*BatteryCellCount;
 CfgFile.VoltageTrip=2.8*BatteryCellCount;
 CfgFile.VoltageOverTrip=14.5;//过压保护值14.5V
 CfgFile.OverCurrentTrip=IsUsingHighTemp?20:15;// 如果是SBT90.2，则使用20A否则15A的电池端过流保护值
 } 
//检查ROM中的数据是否损坏
int CheckConfigurationInROM(cfgfiletype cfgtyp,unsigned int *CRCResultO)
 {
 int CfgsizePage,i,CRCDcount;
 int CfgFilebase,j,CRCResultindex;
 char BUF[16];
 char FRUHwResult;
 char CRCResultptr[4];
 FRUBlockUnion FRU;
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
 //读取FRU并计算数值
 if(ReadFRU(&FRU))return 1; //读取FRU失败
 FRUHwResult=0x32;
 for(i=0;i<2;i++)FRUHwResult^=FRU.FRUBlock.Data.Data.FRUVersion[i]; //仅仅是把LED类型和major version加入计算
 //开始校验
 if(ReadFRU(&FRU))return 1; //读取FRU失败
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
		 wb(&HT_CRC->DR,BUF[j]^FRUHwResult);//将数据和FRU信息XOR后丢入寄存器内
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
 char FRUHwResult;
 FRUBlockUnion FRU;
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
 //读取FRU并计算数值
 if(ReadFRU(&FRU))return 1; //读取FRU失败
 FRUHwResult=0x32;
 for(i=0;i<2;i++)FRUHwResult^=FRU.FRUBlock.Data.Data.FRUVersion[i]; //仅仅是把LED类型和major version加入计算
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
	   for(j=0;j<CRCDcount;j++)wb(&HT_CRC->DR,BUF[j]^FRUHwResult);//将数据和FRU信息XOR后丢入寄存器内
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
 char FRUHwResult;
 CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
 FRUBlockUnion FRU;
 //初始化CRC32      
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,ENABLE);//启用CRC-32时钟  
 CRC_DeInit(HT_CRC);//清除配置
 HT_CRC->SDR = 0x0;//CRC-32 poly: 0x04C11DB7  
 HT_CRC->CR = CRC_32_POLY | CRC_BIT_RVS_WR | CRC_BIT_RVS_SUM | CRC_BYTE_RVS_SUM | CRC_CMPL_SUM;
 //读取FRU并计算数值
 if(ReadFRU(&FRU))return 1; //读取FRU失败
 FRUHwResult=0x32;
 for(i=0;i<2;i++)FRUHwResult^=FRU.FRUBlock.Data.Data.FRUVersion[i]; //仅仅是把LED类型和major version加入计算
 //开始校验
 for(i=0;i<sizeof(CfgFile);i++)
	 wb(&HT_CRC->DR,CfgFileUnion.StrBUF[i]^FRUHwResult);//将内容写入到CRC寄存器内
 //校验完毕计算结果
 DATACRCResult=HT_CRC->CSR;
 CRC_DeInit(HT_CRC);//清除CRC结果
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,DISABLE);//禁用CRC-32时钟节省电力
 return DATACRCResult;
 }

//校验结果显示 
void DisplayCheckResult(int Result,bool IsProgram)
  {
	 switch(Result)
     {
		 case 1:
			 UartPost(Msg_critical,EEPModName,"Config EEPROM offline.");
		    CurrentLEDIndex=6;
		    SelfTestErrorHandler();
		 case 2:
			  UartPost(Msg_critical,EEPModName,"AES256 routine error.");
		    CurrentLEDIndex=7;
		    SelfTestErrorHandler();
	   case 3:
			 if(IsProgram)break;
			 UartPost(Msg_warning,EEPModName,"Config file Verification Error.");
		   break;
	   }
	}
//自检校验handler
void PORConfHandler(void)
 {
 unsigned int MainCRC,BackupCRC;
 int checkresult,backupcheckresult;
 UartPost(Msg_info,EEPModName,(char *)CheckingFileInt,"main");
 checkresult=CheckConfigurationInROM(Config_Main,&MainCRC);
 DisplayCheckResult(checkresult,false);
 UartPost(Msg_info,EEPModName,(char *)CheckingFileInt,"backup");
 backupcheckresult=CheckConfigurationInROM(Config_Backup,&BackupCRC);
 DisplayCheckResult(backupcheckresult,false);
 //主文件OK
 if(!checkresult)
   {
	 UartPost(Msg_info,EEPModName,(char *)LoadFileInfo,"main");
	 ReadConfigurationFromROM(Config_Main);
	 UartPost(Msg_info,EEPModName,(char *)ConfigHasBeenLoaded,"Main",MainCRC);		 
	 //更新备份文件
	 if(backupcheckresult)
	   {
		 UartPost(Msg_info,EEPModName,(char *)FixingConfigFile,"backup");
		 WriteConfigurationToROM(Config_Backup);
		 UartPost(Msg_info,EEPModName,(char *)ConfigHasRestored,"Backup","main");
		 }
	 }
 //主文件不OK，检查备份文件是否OK
 else if(!backupcheckresult)
   {
	 UartPost(Msg_info,EEPModName,(char *)LoadFileInfo,"backup");
	 ReadConfigurationFromROM(Config_Backup);
	 UartPost(Msg_info,EEPModName,(char *)ConfigHasBeenLoaded,"Backup",BackupCRC);	
	 //更新主文件
	 if(checkresult)
	   {
		 UartPost(Msg_info,EEPModName,(char *)FixingConfigFile,"main");
		 WriteConfigurationToROM(Config_Main);
		 UartPost(Msg_info,EEPModName,(char *)ConfigHasRestored,"Main","backup");
		 }
	 }
 //全坏了
 else 
   {
	 LoadDefaultConf();
	 UartPost(Msg_critical,EEPModName,"No usable config found,driver will reverb to default.");
	 UartPost(Msg_info,EEPModName,(char *)RestoreCfg,"Main");	 
	 DisplayCheckResult(WriteConfigurationToROM(Config_Main),true);
	 UartPost(Msg_info,EEPModName,(char *)RestoreCfg,"Backup"); 
	 DisplayCheckResult(WriteConfigurationToROM(Config_Backup),true);
	 delay_Second(1);		
   NVIC_SystemReset();//硬重启
	 }
 //显示警告和重新初始化休眠定时器
 if(CfgFile.DeepSleepTimeOut>0)DeepSleepTimer=CfgFile.DeepSleepTimeOut;
 else DeepSleepTimer=-1;
 }
