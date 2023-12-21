#include "LEDMgmt.h"
#include "modelogic.h"
#include "ADC.h"
#include "Cfgfile.h"
#include "runtimelogger.h"
#include "LEDMgmt.h"
#include <string.h>
#include <math.h>

//函数声明
int iroundf(float IN);
float GetActualTemp(ADCOutTypeDef *ADCResult);

//显示驱动MOS和LED的降档情况
void DisplayTemp(float TempIN)
  {
  if(TempIN>=CfgFile.PIDTriggerTemp)
	  strncat(LEDModeStr,"222",sizeof(LEDModeStr)-1);
	else if(TempIN>=CfgFile.PIDTargetTemp)
		strncat(LEDModeStr,"333",sizeof(LEDModeStr)-1);
	else 
		strncat(LEDModeStr,"111",sizeof(LEDModeStr)-1);
  strncat(LEDModeStr,"00",sizeof(LEDModeStr)-1);//显示个位
	}

//当用户三击的时候显示LED温度
void DisplayLEDTemp(void)
  {
  ADCOutTypeDef ADCO;
	ModeConfStr *CurrentMode;
	float SPSTemp,LEDTemp;
	bool IsPowerON;
	//获得挡位
	CurrentMode=GetCurrentModeConfig();
	IsPowerON=(SysPstatebuf.Pstate==PState_LEDOn||SysPstatebuf.Pstate==PState_LEDOnNonHold)?true:false; //获取是否开机
	if(CurrentMode==NULL)return;
	//令ADC得到温度和电流
  if(!ADC_GetResult(&ADCO))
     {
		 if(IsPowerON)//运行时ADC转换失败,这是严重故障,立即写log并停止驱动运行
			 RunTimeErrorReportHandler(Error_ADC_Logic);		 
		 else //待机状态转换失败,报错
		   {
			 SysPstatebuf.Pstate=PState_Error;
			 SysPstatebuf.ErrorCode=Error_ADC_Logic;
			 }
		 return; //直接退出
	   }
	//如果待机状态下LED温度传感器未就绪则不显示温度
	if(!IsPowerON&&ADCO.NTCState!=LED_NTC_OK)return;
	//开始显示结果
	LED_Reset();//复位LED管理器
  memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
	strncat(LEDModeStr,IsPowerON?"D":"0",sizeof(LEDModeStr)-1);
	strncat(LEDModeStr,"2310020",sizeof(LEDModeStr)-1); //红黄绿切换之后熄灭,然后绿色闪一次表示LED温度
	LEDTemp=IsPowerON?GetActualTemp(&ADCO):ADCO.LEDTemp; //关机状态下显示LED温度，开机状态显示手电的加权平均温度		 
	if(LEDTemp<0)
	 {
	 LEDTemp=-LEDTemp;//取正数
	 strncat(LEDModeStr,"30",sizeof(LEDModeStr)-1); //如果LED温度为负数则闪烁一次黄色
	 }
	strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
	//显示数值
	LED_AddStrobe((int)(LEDTemp/100),"20");
	strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1); //显示百位
	LED_AddStrobe(((int)LEDTemp%100)/10,"30"); 
	strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1); //显示十位
	LED_AddStrobe((iroundf(LEDTemp)%100)%10,"10");
	strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);//显示个位
	//降档提示，当温度超过降档温度时显示信息	 
	LEDTemp=ADCO.NTCState==LED_NTC_OK?ADCO.LEDTemp:25; //获取LED温度
	if(IsPowerON)  //运行状态
	  {
	  if(ADCO.SPSTMONState==SPS_TMON_OK)SPSTemp=ADCO.SPSTemp;
	  else SPSTemp=RunLogEntry.Data.DataSec.AverageSPSTemp; //取温度
	  }
	else SPSTemp=LEDTemp; //非运行状态,按照LED温度显示
	DisplayTemp(LEDTemp);	
	DisplayTemp(SPSTemp);  //显示降档情况	
	//结束显示，传指针过去
	strncat(LEDModeStr,IsPowerON?"DE":"E",sizeof(LEDModeStr)-1); //添加结束符
	ExtLEDIndex=&LEDModeStr[0];//传指针过去
	}
