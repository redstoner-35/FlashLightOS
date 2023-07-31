#include "PMBUS.h"
#include "delay.h"
#include "INA219.h"
#include "LEDMgmt.h"
#include "console.h"

//读取数值
bool INA219_GetBusInformation(INADoutSreDef *INADout)
 {
 unsigned int regtemp;
 signed int calctemp;
 int retry=0;
 //等待转换ready
 while(retry<200)
   {
	 delay_ms(1);
	 if(!PMBUS_WordReadWrite(true,false,&regtemp,INADout->TargetSensorADDR,0x02))return false;
	 if(regtemp&0x2)break;
	 retry++;//重试失败次数+1
	 }
 if(retry==200)return false;//重试超时，失败
 //开始获取数据
 if(regtemp&0x1)//数据溢出
	 {
	 #ifdef Internal_Driver_Debug
   UartPost(msg_error,"INA21x","Bus Volt Reg Reports overrange error.");
   #endif
	 INADout->BusCurrent=0;
	 INADout->BusVolt=0;
	 INADout->BusPower=0;//数据无效
	 }
  else
	 {
	 //转换电压数据
	 regtemp>>=3;//右移三位去掉标志位和无效bit
	 INADout->BusVolt=(float)((float)regtemp*(float)BusVoltLSB/(float)1000);//计算出电压
	 //读取电流功率
	 if(!PMBUS_WordReadWrite(true,false,&regtemp,INADout->TargetSensorADDR,0x04))return false;
	 if(regtemp&0x8000)//如果符号位为1说明电流值是负数
			calctemp=regtemp|0xFFFF8000;
		else 
			calctemp=regtemp&0x7FFF;//过滤掉最高位
		INADout->BusCurrent=(float)((float)calctemp*CurrentLSB);//按照设置的LSB计算出电流
		if(!PMBUS_WordReadWrite(true,false,&regtemp,INADout->TargetSensorADDR,0x03))return false;
		INADout->BusPower=(float)((float)regtemp*PowerLSB);//计算出功率，LSB是电流值*20(单位W)			
		} 
 //操作完毕，返回true
 return true;
 }

//初始化INA219
//0表示完成，1表示失败 2表示参数错误
char INA219_INIT(INAinitStrdef * INAConf)
 {
  unsigned int ConfREG=0x0000;//默认寄存器为0
	double shuntResValue;//浮点数，表示检流电阻的阻值
	unsigned int buf;
	unsigned int CalREG;
	char Retrycount=0;
	//首先进行复位
  do{
		 //写数据
		 buf=0x399F;
	   PMBUS_WordReadWrite(true,true,&buf,INAConf->TargetSensorADDR,0);//往寄存器内写默认配置
	   delay_ms(1);
		 //读取数据
		 buf=0;
		 Retrycount++;//重试次数+1
	   if(!PMBUS_WordReadWrite(true,false,&buf,INAConf->TargetSensorADDR,0))continue;//读取失败
		 if(buf==0x399F)break;//验证通过
	  }while(Retrycount<5);	 
  //校验复位是否成功
  if(Retrycount>=5)return 1;//如果尝试了很多次复位还是失败则退出
  //首先写入DC母线的电压满量程
	switch(INAConf->VoltageFullScale)
   {
	  case 16:{ConfREG=ConfREG&0xDFFF;break;}//满量程16V，配置寄存器第13位写0
	  case 32:{ConfREG=ConfREG|0x2000;break;}//满量程32V，配置寄存器第13位写1
	  default:{return 2;}//其他无效值则函数结束
	 }
  //第二步，设置分流器电压的满量程（单位mV）
  switch(INAConf->ShuntVoltageFullScale)
   {
	  case 40:{ConfREG=ConfREG&0xE7FF;break;}//满量程40mV，PGA配置00
	  case 80:{ConfREG=ConfREG|0x0800;break;}//满量程80mV，PGA配置01
	  case 160:{ConfREG=ConfREG|0x1000;break;}//满量程160mV，PGA配置10
		case 320:{ConfREG=ConfREG|0x1800;break;}//满量程320mV，PGA配置11
		default:{return 2;}//其他无效值则函数结束
	 }
  //第三步，设置ADC的采样次数用于得到更精确的结果（同时影响总线和分流器）
	//注：这里为了精度默认就使用了12bit模式
	switch(INAConf->ADCAvrageCount)
	 {
	  case 1:{ConfREG=ConfREG|0x0440;break;}//不进行平均，只采样1次
		case 2:{ConfREG=ConfREG|0x04C8;break;}//采样2次求平均
	  case 4:{ConfREG=ConfREG|0x0550;break;}//采样4次求平均
	  case 8:{ConfREG=ConfREG|0x05D8;break;}//采样8次求平均
		case 16:{ConfREG=ConfREG|0x0660;break;}//采样16次求平均
		case 32:{ConfREG=ConfREG|0x06E8;break;}//采样32次求平均
		case 64:{ConfREG=ConfREG|0x0770;break;}//采样64次求平均
		case 128:{ConfREG=ConfREG|0x07F8;break;}//采样128次求平均
		default:{return 2;}//其他无效值则函数结束
	 }
	 //最后一步，写入模式配置bit
	 ConfREG&=0xFFF8;
	 ConfREG|=(unsigned int)INAConf->ConvMode;
	 //将配置bit写入进配置寄存器并且校验
	 PMBUS_WordReadWrite(true,true,&ConfREG,INAConf->TargetSensorADDR,0x0);//写入
	 delay_ms(1);
	 buf=0;
	 PMBUS_WordReadWrite(true,false,&buf,INAConf->TargetSensorADDR,0);
	 if(buf!=ConfREG)return 1;//校验，如果写进去不对则报错
   //计算出校准寄存器的值
	 shuntResValue=(double)(INAConf->ShuntValue)/(double)500;//将电阻值单位从毫欧转换为欧
	 shuntResValue=(double)0.04096/(CurrentLSB*shuntResValue);//计算出校准寄存器的内容并且强制取整
	 CalREG=(int)shuntResValue;//取整
	 //检查计算好的校验值是否合法
	 #ifdef Internal_Driver_Debug
	 UartPost(Msg_info,"INA21x","Calibration Reg Value:%d",CalREG);
	 #endif
	 if(CalREG&0x8000)return 3;//检查最高位是否有数据，如果有则返回计算值
	 CalREG<<=1;//把全部数据往左移一位，因为第一个bit必须得是空的
	 //把校准数据写进INA219
	 PMBUS_WordReadWrite(true,true,&CalREG,INAConf->TargetSensorADDR,0x5);//写入
	 delay_ms(1);
	 buf=0;
	 PMBUS_WordReadWrite(true,false,&buf,INAConf->TargetSensorADDR,5);
	 if(buf!=CalREG)return 1;//校验，如果写进去不对则报错
   //这些步骤全部完成，函数结束返回0表示完成
	 return 0;
 }
