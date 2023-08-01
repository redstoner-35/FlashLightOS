#include "LEDMgmt.h"
#include "modelogic.h"
#include "ADC.h"
#include "Cfgfile.h"
#include "runtimelogger.h"
#include "LEDMgmt.h"
#include <string.h>
#include <math.h>

//显示降档程度
static void DisplayStepTable(float Value)
  {
	if(Value==NAN)return; 
	if(Value>97)strncat(LEDModeStr,"111",sizeof(LEDModeStr)-1); //降档值小于3%，绿色
	else if(Value>75)strncat(LEDModeStr,"333",sizeof(LEDModeStr)-1);//降档值3-25%，黄色
	else strncat(LEDModeStr,"222",sizeof(LEDModeStr)-1); //降档值大于40%，红色	
	strncat(LEDModeStr,"0D",sizeof(LEDModeStr)-1);
	}

//当用户三击的时候显示LED温度
void DisplayLEDTemp(void)
  {
  ADCOutTypeDef ADCO;
	float buf[2],SPSTemp;
  int i;
	ModeConfStr *CurrentMode;
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
  strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1); //显示个位
	//显示降档幅值
	if(!CurrentMode->IsModeAffectedByStepDown) //降档被禁用
	   {
	   buf[0]=100;
	   buf[1]=100;
		 }
	else //温度降档正常运行
	   {		
	   buf[0]=QueueLinearTable(5,ADCO.LEDTemp,CfgFile.LEDThermalStepThr,CfgFile.LEDThermalStepRatio);//根据LED的温度取降档数值
	   if(ADCO.SPSTMONState!=SPS_TMON_OK)SPSTemp=RunLogEntry.Data.DataSec.AverageSPSTemp;
		 else SPSTemp=ADCO.SPSTemp;  //如果当前数据不可用则使用最近的均值
		 buf[1]=QueueLinearTable(5,SPSTemp,CfgFile.SPSThermalStepThr,CfgFile.SPSThermalStepRatio);//根据SPS的温度取降档数值
		 }
  for(i=0;i<2;i++)DisplayStepTable(buf[i]);
	//结束显示，传指针过去
	strncat(LEDModeStr,SysPstatebuf.Pstate==PState_LEDOn||SysPstatebuf.Pstate==PState_LEDOnNonHold?"0DE":"0E",sizeof(LEDModeStr)-1); //添加结束符
	ExtLEDIndex=&LEDModeStr[0];//传指针过去
	}
