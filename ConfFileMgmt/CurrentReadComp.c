#include "CurrentReadComp.h"
#include "LEDMgmt.h"
#include "ADC.h"
#include "PWMDIM.h"
#include "AD5693R.h"
#include "delay.h"
#include "modelogic.h"
#include "SideKey.h"
#include <string.h>
#include <math.h>

//外部函数
bool CheckLinearTable(int TableSize,float *TableIn);//检查校准数据库的LUT
bool CheckLinearTableValue(int TableSize,float *TableIn);//检查补偿数据库的数据域

//字符串和常量
const char *DBErrorStr[]={"","EEP_Reload","DB_Integrity","DB_Value_Invalid"};
const float LowCurrentDimmingTable[]={1.45,1.4899,1.35,1.27,1.18,1.18,1.18,1.18,1.18,1.18,1.18,1.18}; //对于不进行自动校准情况下的修正值

//全局变量
CompDataStorUnion CompData; //全局补偿数据

//计算补偿数据组的CRC校验和
static int CalcCompCRC32(CompDataUnion *CompIN)
 {
 unsigned int DATACRCResult;
 #ifdef EnableSecureStor
 char UIDbuf[4];
 unsigned int UID;
 #endif
 int i;
 CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
 //初始化CRC32      
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,ENABLE);//启用CRC-32时钟  
 CRC_DeInit(HT_CRC);//清除配置
 HT_CRC->SDR = 0x0;//CRC-32 poly: 0x04C11DB7  
 HT_CRC->CR = CRC_32_POLY | CRC_BIT_RVS_WR | CRC_BIT_RVS_SUM | CRC_BYTE_RVS_SUM | CRC_CMPL_SUM;
 //开始校验
 for(i=0;i<sizeof(CompDataUnion);i++)
	 wb(&HT_CRC->DR,CompIN->ByteBuf[i]);//将内容写入到CRC寄存器内
 //校验完毕计算结果
 DATACRCResult=HT_CRC->CSR;
 CRC_DeInit(HT_CRC);//清除CRC结果
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,DISABLE);//禁用CRC-32时钟节省电力
 //返回比对结果
 #ifndef EnableSecureStor
 DATACRCResult^=0xEFD32833;
 #else
 UID=0;
 if(M24C512_ReadUID(UIDbuf,4))return false; //读取失败
 for(i=0;i<4;i++)
  {
	UID<<=8;
	UID|=UIDbuf[i];//将读取到的UID拼接一下
	}
 DATACRCResult^=UID;
 DATACRCResult^=0x356AF8E5;
 #endif
 return DATACRCResult;
 }

//从ROM中读取校准数据
static char ReadCompDataFromROM(CompDataStorUnion *DataOut)
 {
 if(M24C512_PageRead(DataOut->ByteBuf,SelftestLogEnd,sizeof(CompDataStorUnion)))return 1;
 //读取完毕，返回0
 return 0;
 }

#ifdef FlashLightOS_Debug_Mode 
//写校准数据到ROM
char WriteCompDataToROM(void)
 {
 int Result;
 Result=CalcCompCRC32(&CompData.CompDataEntry.CompData); //计算CRC32值
 CompData.CompDataEntry.Checksum=Result; //填写数值
 if(M24C512_PageWrite(CompData.ByteBuf,SelftestLogEnd,sizeof(CompDataStorUnion)))return 1; //尝试保存到ROM中
 //写入完毕，返回0
 return 0;
 }

