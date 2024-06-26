#include "cfgfile.h"
#include <string.h>
#include <stdio.h>
#include "AES256.h"
#include "delay.h"
#include "console.h"
#include "LEDMgmt.h"
#include "Xmodem.h"
#include "FRU.h"
#include "selftestlogger.h"
#include "CurrentReadComp.h"
#include "Password.h"

ConfUnionDef CfgFileUnion;
const char ZeroConstBuf[32]={0};

//字符串
const char *EEPModName="CfgEEP";
static const char *CheckingFileInt="Checking %s configuration file integrity...";
static const char *LoadFileInfo="Loading %s config file into RAM...";
static const char *ConfigHasBeenLoaded="%s config file has been loaded,CRC-32 value:0x%8X.";
static const char *ConfigHasRestored="%s config file has been re-writed with %s config.";
static const char *FixingConfigFile="fixing corrupted %s config file...";
static const char *RestoreCfg="Restoring %s config file...";

//外部变量
extern int DeepSleepTimer;  //外部休眠定时器

//加载默认的配置
void LoadDefaultConf(bool IsOverridePassword)
 {
 int i;
 LEDThermalConfStrDef ParamOut;
 //系统基本设置
 CfgFile.USART_Baud=115200;
 CfgFile.EnableRunTimeLogging=true;
 #ifdef AMUTorchMode
 CfgFile.IsHoldForPowerOn=false;//使用阿木的操控逻辑，单击开机长按关机
 #else
 CfgFile.IsHoldForPowerOn=true;//使用默认的操控逻辑，长按开关机	 
 #endif
 CfgFile.IsDriverLockedAfterPOR=false; //上电不自锁
 CfgFile.DeepSleepTimeOut=8*DeepsleepDelay;//深度睡眠时间
 CfgFile.IdleTimeout=8*DefaultTimeOutSec; //定时器频率乘以超时时间得到超时值
 CfgFile.AutoLockTimeOut=300; //默认驱动在没有任何操作的5分钟后触发锁定
 #ifdef EnableSideLocLED	 
 CfgFile.EnableLocatorLED=true;//启用侧按定位LED
 #else
 CfgFile.EnableLocatorLED=false;//禁用侧按定位LED
 #endif
 CfgFile.RevTactalSettings=RevTactical_Off; //默认情况下，反向战术模式将会把手电筒设置为关闭
 //恢复主机名和账户凭据
 strncpy(CfgFile.HostName,"Xtern-Ripper",20);
 if(IsOverridePassword)
   {
	 strncpy(CfgFile.AdminAccountname,"ADMIN",20); //管理员账户名
   strncpy(CfgFile.RootAccountPassword,RootPassword,16);//root密码		 
   strncpy(CfgFile.AdminAccountPassword,AdminPassword,16);//管理员密码
	 }
 //恢复挡位配置	 
 RestoreFactoryModeCfg();	 
 //恢复无极调光设置
 for(i=0;i<13;i++)
	 {
	 CfgFile.DefaultLevel[i]=0.10;
	 CfgFile.IsRememberBrightNess[i]=true; //无极调光默认记忆亮度
	 }
 CfgFile.IsNoteLEDEnabled=true;// 启用无极调光的提示
 //恢复温控设置
 QueueLEDThermalSettings(&ParamOut); //查表
 CfgFile.MOSFETThermalTripTemp=135; //MOS热跳闸为135度
 CfgFile.LEDThermalTripTemp=ParamOut.MaxLEDTemp; //设置LED热跳闸温度
 CfgFile.PIDTriggerTemp=ParamOut.PIDTriggerTemp; //设置温控接入温度
 CfgFile.PIDTargetTemp=ParamOut.PIDMaintainTemp; //PID目标温度
 CfgFile.PIDRelease=ParamOut.PIDMaintainTemp-15; //PID停止调节温度为维持温度减去15度
 CfgFile.ThermalPIDKp=4.30;
 CfgFile.ThermalPIDKi=1.20;
 CfgFile.ThermalPIDKd=1.05; //PID温控的P I D
 CfgFile.LEDThermalWeight=60;//LED温度加权值
 //恢复电池设置
 CfgFile.VoltageFull=4.0*BatteryCellCount;
 CfgFile.VoltageAlert=3.0*BatteryCellCount;
 CfgFile.VoltageTrip=2.8*BatteryCellCount;
 CfgFile.VoltageOverTrip=14.5;//过压保护值14.5V 
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
 FRUHwResult^=FRU.FRUBlock.Data.Data.CustomLEDIDCode&0xFF; 
 FRUHwResult^=(FRU.FRUBlock.Data.Data.CustomLEDIDCode>>8)&0xFF; //加入特殊LED Code
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
 FRUHwResult^=FRU.FRUBlock.Data.Data.CustomLEDIDCode&0xFF; 
 FRUHwResult^=(FRU.FRUBlock.Data.Data.CustomLEDIDCode>>8)&0xFF; //加入特殊LED Code
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
 FRUHwResult^=FRU.FRUBlock.Data.Data.CustomLEDIDCode&0xFF; 
 FRUHwResult^=(FRU.FRUBlock.Data.Data.CustomLEDIDCode>>8)&0xFF; //加入特殊LED Code
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
 #ifdef FlashLightOS_Init_Mode
 UartPost(Msg_info,EEPModName,"Config 1 Offset:0x%04X",CfgFileSize);	
 UartPost(Msg_info,EEPModName,"Error log Area:0x%04X to 0x%04X",LoggerBase,RunTimeLogBase);	
 UartPost(Msg_info,EEPModName,"Runtime log Area:0x%04X to 0x%04X",RunTimeLogBase,RunTimeLogSize);
 UartPost(Msg_info,EEPModName,"Selftest log Area:0x%04X to 0x%04X",SelfTestLogBase,SelftestLogEnd);
 UartPost(Msg_info,EEPModName,"Calibration Record Area:0x%04X to 0x%04X",SelftestLogEnd,CalibrationRecordEnd);
 UartPost(Msg_info,EEPModName,"FRU Area:0x%04X to 0x%04X",CalibrationRecordEnd,CalibrationRecordEnd+sizeof(FRUBlockUnion));
 UartPost(Msg_info,EEPModName,"Total ROM Spaces:%d Bytes",CalibrationRecordEnd+sizeof(FRUBlockUnion));
 #endif
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
	 LoadDefaultConf(true);
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
