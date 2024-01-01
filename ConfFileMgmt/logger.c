#include <string.h>
#include <stdio.h>
#include "console.h"
#include "delay.h"
#include "modelogic.h"
#include "runtimelogger.h"
#include "ADC.h"
#include "I2C.h"
#include "logger.h"
#include <math.h>

//静态变量声明
LoggerHeaderUnion LoggerHdr;
static unsigned char UpdateHeaderCounter;

//复位logger定时器的函数
void ResetLoggerHeader_AutoUpdateTIM(void)
 {
 //将定时器设置为停止值
 UpdateHeaderCounter=LoggerAutoHeaderSaveTime*8;
 } 
//自动检测日志记录header更新并将更新结果写到ROM的记录器
void LoggerHeader_AutoUpdateHandler(void)
 {
 //写日志头部到ROM
 if(UpdateHeaderCounter==(LoggerAutoHeaderSaveTime*8)-1)
  {
	UpdateHeaderCounter++;
	if(!WriteLoggerHeader(&LoggerHdr))//写日志头部，如果失败则重试
		UpdateHeaderCounter=0;
	}
 //还没到时间，继续累加
 else UpdateHeaderCounter=UpdateHeaderCounter<(LoggerAutoHeaderSaveTime*8)?UpdateHeaderCounter+1:UpdateHeaderCounter;
 }
//恢复日志的时候检查指定的log区域里面的内容是否合法
bool CheckForErrLogStatu(int ROMAddr)
 {
 LoggerHeaderUnion TempLoggerHdr;
 LoggerDataUnion LogData;
 int i;
 unsigned int CRCResult;
 bool IsEntryContainError;
 //读取头部然后检查key
 if(M24C512_PageRead(TempLoggerHdr.LoggerHeaderbuf,ROMAddr,sizeof(LoggerHeaderUnion)))return false;
 if(strncmp(TempLoggerHdr.LoggerHeader.CheckKey,LoggerHeaderKey,5))return false;//检查失败
 //遍历所有内容
 for(i=0;i<MaximumLoggerDepth;i++)
		{
		//从ROM内读取数据
		if(M24C512_PageRead(LogData.LoggerDatabuf,ROMAddr+sizeof(LoggerHeaderUnion)+(i*sizeof(LoggerDataUnion)),sizeof(LoggerDataUnion)))
			return false;
	  //验证log entry的CRC32，如果不一致，则报错
		CRCResult=calculateLogEntryCRC32(&LogData);
		if(CRCResult!=TempLoggerHdr.LoggerHeader.EntryCRC[i])return false;
		//log错误类型和header中是否有错误的表不一致，更新header
	  if(LogData.LoggerDateSection.ErrorCode!=Error_None)IsEntryContainError=true;
	  else IsEntryContainError=false; //判断有没有错误
		if(IsEntryContainError!=TempLoggerHdr.LoggerHeader.IsEntryHasError[i])return false;//错误数据不一致
	  }
 //检查通过
 return true;
 }
//上电自检的时候初始化log记录器
void LoggerHeader_POR(void)
 {
 int i,faultentry;
 unsigned int CRCResult;
 bool HeaderUpdated=false;
 bool IsEntryContainError;
 LoggerDataUnion LogData;
 //读取logger内容
 UartPost(Msg_info,"Logger","System error logger Init start...");
 ResetLoggerHeader_AutoUpdateTIM();//复位TIM
 if(!fetchloggerheader(&LoggerHdr))
  {
	UartPost(Msg_critical,"Logger","Failed to load logger header from EEPROM.");
	SelfTestErrorHandler();
	}
 //检查读出来的logger头部信息
 if(!strncmp(LoggerHdr.LoggerHeader.CheckKey,LoggerHeaderKey,5))//检查通过
  {
	faultentry=0;
	for(i=0;i<MaximumLoggerDepth;i++)
		{
		//从ROM内读取数据
		if(!FetchLoggerData(&LogData,i))
      {
	    UartPost(Msg_critical,"Logger","Failed to load logger data section #%d from EEPROM.",i);
	    SelfTestErrorHandler();
	    }
	  //验证log entry的CRC32，如果不一致，则写入数据
		CRCResult=calculateLogEntryCRC32(&LogData);
		if(CRCResult!=LoggerHdr.LoggerHeader.EntryCRC[i])
		  {
		  HeaderUpdated=true;
			UartPost(Msg_warning,"Logger","Expected CRC32 value of log entry #%d is 0x%08X but get 0x%08X",i,LoggerHdr.LoggerHeader.EntryCRC[i],CRCResult);
			PushDefaultInfoInsideLog(&LogData);
			LoggerHdr.LoggerHeader.IsEntryHasError[i]=false;
		  LoggerHdr.LoggerHeader.EntryCRC[i]=calculateLogEntryCRC32(&LogData);//加入默认配置并计算CRC-32
			if(!WriteLoggerData(&LogData,i))
				UartPost(msg_error,"Logger","Failed to overwrite broken error log entry #%d in ROM.",i);
			faultentry++;
			continue;
			}
	  //log错误类型和header中是否有错误的表不一致，更新header
	  if(LogData.LoggerDateSection.ErrorCode!=Error_None)IsEntryContainError=true;
	  else IsEntryContainError=false; //判断有没有错误
		if(IsEntryContainError!=LoggerHdr.LoggerHeader.IsEntryHasError[i])//错误数据不一致
		  {
			UartPost(Msg_warning,"Logger","error log entry #%d has mismatch information compared to log-header,fixing...",i);
			LoggerHdr.LoggerHeader.IsEntryHasError[i]=IsEntryContainError;//覆写对应的未知
			HeaderUpdated=true;
			faultentry++;
			}
		}
  UartPost(Msg_info,"Logger","Error logger has been started with %d problematic log entry.",faultentry);
	if(HeaderUpdated)//需要更新header
	  {
		UartPost(Msg_info,"Logger","Writing updated log-header...");
		if(!WriteLoggerHeader(&LoggerHdr))
			UartPost(msg_error,"Logger","Failed to overwrite log-header in ROM.");
		}
	}
 //检查不通过
 else
  {
	UartPost(msg_error,"Logger","Logger header has been broken,system will re-initialize log area...");
	if(!ReInitLogArea())
		UartPost(msg_error,"Logger","Failed to re-initialize log area.");
	else
		UartPost(Msg_info,"Logger","Log area has been re-initialized.");
  NVIC_SystemReset();//reboot
	}
 }
