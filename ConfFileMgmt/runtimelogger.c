#include <string.h>
#include "console.h"
#include "cfgfile.h"
#include "delay.h"
#include "modelogic.h"
#include "ADC.h"
#include "LinearDIM.h"
#include "runtimelogger.h"
#include <math.h>

//函数声明
float fmaxf(float x,float y);
float fminf(float x,float y);

//静态变量
static char RuntimeAverageCount=0;
static char LEDRuntimeAverageCount=0;
bool IsRunTimeLoggingEnabled;
static float AverageBuf[7]={0};
static float MaxEfficiencyCalcBuf[5]={0};//效率计算缓存
RunLogEntryStrDef RunLogEntry;

//外部变量
extern float UsedCapacity;
extern unsigned char PSUState;

/*******************************************
在驱动的库仑计运行期间计算不被INA219部分所测
量的模块的本底电流
*******************************************/
float CalcDriverIq(void)
 {
 float Iq=17; //单片机本底17mA
 if(GPIO_ReadOutBit(AUXV33_IOG,AUXV33_IOP))Iq+=5; //辅助3V3DCDC送电，电流+5mA
 if(GPIO_ReadOutBit(BUCKSEL_IOG,BUCKSEL_IOP)&&GPIO_ReadOutBit(ToggleFlash_IOG,ToggleFlash_IOP))Iq+=11; //主BUCK送电，加上LT3741的消耗
 if(GPIO_ReadOutBit(AUXV33_IOG,AUXV33_IOP)&&!GPIO_ReadOutBit(BUCKSEL_IOG,BUCKSEL_IOP))Iq+=3; //3V3DCDC送电且bucksel=0，加上LT3935的静态消耗
 //计算结束，返回Iq
 return Iq;
 }
