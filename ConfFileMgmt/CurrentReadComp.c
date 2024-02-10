#include "CurrentReadComp.h"
#include "LEDMgmt.h"
#include "ADC.h"
#include "AD5693R.h"
#include "MCP3421.h"
#include "LinearDIM.h"
#include "AD5693R.h"
#include "delay.h"
#include "MCP3421.h"
#include "modelogic.h"
#include "SideKey.h"
#include <string.h>
#include <math.h>

//外部函数
bool CheckLinearTable(int TableSize,float *TableIn);//检查校准数据库的LUT
bool CheckLinearTableValue(int TableSize,float *TableIn);//检查补偿数据库的数据域

//字符串和常量
const char *DBErrorStr[]={"","EEP_Reload","DB_Integrity","DB_Value_Invalid"};

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

//运行主LED并处理校准功能的函数
void RunMainLEDHandler(bool IsMainBuck,int Pass)
 {
 float TargetCurrent,ActualCurrent,CurrentREF;
 float DimValue,IMONValue;
 ADCOutTypeDef ADCO;
 float DACVID,delta,AllowedError,MinimumVID;
 int j=0,fixcount;
 //计算目标电流和DACVID
 UartPrintf("\r\n[Calibration]%s Buck Pass #%d begin.",IsMainBuck?"Main":"Aux",Pass);
 if(!IsMainBuck) //使用副buck
   { 
   TargetCurrent=MinimumLEDCurrent+(((3.9-MinimumLEDCurrent)/50)*(float)Pass); //计算目标电流(电流从最低开始-3.9A)
   DACVID=250+(TargetCurrent*AuxBuckIsensemOhm*10); //LT3935 VIset=250mV(offset)+[额定电流(A)*电流检测电阻数值(mΩ)*电流检测放大器倍数(10X)]
	 AllowedError=TargetCurrent<0.5?0.1:0.02; //副buck在极低电流下允许10%的误差
	 if(DACVID<270)DACVID=270; //幅度限制确保DAC可以启动
	 }
 else //使用主buck
   {	 
	 TargetCurrent=3.9+(((FusedMaxCurrent-3.9)/50)*(float)Pass); //计算目标电流(电流从3.9A开始到极亮电流)
   DACVID=40+(TargetCurrent*30); //主Buck VIset=40mV(offset)+(30mv/A)
	 AllowedError=TargetCurrent<6?0.03:0.015; //主buck在极低电流下允许3%左右的误差
	 }
 //设置AUX PowerPIN
 SetAUXPWR(IsMainBuck);
 AD5693R_SetOutput(DACVID/(float)1000,IsMainBuck?MainBuckAD5693ADDR:AuxBuckAD5693ADDR); //设置电压
 UartPrintf("\r\n[Calibration]Target LED Current=%.2fA.VID has been programmed,initial VID=%.2fmV.",TargetCurrent,DACVID);
 //开始对比VID修正电流误差
 ActualCurrent=0;
 fixcount=0;
 MinimumVID=IsMainBuck?50:250; //最小VID输出，主buck=50mV，辅助buck=250mV
 while(j<6)
    {
    delay_ms(1);
    ADC_GetResult(&ADCO);
    delta=TargetCurrent-ADCO.LEDCalIf; //计算误差
	  if(delta>0)DACVID+=0.01; 
    else if(delta<0)DACVID-=0.01;
	  if(DACVID<MinimumVID)DACVID=MinimumVID;
		if(DACVID>1250)DACVID=1250; //根据误差对DACVID进行调整，然后对调整后的DACVID限幅    
		if(fixcount>=500) //每500mS输出一次
		   {
			 fixcount=0;
			 UartPrintf("\r\n[Calibration]Actual LEDIf=%.2fA,Currently VID=%.2fmV.",ADCO.LEDCalIf,DACVID);
			 }
		else fixcount++;
		AD5693R_SetOutput(DACVID/(float)1000,IsMainBuck?MainBuckAD5693ADDR:AuxBuckAD5693ADDR); //设置电压
    if(fabsf(delta)<=(TargetCurrent*AllowedError))j=j<6?j+1:j;
    else j=j>0?j-1:j; //如果误差修正OK则退出
    }
	 //误差修正完毕，首先填写调光误差
	 UartPrintf("\r\n[Calibration]VID Adjust completed,Adjusted VID=%.2fmV.",DACVID);
	 delta=!IsMainBuck?250+(TargetCurrent*AuxBuckIsensemOhm*10):40+(TargetCurrent*30); //计算原始VID值
	 DimValue=DACVID/delta; //计算出预期的VID偏差之后算出补偿值
	 if(IsMainBuck)
	   {
		 CompData.CompDataEntry.CompData.Data.MainBuckDimmingValue[Pass]=DimValue;
     CompData.CompDataEntry.CompData.Data.MainBuckDimmingThreshold[Pass]=TargetCurrent; //填写目标电流  
		 }
	 else
	   {
		 CompData.CompDataEntry.CompData.Data.AuxBuckDimmingValue[Pass]=DimValue; 
     CompData.CompDataEntry.CompData.Data.AuxBuckDimmingThreshold[Pass]=TargetCurrent; //填写目标电流  		 		 
		 }
	 //然后使能ADC进行电流采集
   ActualCurrent=0;
	 CurrentREF=0;
	 delta=0; //默认电流反馈=0
   for(j=0;j<10;j++)
     {
		 delay_ms(MCP_waitTime);
		 ADC_GetResult(&ADCO);
		 MCP3421_ReadVoltage(&delta);
		 if(!IsMainBuck)ActualCurrent+=ConvertAuxBuckIsense(delta);
		 else ActualCurrent+=ADCO.LEDIfNonComp; //读取电流设置
		 CurrentREF+=ADCO.LEDCalIf;
		 }
	 CurrentREF/=10; 
   ActualCurrent/=10; //对实际测量到的电流和预估电流求平均值		 
	 //填写电流读取补偿值
	 IMONValue=CurrentREF/ActualCurrent; //将实际电流和设置值的差距填进去
	 if(IsMainBuck)
	   {	 
	   CompData.CompDataEntry.CompData.Data.MainBuckIFBValue[Pass]=IMONValue; 
	   CompData.CompDataEntry.CompData.Data.MainBuckIFBThreshold[Pass]=ActualCurrent; //阈值写目标电流
		 }
	 else
	   {
	   CompData.CompDataEntry.CompData.Data.AuxBuckIFBValue[Pass]=IMONValue; 
	   CompData.CompDataEntry.CompData.Data.AuxBuckIFBThreshold[Pass]=ActualCurrent; //阈值写目标电流		 		 
		 }
	 UartPrintf("\r\n[Calibration]%s Buck Pass #%d complete.Target LEDIf=%.2fA,Adjusted Actual LEDIf=%.2fA.",IsMainBuck?"Main":"Aux",Pass,TargetCurrent,CurrentREF);	
	 UartPrintf("\r\n[Calibration]Dimming Comp Value=%.3f,IMON CompValue=%.3f,LEDIf Error=%.2f%%,RAW IMON=%.2fA\r\n",DimValue,IMONValue,((CurrentREF/TargetCurrent)*100)-100,ActualCurrent);	 
 }
