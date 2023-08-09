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
static bool DoubleClickPressed = false;//缓存，记录双击+长按是否按下
float ThermalFilterBuf[12]={0}; //温度滤波器
static char RemainingMomtBurstCount=0; //短时间鸡血技能的剩余次数

//显示已经没有鸡血技能了
void DisplayNoMomentTurbo(void)
  {
  LED_Reset();//复位LED管理器
  memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
  strncat(LEDModeStr,"0003020000E",sizeof(LEDModeStr)-1);//填充指示内容	
	ExtLEDIndex=&LEDModeStr[0];//传指针过去	
	}

//自检协商完毕之后填充温度缓冲
void FillThermalFilterBuf(ADCOutTypeDef *ADCResult)
  {
	int i;
	float ActualTemp;
	//获得实际温度
  if(ADCResult->NTCState==LED_NTC_OK)//LED温度就绪，取SPS温度
	  { 
		ActualTemp=ADCResult->LEDTemp*CfgFile.LEDThermalWeight/100;
		if(ADCResult->SPSTMONState==SPS_TMON_OK) //现场测量值有效，取现场测量值
		  ActualTemp+=ADCResult->SPSTemp*(100-CfgFile.LEDThermalWeight)/100;
		else //否则取运行日志里面的平均SPS温度
			ActualTemp+=RunLogEntry.Data.DataSec.AverageSPSTemp*(100-CfgFile.LEDThermalWeight)/100;
		}
	else //温度完全使用驱动MOS温度
	  {
		if(ADCResult->SPSTMONState==SPS_TMON_OK) //现场测量值有效，取现场测量值
		  ActualTemp=ADCResult->SPSTemp;
		else //否则取运行日志里面的平均SPS温度
			ActualTemp=RunLogEntry.Data.DataSec.AverageSPSTemp;
		}
	//写缓冲区
	for(i=0;i<12;i++)ThermalFilterBuf[i]=ActualTemp;
	}

//基于PID算法的温度控制模块
float PIDThermalControl(ADCOutTypeDef *ADCResult)
  {
	float ActualTemp,err_temp,AdjustValue;
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
	//获得实际温度
  if(ADCResult->NTCState==LED_NTC_OK)//LED温度就绪，取SPS温度
	  { 
		ActualTemp=ADCResult->LEDTemp*CfgFile.LEDThermalWeight/100;
		if(ADCResult->SPSTMONState==SPS_TMON_OK) //现场测量值有效，取现场测量值
		  ActualTemp+=ADCResult->SPSTemp*(100-CfgFile.LEDThermalWeight)/100;
		else //否则取运行日志里面的平均SPS温度
			ActualTemp+=RunLogEntry.Data.DataSec.AverageSPSTemp*(100-CfgFile.LEDThermalWeight)/100;
		}
	else //温度完全使用驱动MOS温度
	  {
		if(ADCResult->SPSTMONState==SPS_TMON_OK) //现场测量值有效，取现场测量值
		  ActualTemp=ADCResult->SPSTemp;
		else //否则取运行日志里面的平均SPS温度
			ActualTemp=RunLogEntry.Data.DataSec.AverageSPSTemp;
		}
	ActualTemp=LEDFilter(ActualTemp,ThermalFilterBuf,12); //实际温度等于温度降档滤波器的均分输出
	if(TargetMode!=NULL&&ActualTemp<=CfgFile.PIDRelease)   //温控低于release点，装填鸡血设置
		RemainingMomtBurstCount=TargetMode->MaxMomtTurboCount;	
	//判断温控是否达到release或者trigger点
	if(!TempControlEnabled&&ActualTemp>=CfgFile.PIDTriggerTemp)TempControlEnabled=true;
	else if(TempControlEnabled&&ActualTemp<=CfgFile.PIDRelease) TempControlEnabled=false;  //温控解除，恢复turbo次数
	//温控不需要接入或者当前LED是熄灭状态，直接返回100%
	if(!TempControlEnabled||!SysPstatebuf.ToggledFlash||SysPstatebuf.TargetCurrent==0)return 100;
	//温控的PID部分
	err_temp=CfgFile.PIDTargetTemp-ActualTemp; //计算误差值
	integral_temp+=err_temp;
	if(integral_temp>10)integral_temp=10;
	if(integral_temp<-85)integral_temp=-85; //积分和积分限幅
	AdjustValue=CfgFile.ThermalPIDKp*err_temp+CfgFile.ThermalPIDKi*integral_temp+CfgFile.ThermalPIDKd*(err_temp-err_last_temp); //PID算法算出调节值
	err_last_temp=err_temp;//记录上一个误差值
	return 90+AdjustValue; //返回base值+调节value
	}