//在错误发生时收集错误日志并保存到ROM内
void CollectLoginfo(const char *ErrorArea,INADoutSreDef *BattStat)
 {
 LoggerDataUnion LogData;
 ADCOutTypeDef ADCO;
 //填写基本信息
 strncpy(LogData.LoggerDateSection.ErrorStageText,ErrorArea,32);//错误日志输入
 LogData.LoggerDateSection.SystemPstate=SysPstatebuf.Pstate;
 LogData.LoggerDateSection.ErrorCode=SysPstatebuf.ErrorCode;
 LogData.LoggerDateSection.IsLinearDim=SysPstatebuf.IsLinearDim;
 LogData.LoggerDateSection.CurrentDACVID=SysPstatebuf.CurrentDACVID;
 if(SysPstatebuf.CurrentThrottleLevel<0)SysPstatebuf.CurrentThrottleLevel=0;
 if(SysPstatebuf.CurrentThrottleLevel>100)SysPstatebuf.CurrentThrottleLevel=100;
 LogData.LoggerDateSection.CurrentThrottleLevel=SysPstatebuf.CurrentThrottleLevel;
 //开始填写挡位部分
 LogData.LoggerDateSection.CurrentModeSel.ModeGrpSel=CurMode.ModeGrpSel;
 LogData.LoggerDateSection.CurrentModeSel.RegularGrpMode=CurMode.RegularGrpMode;
 LogData.LoggerDateSection.CurrentModeSel.SpecialGrpMode=CurMode.SpecialGrpMode;
 //开始遥测ADC的部分
 if(!ADC_GetResult(&ADCO))//遥测数据不可用
   {
   LogData.LoggerDateSection.DriverTelem.LEDIf=NAN;
   LogData.LoggerDateSection.DriverTelem.LEDTemp=NAN;
   LogData.LoggerDateSection.DriverTelem.LEDVf=NAN;
   LogData.LoggerDateSection.DriverTelem.SPSTemp=NAN;	  
	 LogData.LoggerDateSection.SPSTMONStatu=SPS_TMON_OK;
   LogData.LoggerDateSection.LEDBaseNTCStatu=LED_NTC_OK; 
	 }
  else //填写数据
	 {
   LogData.LoggerDateSection.DriverTelem.LEDIf=ADCO.LEDIf;
	 LogData.LoggerDateSection.DriverTelem.LEDTemp=ADCO.NTCState==LED_NTC_OK?ADCO.LEDTemp:NAN;
   LogData.LoggerDateSection.DriverTelem.LEDVf=ADCO.LEDVf;
	 LogData.LoggerDateSection.DriverTelem.SPSTemp=ADCO.SPSTMONState==SPS_TMON_OK?ADCO.SPSTemp:NAN;	  
	 LogData.LoggerDateSection.SPSTMONStatu=ADCO.SPSTMONState;
   LogData.LoggerDateSection.LEDBaseNTCStatu=ADCO.NTCState; 
	 }
 //将传入的INA219的部分进行判断
 if(BattStat!=NULL)
   {
	 LogData.LoggerDateSection.IsBattTelemWorking=true;
	 LogData.LoggerDateSection.DriverTelem.BattCurrent=BattStat->BusCurrent;
   LogData.LoggerDateSection.DriverTelem.BattPower=BattStat->BusPower;
   LogData.LoggerDateSection.DriverTelem.BattVoltage=BattStat->BusVolt; 
	 LogData.LoggerDateSection.DriverTelem.BatteryVoltageOVLO=BattStat->BusVolt>CfgFile.VoltageOverTrip?true:false;
	 LogData.LoggerDateSection.DriverTelem.BatteryVoltageUVLO=BattStat->BusVolt<CfgFile.VoltageTrip?true:false;
	 LogData.LoggerDateSection.DriverTelem.BatteryVoltageAlert=BattStat->BusVolt<CfgFile.VoltageAlert?true:false;
	 LogData.LoggerDateSection.DriverTelem.BatteryOverCurrent=BattStat->BusCurrent<CfgFile.OverCurrentTrip?false:true;
	 }
 else
   {
	 LogData.LoggerDateSection.IsBattTelemWorking=false;
	 LogData.LoggerDateSection.DriverTelem.BattCurrent=NAN;
   LogData.LoggerDateSection.DriverTelem.BattPower=NAN;
   LogData.LoggerDateSection.DriverTelem.BattVoltage=NAN;
	 LogData.LoggerDateSection.DriverTelem.BatteryVoltageAlert=false;
	 LogData.LoggerDateSection.DriverTelem.BatteryVoltageOVLO=false;
	 LogData.LoggerDateSection.DriverTelem.BatteryVoltageUVLO=false;
	 LogData.LoggerDateSection.DriverTelem.BatteryOverCurrent=false;
	 }
 //如果运行日志记录器开启，则根据传入的error code增加对应的错误计数
 if(IsRunTimeLoggingEnabled)
   {
	 switch(SysPstatebuf.ErrorCode)
	   {
		 case Error_LED_OverCurrent:
		 case Error_Input_OCP:
			 if(RunLogEntry.Data.DataSec.OCPFaultCount<32766)RunLogEntry.Data.DataSec.OCPFaultCount++; //OCP
		   break;
		 case Error_LED_Open:
		 case Error_LED_Short:
			 if(RunLogEntry.Data.DataSec.LEDOpenShortCount<32766)RunLogEntry.Data.DataSec.LEDOpenShortCount++;//开短路
			 break;
		 case Error_LED_ThermTrip:
			 if(RunLogEntry.Data.DataSec.LEDThermalFaultCount<32766)RunLogEntry.Data.DataSec.LEDThermalFaultCount++;//LED过热
		   break;
		 case Error_SPS_ThermTrip:
			 if(RunLogEntry.Data.DataSec.DriverThermalFaultCount<32766)RunLogEntry.Data.DataSec.DriverThermalFaultCount++;//驱动过热
		   break;
		 default: //其余故障
			 if(RunLogEntry.Data.DataSec.OtherFaultCount<32766)RunLogEntry.Data.DataSec.OtherFaultCount++;
		 }
	 CalcLastLogCRCBeforePO();//重新计算CRC-32  
	 WriteRuntimeLogToROM();//尝试写ROM
	 }
 //开始计算CRC32然后更新头并写入到ROM内
 UpdateHeaderCounter=0;//标记头部需要更新
 LoggerHdr.LoggerHeader.IsEntryHasError[LoggerHdr.LoggerHeader.CurrentLoggerIndex]=true; 
 LoggerHdr.LoggerHeader.EntryCRC[LoggerHdr.LoggerHeader.CurrentLoggerIndex]=calculateLogEntryCRC32(&LogData);//填写CRC信息
 WriteLoggerData(&LogData,LoggerHdr.LoggerHeader.CurrentLoggerIndex);//向EEPROM内填写信息
 LoggerHdr.LoggerHeader.CurrentLoggerIndex=(LoggerHdr.LoggerHeader.CurrentLoggerIndex+1)%MaximumLoggerDepth;//指向下一个logger slot
 }
 
