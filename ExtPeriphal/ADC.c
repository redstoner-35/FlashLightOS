#include "console.h"
#include "cfgfile.h"
#include "delay.h"
#include "ADC.h"
#include "LEDMgmt.h"
#include <math.h>

const char *NTCStateString[3]={"正常","开路","未连接"};
const char *SPSTMONString[3]={"正常","未连接","致命错误"};

static bool ADCEOCFlag=false;
short NTCBValue=3450;

//ADC结束转换回调
void ADC_EOC_interrupt_Callback(void)
  {
	ADC_ClearIntPendingBit(HT_ADC0, ADC_FLAG_CYCLE_EOC);//清除中断
  ADCEOCFlag=true;
	}
//片内ADC异常处理
void OnChipADC_FaultHandler(void)
  {
	UartPost(Msg_critical,"IntADC","On-Chip ADC Error::Conversion timed out.");
	CurrentLEDIndex=13;//指示ADC异常
	SelfTestErrorHandler();	
	}
//ADC获取LED电流测量引脚的电压输入值
bool ADC_GetLEDIfPinVoltage(float *VOUT)
  {
	int retry,avgcount;
	unsigned int ADCResult=0;
	for(avgcount=0;avgcount<ADCAvg;avgcount++)
		{
		//初始化变量
		retry=0;
		ADCEOCFlag=false;
		//设置ADC的配置，然后启动转换
		ADC_RegularGroupConfig(HT_ADC0,DISCONTINUOUS_MODE, 1, 0);//单次触发模式,一次完成1个数据的转换
	  ADC_RegularChannelConfig(HT_ADC0, _LED_If_Ch, 0, 0);	
	  ADC_SoftwareStartConvCmd(HT_ADC0, ENABLE); 
		//等待转换结束
		while(!ADCEOCFlag)
		  {
			retry++;//重试次数+1
			delay_ms(1);
			if(retry==400)return false;//转换超时
			}
		//获取数据
		ADCResult+=HT_ADC0->DR[0]&0x0FFF;//采集四个规则通道的结果		
		}
	//转换完毕，求平均
  ADCResult/=avgcount;
	*VOUT=(float)ADCResult*(3.3/(float)4096);//将AD值转换为电压
	return true;
	}
