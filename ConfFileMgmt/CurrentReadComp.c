#include "CurrentReadComp.h"
#include "LEDMgmt.h"
#include "ADC.h"
#include "PWMDIM.h"
#include "AD5693R.h"
#include "delay.h"
#include "modelogic.h"
#include "SideKey.h"

//外部函数
bool CheckLinearTable(int TableSize,float *TableIn);//检查校准数据库的LUT
bool CheckLinearTableValue(int TableSize,float *TableIn);//检查补偿数据库的数据域

//字符串
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

//用于让固件可以调出极亮，测量电流数据的函数
void DoTurboRunTest(void)
 {
 float DACVID;
 bool resultOK;
 int i;
 ADCOutTypeDef ADCO;
 //按键没有双击或者数据库检查错误，不执行
 if(getSideKeyShortPressCount(true)<2)return;
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
 
//自动进行电流回读的校准
void DoSelfCalibration(void)
 {
 float TargetCurrent,CurrentRatio,ActualCurrent;
 ADCOutTypeDef ADCO;
 int i,j;
 float DACVID;
 #ifndef Skip_DimmingCalibration
 bool resultOK;
 int DimCalibrationFailCount=0;
 #endif
 //按键没有按下，不执行
 if(!getSideKeyLongPressEvent())return; 
 CurrentLEDIndex=2; 
 LEDMgmt_CallBack();//LED管理器指示绿灯常亮	 
 //设置目标电流和测试参数	 
 TargetCurrent=1.0;
 CurrentRatio=(FusedMaxCurrent-1.0)/(float)50; //分为50份
 //启动LED输出
 SetPWMDuty(100);
 AD5693R_SetOutput(0); //将DAC设置为0V输出，100%占空比
 delay_ms(10);
 SetAUXPWR(true);
 #ifndef Skip_DimmingCalibration
 while(DimCalibrationFailCount<5)
  {
  //开始第一组运行（对调光模块进行校准）
  for(i=0;i<50;i++)
	 {
	 //计算DACVID
	 DACVID=TargetCurrent*(float)30;//按照30mV 1A设置输出电流
	 AD5693R_SetOutput(DACVID/(float)1000); //设置输出电流
	 //读取电流
	 ActualCurrent=0; //清零缓冲区
	 for(j=0;j<10;j++)
	   {
	   delay_ms(5);
	   ADC_GetResult(&ADCO); //读取数值
	   ActualCurrent+=ADCO.LEDCalIf; //累加
	   }
	 ActualCurrent/=10; //求平均
	 //计算补偿值
	 CompData.CompDataEntry.CompData.Data.DimmingCompValue[i]=TargetCurrent/ActualCurrent; //填写补偿值
	 CompData.CompDataEntry.CompData.Data.DimmingCompThreshold[i]=TargetCurrent; //当前目标电流 
	 //电流加一点，开始继续计算
	 TargetCurrent+=CurrentRatio;	 	 
	 }
  //清零输出，准备进行误差验证
  TargetCurrent=1.0;
  AD5693R_SetOutput(0); //将DAC设置为0V输出
  //开始第二组运行（检查调光模块的电流误差范围是否达标）
  for(i=0;i<50;i++)
	 {
	 //计算DACVID
	 DACVID=TargetCurrent*(float)30;//按照30mV 1A设置输出电流
	 DACVID*=QueueLinearTable(50,TargetCurrent,CompData.CompDataEntry.CompData.Data.DimmingCompThreshold,CompData.CompDataEntry.CompData.Data.DimmingCompValue,&resultOK); //从校准记录里面读取电流补偿值
	 AD5693R_SetOutput(DACVID/(float)1000); //设置输出电流
	 //读取电流
	 ActualCurrent=0; //清零缓冲区
	 for(j=0;j<10;j++)
	   {
	   delay_ms(5);
	   ADC_GetResult(&ADCO); //读取数值
	   ActualCurrent+=ADCO.LEDCalIf; //累加
	   }
	 ActualCurrent/=10; //求平均 
   if(ActualCurrent>TargetCurrent*1.02||ActualCurrent<TargetCurrent*0.98)
	    {
			TargetCurrent=1.0;
      AD5693R_SetOutput(0); //将DAC设置为0V输出
			DimCalibrationFailCount++; //校准失败次数+1
			continue; //误差超范围，重新校准
		  }
	 //电流加一点，开始继续检查
	 TargetCurrent+=CurrentRatio;
	 }
	//50个点的误差范围都小于标称值，校准结束
  if(i==50)break;
	}
 //清零输出，准备第二次输出
 TargetCurrent=1.0;
 AD5693R_SetOutput(0); //将DAC设置为0V输出
 if(DimCalibrationFailCount==5)
   {
	 AD5693R_SetOutput(0); 
   SetAUXPWR(false);
	 CurrentLEDIndex=2; //输出电流误差超范围，按键红灯常亮提示出现错误
	 return;
	 }
 delay_ms(100);
 #endif
 //开始第二次运行（对电流测量设备进行校准）
 for(i=1;i<51;i++)
	 {
	 //计算DACVID
	 DACVID=TargetCurrent*(float)30;//按照30mV 1A设置输出电流
	 #ifndef Skip_DimmingCalibration
	 DACVID*=QueueLinearTable(50,TargetCurrent,CompData.CompDataEntry.CompData.Data.DimmingCompThreshold,CompData.CompDataEntry.CompData.Data.DimmingCompValue,&resultOK); //从校准记录里面读取电流补偿值
	 if(!resultOK)
	   {
		 AD5693R_SetOutput(0); //将DAC设置为0V输出
     SetAUXPWR(false); //关闭辅助电源
		 CurrentLEDIndex=31; //提示用户校准数据库错误 
		 }		 
	 #endif
	 AD5693R_SetOutput(DACVID/(float)1000); //设置输出电流
	 //读取电流
	 ActualCurrent=0; //清零缓冲区
	 for(j=0;j<10;j++)
	   {
	   delay_ms(5);
	   ADC_GetResult(&ADCO); //读取数值
	   ActualCurrent+=ADCO.LEDIfNonComp; //累加
	   }
	 ActualCurrent/=10; //求平均
	 //电流数值出现异常，退出
   if(ActualCurrent<0.03||ActualCurrent>65)		 
	   {
		 AD5693R_SetOutput(0); //将DAC设置为0V输出
     SetAUXPWR(false); //关闭辅助电源
		 CurrentLEDIndex=13; //显示ADC错误
		 return;
		 }
	 //计算补偿值
	 CompData.CompDataEntry.CompData.Data.CurrentCompValue[i]=ActualCurrent/TargetCurrent; //填写补偿值
	 CompData.CompDataEntry.CompData.Data.CurrentCompThershold[i]=TargetCurrent; //当前目标电流
	 //电流加一点，开始继续计算
	 TargetCurrent+=CurrentRatio;
	 }
 //运行结束，关闭LED输出
 AD5693R_SetOutput(0); //将DAC设置为0V输出
 SetAUXPWR(false); //关闭辅助电源
 CurrentLEDIndex=0; 
 //存储数据
 CompData.CompDataEntry.CompData.Data.CurrentCompValue[0]=0.8196; //补偿值0.8196
 CompData.CompDataEntry.CompData.Data.CurrentCompThershold[0]=0.7; //阈值0.7
 WriteCompDataToROM(); //保存数据
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
	 for(i=0;i<50;i++)
		 {
		 CompData.CompDataEntry.CompData.Data.DimmingCompThreshold[i]=1.00+(((FusedMaxCurrent-1)/50)*(float)i);
		 CompData.CompDataEntry.CompData.Data.DimmingCompValue[i]=1.00; //覆盖调光表
		 }
	 }
 #endif
 }