//用于让固件可以调出极亮，测量电流数据的函数
void DoTurboRunTest(void)
 {
 float DACVID;
 bool resultOK;
 int i;
 ADCOutTypeDef ADCO;
 //按键没有双击或者数据库检查错误，不执行
 if(getSideKeyShortPressCount(false)<2)return;
 if(CheckCompData()!=Database_No_Error)return; 
 CurrentLEDIndex=2; 
 LEDMgmt_CallBack();//LED管理器指示绿灯常亮	 
 //启动LED输出
 SetPWMDuty(100);
 AD5693R_SetOutput(0); //将DAC设置为0V输出，100%占空比
 delay_ms(10);
 SetAUXPWR(true);
 delay_ms(10);
 //计算DACVID让LED以极亮电流运行
 DACVID=FusedMaxCurrent*(float)30;//按照30mV 1A设置输出电流
 DACVID*=QueueLinearTable(50,FusedMaxCurrent,CompData.CompDataEntry.CompData.Data.DimmingCompThreshold,CompData.CompDataEntry.CompData.Data.DimmingCompValue,&resultOK); //从校准记录里面读取电流补偿值
 AD5693R_SetOutput(DACVID/(float)1000); //设置输出电流
 if(resultOK)for(i=0;i<500;i++)
	 {
	 delay_ms(10);
	 ADC_GetResult(&ADCO); //读取数值
	 if(ADCO.SPSTMONState!=SPS_TMON_OK||ADCO.SPSTemp>=CfgFile.MOSFETThermalTripTemp)break; //驱动超温或者SPS报错，直接结束运行
	 }
 //运行结束，关闭输出
 CurrentLEDIndex=0; 
 AD5693R_SetOutput(0); //将DAC设置为0V输出
 SetAUXPWR(false); //关闭辅助电源
 }

//自动调用电流监视器进行输出电流控制校准的工具
void DoDimmingCalibration(void)
 {
 float TargetCurrent,ActualCurrent,DACVID,delta;
 int i,j;
 ADCOutTypeDef ADCO;
 //按键没有三击或者数据库检查错误，不执行
 j=getSideKeyShortPressCount(true);
 if(j!=3&&j!=1)return;
 if(CheckCompData()!=Database_No_Error)return; 
 CurrentLEDIndex=2; 
 //启动LED输出
 SetPWMDuty(100);
 AD5693R_SetOutput(0); //将DAC设置为0V输出，100%占空比
 delay_ms(10);
 SetAUXPWR(true);
 delay_ms(10);
 if(j==1) 
  {
	for(i=0;i<20;i++)
    {
		AD5693R_SetOutput((float)(i*5)/1000); //设置输出电流
		UartPrintf("\r\nCurrent DACVID=%dmV",i*5);
	  delay_Second(1); //延迟1秒
		}		
	AD5693R_SetOutput(0); //将DAC设置为0V输出
  SetAUXPWR(false); //关闭辅助电源
  CurrentLEDIndex=0;  
	return;//结束运行
	}
 //开始第1次运行（校准调光模块和输出VID）
 for(i=0;i<50;i++)
	 {
	 //计算和设置DAC初始VID
	 TargetCurrent=2.0+(((FusedMaxCurrent-2.0)/(float)50)*i); //计算目标电流
	 DACVID=TargetCurrent*(float)30;//按照30mV 1A设置输出电流
	 DACVID+=40; //DACVID加上40mV offset
	 AD5693R_SetOutput(DACVID/(float)1000); //设置输出电流
	 //开始进行电流锁相校准VID输出
	 j=0; 
	 while(j<6)//开始进行循环
	   {
		 delay_ms(1);
		 ADC_GetResult(&ADCO);
	   ActualCurrent=ADCO.LEDIf;
     delta=TargetCurrent-ActualCurrent; //计算误差
		 if(delta>0)DACVID+=0.025; //实际电流比目标值小，增加VID
		 else if(delta<0)DACVID-=0.025;//实际电流比目标值大，减少VID
		 if(DACVID<50)break;
		 AD5693R_SetOutput(DACVID/(float)1000); //设置输出电流
		 if(fabsf(delta)<=(TargetCurrent*(TargetCurrent>4?0.015:0.05)))j=j<6?j+1:j;
		 else j=j>0?j-1:j; //误差如果达标则达标计数+1
		 }
	 //计算DAC电流输出的补偿值
	 CompData.CompDataEntry.CompData.Data.DimmingCompValue[i]=DACVID/((TargetCurrent*30)+40); //填写补偿值
	 CompData.CompDataEntry.CompData.Data.DimmingCompThreshold[i]=TargetCurrent; //当前目标电流 	 
   }
 WriteCompDataToROM(); //保存数据
 //运行结束，关闭LED输出
 AD5693R_SetOutput(0); //将DAC设置为0V输出
 SetAUXPWR(false); //关闭辅助电源
 CurrentLEDIndex=0;  
 }
 
