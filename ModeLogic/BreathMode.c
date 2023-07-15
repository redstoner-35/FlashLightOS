/*
该文件负责处理信标闪的呼吸模式，也是基于状态机的思想去编写。
信标模式下一共分为四个阶段，四个阶段一循环。循环的结构如下：

   爬升->保持最高电流->下滑->保持最低电流
	  |                              |
		-<--------------<----------<----

其中:
  爬升模式的斜率由CurrentRampUpTime设定。
	最高电流的保持时间由MaxCurrentHoldTime设定。
	下滑模式的斜率由CurrentRampDownTime设定。
	最低电流的保持时间由MinCurrentHoldTime设定。
*/

#include "modelogic.h"
#include "cfgfile.h"
#include "delay.h"
#include "PWMDIM.h"
#include <math.h>

//状态机状态声明
typedef enum
 {
 Breathing_RampUp,
 Breathing_MaxCurrWait,
 Breathing_RampDown,
 Breathing_MinCurrWait
 }BreathingStateDef;

//静态变量和全局变量
static BreathingStateDef BreathState=Breathing_RampUp; 
float BreathCurrent=0;
static float BrightNess=0;
static char BreathTimer=0;

//重置呼吸模式的状态机的函数(在换挡和关闭手电时重置状态机)
void ResetBreathStateMachine(void)
 {
 BreathState=Breathing_RampUp; 
 BreathCurrent=0;
 BreathTimer=0;
 SysPstatebuf.ToggledFlash=true;//恢复点亮
 }	
//负责处理呼吸模块状态机的处理函数
void BreatheStateMachine(void)
 {
 ModeConfStr *CurrentMode;
 float Buf;
 //获取当前挡位
 CurrentMode=GetCurrentModeConfig();
 if(CurrentMode==NULL)return; //返回空指针，退出
 //状态机
 switch(BreathState)
  {
	/******************************************************
  当电流在最低挡位停留超过MinCurrentHoldTime(秒)之后，将
	会按照CurrentRampUpTime(秒)的时间内从最低电流爬升到你
	设定的最高电流，然后保持MaxCurrentHoldTime(秒)。
	******************************************************/
	case Breathing_RampUp:
	  {
		if(BrightNess==1.00)//亮度已经到了最高点了，进入等待时间
		   {
			 BreathTimer=(int)(CurrentMode->MaxCurrentHoldTime*(float)BreathTIMFreq);//设置计时器
			 BreathState=Breathing_MaxCurrWait;//跳转到等待时间
			 break;
			 }
		else BrightNess+=(1.00/(float)BreathTIMFreq);//逐步增加亮度
		//加减完毕之后限幅
		if(BrightNess>1.00)BrightNess=1.00;
		if(BrightNess<0)BrightNess=0; 
		//拟合亮度
	  Buf=CurrentMode->LEDCurrentHigh-CurrentMode->LEDCurrentLow;
		BreathCurrent=CurrentMode->LEDCurrentLow;//从低电流开始
		BreathCurrent+=Buf*pow(BrightNess,GammaCorrectionValue);//使用幂函数模拟人眼亮度曲线
		break;
		}
	/******************************************************
  当LED电流爬升到由LEDCurrentHigh(A)定义的最大LED电流后.
	系统将在这个状态内保持保持MaxCurrentHoldTime(秒),然后
	跳转到递减状态
	******************************************************/
	case Breathing_MaxCurrWait:
	  {
		if(BreathTimer>0)BreathTimer--;
		else BreathState=Breathing_RampDown;//跳转到最低电流呼吸时间
		break;
		}
	/******************************************************
  当电流在最高挡位停留超过MaxCurrentHoldTime(秒)之后，将
	会按照CurrentRampDownTime(秒)的时间内从最高电流下滑到你
	设定的最高最低，然后保持MinCurrentHoldTime(秒)。
	******************************************************/
  case Breathing_RampDown:
	  {
		if(BrightNess==0.00)//亮度已经到了最低点了，进入等待时间
		   {
			 BreathTimer=(int)(CurrentMode->MinCurrentHoldTime*(float)BreathTIMFreq);//设置计时器
			 BreathState=Breathing_MinCurrWait;//跳转到等待时间
			 break;
			 }
		else BrightNess-=(1.00/(float)BreathTIMFreq);//逐步减少亮度
		//加减完毕之后限幅
		if(BrightNess>1.00)BrightNess=1.00;
		if(BrightNess<0)BrightNess=0; 
		//拟合亮度
	  Buf=CurrentMode->LEDCurrentHigh-CurrentMode->LEDCurrentLow;
		BreathCurrent=CurrentMode->LEDCurrentLow;//从低电流开始
		BreathCurrent+=Buf*pow(BrightNess,GammaCorrectionValue);//使用幂函数模拟人眼亮度曲线	 
		break;
		}
 /******************************************************
 当LED电流下滑到由LEDCurrentLow(A)定义的最小LED电流后.
 系统将在这个状态内保持保持MinCurrentHoldTime(秒),然后
 跳转到爬升状态让电流重新升高，以此反复循环。
 ******************************************************/
 case Breathing_MinCurrWait:
	  {
		if(BreathTimer>0)BreathTimer--;
		else BreathState=Breathing_RampUp;//跳转到最低电流呼吸时间
		break;
		}
	}
 }
