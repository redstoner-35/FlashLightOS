#include "cfgfile.h"
#include "runtimelogger.h"
#include "SideKey.h"
#include "ADC.h"
#include <math.h>
#include <string.h>
#include "LEDMgmt.h"

//声明函数
float fmaxf(float x,float y);
float fminf(float x,float y);
float LEDFilter(float DIN,float *BufIN,int bufsize);

//全局变量
static float err_last_temp = 0.0;	//上一温度误差
static float integral_temp = 0.0; //积分后的温度误差值
static bool TempControlEnabled = false; //是否激活温控
static bool CalculatePIDRequest = false; //PID计算请求
static bool DoubleClickPressed = false;//缓存，记录双击+长按是否按下
float ThermalLowPassFilter[8*ThermalLPFTimeConstant]={0}; //低通滤波器
static volatile float PIDInputTemp=25; //低通滤波结果
static char RemainingMomtBurstCount=0; //短时间鸡血技能的剩余次数

//显示已经没有鸡血技能了
void DisplayNoMomentTurbo(void)
  {
  LED_Reset();//复位LED管理器
  memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
  strncat(LEDModeStr,"D3020000E",sizeof(LEDModeStr)-1);//填充指示内容	
	ExtLEDIndex=&LEDModeStr[0];//传指针过去	
	}

//根据当前温度传感器的配置返回实际加权之后输出给温度计的温度值
float GetActualTemp(ADCOutTypeDef *ADCResult)
  {
  float ActualTemp;
  float Weight;
	//获取加权值
  Weight=CfgFile.LEDThermalWeight;
  if(ADCResult->SPSTMONState==SPS_TMON_OK&&ADCResult->SPSTemp>100)
    Weight-=(ADCResult->SPSTemp-100); //当MOS过热后，减少LED热量的权重	
	if(Weight>80)Weight=80;
  if(Weight<5)Weight=5; //权重值限幅		
	//计算温度	
  if(ADCResult->NTCState==LED_NTC_OK)//LED温度就绪，取SPS温度
	  { 
		ActualTemp=ADCResult->LEDTemp*Weight/100;
		if(ADCResult->SPSTMONState==SPS_TMON_OK) //现场测量值有效，取现场测量值
		  ActualTemp+=ADCResult->SPSTemp*(100-Weight)/100;
		else //否则取运行日志里面的平均SPS温度
			ActualTemp+=RunLogEntry.Data.DataSec.AverageSPSTemp*(100-Weight)/100;
		}
	else //温度完全使用驱动MOS温度
	  {
		if(ADCResult->SPSTMONState==SPS_TMON_OK) //现场测量值有效，取现场测量值
		  ActualTemp=ADCResult->SPSTemp;
		else //否则取运行日志里面的平均SPS温度
			ActualTemp=RunLogEntry.Data.DataSec.AverageSPSTemp;
		}
	return ActualTemp; //返回实际温度
	}	
//放在日志记录器里面进行温度常数计算的函数
void CalculatePIDTempInput(ADCOutTypeDef *ADCResult)
  {
  //如果当前LED电流为0，则不允许积分器进行累加
	if(!SysPstatebuf.ToggledFlash||SysPstatebuf.TargetCurrent<=0)return;	
	//运行读取函数并赋值最新的PID数值
	PIDInputTemp=LEDFilter(GetActualTemp(ADCResult),ThermalLowPassFilter,8*ThermalLPFTimeConstant);
	CalculatePIDRequest = true; //请求PID计算
	}

//自检协商完毕之后填充温度滤波器
void FillThermalFilterBuf(ADCOutTypeDef *ADCResult)
  {
	int i;
	float Temp;
	//写缓冲区
	Temp=GetActualTemp(ADCResult);
	PIDInputTemp=Temp; //存储温度值
  for(i=0;i<(8*ThermalLPFTimeConstant);i++)ThermalLowPassFilter[i]=Temp;
	}	

//基于PID算法的温度控制模块
float PIDThermalControl(void)
  {
	float err_temp,AdjustValue;
	bool result;
	ModeConfStr *TargetMode=GetCurrentModeConfig();
	//如果当前挡位支持鸡血，且自动关机定时器没有启用则进行逻辑检测
	if(TargetMode!=NULL&&TargetMode->MaxMomtTurboCount>0&&TargetMode->PowerOffTimer==0)
	  {
		result=getSideKeyDoubleClickAndHoldEvent();//获取用户是否使能操作
		if(DoubleClickPressed!=result)
		  {
			DoubleClickPressed=result;//同步结果判断用户是否按下
			if(DoubleClickPressed&&TempControlEnabled) //用户按下
			  {
				if(RemainingMomtBurstCount>0)
				  {
					if(RunLogEntry.Data.DataSec.TotalMomtTurboCount<65534)RunLogEntry.Data.DataSec.TotalMomtTurboCount++; //技能次数+1
					RemainingMomtBurstCount--;
					TempControlEnabled=false; //技能还有剩余次数，发动技能禁用温控强制最高亮度
					}
				else //技能已经用光了无法发功,提示用户
					DisplayNoMomentTurbo();
				}			
			}
		}
  //判断温控是否达到release或者trigger点
	if(TargetMode!=NULL&&PIDInputTemp<=CfgFile.PIDRelease)   //温控低于release点，装填鸡血设置
		RemainingMomtBurstCount=TargetMode->MaxMomtTurboCount;	
	if(!TempControlEnabled&&PIDInputTemp>=CfgFile.PIDTriggerTemp)TempControlEnabled=true;
	else if(TempControlEnabled&&PIDInputTemp<=CfgFile.PIDRelease)TempControlEnabled=false;  //温控解除，恢复turbo次数
	//温控不需要接入或者当前LED是熄灭状态，直接返回100%
	if(!TempControlEnabled||!SysPstatebuf.ToggledFlash||SysPstatebuf.TargetCurrent==0)return 100;
	//温控的PID部分
	err_temp=CfgFile.PIDTargetTemp-PIDInputTemp; //计算误差值
	if(CalculatePIDRequest)
	  {
		integral_temp+=(err_temp*((float)2/(float)ThermalLPFTimeConstant)); //积分限幅(拓展系数取低通滤波器时间常数的0.5倍)
	  if(integral_temp>10)integral_temp=10;
	  if(integral_temp<-85)integral_temp=-85; //积分和积分限幅
		CalculatePIDRequest=false; //积分统计完成
		}
	AdjustValue=CfgFile.ThermalPIDKp*err_temp+CfgFile.ThermalPIDKi*integral_temp+CfgFile.ThermalPIDKd*(err_temp-err_last_temp); //PID算法算出调节值
	err_last_temp=err_temp;//记录上一个误差值
	return 90+AdjustValue; //返回base值+调节value
	}