//自动进行电流回读的校准
void DoSelfCalibration(void)
 {
 float TargetCurrent,ActualCurrent,EffCalcbuf;
 ADCOutTypeDef ADCO;
 INADoutSreDef BattStat;
 int i,j,TargetLogAddr;
 float DACVID,delta;
 bool resultOK;
 char CSVStrbuf[128]; //字符缓冲
 //按键没有按下，不执行
 if(!getSideKeyLongPressEvent())return; 
 CurrentLEDIndex=2; 
 LEDMgmt_CallBack();//LED管理器指示绿灯常亮	 
 //启动LED输出
 SetPWMDuty(100);
 AD5693R_SetOutput(0); //将DAC设置为0V输出，100%占空比
 delay_ms(10);
 SetAUXPWR(true);
 //开始第1次运行（校准调光模块和输出VID）
 for(i=0;i<50;i++)
	 {
	 //计算和设置DAC初始VID
	 TargetCurrent=2.0+(((FusedMaxCurrent-2.0)/(float)50)*i); //计算目标电流
	 DACVID=TargetCurrent*(float)30;//按照30mV 1A设置输出电流
	 DACVID+=40; //DACVID加上40mV offset
	 AD5693R_SetOutput(DACVID/(float)1000); //设置输出电流
	 //开始进行电流锁相校准VID输出
	 j=0; 
	 while(j<6)//开始进行循环
	   {
		 delay_ms(1);
		 ADC_GetResult(&ADCO);
	   ActualCurrent=ADCO.LEDCalIf;
     delta=TargetCurrent-ActualCurrent; //计算误差
		 if(delta>0)DACVID+=0.025; //实际电流比目标值小，增加VID
		 else if(delta<0)DACVID-=0.025;//实际电流比目标值大，减少VID
		 AD5693R_SetOutput(DACVID/(float)1000); //设置输出电流
		 if(fabsf(delta)<=(TargetCurrent*(TargetCurrent>4?0.015:0.05)))j=j<6?j+1:j;
		 else j=j>0?j-1:j; //误差如果达标则达标计数+1
		 }
	 //计算DAC电流输出的补偿值
	 CompData.CompDataEntry.CompData.Data.DimmingCompValue[i]=DACVID/((TargetCurrent*30)+40); //填写补偿值
	 CompData.CompDataEntry.CompData.Data.DimmingCompThreshold[i]=TargetCurrent; //当前目标电流 	 
   //再运行一次ADC
	 ActualCurrent=0;
   for(j=0;j<10;j++)
     {
		 delay_ms(5);
		 ADC_GetResult(&ADCO);
		 ActualCurrent+=ADCO.LEDIfNonComp;
		 }
   ActualCurrent/=10;		 
	 //计算输出的补偿值
	 CompData.CompDataEntry.CompData.Data.CurrentCompValue[i+1]=ADCO.LEDCalIf/ActualCurrent; //填写补偿值
	 CompData.CompDataEntry.CompData.Data.CurrentCompThershold[i+1]=ActualCurrent; //当前目标电流
	 UartPrintf("\r\n[Calibration]Pass #%d complete.Actual LEDIf=%.2fA,UnCompIf=%.2fA.",i,ActualCurrent,TargetCurrent);
	 }
 //填写第0组数值然后保存数据
 CompData.CompDataEntry.CompData.Data.CurrentCompValue[0]=23.8196; //补偿值20.8196
 CompData.CompDataEntry.CompData.Data.CurrentCompThershold[0]=1.0; //阈值1.0
 WriteCompDataToROM(); //保存数据
 //准备第三次运行，取70个点采集效率信息
 AD5693R_SetOutput(0); //将DAC设置为0V输出
 TargetLogAddr=0xD6EE; //log起始点为D6EE
 memset(CSVStrbuf,0,sizeof(CSVStrbuf));
 #ifndef Skip_DimmingCalibration
 snprintf(CSVStrbuf,128,"Input Volt,Input Amp,Input PWR,LED If(Target),LED If(Actual),LED Vf,LED PWR,FET Tj,Efficiency,CRegERR,ImonERR,\r\n");
 #else
 snprintf(CSVStrbuf,128,"Input Volt,Input Amp,Input PWR,LED If(Target),LED If(Actual),LED Vf,LED PWR,FET Tj,Efficiency,\r\n"); 
 #endif
 M24C512_PageWrite(CSVStrbuf,TargetLogAddr,strlen(CSVStrbuf)); //写入最开始的CSV文件头
 TargetLogAddr+=strlen(CSVStrbuf);//加上长度
 delay_ms(500);
 //开始运行
 for(i=0;i<70;i++)
   {
	 //计算DACVID
	 TargetCurrent=2.0+(((FusedMaxCurrent-2.0)/(float)70)*i); //计算目标电流
	 DACVID=TargetCurrent*(float)30;//按照30mV 1A设置输出电流
	 DACVID*=QueueLinearTable(50,TargetCurrent,CompData.CompDataEntry.CompData.Data.DimmingCompThreshold,CompData.CompDataEntry.CompData.Data.DimmingCompValue,&resultOK); //从校准记录里面读取电流补偿值
	 if(!resultOK)
	   {
		 INA219_SetConvMode(INA219_PowerDown,INA219ADDR);//关闭INA219
		 AD5693R_SetOutput(0); //将DAC设置为0V输出
     SetAUXPWR(false); //关闭辅助电源
		 CurrentLEDIndex=31; //提示用户校准数据库错误 
		 }		 
	 AD5693R_SetOutput(DACVID/(float)1000); //设置输出电流
	 INA219_SetConvMode(INA219_Cont_Both,INA219ADDR); //启动INA219开始读取信息
	 //读取电流
	 ActualCurrent=0; //清零缓冲区
	 for(j=0;j<10;j++)
	   {
	   delay_ms(15);
	   ADC_GetResult(&ADCO); //读取数值
	   ActualCurrent+=ADCO.LEDIf; //累加
	   }
	 ActualCurrent/=10; //求平均
	 //读取INA219的数据
	 BattStat.TargetSensorADDR=INA219ADDR; //设置地址
	 if(!INA219_GetBusInformation(&BattStat))break;
	 if(!INA219_SetConvMode(INA219_PowerDown,INA219ADDR))break;//关闭INA219
	 //开始commit CSV数据
	 memset(CSVStrbuf,0,sizeof(CSVStrbuf));
	 snprintf(CSVStrbuf,128,"%.2f,%.2f,%.2f,%.2f,",BattStat.BusVolt,BattStat.BusCurrent,BattStat.BusPower,TargetCurrent);
	 j=strlen(CSVStrbuf);
	 EffCalcbuf=100*((ADCO.LEDVf*ActualCurrent)/BattStat.BusPower); //计算效率
	 if(EffCalcbuf<99.5&&EffCalcbuf>2)
      snprintf(&CSVStrbuf[j],128-j,"%.2f,%.2f,%.2f,%.2fC,%.2f%%",ActualCurrent,ADCO.LEDVf,ADCO.LEDVf*ActualCurrent,ADCO.SPSTemp,EffCalcbuf);	//写入电池电压信息	 
	 else
		  snprintf(&CSVStrbuf[j],128-j,"%.2f,%.2f,%.2f,%.2fC,No Info",ActualCurrent,ADCO.LEDVf,ADCO.LEDVf*ActualCurrent,ADCO.SPSTemp);	//写入电池电压信息
	 j=strlen(CSVStrbuf);
   #ifndef Skip_DimmingCalibration
   snprintf(&CSVStrbuf[j],128-j,",%.3f%%,%.3f%%,\r\n",((TargetCurrent/ADCO.LEDCalIf)*100)-100,((TargetCurrent/ActualCurrent)*100)-100); //连接了校准器，写入电流检测误差值
   #else
   snprintf(&CSVStrbuf[j],128-j,",\r\n"); 
   #endif	  
   M24C512_PageWrite(CSVStrbuf,TargetLogAddr,strlen(CSVStrbuf)); 
   TargetLogAddr+=strlen(CSVStrbuf);//加上长度
	 } 	 
 //输出log底部截止的信息
 memset(CSVStrbuf,0,sizeof(CSVStrbuf));
 snprintf(CSVStrbuf,128,"This is an automatically generated log file by FlashLight OS version %d.%d.%d At Debug mode,\r\n",MajorVersion,MinorVersion,HotfixVersion);
 M24C512_PageWrite(CSVStrbuf,TargetLogAddr,strlen(CSVStrbuf)); 
 TargetLogAddr+=strlen(CSVStrbuf);//写入版本信息
 memset(CSVStrbuf,0,sizeof(CSVStrbuf));
 snprintf(CSVStrbuf,128,"CopyRight to redstoner_35 @ 35's Embedded Systems Inc. JAN 2024,\r\n\xFF");
 M24C512_PageWrite(CSVStrbuf,TargetLogAddr,strlen(CSVStrbuf)); //写入版权信息
 //运行结束，关闭LED输出
 INA219_SetConvMode(INA219_PowerDown,INA219ADDR);//关闭INA219
 AD5693R_SetOutput(0); //将DAC设置为0V输出
 SetAUXPWR(false); //关闭辅助电源
 CurrentLEDIndex=0; 
 }	