//重新初始化log区域
bool ReInitLogArea(void)
 {
 LoggerDataUnion LogData;
 unsigned int CRCResult;
 int i;
 //写logger基本头部信息
 strncpy(LoggerHdr.LoggerHeader.CheckKey,LoggerHeaderKey,5);
 LoggerHdr.LoggerHeader.CurrentLoggerIndex=0;//从entry0开始写入数据
 //写数据区
 PushDefaultInfoInsideLog(&LogData);
 CRCResult=calculateLogEntryCRC32(&LogData);//加入默认配置并计算CRC-32
 for(i=0;i<MaximumLoggerDepth;i++)
	  {
		LoggerHdr.LoggerHeader.IsEntryHasError[i]=false; 
		LoggerHdr.LoggerHeader.EntryCRC[i]=CRCResult;
	  if(!WriteLoggerData(&LogData,i))return false;
		}
 //写header
 if(!WriteLoggerHeader(&LoggerHdr))return false;
 //操作完毕，返回成功
 return true;		
 }
//将默认的log信息写入到对应的entry
void PushDefaultInfoInsideLog(LoggerDataUnion *LogDataIN)
 {
 //基本信息
 LogDataIN->LoggerDateSection.IsLinearDim=false;
 strncpy(LogDataIN->LoggerDateSection.ErrorStageText,"Unknown",32);
 LogDataIN->LoggerDateSection.SystemPstate=PState_Standby;
 LogDataIN->LoggerDateSection.ErrorCode=Error_None;
 LogDataIN->LoggerDateSection.CurrentDACVID=0.00;
 LogDataIN->LoggerDateSection.CurrentThrottleLevel=0.00;	 
 LogDataIN->LoggerDateSection.SPSTMONStatu=SPS_TMON_OK;
 LogDataIN->LoggerDateSection.LEDBaseNTCStatu=LED_NTC_OK;
 //当前挡位数据组
 LogDataIN->LoggerDateSection.CurrentModeSel.RegularGrpMode=0;
 LogDataIN->LoggerDateSection.CurrentModeSel.SpecialGrpMode=0;
 LogDataIN->LoggerDateSection.CurrentModeSel.ModeGrpSel=ModeGrp_Regular;
 //遥测数据组
 LogDataIN->LoggerDateSection.DriverTelem.BattCurrent=NAN;
 LogDataIN->LoggerDateSection.DriverTelem.BattPower=NAN;
 LogDataIN->LoggerDateSection.DriverTelem.BattVoltage=NAN;
 LogDataIN->LoggerDateSection.DriverTelem.LEDIf=NAN;
 LogDataIN->LoggerDateSection.DriverTelem.LEDTemp=NAN;
 LogDataIN->LoggerDateSection.DriverTelem.LEDVf=NAN;
 LogDataIN->LoggerDateSection.DriverTelem.SPSTemp=NAN;
 }
 