//ADC获取数值
bool ADC_GetResult(ADCOutTypeDef *ADCOut)
  {
  int retry,avgcount,i;
	unsigned int ADCResult[4]={0};
  float buf,Rt,Comp;
	//开始测量
	for(avgcount=0;avgcount<ADCAvg;avgcount++)
		{
		//初始化变量
		retry=0;
		ADCEOCFlag=false;
		//设置单次转换的组别后启动转换
		ADC_RegularGroupConfig(HT_ADC0,DISCONTINUOUS_MODE, 4, 0);//单次触发模式,一次完成4个数据的转换
    ADC_RegularChannelConfig(HT_ADC0, _LED_Vf_ADC_Ch, LED_Vf_ADC_Ch, 0);
    ADC_RegularChannelConfig(HT_ADC0, _NTC_ADC_Ch, NTC_ADC_Ch, 0);	
	  ADC_RegularChannelConfig(HT_ADC0, _LED_If_Ch, LED_If_Ch, 0);	
	  ADC_RegularChannelConfig(HT_ADC0, _SPS_Temp_Ch, SPS_Temp_Ch, 0);
	  ADC_SoftwareStartConvCmd(HT_ADC0, ENABLE); 
		//等待转换结束
		while(!ADCEOCFlag)
		  {
			retry++;//重试次数+1
			delay_ms(1);
			if(retry==400)return false;//转换超时
			}
		//获取数据
		for(i=0;i<4;i++)ADCResult[i]+=HT_ADC0->DR[i]&0x0FFF;//采集四个规则通道的结果		
		}
	//转换完毕，求平均
  for(i=0;i<4;i++)ADCResult[i]/=avgcount;
	//计算LEDVf
	buf=(float)ADCResult[LED_Vf_ADC_Ch]*(ADC_AVRef/(float)4096);//将AD值转换为电压
	buf*=(float)3;//乘以分压比得到最终Vf
	ADCOut->LEDVf=buf;
  //计算温度
	buf=(float)ADCResult[NTC_ADC_Ch]*(ADC_AVRef/(float)4096);//将AD值转换为电压
	Rt=((float)NTCUpperResValueK*buf)/(ADC_AVRef-buf);//得到NTC的电阻
	buf=1/((1/(273.15+(float)NTCT0))+log(Rt/(float)NTCUpperResValueK)/(float)NTCBValue);//计算出温度
	buf-=273.15;//减去开氏温标常数变为摄氏度
	buf+=(float)NTCTRIM;//加上修正值	
	if(buf<(-40)||buf>125)	//温度传感器异常
	  {
		ADCOut->NTCState=buf<(-40)?LED_NTC_Open:LED_NTC_Short;
		ADCOut->LEDTemp=0;
		}
	else  //温度传感器正常
	  {
	  ADCOut->NTCState=LED_NTC_OK;
		ADCOut->LEDTemp=buf;
		}
	//计算LED输出电流
	buf=(float)ADCResult[LED_If_Ch]*(ADC_AVRef/(float)4096);//将AD值转换为电压
	buf=(buf*(float)1000)/(float)SPSIMONDiffOpGain;//将算出的电压转为mV单位，然后除以INA199放大器的增益值得到原始的电压
	buf/=(float)SPSIMONShunt;//欧姆定律，I=U/R计算出SPS往外怼出来的电流（单位mA）
	buf/=((float)SPSIMONScale/(float)1000);//将算出来的电流除以SPS的电流反馈系数（换算为mA/A）得到实际的电流值
  ADCOut->LEDIfNonComp=buf;//将未补偿的LEDIf放置到结构体内
	Comp=QueueLinearTable(SPSCompensateTableSize,buf,CfgFile.LEDIMONCalThreshold,CfgFile.LEDIMONCalGain);//查曲线得到矫正系数
	if(Comp!=NAN)buf*=Comp;//如果补偿系数合法则得到最终结果
	#ifdef Internal_Driver_Debug
	UartPost(msg_error,"IntADC","Illegal SPS Current Compensation factor value.");
  #endif
	ADCOut->LEDIf=buf;//计算完毕返回结果
	//计算SPS温度数值
	buf=(float)ADCResult[SPS_Temp_Ch]*(ADC_AVRef/(float)4096);//将AD值转换为电压
	if(buf>3.05)
	  {
		ADCOut->SPSTemp=-20;//温度反馈为-20度		
    ADCOut->SPSTMONState=SPS_TMON_CriticalFault;//SPS会在故障时将温度检测拉高到3.3V	
		}
	else if(buf<SPSTMONStdVal)//温度为负数
	  {
		buf=SPSTMONStdVal-buf;//计算出电压差
		buf*=(float)1000;//将电压单位变为mV
		buf/=(float)SPSTMONScale;//除以scale换算出温度
	  buf*=(float)(-1);//将温度换算为负数
		if(buf<(-40))
		  {
			ADCOut->SPSTMONState=SPS_TMON_Disconnect;//温度过低视为断路
			ADCOut->SPSTemp=0;//温度为0
			}
    else 
		  {
			ADCOut->SPSTMONState=SPS_TMON_OK;//温度正常
			ADCOut->SPSTemp=buf;//温度反馈为buf值				
			}
		}
	else if(buf>SPSTMONStdVal)//温度为正数
	  {
		buf-=SPSTMONStdVal;//计算出电压差
		buf*=(float)1000;//将电压单位变为mV
		buf/=(float)SPSTMONScale;//除以scale换算出温度
		ADCOut->SPSTMONState=SPS_TMON_OK;//温度正常
		ADCOut->SPSTemp=buf;//温度反馈为buf值					
		}
	else //输出为0.6V，温度正常
		{
	  ADCOut->SPSTMONState=SPS_TMON_OK;//温度正常
		ADCOut->SPSTemp=0;//温度反馈为0度		
		}		
	#ifdef Internal_Driver_Debug
	UartPost(Msg_info,"IntADC","SPS State=%d,Temp=%.2f'C",ADCOut->SPSTMONState,ADCOut->SPSTemp);
	#endif
	//计算完毕
	return true;
	}