/*******************************************
在驱动正常运行时，读取LED和电池信息并记录到
RAM空间内的记录函数。
*******************************************/
void RunTimeDataLogging(void)
 {
 ADCOutTypeDef ADCO;
 double Buf;
 float EffCalcBuf;
 int i;
 //logger没有激活或者当前手电筒不在运行，退出 
 if(SysPstatebuf.Pstate!=PState_LEDOn&&SysPstatebuf.Pstate!=PState_LEDOnNonHold)return;//LED没开启    
 //令ADC采样参数
 if(!ADC_GetResult(&ADCO))
    {
		//ADC转换失败,这是严重故障,立即写log并停止驱动运行
		RunTimeErrorReportHandler(Error_ADC_Logic);
		return;
	  }		
 //求LED和MOSFET温度的平均值
 if(RuntimeAverageCount==0)//第一次，载入旧数据
   {
	 AverageBuf[3]=RunLogEntry.Data.DataSec.AverageLEDTemp==NAN?0:RunLogEntry.Data.DataSec.AverageLEDTemp; //LED温度测量
	 AverageBuf[4]=RunLogEntry.Data.DataSec.AverageSPSTemp; //MOS温度
	 RuntimeAverageCount++;
	 }
 else	if(RuntimeAverageCount==5)//平均完毕，输出结果后准备下一轮计算
   {
	 RunLogEntry.Data.DataSec.AverageLEDTemp=AverageBuf[3]==NAN?NAN:AverageBuf[3]/(float)5; //LED温度，如果等于NAN则表示没数据
	 RunLogEntry.Data.DataSec.AverageSPSTemp=AverageBuf[4]/(float)5; //SPS温度
	 for(i=3;i<5;i++)AverageBuf[i]=0;
	 RuntimeAverageCount=0;
	 }
 else //累加阶段，累计实测数据
   {
	 if(ADCO.NTCState==LED_NTC_OK)AverageBuf[3]+=ADCO.LEDTemp; 
	 else AverageBuf[3]=NAN;
	 if(ADCO.SPSTMONState==SPS_TMON_OK)AverageBuf[4]+=ADCO.SPSTemp;
	 else AverageBuf[4]+=RunLogEntry.Data.DataSec.AverageSPSTemp;  //如果在本次平均的时候温度数据不可用则使用老数据
	 RuntimeAverageCount++;
	 }
 if(!IsRunTimeLoggingEnabled)return;//logger被禁止，不需要采集后面的日志数据只需要采集温度信息保障温控系统正常运作即可。
 //开始处理最大值最小值类的采样
 if(ADCO.NTCState==LED_NTC_OK) //LED温度
    {
    if(RunLogEntry.Data.DataSec.MaximumLEDTemp==NAN)RunLogEntry.Data.DataSec.MaximumLEDTemp=-127;
	  RunLogEntry.Data.DataSec.MaximumLEDTemp=fmaxf(ADCO.LEDTemp,RunLogEntry.Data.DataSec.MaximumLEDTemp);//NTC正常，正常获取温度
		}
 else RunLogEntry.Data.DataSec.MaximumLEDTemp=NAN;//温度为非法数值
 if(SysPstatebuf.ToggledFlash&&ADCO.SPSTMONState==SPS_TMON_OK) //LED开启且SPS温度反馈OK的情况下获取温度
	  RunLogEntry.Data.DataSec.MaximumSPSTemp=fmaxf(ADCO.SPSTemp,RunLogEntry.Data.DataSec.MaximumSPSTemp);//驱动MOSFET(SPS)的温度		
 RunLogEntry.Data.DataSec.MaximumLEDIf=fmaxf(ADCO.LEDIf,RunLogEntry.Data.DataSec.MaximumLEDIf);
 RunLogEntry.Data.DataSec.MaximumLEDVf=fmaxf(ADCO.LEDVf,RunLogEntry.Data.DataSec.MaximumLEDVf); //LED Vf If
 RunLogEntry.Data.DataSec.MinimumBatteryVoltage=fminf(RunTimeBattTelemResult.BusVolt,RunLogEntry.Data.DataSec.MinimumBatteryVoltage);
 RunLogEntry.Data.DataSec.MaximumBatteryVoltage=fmaxf(RunTimeBattTelemResult.BusVolt,RunLogEntry.Data.DataSec.MaximumBatteryVoltage); //最大和最小电池电压
 RunLogEntry.Data.DataSec.MaximumBatteryCurrent=fmaxf(RunTimeBattTelemResult.BusCurrent,RunLogEntry.Data.DataSec.MaximumBatteryCurrent);
 RunLogEntry.Data.DataSec.MaximumBatteryPower=fmaxf(RunTimeBattTelemResult.BusPower,RunLogEntry.Data.DataSec.MaximumBatteryPower); //最大电池电流和功率
 //实现LED运行时间和库仑计积分的模块 
 if(SysPstatebuf.ToggledFlash)RunLogEntry.Data.DataSec.LEDRunTime+=0.125;//如果LED激活，则运行时间每次加1/8秒
 Buf=(double)RunTimeBattTelemResult.BusCurrent*(double)1000;//将A转换为mA方便积分
 Buf+=CalcDriverIq();//加上驱动未被INA219所测量的积分模块
 Buf*=0.125;//将mA转换为mAS(每秒的毫安数，这里乘以0.125是因为每秒钟会积分8次)
 Buf/=(double)(60*60);//将mAS转换为mAH累加到缓冲区内
 UsedCapacity+=(float)Buf; //已用容量加上本次的结果
 RunLogEntry.Data.DataSec.TotalBatteryCapDischarged+=Buf;//加上本次放电的毫安数
 //LED和电池电压和电流的平均值数据积分模块
 if(LEDRuntimeAverageCount==0) //载入旧数据
   {
	 AverageBuf[0]=RunLogEntry.Data.DataSec.AverageBatteryCurrent;
	 AverageBuf[1]=RunLogEntry.Data.DataSec.AverageBatteryPower;
	 AverageBuf[2]=RunLogEntry.Data.DataSec.AverageBatteryVoltage;
	 AverageBuf[5]=RunLogEntry.Data.DataSec.AverageLEDVf;
   AverageBuf[6]=RunLogEntry.Data.DataSec.AverageLEDIf;
	 LEDRuntimeAverageCount++; 
	 }
 else if(LEDRuntimeAverageCount==5)//平均完毕
   {
	 //填写平均的电池电量消耗
	 RunLogEntry.Data.DataSec.AverageBatteryCurrent=AverageBuf[0]/(float)5;
	 RunLogEntry.Data.DataSec.AverageBatteryVoltage=AverageBuf[2]/(float)5;
	 RunLogEntry.Data.DataSec.AverageBatteryPower=AverageBuf[1]/(float)5; //计算电池功耗
	 //填写平均的LEDVf If
	 RunLogEntry.Data.DataSec.AverageLEDVf=AverageBuf[5]/(float)5;
   RunLogEntry.Data.DataSec.AverageLEDIf=AverageBuf[6]/(float)5;
	 for(i=0;i<7;i++)//清空平均缓存
		 {
		 AverageBuf[i]=0;
		 if(i==2)i=4; //到最后一组数据，跳到5
	   }
	 LEDRuntimeAverageCount=0;
	 //驱动的平均效率计算
	 EffCalcBuf=RunLogEntry.Data.DataSec.AverageLEDVf*RunLogEntry.Data.DataSec.AverageLEDIf;//加入输出功率
   EffCalcBuf/=RunLogEntry.Data.DataSec.AverageBatteryPower;//除以输入功率的平均值得到效率
   EffCalcBuf*=(float)100;//将算出的值*100得到百分比
	 if(EffCalcBuf>99)EffCalcBuf=101; //设置为无效值标记出错
	 if(EffCalcBuf<5)EffCalcBuf=5;//算出的效率值进行限幅确保大于5
   RunLogEntry.Data.DataSec.AverageDriverEfficiency=EffCalcBuf;
   //驱动的峰值效率计算 
   if(RunLogEntry.Data.DataSec.AverageDriverEfficiency!=101)
	    {		 
		  for(i=4;i>0;i--)MaxEfficiencyCalcBuf[i]=MaxEfficiencyCalcBuf[i-1];//搬运数据
      MaxEfficiencyCalcBuf[0]=RunLogEntry.Data.DataSec.AverageDriverEfficiency;//将刚刚算出的平均效率加入到计算区域内 
			EffCalcBuf=-10; //将效率值设置为负数来方便找到最高的效率点
      for(i=0;i<5;i++)EffCalcBuf=fmaxf(MaxEfficiencyCalcBuf[i],EffCalcBuf);//取一段时间内的最大值
      RunLogEntry.Data.DataSec.MaximumEfficiency=EffCalcBuf;//峰值效率
			}
   else RunLogEntry.Data.DataSec.MaximumEfficiency=101;//标记效率计算出错
	 }
 else if(SysPstatebuf.ToggledFlash&&SysPstatebuf.TargetCurrent>0)//LED点亮运行时累加电压和电流数据进去
   {
	 AverageBuf[0]+=RunTimeBattTelemResult.BusCurrent;	 
	 AverageBuf[2]+=RunTimeBattTelemResult.BusVolt;//填写电池电压电流
	 AverageBuf[1]+=RunTimeBattTelemResult.BusPower; //将平均功率写进去
	 AverageBuf[5]+=ADCO.LEDVf;
	 AverageBuf[6]+=ADCO.LEDIf;
	 LEDRuntimeAverageCount++; 
	 }
 //填写降档等级
 RunLogEntry.Data.DataSec.ThermalStepDownValue=SysPstatebuf.CurrentThrottleLevel;
 RunLogEntry.Data.DataSec.MaximumThermalStepDown=fmaxf(SysPstatebuf.CurrentThrottleLevel,RunLogEntry.Data.DataSec.MaximumThermalStepDown);
 }
