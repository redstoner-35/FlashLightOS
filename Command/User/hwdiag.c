#include "console.h"
#include "I2C.h"
#include "ADC.h"
#include "INA219.h"
#include "AD5693R.h"
#include "MCP3421.h"
#include "delay.h"

#ifdef FlashLightOS_Debug_Mode
//变量
extern bool IsTimeExceeded;

//处理函数
void hwdiaghandler(void)
  {
	DACInitStrDef DACTest;
  ADCOutTypeDef IntADTest;
	INAinitStrdef PORINACfg;
	INADoutSreDef PORINADout;
	float buf;
  char result[384];
	int i;
	//开始测试	
	UARTPuts("\r\n---------- FlashLightOS 高级硬件测试 ----------");
  UARTPuts("\r\nPart1 DAC部分:");  
	DACTest.DACPState=DAC_Normal_Mode;
	DACTest.DACRange=DAC_Output_REF;
  DACTest.IsOnchipRefEnabled=true;
	UartPrintf("\r\n月光DLC小板DAC  连接%s",AD5693R_Detect(AuxBuckAD5693ADDR)?"成功":"失败");
	UartPrintf("\r\n月光DLC小板DAC初始化  %s",AD5693R_SetChipConfig(&DACTest,AuxBuckAD5693ADDR)?"成功":"失败");
	UartPrintf("\r\n月光DLC小板DAC设置电压  %s",AD5693R_SetOutput(1.35,AuxBuckAD5693ADDR)?"成功":"失败");
	UartPrintf("\r\n功率板DAC  连接%s",AD5693R_Detect(MainBuckAD5693ADDR)?"成功":"失败");
	UartPrintf("\r\n功率板DAC初始化  %s",AD5693R_SetChipConfig(&DACTest,MainBuckAD5693ADDR)?"成功":"失败");
	UartPrintf("\r\n功率板DAC设置电压  %s",AD5693R_SetOutput(1.35,MainBuckAD5693ADDR)?"成功":"失败");\
  //片内ADC		
	UARTPuts("\r\n\r\nPart2 片内ADC部分:");  
	UartPrintf("\r\n片内ADC转换数据  %s",ADC_GetResult(&IntADTest)?"成功":"失败");
	UartPrintf("\r\n当前LED电流读数(补偿后) : %.2fA",IntADTest.LEDIf);
	UartPrintf("\r\n当前LED电流读数(基准模块) : %.2fA",IntADTest.LEDCalIf);
	UartPrintf("\r\n当前LED电流读数(补偿前) : %.2fA",IntADTest.LEDIfNonComp);
  //月光小板ADC
	UARTPuts("\r\n\r\nPart3 月光小板ADC部分:");
  UartPrintf("\r\n初始化  %s",MCP3421_SetChip(PGA_Gain2to1,Sample_14bit_60SPS,false)?"成功":"失败"); 		
	delay_ms(50);
	buf=3072;
	UartPrintf("\r\n尝试读取电压  %s",MCP3421_ReadVoltage(&buf)?"成功":"失败");
	if(buf!=3072)
		UartPrintf("\r\nADC结果 : %.3fV",buf);
	else
		UARTPuts("\r\n错误:月光DLC小板ADC结果无效.");
	//EEPROM
	UARTPuts("\r\n\r\nPart4 EEPROM部分:");
	UartPrintf("\r\n读取测试   %s",M24C512_PageRead(result,0,384)?"失败":"成功");
	UartPrintf("\r\n写入测试   %s",M24C512_PageWrite(result,0,384)?"失败":"成功");
	//电池功率级
	UARTPuts("\r\n\r\nPart5 电池功率级部分:");
	PORINACfg.VoltageFullScale=16; //总线电压满量程16V
  PORINACfg.ADCAvrageCount=64; //平均次数64
  PORINACfg.TargetSensorADDR=INA219ADDR; //地址设置为目标值
  PORINACfg.ShuntValue=0.5;//设置为0.5
  PORINACfg.ConvMode=INA219_PowerDown;//配置为powerdown模式
  PORINACfg.ShuntVoltageFullScale=80;//分流电阻阻值满量程为80mV
	UartPrintf("\r\n初始化  %s",INA219_INIT(&PORINACfg)==A219_Init_OK?"成功":"失败");
	UartPrintf("\r\n设置模式  %s",INA219_SetConvMode(INA219_Cont_Both,INA219ADDR)?"成功":"失败");
	PORINADout.TargetSensorADDR=INA219ADDR;
	UartPrintf("\r\n尝试读数  %s",INA219_GetBusInformation(&PORINADout)?"成功":"失败");
	UartPrintf("\r\n当前电池电压读数 : %.2fV",PORINADout.BusVolt);
	UartPrintf("\r\n当前电池电流读数 : %.2fA",PORINADout.BusCurrent);
	UartPrintf("\r\n当前电池功率读数 : %.2fW",PORINADout.BusPower);
  //RTC
	UARTPuts("\r\n\r\nPart6 RTC部分:");
	IsTimeExceeded=false;
	i=0;
	SetupRTCForCounter(true);
	UARTPuts("\r\nRTC配置已完毕，等待倒数...");
	while(i<700&&!IsTimeExceeded)
	  {
	  delay_ms(10); //延时等待RTC计数完毕
	  i++;
		if((i%100)==0)UartPrintf("\r\n驱动已等待%d秒.",i/100);
		}
	if(i==700)UARTPuts("\r\n\r\n错误:片内RTC未正常计数.");
  else UartPrintf("\r\n\r\nRTC自检正常完成,实际到数时间%d毫秒.",i*10);
	//执行完毕
	ClearRecvBuffer();//清除接收缓冲
	CmdHandle=Command_None;//命令执行完毕
	}
#endif