#endif
//校验校准数据库的CRC32结果以及数据域的内容
CalibrationDBErrorDef CheckCompData(void)
 {
	int i,CRCResult;
	bool Result[2];
  for(i=0;i<2;i++)
   {
	 //数据匹配，退出
	 CRCResult=CalcCompCRC32(&CompData.CompDataEntry.CompData);
	 if(CRCResult==CompData.CompDataEntry.Checksum)break;
	 //数据不匹配，重新从ROM里面读取,读取失败也直接报错
	 if(ReadCompDataFromROM(&CompData))return Database_EEPROM_Read_Error;
	 }
 if(i==2)return Database_Integrity_Error;
 //检查数据域和阈值是否合法
 Result[0]=CheckLinearTable(51,CompData.CompDataEntry.CompData.Data.CurrentCompThershold);
 Result[1]=CheckLinearTable(50,CompData.CompDataEntry.CompData.Data.DimmingCompThreshold); //检查阈值区域
 if(!Result[0]||!Result[1])return Database_Value_Error; 
 Result[0]=CheckLinearTableValue(51,CompData.CompDataEntry.CompData.Data.CurrentCompValue);
 Result[1]=	CheckLinearTableValue(50,CompData.CompDataEntry.CompData.Data.DimmingCompValue); //检查数值域 
 if(!Result[0]||!Result[1])return Database_Value_Error;
 //检查通过
 return Database_No_Error;
 }

