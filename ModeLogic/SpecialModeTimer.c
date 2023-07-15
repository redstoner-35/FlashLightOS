/*
该文件负责根据用户指定的挡位配置(例如爆闪)
启用系统内的GPTM1生成精确的定时中断处理特
殊功能的handler。这些中断对于信标闪，爆闪
和摩尔斯电码发送等特殊功能至关重要。
*/

#include "modelogic.h"
#include "cfgfile.h"
#include "delay.h"
#include "PWMDIM.h"
#include "runtimelogger.h"

bool TimerHasStarted=false;
bool TimerCanTrigger=false;
static short LVAlertReload; //低压警告模块的重载值
static short LVAlertTimer; //低压警告计数器
static LightModeDef TimerMode=LightMode_Flash;//用于快速查表的变量

//定时器中断回调函数
void FlashTimerCallbackRoutine(void)
 {
 bool IsProcessLVAlert;
 if(!TimerCanTrigger)return;//这里是为了确保主函数一定跑完再说
 switch(TimerMode)
   {
	 case LightMode_CustomFlash:CustomFlashHandler();break;//自定义闪模式，交给对应的函数去处理
	 case LightMode_Ramp:RampModeHandler();break;//无极调光模式下跳转到相应的函数去处理
	 case LightMode_Breath:BreatheStateMachine();break;//呼吸模式下，处理对应的状态机操作
	 case LightMode_Flash:SysPstatebuf.ToggledFlash=SysPstatebuf.ToggledFlash?false:true;break;//翻转输出形成爆闪
	 case LightMode_SOS:
	 case LightMode_MosTrans:MorseSenderStateMachine();break;//SOS和摩尔斯发送，跳到对应的handler
	 default:return;
	 }
 //处理低压警告
 if(TimerMode==LightMode_MosTrans)IsProcessLVAlert=false;
 else if(TimerMode==LightMode_SOS)IsProcessLVAlert=false;
 else if(TimerMode==LightMode_CustomFlash)IsProcessLVAlert=false;
 else if(TimerMode==LightMode_Flash)IsProcessLVAlert=false;
 else IsProcessLVAlert=true; //除了无极调光和呼吸档会显示以外其他都不显示
 if(RunLogEntry.Data.DataSec.IsLowVoltageAlert&&IsProcessLVAlert)
   {
	 //计时器计满了
	 if(LVAlertTimer==LVAlertReload)LVAlertTimer=0;
	 //计时器在前面4个，翻转
	 else if(LVAlertTimer<(LVAlertReload/32))
	   {
		 if(LVAlertTimer<(LVAlertReload/64))
			 SysPstatebuf.ToggledFlash=false;
		 else
			 SysPstatebuf.ToggledFlash=true; //形成闪烁提示用户没电了
		  LVAlertTimer++;
		 }
	 else LVAlertTimer++;
	 }
 else LVAlertTimer=0;// 低压告警没发生
 //定时器执行完毕，退出
 TimerCanTrigger=false;
 }
//关闭特殊功能定时器
void DisableFlashTimer(void)
 {
 //禁用定时器并清除标志位
 TM_Cmd(HT_GPTM1, DISABLE);
 TM_ClearFlag(HT_GPTM1, TM_FLAG_UEV);
 //将主灯的启用设置为Enable（常亮档需要的东西）
 SysPstatebuf.ToggledFlash=true;
 }	

void FlashTimerInitHandler(void)
 {
 ModeConfStr *CurrentMode;
 TM_TimeBaseInitTypeDef TimeBaseInit;
 float Freq;
 if(TimerHasStarted)return; //当前定时器已被设置,不需要再次配置
 LVAlertTimer=0;//复位低压警告计时器
 //取出挡位配置并且判断定时器的配置
 CurrentMode=GetCurrentModeConfig();
 if(CurrentMode==NULL) //返回空指针，退出
   {
	 TimerHasStarted=true;
	 return;
   }
 switch(CurrentMode->Mode)
   {
	 case LightMode_On:DisableFlashTimer();TimerHasStarted=true;return;//常亮档不需要启用定时器,关闭定时器后退出
	 case LightMode_CustomFlash:Freq=CurrentMode->CustomFlashSpeed*2;break;//自定义闪模式按照指定频率2倍，这样'1010'的序列才是10Hz
	 case LightMode_Ramp:Freq=BreathTIMFreq;break;
	 case LightMode_Breath:Freq=BreathTIMFreq;break;//呼吸和无极调光模式下，定时器生成的中断频率配置为指定值
	 case LightMode_Flash:Freq=(float)2*CurrentMode->StrobeFrequency;break;//按照爆闪频率的2倍去配置
	 case LightMode_SOS:
	 case LightMode_MosTrans:Freq=(float)1/CurrentMode->MosTransferStep;break;//SOS和摩尔斯发送按照指定的每阶频率，求倒数
	 default:TimerHasStarted=true; return;
	 }
 //计算定时器内重载6秒的时间
 LVAlertReload=(short)(8*Freq); //计算定时器计时的重载值
 TimerMode=CurrentMode->Mode;//取当前模式
 //开始设置
 TM_Cmd(HT_GPTM1, DISABLE);//短时间关闭一下定时器
 TimeBaseInit.Prescaler = 4799;                      // 48MHz->10KHz
 TimeBaseInit.CounterReload = (10000/(int)Freq)-1;                  // 10KHz->指定频率
 TimeBaseInit.RepetitionCounter = 0;
 TimeBaseInit.CounterMode = TM_CNT_MODE_UP;
 TimeBaseInit.PSCReloadTime = TM_PSC_RLD_IMMEDIATE;
 TM_TimeBaseInit(HT_GPTM1, &TimeBaseInit);
 TM_ClearFlag(HT_GPTM1, TM_FLAG_UEV);
 //配置好中断然后让定时器运行起来
 NVIC_EnableIRQ(GPTM1_IRQn);
 TM_IntConfig(HT_GPTM1,TM_INT_UEV,ENABLE);
 TM_Cmd(HT_GPTM1, ENABLE);
 TimerHasStarted=true;//定时器配置已经完毕
 }
