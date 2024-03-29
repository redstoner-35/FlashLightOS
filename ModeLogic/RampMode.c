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
bool IsRampAdjusting=false; //外部引用，无极调光是否在调节

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
//在每次开机和换挡之前检测并控制方向
void RampDimmingDirReConfig(void)
 {
 ModeConfStr *CurrentMode;
 RampModeStaticStorDef *RampConfig;
 CurrentMode=GetCurrentModeConfig();//获取目前挡位信息
 if(CurrentMode==NULL||CurrentMode->Mode!=LightMode_Ramp)return; //挡位信息为空
 RampConfig=&RunLogEntry.Data.DataSec.RampModeStor[GetRampConfigIndexFromMode()];//根据当前挡位信息获得目标需要处理的无极调光亮度结构体所在的位置
 if(RampConfig->RampModeConf==1.00)RampConfig->RampModeDirection=true; //当前挡位已经到最大值，直接设置为向下调节
 if(RampConfig->RampModeConf==0.00)RampConfig->RampModeDirection=false;	 //当前挡位已经到最小值，直接设置为向上 
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
		//计算步进值
		incValue=(float)1/(BreathTIMFreq*CurrentMode->RampModeSpeed);//计算出单位的步进值
	  //根据方向增减亮度值
    if(RampConfig->RampModeDirection&&RampConfig->RampModeConf>0)			
			RampConfig->RampModeConf-=incValue;
		else if(!RampConfig->RampModeDirection&&RampConfig->RampModeConf<1.00)
			RampConfig->RampModeConf+=incValue;
		//限制亮度的幅度值为0-1
		if(RampConfig->RampModeConf<0)RampConfig->RampModeConf=0;
		if(RampConfig->RampModeConf>1.0)RampConfig->RampModeConf=1.0;//限幅
		//如果亮度值没到0或者1.0则显示调整方向
		if(CfgFile.IsNoteLEDEnabled&&RampConfig->RampModeConf>0&&RampConfig->RampModeConf<1.0)
		   {
			 IsRampAdjusting=true; //指示正在调光
			 LED_DisplayRampDir(RampConfig->RampModeDirection);
			 }
		else IsRampAdjusting=false; //调光结束
		}
	else IsRampAdjusting=false; //按键松开，调光结束开始进入缓慢锁相模式
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
	BreathCurrent=CurrentMode->LEDCurrentLow<MinimumLEDCurrent?MinimumLEDCurrent:CurrentMode->LEDCurrentLow;//从低电流开始
	BreathCurrent+=incValue*pow(RampConfig->RampModeConf,GammaCorrectionValue);//使用幂函数模拟人眼亮度曲线	
	}
