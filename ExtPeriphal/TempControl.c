#include "Cfgfile.h"
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
float ThermalLowPassFilter[24]={0}; //低通滤波器
static float PIDAdjustVal=100; //PID调节值
static char RemainingMomtBurstCount=0; //短时间鸡血技能的剩余次数

//显示已经没有鸡血技能了
void DisplayNoMomentTurbo(void)
  {
  LED_Reset();//复位LED管理器
  memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
  strncat(LEDModeStr,"D3020000E",sizeof(LEDModeStr)-1);//填充指示内容	
	ExtLEDIndex=&LEDModeStr[0];//传指针过去	
	}
//重置PID温控器的积分器
void ResetThermalPID(void)
  {
	PIDAdjustVal=100; //复位调节值
	integral_temp=0;
	err_last_temp=0; //在温控不需要启动的阶段，需要将温控的微分和积分器复位否则会发生过调
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

//自检协商完毕之后填充温度滤波器
void FillThermalFilterBuf(ADCOutTypeDef *ADCResult)
  {
	int i;
	float Temp;
	//写缓冲区
	Temp=GetActualTemp(ADCResult);
  for(i=0;i<24;i++)ThermalLowPassFilter[i]=Temp;
	}	

//基于PID算法的温度控制模块
float PIDThermalControl(ADCOutTypeDef *ADCResult)
  {
	float err_temp,PIDInputTemp;
	bool result;
	ModeConfStr *TargetMode; 
  float TriggerTemp,MaintainTemp,ReleaseTemp;
  //运行读取函数获取最新的LED温度以及模式组数据
	PIDInputTemp=LEDFilter(GetActualTemp(ADCResult),ThermalLowPassFilter,24);	
	TargetMode=GetCurrentModeConfig();
	//如果当前挡位支持鸡血，且自动关机定时器没有启用则进行逻辑检测
	if(TargetMode!=NULL&&TargetMode->MaxMomtTurboCount>0&&TargetMode->PowerOffTimer==0)
	  {
		result=getSideKeyDoubleClickAndHoldEvent();//获取用户是否使能操作
		if(!RunLogEntry.Data.DataSec.IsFlashLightLocked&&DoubleClickPressed!=result)
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
	//从温控设置里面取出温度点
	if(TargetMode!=NULL) //挡位组设置不为空，取出温度设置之后减去offset
	  {
    TriggerTemp=CfgFile.PIDTriggerTemp-TargetMode->ThermalControlOffset;
    ReleaseTemp=CfgFile.PIDRelease-TargetMode->ThermalControlOffset;
    MaintainTemp=CfgFile.PIDTargetTemp-TargetMode->ThermalControlOffset;
		}
	else //挡位组数据为空，按照配置文件里面的温度来
	  {
		TriggerTemp=CfgFile.PIDTriggerTemp;
		ReleaseTemp=CfgFile.PIDRelease;
		MaintainTemp=CfgFile.PIDTargetTemp;
		}
  //判断温控是否达到release或者trigger点
	if(TargetMode!=NULL&&PIDInputTemp<=ReleaseTemp)   //温控低于release点，装填鸡血设置
		RemainingMomtBurstCount=TargetMode->MaxMomtTurboCount;	
	if(!TempControlEnabled&&PIDInputTemp>=TriggerTemp)TempControlEnabled=true;
	else if(TempControlEnabled&&PIDInputTemp<=ReleaseTemp)TempControlEnabled=false;  //温控解除，恢复turbo次数
	//温控不需要接入或者当前LED是熄灭状态，直接返回100
	if(!TempControlEnabled||!SysPstatebuf.ToggledFlash||SysPstatebuf.TargetCurrent==0)return 100;
	//温控PID的误差计算以及Kp和Kd项调节
  err_temp=MaintainTemp-PIDInputTemp; //计算误差值
	PIDAdjustVal+=err_temp*CfgFile.ThermalPIDKp/100; //Kp(比例项)
	if(fabsf(err_temp)>2) //温度误差的绝对值大于2℃，进行微分调节
	  {	
		PIDAdjustVal+=(err_temp-err_last_temp)*CfgFile.ThermalPIDKd/100; //Kd(微分项)
		err_last_temp=err_temp;//记录上一个误差值
		}
	if(PIDAdjustVal>100)PIDAdjustVal=100;
	if(PIDAdjustVal<5)PIDAdjustVal=5; //PID调节值限幅
	//温控PID的Ki(积分项)
  integral_temp+=(err_temp/(float)150); //积分累加
  if(integral_temp>25)integral_temp=25;
	if(integral_temp<-25)integral_temp=-25; //积分限幅
  //返回计算结束的调节值
	return PIDAdjustVal+CfgFile.ThermalPIDKi*integral_temp; //返回比例和微分结果以及积分结果相加的调节值
	}
