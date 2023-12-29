#include "CurrentReadComp.h"
#include "LEDMgmt.h"
#include "ADC.h"
#include "PWMDIM.h"
#include "AD5693R.h"
#include "delay.h"
#include "modelogic.h"
#include "SideKey.h"

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
 DATACRCResult^=0x3C6AF8E5;
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

//自动进行电流回读的校准
void DoSelfCalibration(void)
 {
 float TargetCurrent,CurrentRatio,ActualCurrent;
 ADCOutTypeDef ADCO;
 int i,j;
 float DACVID;
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
 //开始第一次运行（对调光模块进行校准）
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
 //清零输出，准备第二次输出
 TargetCurrent=1.0;
 AD5693R_SetOutput(0); //将DAC设置为0V输出
 delay_ms(100);
 //开始第二次运行（对电流测量设备进行校准）
 for(i=1;i<51;i++)
	 {
	 //计算DACVID
	 DACVID=TargetCurrent*(float)30;//按照30mV 1A设置输出电流
	 DACVID*=QueueLinearTable(50,TargetCurrent,CompData.CompDataEntry.CompData.Data.DimmingCompThreshold,CompData.CompDataEntry.CompData.Data.DimmingCompValue); //从校准记录里面读取电流补偿值
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
 
//上电初始化的时候加载校准数据
void LoadCalibrationConfig(void)
 {
 bool IsError=false,IsCRCError=false;
 int CRCResult;
 //读取数据，计算CRC32	 
 if(!ReadCompDataFromROM(&CompData))
   {
	 CRCResult=CalcCompCRC32(&CompData.CompDataEntry.CompData); //计算数值
	 IsError=(CRCResult==CompData.CompDataEntry.Checksum)?false:true; //检查是否出错
	 if(IsError)IsCRCError=true;
	 }
 else IsError=true;
 //出现错误，显示校准数据损毁
 if(IsError)
 #ifndef FlashLightOS_Debug_Mode
   {
	 CurrentLEDIndex=6;//EEPROM不工作
	 UartPost(Msg_critical,"IComp","Failed to load Calibration Data,Error:%s",IsCRCError?"CRC32":"EEP");
	 SelfTestErrorHandler();//EEPROM掉线
	 }
 #else
   UartPost(msg_error,"Comp","Failed to load Calibration Data.");
 #endif
 else UartPost(Msg_info,"Comp","Cal. data has loaded.");
 }
