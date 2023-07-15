/*
该文件负责实现无极调光挡位.
无极调光的挡位会被系统记忆
*/

#include "runtimelogger.h"
#include "modelogic.h"
#include "cfgfile.h"
#include "Sidekey.h"
#include <math.h>
#include <stdio.h>

static bool IsKeyPressed=false;
static char MinMaxBrightNessStrobeTimer=0;
static bool IsEnabledLimitTimer=false;

//重置呼吸模式的状态机的函数(在换挡和关闭手电时重置状态机)
void ResetRampMode(void)
 {
 IsKeyPressed=false;
 BreathCurrent=0;
 MinMaxBrightNessStrobeTimer=0;
 IsEnabledLimitTimer=false;
 SysPstatebuf.ToggledFlash=true;//恢复点亮
 }	
//主函数
void RampModeHandler(void)
  {
	bool Keybuf;
  float incValue;
	ModeConfStr *CurrentMode;
	CurrentMode=GetCurrentModeConfig();//获取目前挡位信息
	//检测按键
	Keybuf=getSideKeyClickAndHoldEvent();	
	if(Keybuf!=IsKeyPressed)//按键松开按下的检测
	  {
		IsKeyPressed=Keybuf; 
		if(!IsKeyPressed)
			RunLogEntry.Data.DataSec.RampModeDirection=RunLogEntry.Data.DataSec.RampModeDirection?false:true;//松开时翻转方向
		else
			IsEnabledLimitTimer=true; //每次按键按下时复位亮度达到的定时器的标志位
		}
	//亮度增减
	if(IsKeyPressed)
	  {
		incValue=(float)1/(BreathTIMFreq*CurrentMode->RampModeSpeed);//计算出单位的步进值
		
    if(RunLogEntry.Data.DataSec.RampModeDirection&&RunLogEntry.Data.DataSec.RampModeConf>0)			
			RunLogEntry.Data.DataSec.RampModeConf-=incValue;
		else if(!RunLogEntry.Data.DataSec.RampModeDirection&&RunLogEntry.Data.DataSec.RampModeConf<1.00)
			RunLogEntry.Data.DataSec.RampModeConf+=incValue;
		//限制亮度的幅度值为0-1
		if(RunLogEntry.Data.DataSec.RampModeConf<0)RunLogEntry.Data.DataSec.RampModeConf=0;
		if(RunLogEntry.Data.DataSec.RampModeConf>1.0)RunLogEntry.Data.DataSec.RampModeConf=1.0;//限幅
		}
	//负责短时间熄灭LED指示已经到达地板或者天花板亮度
	if(IsEnabledLimitTimer&&(RunLogEntry.Data.DataSec.RampModeConf==0||RunLogEntry.Data.DataSec.RampModeConf==1.0))
	  {
		IsEnabledLimitTimer=false;
		SysPstatebuf.ToggledFlash=false;
		MinMaxBrightNessStrobeTimer=BreathTIMFreq/2;//熄灭0.5秒
		}
	if(MinMaxBrightNessStrobeTimer>0)
		MinMaxBrightNessStrobeTimer--;
	else if(!RunLogEntry.Data.DataSec.IsLowVoltageAlert)
	  SysPstatebuf.ToggledFlash=true; //时间到只有低压告警没发生的时候才重新打开LED
	//生成最后的电流设置
	
	if(CurrentMode==NULL)return; //退出
	incValue=CurrentMode->LEDCurrentHigh-CurrentMode->LEDCurrentLow;
	BreathCurrent=CurrentMode->LEDCurrentLow<0.5?0.5:CurrentMode->LEDCurrentLow;//从低电流开始
	BreathCurrent+=incValue*pow(RunLogEntry.Data.DataSec.RampModeConf,GammaCorrectionValue);//使用幂函数模拟人眼亮度曲线	
	}