//自动进行电流回读的校准
bool IsStartedCalibrationOverCommand=false; 
 
void DoSelfCalibration(void)
 {
 float TargetCurrent,ActualCurrent,EffCalcbuf;
 ADCOutTypeDef ADCO;
 INADoutSreDef BattStat;
 DACInitStrDef DACInitStr;
 int i,j,TargetLogAddr;
 float delta;
 char CSVStrbuf[128]; //字符缓冲
 //按键没有按下或者没有通过命令启动校准，不执行
 if(!getSideKeyLongPressEvent()&&!IsStartedCalibrationOverCommand)return; 
 CurrentLEDIndex=2; 
 LEDMgmt_CallBack();//LED管理器指示绿灯常亮	 
 //上电，开始初始化DAC ADC
 MCP3421_SetChip(AuxBuckIsenADCGain,AuxBuckIsenADCRes,false);
 SetTogglePin(false);
 SetAUXPWR(false); //主副buck都关闭 
 AD5693R_SetOutput(0,MainBuckAD5693ADDR);	 
 AD5693R_SetOutput(0,AuxBuckAD5693ADDR); //设置输出都为0V 	 
 delay_ms(10); //延迟10mS后再送基准电压启动辅助buck
 DACInitStr.DACPState=DAC_Normal_Mode;
 DACInitStr.DACRange=DAC_Output_REF;
 DACInitStr.IsOnchipRefEnabled=true; 	 
 AD5693R_SetChipConfig(&DACInitStr,AuxBuckAD5693ADDR); //启动辅助buck的基准，辅助buck上电
 delay_ms(10); //延迟10mS后再送基准电压启动辅助buck
 SetTogglePin(true);//令辅助buck进入工作状态
 //开始校准
 for(i=0;i<50;i++)RunMainLEDHandler(false,i); //校准副buck
 AD5693R_SetOutput(0,AuxBuckAD5693ADDR); //设置副buck DAC输出为0 	 
 DACInitStr.IsOnchipRefEnabled=false; 	 
 AD5693R_SetChipConfig(&DACInitStr,AuxBuckAD5693ADDR); //关闭辅助buck的基准使辅助buck下电
 delay_ms(10);
 for(i=0;i<50;i++)RunMainLEDHandler(true,i); //校准主buck
 WriteCompDataToROM(); //校准结束，保存数值到EEPROM内
 //关闭主副buck的输出等待一下	 
 SetTogglePin(false);//关闭输出buck
 AD5693R_SetOutput(0,MainBuckAD5693ADDR);	 
 AD5693R_SetOutput(0,AuxBuckAD5693ADDR); //设置输出都为0V 	 
 SetAUXPWR(false); //主副buck都关闭
 delay_ms(500); 
 SetTogglePin(true);//重新拉高
 //准备第二次运行，取70个点采集效率信息
 TargetLogAddr=0xD6EE; //log起始点为D6EE
 memset(CSVStrbuf,0,sizeof(CSVStrbuf));
 snprintf(CSVStrbuf,128,"Input Volt,Input Amp,Input PWR,LED If(Target),LED If(Actual),LED Vf,LED PWR,FET Tj,Efficiency,CRegERR,ImonERR,\r\n");
 M24C512_PageWrite(CSVStrbuf,TargetLogAddr,strlen(CSVStrbuf)); //写入最开始的CSV文件头
 TargetLogAddr+=strlen(CSVStrbuf);//加上长度
 //开始运行
 for(i=0;i<70;i++)
   {
	 //设置电流
   DoLinearDimControl(MinimumLEDCurrent+(((FusedMaxCurrent-MinimumLEDCurrent)/70)*(float)i),true);//控制主buck设置电流		 
	 INA219_SetConvMode(INA219_Cont_Both,INA219ADDR); //启动INA219开始读取信息
	 //读取电流
	 ActualCurrent=0; //清零缓冲区
	 delta=0; //默认电流反馈=0
	 for(j=0;j<10;j++)
	   {
	   delay_ms(MCP_waitTime);
		 MCP3421_ReadVoltage(&delta);
		 SysPstatebuf.AuxBuckCurrent=ConvertAuxBuckIsense(delta); //读取ADC检测到的电流输入并且进行换算
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
   snprintf(&CSVStrbuf[j],128-j,",%.3f%%,%.3f%%,\r\n",((TargetCurrent/ADCO.LEDCalIf)*100)-100,((TargetCurrent/ActualCurrent)*100)-100); //连接了校准器，写入电流检测误差值
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
 SetTogglePin(false);//关闭输出buck
 AD5693R_SetOutput(0,MainBuckAD5693ADDR);	 
 AD5693R_SetOutput(0,AuxBuckAD5693ADDR); //设置输出都为0V 	 
 SetAUXPWR(false); //主副buck都关闭
 DACInitStr.IsOnchipRefEnabled=false; 	 
 AD5693R_SetChipConfig(&DACInitStr,AuxBuckAD5693ADDR); //关闭基准
 MCP3421_SetChip(AuxBuckIsenADCGain,AuxBuckIsenADCRes,true);//开启PD模式
 CurrentLEDIndex=0; 
 IsStartedCalibrationOverCommand=false;
 UARTPuts("\r\n[Calibration]Auto Calibration and test run completed.");
 }	

#endif
//校验校准数据库的CRC32结果以及数据域的内容
CalibrationDBErrorDef CheckCompData(void)
  {
	int CRCResult;
	bool Result[2];
  #ifndef FlashLightOS_Debug_Mode
	int i;
  for(i=0;i<2;i++)
   {
	 //数据匹配，退出
	 CRCResult=CalcCompCRC32(&CompData.CompDataEntry.CompData);
	 if(CRCResult==CompData.CompDataEntry.Checksum)break;
	 //数据不匹配，重新从ROM里面读取,读取失败也直接报错
	 if(ReadCompDataFromROM(&CompData))return Database_EEPROM_Read_Error;
	 }
 if(i==2)return Database_Integrity_Error;
 #else //debug模式下不会自动重新读取RAM的内容而是直接报错
 CRCResult=CalcCompCRC32(&CompData.CompDataEntry.CompData);
 if(CRCResult!=CompData.CompDataEntry.Checksum)return Database_Integrity_Error; 	 	 
 #endif
 //检查阈值是否合法
 Result[0]=CheckLinearTable(50,CompData.CompDataEntry.CompData.Data.AuxBuckIFBThreshold);
 Result[1]=CheckLinearTable(50,CompData.CompDataEntry.CompData.Data.MainBuckIFBThreshold);	 
 if(!Result[0]||!Result[1])return Database_Value_Error;
 Result[0]=CheckLinearTable(50,CompData.CompDataEntry.CompData.Data.AuxBuckDimmingThreshold);
 Result[1]=CheckLinearTable(50,CompData.CompDataEntry.CompData.Data.MainBuckDimmingThreshold);	  
 if(!Result[0]||!Result[1])return Database_Value_Error;
 //检查数据域是否合法
 Result[0]=CheckLinearTableValue(50,CompData.CompDataEntry.CompData.Data.AuxBuckIFBValue);
 Result[1]=CheckLinearTableValue(50,CompData.CompDataEntry.CompData.Data.MainBuckIFBValue);
 if(!Result[0]||!Result[1])return Database_Value_Error;
 Result[0]=CheckLinearTableValue(50,CompData.CompDataEntry.CompData.Data.AuxBuckDimmingValue);
 Result[1]=CheckLinearTableValue(50,CompData.CompDataEntry.CompData.Data.MainBuckDimmingValue);
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
 else //读取失败，报告错误
    {	 
	  IsError=true;
		DBResult=Database_EEPROM_Read_Error;
		}
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
		 CompData.CompDataEntry.CompData.Data.AuxBuckIFBThreshold[i]=MinimumLEDCurrent+(((3.9-MinimumLEDCurrent)/50)*(float)i);
		 CompData.CompDataEntry.CompData.Data.MainBuckIFBThreshold[i]=3.9+(((FusedMaxCurrent-3.9)/50)*(float)i); //加载电流反馈阈值
		 CompData.CompDataEntry.CompData.Data.AuxBuckDimmingThreshold[i]=MinimumLEDCurrent+(((3.9-MinimumLEDCurrent)/50)*(float)i);
		 CompData.CompDataEntry.CompData.Data.MainBuckDimmingThreshold[i]=3.9+(((FusedMaxCurrent-3.9)/50)*(float)i); //加载调光阈值
		 CompData.CompDataEntry.CompData.Data.AuxBuckDimmingValue[i]=1.00;
		 CompData.CompDataEntry.CompData.Data.MainBuckDimmingValue[i]=1.00;
     CompData.CompDataEntry.CompData.Data.AuxBuckIFBValue[i]=1.00;
     CompData.CompDataEntry.CompData.Data.MainBuckIFBValue[i]=1.00; //加载主副buck的调光和电流反馈默认阈值			 
		 }
	 WriteCompDataToROM(); //保存一份到ROM内
	 }
 #endif
 }