//上电初始化的时候加载校准数据
void LoadCalibrationConfig(void)
 {
 bool IsError=false;
 CalibrationDBErrorDef DBResult;
 #ifdef FlashLightOS_Debug_Mode
 int i; 
 #endif
 //读取数据，如果数据读取成功则计算CRC32检查数据	 
 if(!ReadCompDataFromROM(&CompData))
    {
    DBResult=CheckCompData();//检查数据库
    IsError=(DBResult==Database_No_Error)?false:true;
		}
 else IsError=true;
 //出现错误，显示校准数据损毁
 if(IsError)
 #ifndef FlashLightOS_Debug_Mode
   {
	 CurrentLEDIndex=31;//指示校准数据库异常
	 UartPost(Msg_critical,"Comp","Compensation DB error detected,Error code:%s",DBErrorStr[(int)DBResult]);
	 SelfTestErrorHandler();//EEPROM掉线
	 }
 #else
   {
   UartPost(msg_error,"Comp","Compensation DB error detected,Error code:%s",DBErrorStr[(int)DBResult]);
	 for(i=0;i<50;i++)//加载补偿数据库的默认值，否则ADC自检会出意外。
		 {
		 CompData.CompDataEntry.CompData.Data.CurrentCompThershold[i+1]=2.00+(((FusedMaxCurrent-2)/50)*(float)i);
		 CompData.CompDataEntry.CompData.Data.CurrentCompValue[i+1]=1.00; //覆盖调光表数值
		 CompData.CompDataEntry.CompData.Data.DimmingCompThreshold[i]=2.00+(((FusedMaxCurrent-2)/50)*(float)i);
		 CompData.CompDataEntry.CompData.Data.DimmingCompValue[i]=1.00; //覆盖调光表
		 }
   CompData.CompDataEntry.CompData.Data.CurrentCompValue[0]=0.8196; //补偿值0.8196
   CompData.CompDataEntry.CompData.Data.CurrentCompThershold[0]=1.0; //阈值1.0
	 }
 #endif
 }