/*******************************************
计算传入数据的CRC32校验和用以确认是否要写log
区域等等。
输入：遥测数据的union
输出：该组数据的CRC32校验和
********************************************/
unsigned int CalcRunLogCRC32(RunLogDataUnionDef *DIN)
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
 for(i=0;i<sizeof(RunLogDataUnionDef);i++)
	 wb(&HT_CRC->DR,DIN->DataCbuf[i]);//将内容写入到CRC寄存器内
 //校验完毕计算结果
 DATACRCResult=HT_CRC->CSR;
 CRC_DeInit(HT_CRC);//清除CRC结果
 CKCU_PeripClockConfig(CLKConfig,DISABLE);//禁用CRC-32时钟节省电力
 return DATACRCResult;
}	
/*******************************************
在手电筒转换为运行状态的自检前，计算目前数据
的CRC32并更新结构体内上组数据记录的CRC校验和
方便对比LOG是否被更新
********************************************/ 
void CalcLastLogCRCBeforePO(void)
  {
	//计算CRC并填写结构体
	RunLogEntry.LastDataCRC=CalcRunLogCRC32(&RunLogEntry.Data);
	}

/*******************************************
强制执行将运行日志写入到ROM的动作。这个函数
主要给锁定模式更新数据用
*******************************************/
void ForceWriteRuntimelog(void)
  {
	signed char SelfIncCode,OldCode;
	//如果用户关闭log功能,则不写入
	if(!CfgFile.EnableRunTimeLogging)return;
	//计算新的自增码
	SelfIncCode=RunLogEntry.Data.DataSec.LogIncrementCode;
	OldCode=RunLogEntry.Data.DataSec.LogIncrementCode;
  if(SelfIncCode<0)SelfIncCode--;
  else if(SelfIncCode>0)SelfIncCode++;
	else SelfIncCode=1; //如果自增码位于负数范围，则自增码-1否则加1，对于是0的情况则为1
	if(SelfIncCode<(-RunTimeLoggerDepth))SelfIncCode=1;
	if(SelfIncCode>RunTimeLoggerDepth)SelfIncCode=-1;//如果自增码到达上限则翻转到另一个极性
  RunLogEntry.Data.DataSec.LogIncrementCode=SelfIncCode;//将计算好的自增码写进去
	RunLogEntry.Data.DataSec.TotalLogCount++; //日志写入计数器+1
	//尝试编程
  if(SaveRunLogDataToROM(&RunLogEntry.Data,RunLogEntry.ProgrammedEntry))
	  {
		CalcLastLogCRCBeforePO();  //编程结束后将新的log的CRC-32值替换过去避免重复写入
    RunLogEntry.ProgrammedEntry=(RunLogEntry.ProgrammedEntry+1)%RunTimeLoggerDepth;//编程成功，指向下一个entry，如果达到额定的entry数目则翻转回来  		
		}
	else 
		RunLogEntry.Data.DataSec.LogIncrementCode=OldCode;//编程失败，entry数不增加的同时，还原更改了的自增码
	}
