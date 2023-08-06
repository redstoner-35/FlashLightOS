#include "LEDMgmt.h"
#include "modelogic.h"
#include "ADC.h"
#include "Cfgfile.h"
#include "runtimelogger.h"
#include "LEDMgmt.h"
#include <string.h>
#include <math.h>

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
	//获得挡位
	CurrentMode=GetCurrentModeConfig();
	if(CurrentMode==NULL)return;
  LED_Reset();//复位LED管理器
  memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
	//令ADC得到温度和电流
  if(!ADC_GetResult(&ADCO))
     {
		 if(SysPstatebuf.Pstate==PState_LEDOn||SysPstatebuf.Pstate==PState_LEDOnNonHold)
			 RunTimeErrorReportHandler(Error_ADC_Logic);//运行时ADC转换失败,这是严重故障,立即写log并停止驱动运行
		 else //待机状态转换失败,报错
		   {
			 SysPstatebuf.Pstate=PState_Error;
			 SysPstatebuf.ErrorCode=Error_ADC_Logic;
			 }
		 return; //直接退出
	   }	
	//检查读回来的结果，如果LED基板温度没数据则不显示
	if(ADCO.NTCState!=LED_NTC_OK)return;
	//开始显示结果
  strncat(LEDModeStr,SysPstatebuf.Pstate==PState_LEDOn||SysPstatebuf.Pstate==PState_LEDOnNonHold?"D":"0",sizeof(LEDModeStr)-1);
	strncat(LEDModeStr,"2310020D",sizeof(LEDModeStr)-1); //红黄绿切换之后熄灭,然后绿色闪一次表示LED温度
	LED_AddStrobe((int)(ADCO.LEDTemp/100),"20");
	strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1); //显示百位
	LED_AddStrobe(((int)ADCO.LEDTemp%100)/10,"30"); 
	strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1); //显示十位
	LED_AddStrobe(((int)ADCO.LEDTemp%100)%10,"10");
	strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);//显示个位
	//降档提示，当温度超过降档温度时显示信息	 
	
	LEDTemp=ADCO.NTCState==LED_NTC_OK?ADCO.LEDTemp:25; //获取LED温度
	if(SysPstatebuf.Pstate==PState_LEDOn||SysPstatebuf.Pstate==PState_LEDOnNonHold)  //运行状态
	  {
	  if(ADCO.SPSTMONState==SPS_TMON_OK)SPSTemp=ADCO.SPSTemp;
	  else SPSTemp=RunLogEntry.Data.DataSec.AverageSPSTemp; //取温度
	  }
	else SPSTemp=LEDTemp; //非运行状态,按照LED温度显示
	DisplayTemp(LEDTemp);	
	DisplayTemp(SPSTemp);  //显示降档情况	
	//结束显示，传指针过去
	strncat(LEDModeStr,SysPstatebuf.Pstate==PState_LEDOn||SysPstatebuf.Pstate==PState_LEDOnNonHold?"DE":"E",sizeof(LEDModeStr)-1); //添加结束符
	ExtLEDIndex=&LEDModeStr[0];//传指针过去
	}
