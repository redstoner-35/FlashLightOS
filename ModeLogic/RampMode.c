/*
该文件负责实现无极调光挡位.
无极调光的挡位会被系统记忆
*/

#include "runtimelogger.h"
#include "modelogic.h"
#include "cfgfile.h"
#include "Sidekey.h"
#include "LEDMgmt.h"
#include <math.h>
#include <stdio.h>

static bool IsKeyPressed=false;
static char MinMaxBrightNessStrobeTimer=0;
static bool IsEnabledLimitTimer=false;

//强制重置当前挡位的函数
bool ResetRampBrightness(void)
 {
 ModeConfStr *CurrentMode;
 RampModeStaticStorDef *RampConfig;
 CurrentMode=GetCurrentModeConfig();//获取目前挡位信息
 if(CurrentMode==NULL||CurrentMode->Mode!=LightMode_Ramp)return false; //挡位信息为空
 RampConfig=&RunLogEntry.Data.DataSec.RampModeStor[GetRampConfigIndexFromMode()];//根据当前挡位信息获得目标需要处理的无极调光亮度结构体所在的位置
 RampConfig->RampModeDirection=false;
 RampConfig->RampModeConf=CfgFile.DefaultLevel[GetRampConfigIndexFromMode()]; //填写默认挡位
 return true;//复位成功
 }

//根据当前模式挡位转换到模式设置数组下标的转换函数
int GetRampConfigIndexFromMode(void)
 {
 	switch(CurMode.ModeGrpSel)	
    {
		case ModeGrp_Regular:return (CurMode.RegularGrpMode<8)?CurMode.RegularGrpMode:0;//常规挡位组
	  case ModeGrp_DoubleClick:return 8;//双击功能
		case ModeGrp_Special:return (CurMode.SpecialGrpMode+9)<13?(CurMode.SpecialGrpMode+9):0;//特殊功能挡位
	  default: return 0; //出现错误，返回0
  	}	
 }

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
	RampModeStaticStorDef *RampConfig;
	CurrentMode=GetCurrentModeConfig();//获取目前挡位信息
	RampConfig=&RunLogEntry.Data.DataSec.RampModeStor[GetRampConfigIndexFromMode()];//根据当前挡位信息获得目标需要处理的无极调光亮度结构体所在的位置
	//检测按键
	Keybuf=getSideKeyClickAndHoldEvent();	
	if(Keybuf!=IsKeyPressed)//按键松开按下的检测
	  {
		IsKeyPressed=Keybuf; 
		if(!IsKeyPressed)
			RampConfig->RampModeDirection=RampConfig->RampModeDirection?false:true;//松开时翻转方向
		else
			IsEnabledLimitTimer=true; //每次按键按下时复位亮度达到的定时器的标志位
		}
	//亮度增减
	if(IsKeyPressed)
	  {
		incValue=(float)1/(BreathTIMFreq*CurrentMode->RampModeSpeed);//计算出单位的步进值
    if(RampConfig->RampModeDirection&&RampConfig->RampModeConf>0)			
			RampConfig->RampModeConf-=incValue;
		else if(!RampConfig->RampModeDirection&&RampConfig->RampModeConf<1.00)
			RampConfig->RampModeConf+=incValue;
		//限制亮度的幅度值为0-1
		if(RampConfig->RampModeConf<0)RampConfig->RampModeConf=0;
		if(RampConfig->RampModeConf>1.0)RampConfig->RampModeConf=1.0;//限幅
		//如果亮度值没到0或者1.0则显示调整方向
		if(CfgFile.IsNoteLEDEnabled&&RampConfig->RampModeConf>0&&RampConfig->RampModeConf<1.0)
       LED_DisplayRampDir(RampConfig->RampModeDirection);
		}
	//负责短时间熄灭LED指示已经到达地板或者天花板亮度
	if(IsEnabledLimitTimer&&(RampConfig->RampModeConf==0||RampConfig->RampModeConf==1.0))
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
	BreathCurrent+=incValue*pow(RampConfig->RampModeConf,GammaCorrectionValue);//使用幂函数模拟人眼亮度曲线	
	}