/*******************************************
在手电筒关闭后，我们需要将运行log写入到ROM内
在这期间，我们首先需要验证运行log是否发生变化
如果发生变化，则开始写入。
*******************************************/
void WriteRuntimeLogToROM(void)
  {
  //如果用户关闭log功能，或者CRC-32相同说明运行的log没有发生改变,不需要操作
	if(!CfgFile.EnableRunTimeLogging)return;
	if(RunLogEntry.LastDataCRC==RunLogEntry.CurrentDataCRC)return;
  //开始编程
	ForceWriteRuntimelog();
	}
	
/*******************************************
从读取到的increment-code数组中找出最新记录的
entry。
输入：包含自增code的数组
输出：最新的一组entry所在的位置
********************************************/
int FindLatestEntryViaIncCode(signed char *CodeIN)
  {
	int i;
  //判断数组的第0个元素是正还是负还是0
	if(CodeIN[0]>0)
	  {
		for(i=0;i<RunTimeLoggerDepth-1;i++)//大于0
			{
			/*
								i i+1
			[1 2 3 4 5 6 0 0 0 0 0 ]这种情况.
			6是最新的，后面啥也没有了返回结果	
			*/
			if(CodeIN[i+1]==0)return i;
			/*
								i i+1
			[1 2 3 4 5 6 -5 -4 -3 -2 -1]这种情况.
			6是最新的，后面是旧数据，返回结果	
			*/		
			if(CodeIN[i+1]<0)return i;
			}
		return RunTimeLoggerDepth-1;//找到序列末尾，返回序列末尾的值
		}
	else if(CodeIN[0]<0)
	  {
		for(i=0;i<RunTimeLoggerDepth-1;i++)//小于0
			{
			/*
	                i  i+1
			[-10 -9 -8 -7 -6 6 7 8 9 10]这种情况.
			-6是最新的，后面的是旧数据，返回结果	
			*/
			if(CodeIN[i+1]>0)return i;
			/*
										i  i+1
			[-10 -9 -8 -7 -6 0 0 0 0 0]这种情况.
			6是最新的，后面啥也没有了,返回结果
			*/		
			if(CodeIN[i+1]==0)return i;
			}	
		return RunTimeLoggerDepth-1;//找到序列末尾，返回序列末尾的值
		}
	return 0;//等于0，直接从这里开始
	}
