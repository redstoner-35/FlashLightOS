#include "CurrentReadComp.h"
#include "console.h"
#include "CfgFile.h"
#include "ADC.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

//内部define
#define SPSCompensateTableSize 51

//参数帮助entry
#ifdef FlashLightOS_Debug_Mode	
const char *imonadjArgument(int ArgCount)
  {	
	return "";	
	}
#endif
//命令处理
void Imonadjhandler(void)
 {
 #ifdef FlashLightOS_Debug_Mode
 int i;
 ADCOutTypeDef ADCO;
 char *ParamPtr;
 float buf,oldvalue;
 char paramok;
 bool CommandParamOK=false;
 //--v参数
 IsParameterExist("01",10,&paramok);
 if(paramok)
	 {
   UARTPuts("\r\n--------------- 智能功率级LED电流探头补偿设置 --------------");
	 UARTPuts("\r\nLED 目标电流(A) | 电流补偿数值  | 目标输出电流(A) | 补偿值 |"); 
	 for(i=0;i<SPSCompensateTableSize;i++)
		 {
		 UartPrintf("\r\n%.3fA | ",CompData.CompDataEntry.CompData.Data.CurrentCompThershold[i]);
     buf=CompData.CompDataEntry.CompData.Data.CurrentCompValue[i]*100;			 
	   UartPrintf("%.3f(%.2f%%) | ",CompData.CompDataEntry.CompData.Data.CurrentCompValue[i],buf);
		 UartPrintf("%.3fA | ",CompData.CompDataEntry.CompData.Data.DimmingCompThreshold[i]);
     buf=CompData.CompDataEntry.CompData.Data.DimmingCompValue[i]*100;			 
	   UartPrintf("%.3f(%.2f%%) | ",CompData.CompDataEntry.CompData.Data.DimmingCompValue[i],buf);		 
		 }
	 if(ADC_GetResult(&ADCO))
	  {
		UARTPuts("\r\n--------- 当前系统ADC读数 ---------");
    UartPrintf("\r\n当前LED电压 : %.2fV",ADCO.LEDVf);
		UartPrintf("\r\n当前LED电流(原始值) : %.2fA",ADCO.LEDIfNonComp);
		buf=QueueLinearTable(SPSCompensateTableSize,ADCO.LEDIfNonComp,CompData.CompDataEntry.CompData.Data.CurrentCompThershold,CompData.CompDataEntry.CompData.Data.CurrentCompValue);
		UartPrintf("\r\n当前补偿值 : %.3f",buf);//计算并显示补偿值
		UartPrintf("\r\n当前LED电流(补偿后) : %.2fA",ADCO.LEDIf);	
		UartPrintf("\r\n当前驱动集成MOS温度 : %.2f'C",ADCO.SPSTemp);	
	  UartPrintf("\r\nLED基板NTC电阻状态 : %s",NTCStateString[ADCO.NTCState]);
	  if(ADCO.NTCState==LED_NTC_OK)UartPrintf("\r\nLED基板温度 : %.1f'C",ADCO.LEDTemp);
		}	 
	 CommandParamOK=true;
	 }
 //检查传入设置节点参数的同时是否传入了节点数值
 IsParameterExist("2345",10,&paramok);
 if(paramok&&IsParameterExist("67",10,NULL)==NULL)
	 UARTPuts("\r\n错误:欲设置系统的电流检查补偿参数，您需要使用'-n'或'--node'设置目标操作的节点！");
   //传入了节点参数，检查是否有电流和阈值传入
 else if(paramok)
   {
	 CommandParamOK=false;//清空结果
   ParamPtr=IsParameterExist("67",10,NULL);
   i=(!CheckIfParamOnlyDigit(ParamPtr))?atoi(ParamPtr):-1;//如果字符串为合法值则传入数据
   if(i<0||i>=SPSCompensateTableSize)
		 UartPrintf("\r\n错误:您未指定或指定的欲编辑节点ID超出合法值！允许的值为0-%d.",SPSCompensateTableSize-1);
   else //检测其余部分
     {
		 //更新电流阈值
		 ParamPtr=IsParameterExist("45",10,NULL);
		 buf=ParamPtr!=NULL?atof(ParamPtr):-1;
		 if(ParamPtr!=NULL&&(buf<0.5||buf>(float)FusedMaxCurrent))
			 UartPrintf("\r\n错误:您指定的电流阈值无效！允许的电流阈值为0.5-%.1f(A).",(float)FusedMaxCurrent);
		 else if(ParamPtr!=NULL&&buf>=0.5&&buf<=(float)FusedMaxCurrent)//传入的电流数值合法且存在
		   {
			 oldvalue=CompData.CompDataEntry.CompData.Data.CurrentCompThershold[i];//存下旧的数值
			 CompData.CompDataEntry.CompData.Data.CurrentCompThershold[i]=buf;
			 if(QueueLinearTable(SPSCompensateTableSize,1.2,CompData.CompDataEntry.CompData.Data.CurrentCompThershold,CompData.CompDataEntry.CompData.Data.CurrentCompValue)==NAN)//替换并检查
		    {
				UARTPuts("\r\n错误:您提供的阈值参数不合法,已恢复为原始数值.");
			  CompData.CompDataEntry.CompData.Data.CurrentCompThershold[i]=oldvalue;
			  }
			 else //正常更新
			  {
				if(WriteCompDataToROM()) //将数值保存到ROM中
					UARTPuts("\r\n保存校准数据时出现错误.");
			  else 
					UartPrintf("\r\n补偿曲线节点%d的电流阈值已更新为%.3fA.",i,CompData.CompDataEntry.CompData.Data.CurrentCompThershold[i]);
			  CommandParamOK=true;
				}
			 }
		 //更新补偿增益
		 ParamPtr=IsParameterExist("23",10,NULL);
		 buf=ParamPtr!=NULL?atof(ParamPtr):-1;
		 if(ParamPtr!=NULL&&(buf<0.5||buf>1.5))
			 UARTPuts("\r\n错误:您指定的电流补偿增益无效！允许的值为0.5-1.5.");
		 else if(ParamPtr!=NULL&&buf>=0.5&&buf<=(float)FusedMaxCurrent)//传入的电流数值合法且存在
		   {
			 CompData.CompDataEntry.CompData.Data.CurrentCompValue[i]=buf;
			 if(WriteCompDataToROM()) //将数值保存到ROM中
				 UARTPuts("\r\n保存校准数据时出现错误.");
			 else 
			   UartPrintf("\r\n补偿曲线节点%d的电流补偿增益已更新为%.3f.",i,CompData.CompDataEntry.CompData.Data.CurrentCompValue[i]);
			 CommandParamOK=true;
			 }			 
		 //显示结果
		 if(!CommandParamOK)
		   {
			 UARTPuts("\r\n警告:由于您指定的参数不合法,程序未对补偿参数进行任何操作.");
			 CommandParamOK=true;
			 }			 
		 }
	 } 
 //非法的参数
 if(!CommandParamOK)UartPrintCommandNoParam(10);//显示啥也没找到的信息 
 #endif
 //命令完毕的回调处理	 
 ClearRecvBuffer();//清除接收缓冲
 CmdHandle=Command_None;//命令执行完毕			
 }
