#include "CurrentReadComp.h"
#include "console.h"
#include "CfgFile.h"
#include "ADC.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

//参数帮助entry
#ifdef FlashLightOS_Init_Mode	
const char *imonadjArgument(int ArgCount)
  {	
	if(ArgCount<2)return "查看主buck的参数";
	return "查看副buck的参数";	
	}
#endif
//命令处理
void Imonadjhandler(void)
 {
 #ifdef FlashLightOS_Init_Mode
 int i;
 float buf;
 float *DimCompValue;
 float *DimCompThr;
 float *IFBCompValue;
 float *IFBCompThr;
 char Mainparamok,AuxParamOK;
 bool CommandParamOK=false;
 //--v参数
 IsParameterExist("01",10,&Mainparamok);
 IsParameterExist("23",10,&AuxParamOK);
 if(Mainparamok||AuxParamOK)
	 {
	 DimCompValue=Mainparamok?CompData.CompDataEntry.CompData.Data.MainBuckDimmingValue:CompData.CompDataEntry.CompData.Data.AuxBuckDimmingValue;
	 DimCompThr=Mainparamok?CompData.CompDataEntry.CompData.Data.MainBuckDimmingThreshold:CompData.CompDataEntry.CompData.Data.AuxBuckDimmingThreshold;
	 IFBCompValue=Mainparamok?CompData.CompDataEntry.CompData.Data.MainBuckIFBValue:CompData.CompDataEntry.CompData.Data.AuxBuckIFBValue;
	 IFBCompThr=Mainparamok?CompData.CompDataEntry.CompData.Data.MainBuckIFBThreshold:CompData.CompDataEntry.CompData.Data.AuxBuckIFBThreshold;	  //传入指针
	 UartPrintf("\r\n--------------- 驱动电流控制校准数据库(%sBuck) -------------",Mainparamok?"主":"副");
	 UARTPuts("\r\nLED 目标电流(A) | 电流补偿数值  | 目标输出电流(A) | 补偿值 |"); 
	 for(i=0;i<50;i++)
		 {
		 //电流测量补偿值
		 UartPrintf("\r\n%.3fA | ",IFBCompThr[i]);
     buf=IFBCompValue[i]*100;			 
	   UartPrintf("%.3f(%.2f%%) | ",IFBCompValue[i],buf);
		 //输出控制补偿值
		 UartPrintf("%.3fA | ",DimCompThr[i]);
     buf=DimCompValue[i]*100;			 
	   UartPrintf("%.3f(%.2f%%) | ",DimCompValue[i],buf);		 
		 }
	 UARTPuts("\r\n");
	 CommandParamOK=true;
	 }
 //非法的参数
 if(!CommandParamOK)UartPrintCommandNoParam(10);//显示啥也没找到的信息 
 #endif
 //命令完毕的回调处理	 
 ClearRecvBuffer();//清除接收缓冲
 CmdHandle=Command_None;//命令执行完毕			
 }