/*******************************************
使用空的内容填充运行日志结构体的数据部分。这
个函数主要是在上电自检时检测到损坏的log entry
以及清空整个运行日志的时候用的。
输入：遥测数据的union
********************************************/
void LogDataSectionInit(RunLogDataUnionDef *DIN)
  {
	int i;
	//恢复基础设置
	DIN->DataSec.AverageBatteryCurrent=0;
	DIN->DataSec.AverageBatteryPower=0;
	DIN->DataSec.AverageBatteryVoltage=0; 
	DIN->DataSec.AverageDriverEfficiency=90;
	DIN->DataSec.MaximumEfficiency=10;
	DIN->DataSec.AverageLEDIf=0;
  DIN->DataSec.AverageLEDTemp=25;
  DIN->DataSec.AverageLEDVf=0;
  DIN->DataSec.AverageSPSTemp=25;
  DIN->DataSec.LEDRunTime=0;
  DIN->DataSec.LogIncrementCode=0;
	strncpy(DIN->DataSec.LogKey,RunTimeLogKey,4);
  DIN->DataSec.MaximumBatteryCurrent=0;
  DIN->DataSec.MaximumBatteryPower=0;
	DIN->DataSec.MaximumBatteryVoltage=0;
	DIN->DataSec.MaximumLEDIf=0;
	DIN->DataSec.MaximumLEDTemp=0;
	DIN->DataSec.MaximumLEDVf=0;
  DIN->DataSec.MaximumSPSTemp=0;
	DIN->DataSec.MinimumBatteryVoltage=CfgFile.VoltageOverTrip;
	DIN->DataSec.TotalBatteryCapDischarged=0;
	DIN->DataSec.ThermalStepDownValue=0;
	DIN->DataSec.IsRunlogHasContent=false;
	DIN->DataSec.IsLowQualityBattAlert=false;
	DIN->DataSec.LowVoltageShutDownCount=0;
	DIN->DataSec.DriverThermalFaultCount=0;
	DIN->DataSec.LEDThermalFaultCount=0;
	DIN->DataSec.OCPFaultCount=0;
	DIN->DataSec.OtherFaultCount=0;
	DIN->DataSec.LEDOpenShortCount=0;
	DIN->DataSec.TotalLogCount=0;
	DIN->DataSec.MaximumThermalStepDown=0;
	DIN->DataSec.TotalMomtTurboCount=0;
	DIN->DataSec.IsFlashLightLocked=false;
	DIN->DataSec.BattUsage.DesignedCapacity=3000;
	DIN->DataSec.BattUsage.IsCalibrationDone=false;
  DIN->DataSec.BattUsage.IsLearningEnabled=false;
	//重置无极调光设置
	for(i=0;i<13;i++)
	  {
		DIN->DataSec.RampModeStor[i].RampModeConf=0.00;
		DIN->DataSec.RampModeStor[i].RampModeDirection=false;//默认从0%开始，向上
		}
	}