//内部ADC快速初始化
void InternalADC_QuickInit(void)
  {
	CKCU_SetADCnPrescaler(CKCU_ADCPRE_ADC0, CKCU_ADCPRE_DIV16);//ADC时钟为主时钟16分频=3MHz                                               
  ADC_RegularGroupConfig(HT_ADC0,DISCONTINUOUS_MODE, 4, 0);//单次触发模式,一次完成4个数据的转换
  ADC_SamplingTimeConfig(HT_ADC0,25); //采样时间（25个ADC时钟）
  ADC_RegularTrigConfig(HT_ADC0, ADC_TRIG_SOFTWARE);//软件启动	
	ADC_EOC_interrupt_Callback();
	ADC_IntConfig(HT_ADC0,ADC_INT_CYCLE_EOC,ENABLE);                                                                            
  NVIC_EnableIRQ(ADC0_IRQn);//打开中断，重新启用
	ADC_Cmd(HT_ADC0, ENABLE);
	}
//内部ADC初始化
void InternalADC_Init(void)
  {
	 int i;
	 ADCOutTypeDef ADCO;
	 UartPost(Msg_info,"IntADC","Configuring On-Chip ADC For System Monitor...");
	 //将LED Vf测量引脚和温度引脚配置为AD模式
	 AFIO_GPxConfig(GPIO_PA, LED_Vf_IOP,AFIO_FUN_ADC0);
	 AFIO_GPxConfig(GPIO_PA,NTC_IOP,AFIO_FUN_ADC0);
	 AFIO_GPxConfig(GPIO_PA,LED_If_IOP,AFIO_FUN_ADC0);
   AFIO_GPxConfig(GPIO_PA,SPS_Temp_IOP,AFIO_FUN_ADC0);	
   GPIO_PullResistorConfig(HT_GPIOA,LED_If_IOP,GPIO_PR_DOWN);//LED If输入启用下拉电阻	
   //设置转换组别类型，转换时间，转换模式      
	 CKCU_SetADCnPrescaler(CKCU_ADCPRE_ADC0, CKCU_ADCPRE_DIV16);//ADC时钟为主时钟16分频=3MHz                                               
   ADC_RegularGroupConfig(HT_ADC0,DISCONTINUOUS_MODE, 4, 0);//单次触发模式,一次完成4个数据的转换
   ADC_SamplingTimeConfig(HT_ADC0,25); //采样时间（25个ADC时钟）
   ADC_RegularTrigConfig(HT_ADC0, ADC_TRIG_SOFTWARE);//软件启动	
	 //设置单次转换的组别
   ADC_RegularChannelConfig(HT_ADC0, _LED_Vf_ADC_Ch, LED_Vf_ADC_Ch, 0);
   ADC_RegularChannelConfig(HT_ADC0, _NTC_ADC_Ch, NTC_ADC_Ch, 0);	
	 ADC_RegularChannelConfig(HT_ADC0, _LED_If_Ch, LED_If_Ch, 0);	
	 ADC_RegularChannelConfig(HT_ADC0, _SPS_Temp_Ch, SPS_Temp_Ch, 0);
	 //启用ADC中断    
   ADC_IntConfig(HT_ADC0,ADC_INT_CYCLE_EOC,ENABLE);                                                                            
   NVIC_EnableIRQ(ADC0_IRQn);
	 //启用ADC
	 ADC_Cmd(HT_ADC0, ENABLE);
	 //触发ADC转换
	 for(i=0;i<3;i++)
	   if(!ADC_GetResult(&ADCO))OnChipADC_FaultHandler();//出现异常
	 //开始显示
	 UartPost(Msg_info,"IntADC","-------- System Monitor ADC Result --------");
   UartPost(Msg_info,"IntADC","LED Vf Pin Volt : %.2fV",ADCO.LEDVf);
	 UartPost(Msg_info,"IntADC","LED BasePlate NTC Status : %s",NTCStateString[ADCO.NTCState]);
	 if(ADCO.NTCState==LED_NTC_OK)
		 UartPost(Msg_info,"IntADC","LED BasePlate Temperature : %.1f'C",ADCO.LEDTemp);
	 else 
	   {
		 #ifdef FlashLightOS_Debug_Mode
     UartPost(msg_error,"IntADC","Base Plate temperature sensor did not work properly.");
     return;
		 #endif
     //非自检模式正常输出的部分			 
		 #ifndef ForceRequireLEDNTC
		 UartPost(msg_error,"IntADC","Base Plate temperature sensor did not work properly.");
		 #else
		 UartPost(Msg_critical,"IntADC","LED Base Plate temperature sensor failure detected.");
		 CurrentLEDIndex=28;//指示LED基板NTC异常
		 SelfTestErrorHandler();	 
		 #endif
		 }
	}