//计算log数据域的CRC32
unsigned int calculateLogEntryCRC32(LoggerDataUnion *LogDataIN)
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
 for(i=0;i<sizeof(LoggerDataUnion);i++)
	 wb(&HT_CRC->DR,LogDataIN->LoggerDatabuf[i]);//将内容写入到CRC寄存器内
 //校验完毕计算结果
 DATACRCResult=HT_CRC->CSR;
 CRC_DeInit(HT_CRC);//清除CRC结果
 CKCU_PeripClockConfig(CLKConfig,DISABLE);//禁用CRC-32时钟节省电力
 return DATACRCResult;
 }	

//从ROM内读取logger的头部区域
bool fetchloggerheader(LoggerHeaderUnion *LogHdrOut)
 {
 //传进来的参数是错的
 if(LogHdrOut==NULL)return false;
 //开始写入
 if(M24C512_PageRead(LogHdrOut->LoggerHeaderbuf,LoggerBase,sizeof(LoggerHeaderUnion)))
		return false;
 //写入完毕，返回true
 return true;
 }

//向ROM内写入logger的头部区域 
bool WriteLoggerHeader(LoggerHeaderUnion *LogHdrIn) 
 {
 //传进来的参数是错的
 if(LogHdrIn==NULL)return false;
 //开始写入
 if(M24C512_PageWrite(LogHdrIn->LoggerHeaderbuf,LoggerBase,sizeof(LoggerHeaderUnion)))
		return false;
 //写入完毕，返回true
 return true;
 }

//读取logger的数据域
bool FetchLoggerData(LoggerDataUnion *LogDataOut,int LogEntryNum)
 {
 //传进来的参数是错的
 if(LogDataOut==NULL||LogEntryNum<0||LogEntryNum>MaximumLoggerDepth-1)return false;
 //开始读取
 if(M24C512_PageRead(LogDataOut->LoggerDatabuf,LoggerDatabase+(LogEntryNum*sizeof(LoggerDataUnion)),sizeof(LoggerDataUnion)))
	 return false;
 //读取完毕，返回true
 return true;
 }

//将logger的数据域写入到ROM
bool WriteLoggerData(LoggerDataUnion *LogDataIn,int LogEntryNum)
 {
 //传进来的参数是错的
 if(LogDataIn==NULL||LogEntryNum<0||LogEntryNum>MaximumLoggerDepth-1)return false;
 //开始写数据
 if(M24C512_PageWrite(LogDataIn->LoggerDatabuf,LoggerDatabase+(LogEntryNum*sizeof(LoggerDataUnion)),sizeof(LoggerDataUnion)))
	 return false;
 //写入完毕，返回true
 return true;
 }