/*******************************************
将运行日志的log区域清空恢复为初始状态，清除
掉所有的日志内容。
输出:如果成功清除,则返回true,否则返回false
*******************************************/
bool ResetRunTimeLogArea(void)
 {
 int i;
 //复位系统RAM中的数据区域和存储
 LogDataSectionInit(&RunLogEntry.Data);
 RunLogEntry.ProgrammedEntry=0;
 CalcLastLogCRCBeforePO();
 RunLogEntry.CurrentDataCRC=RunLogEntry.LastDataCRC;//计算CRC-32
 //重置RAM内的数据
 for(i=0;i<RunTimeLoggerDepth;i++)
	 if(!SaveRunLogDataToROM(&RunLogEntry.Data,i))return false;
 //操作完毕，返回true
 return true;
 }
/*******************************************
将指定的运行日志的数据域从ROM内指定的entry中
读出并写入到RAM内。

输入：输出遥测数据的union，目标读取的entry
输出:如果成功读取,则返回true,否则返回false
********************************************/
bool LoadRunLogDataFromROM(RunLogDataUnionDef *DataOut,int LogEntryNum)
 {
 //传进来的参数是错的
 if(DataOut==NULL||LogEntryNum<0||LogEntryNum>RunTimeLoggerDepth-1)return false;
 //开始读取
 if(M24C512_PageRead(DataOut->DataCbuf,RunTimeLogBase+(LogEntryNum*sizeof(RunLogDataUnionDef)),sizeof(RunLogDataUnionDef)))
	 return false;
 //读取完毕，返回true
 return true;
 }

/*******************************************
将指定的运行日志的数据域写入到ROM内指定的
entry中。
输入：输出遥测数据的union，目标写入的entry
输出:如果成功读取,则返回true,否则返回false
********************************************/
bool SaveRunLogDataToROM(RunLogDataUnionDef *DataIn,int LogEntryNum)
 {
 //传进来的参数是错的
 if(DataIn==NULL||LogEntryNum<0||LogEntryNum>RunTimeLoggerDepth-1)return false;
 //开始写入
 if(M24C512_PageWrite(DataIn->DataCbuf,RunTimeLogBase+(LogEntryNum*sizeof(RunLogDataUnionDef)),sizeof(RunLogDataUnionDef)))
	 return false;
 //写入完毕，返回true
 return true;
 }
/********************************************
在恢复Log区域的时候，用来验证log区域完整性的
这个函数主要是给Xmodem恢复log时检查log区域
是否完整，再以此决定是否copy到实际的存储区
********************************************/
bool IsRunLogAreaOK(int RunLogAreaOffset)
 {
 int i;
 RunLogDataUnionDef Data;
 for(i=0;i<RunTimeLoggerDepth;i++)
	 {
	 //从ROM内读取数据，失败则返回否
	 if(M24C512_PageRead(Data.DataCbuf,RunLogAreaOffset+(i*sizeof(RunLogDataUnionDef)),sizeof(RunLogDataUnionDef)))
		 return false;
	 //检查log entry
	 if(strncmp(Data.DataSec.LogKey,RunTimeLogKey,4))return false;
	 }
 //检查通过，返回true
 return true;
 }