//设置INA219的转换模式
bool INA219_SetConvMode(INA219ModeDef ConvMode,char SensorADDR)
 {
 unsigned int regtemp;
 if(!PMBUS_WordReadWrite(true,false,&regtemp,SensorADDR,0))return false;//读取失败
 regtemp&=0xFFF8;
 regtemp|=(unsigned int)ConvMode;
 if(!PMBUS_WordReadWrite(true,true,&regtemp,SensorADDR,0))return false;//写入失败
 //转换模式更改完毕，返回成功
 return true;	 
 }
//INA219初始化
const char *Change219PstateErrStr="Failed to change P-states of SMBUS Power Gauge to %s"; 
 
void INA219_POR(void)
 {
 INAinitStrdef PORINACfg;
 INADoutSreDef PORINADout;
 char Result;
 UartPost(Msg_info,"INA21x","Configuring SMBUS Power Gauge for Battery...");
 PORINACfg.VoltageFullScale=16; //总线电压满量程16V
 PORINACfg.ADCAvrageCount=64; //平均次数64
 PORINADout.TargetSensorADDR=INA219ADDR;
 PORINACfg.TargetSensorADDR=INA219ADDR; //地址设置为目标值
 PORINACfg.ShuntValue=1.0; //分流电阻阻值设置为1mR
 PORINACfg.ConvMode=INA219_PowerDown;//配置为powerdown模式
 PORINACfg.ShuntVoltageFullScale=80;//分流电阻阻值满量程为80mV
 Result=INA219_INIT(&PORINACfg);
 if(Result)//尝试配置
  {
	UartPost(Msg_critical,"INA21x","Failed to start SMBUS Power Gauge @0x%02X,Error:%d",INA219ADDR,Result);
	CurrentLEDIndex=4;//指示219初始化异常
	#ifndef FlashLightOS_Debug_Mode
	SelfTestErrorHandler();
	#else
	return;
	#endif
	}
 if(!INA219_SetConvMode(INA219_Cont_Both,INA219ADDR))
  {
	UartPost(Msg_critical,"INA21x",(char *)Change219PstateErrStr,"active");
	CurrentLEDIndex=5;//指示219初次测量异常
	#ifndef FlashLightOS_Debug_Mode
	SelfTestErrorHandler();
	#else
	return;
	#endif
	}
 if(!INA219_GetBusInformation(&PORINADout))
  {
	UartPost(Msg_critical,"INA21x","Failed to complete the first measurement of SMBUS Power Gauge.");
	CurrentLEDIndex=5;//指示219初次测量异常
	#ifndef FlashLightOS_Debug_Mode
	SelfTestErrorHandler();
	#else
	return;
	#endif
	}
 if(!INA219_SetConvMode(INA219_PowerDown,INA219ADDR))
  {
	UartPost(Msg_critical,"INA21x",(char *)Change219PstateErrStr,"inactive");
	CurrentLEDIndex=5;//指示219初次测量异常
	#ifndef FlashLightOS_Debug_Mode
	SelfTestErrorHandler();
	#else
	return;
	#endif
	}
 //正常显示
 UartPost(Msg_info,"INA21x","SMBUS Power Gauge has been started without error.");
 UartPost(Msg_info,"INA21x","Current Battery Voltage : %.2fV",PORINADout.BusVolt);
 UartPost(Msg_info,"INA21x","Current Battery Current : %.2fA",PORINADout.BusCurrent);
 UartPost(Msg_info,"INA21x","Current Battery Power : %.2fW\r\n",PORINADout.BusPower);
 }