/********************************************
驱动上电自检时检测整个运行数据区域的自检函数
负责检查并修复损坏的log entry，然后根据entry
内写入的自增码判断哪个entry是最新的，从里面
读取数据
********************************************/
void RunLogModule_POR(void)
 {
 int i,errorlog=0,j;
 RunLogDataUnionDef Data;
 bool IsLogEmpty;
 signed char SelfIncBuf[RunTimeLoggerDepth];
 //日志功能被关闭
 if(!CfgFile.EnableRunTimeLogging)
   {
	 UartPost(Msg_info,"RTLogger","System run-time logger has been disabled by user.");
	 IsRunTimeLoggingEnabled=false;
	 //初始化锁定模式以及库仑计的变量
	 RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone=false;
	 RunLogEntry.Data.DataSec.BattUsage.DesignedCapacity=3000;
	 RunLogEntry.Data.DataSec.BattUsage.IsLearningEnabled=false;
	 RunLogEntry.Data.DataSec.IsFlashLightLocked=false;
	 RunLogEntry.Data.DataSec.TotalLogCount=0;
	 RunLogEntry.Data.DataSec.IsLowVoltageAlert=false;
	 RunLogEntry.Data.DataSec.AverageSPSTemp=20;//初始的平均SPS温度设置为20度
	 RunLogEntry.Data.DataSec.TotalMomtTurboCount=0;//总共turbo次数为0
	 //重置无极调光设置
	 for(i=0;i<13;i++)
	  {
		RunLogEntry.Data.DataSec.RampModeStor[i].RampModeConf=CfgFile.DefaultLevel[i];
		RunLogEntry.Data.DataSec.RampModeStor[i].RampModeDirection=false;//默认从0%开始，向上
		}
	 return;	 
	 }
 //首先我们需要把整个log区域遍历一遍
 for(i=0;i<RunTimeLoggerDepth;i++)
	 {
	 //从ROM内读取数据
	 if(!LoadRunLogDataFromROM(&Data,i))
      {
	    UartPost(Msg_critical,"RTLogger","EEPROM error occurred during verify #%d entry.",i);
	    SelfTestErrorHandler();
	    }
	 //检查log entry(如果发生损坏，则使用默认配置去重写)
	 if(strncmp(Data.DataSec.LogKey,RunTimeLogKey,4))
	   {
		 errorlog++;//标记损坏的log区域数目
		 SelfIncBuf[i]=0;//该处因为已经损坏，读取到的自增码等于0
		 UartPost(Msg_warning,"RTLogger","Find broken runtime log #%d in ROM,overwriting...",i);
		 LogDataSectionInit(&Data);
		 if(!SaveRunLogDataToROM(&Data,i))
			 UartPost(msg_error,"RTLogger","Failed to overwrite runtime log #%d in ROM.",i);
		 continue;
		 }
	 //检查通过的entry，将自增码写入到缓冲区内
	 SelfIncBuf[i]=Data.DataSec.LogIncrementCode;
	 }
 //遍历完毕，查询自增码获得最新的log entry并计算CRC32
 #ifdef FlashLightOS_Init_Mode
 UartPost(Msg_info,"RTLogger","Check completed,find %d broken runtime log entry.",errorlog);
 #endif
 i=FindLatestEntryViaIncCode(SelfIncBuf);
 if(!LoadRunLogDataFromROM(&RunLogEntry.Data,i))//从ROM内读取选择的Entry作为目前数据的内容
    {
	  UartPost(Msg_critical,"RTLogger","Failed to load runtime log section #%d.",i);
	  SelfTestErrorHandler();
	  }
 IsLogEmpty=true;
 for(j=0;j<RunTimeLoggerDepth;j++)if(SelfIncBuf[j])IsLogEmpty=false; //检查entry是不是已经空了
 if(IsLogEmpty)RunLogEntry.ProgrammedEntry=0;//如果目前事件日志一组记录都没有，则从0开始记录
 else RunLogEntry.ProgrammedEntry=(i+1)%RunTimeLoggerDepth;//目前entry已经有数据了，从下一条entry开始
 #ifdef FlashLightOS_Init_Mode
 UartPost(Msg_info,"RTLogger","Last Entry is #%d,Logger will log data to entry #%d.",i,RunLogEntry.ProgrammedEntry);
 #endif
 CalcLastLogCRCBeforePO();
 RunLogEntry.Data.DataSec.IsLowVoltageAlert=false;//已经重启了需要重置低压警告不然用户会发现换了电池还是低压警告.
 RunLogEntry.CurrentDataCRC=CalcRunLogCRC32(&RunLogEntry.Data);//计算CRC-32
 UartPost(Msg_info,"RTLogger","Run-Time logger has started,last log data CRC-32 is 0x%08X.",RunLogEntry.CurrentDataCRC);
 IsRunTimeLoggingEnabled=true; //logger启动
 }
