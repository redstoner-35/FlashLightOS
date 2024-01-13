/*
该文件负责根据用户指定的挡位配置(例如爆闪)
启用系统内的GPTM1生成精确的定时中断处理特
殊功能的handler。这些中断对于信标闪，爆闪
和摩尔斯电码发送等特殊功能至关重要。
*/

#include "modelogic.h"
#include "cfgfile.h"
#include "delay.h"
#include "runtimelogger.h"

bool TimerHasStarted=false;
bool TimerCanTrigger=false;
static short LVAlertReload; //低压警告模块的重载值
static short LVAlertTimer; //低压警告计数器
static LightModeDef TimerMode=LightMode_Flash;//用于快速查表的变量

//在低压告警触发的时候，显示低压警告发生的函数
static void DisplayLowVoltageAlert(void)
 {
 float OffTime,buf; //关闭的数值，单位秒	 
 int ReloadValue,ResetValue; //计算缓存,用于计算软件定时器的重装值和输出翻转为高的配置值
 //低压告警没有发生
 if(!RunLogEntry.Data.DataSec.IsLowVoltageAlert)
   {
   LVAlertTimer=0;
   return;
	 }
 //根据定时器模式获取闪烁定时器的配置值
 switch(TimerMode)
   {
	 case LightMode_BreathFlash://线性变频闪模式 
	 case LightMode_Flash: //变频闪模式
	 case LightMode_RandomFlash:OffTime=0.8;break;//各种闪烁模式，此时中断输出0.8秒
	 case LightMode_Ramp:OffTime=0.4;break;//无极调光模式,中断输出0.4秒
	 default:return; //其余模式不需要操作
	 }
 buf=OffTime*(float)LVAlertReload/10; //将关闭时间乘以每秒的中断数得到总共的中断数
 ReloadValue=(int)buf; //计算出软件定时器到指定的关闭时间的重装值
 buf=(OffTime+6)*(float)LVAlertReload/10; //将关闭时间+重置时间乘以每秒的中断数得到总共的中断数
 ResetValue=(int)buf-1;
 //软件定时器的实现部分
 if(LVAlertTimer>=ResetValue) //定时器自动重装载
    LVAlertTimer=0;
 else if(LVAlertTimer<ReloadValue)  //定时器小于装载值，强制将Flash设置为off使LED熄灭
    {
		SysPstatebuf.ToggledFlash=false;
		LVAlertTimer++;
		}
 else if(LVAlertTimer==ReloadValue)
    {
		SysPstatebuf.ToggledFlash=true; //在等于重载值的时候重置一次然后
		LVAlertTimer++;
		}
 else LVAlertTimer++;
 }

//定时器中断回调函数
void FlashTimerCallbackRoutine(void)
 {
 if(!TimerCanTrigger)return;//这里是为了确保主函数一定跑完再说
 switch(TimerMode)
   {
	 case LightMode_CustomFlash:CustomFlashHandler();break;//自定义闪模式，交给对应的函数去处理
	 case LightMode_BreathFlash:LinearFlashHandler();break;//线性变频闪模式，交给对应函数处理
	 case LightMode_Ramp:RampModeHandler();break;//无极调光模式下跳转到相应的函数去处理
	 case LightMode_Breath:BreatheStateMachine();break;//呼吸模式下，处理对应的状态机操作
	 case LightMode_RandomFlash:RandomFlashHandler();//执行handler，然后跳转到下面的翻转输出
	 case LightMode_Flash:SysPstatebuf.ToggledFlash=SysPstatebuf.ToggledFlash?false:true;break;//翻转输出形成爆闪
	 case LightMode_SOS:
	 case LightMode_MosTrans:MorseSenderStateMachine();break;//SOS和摩尔斯发送，跳到对应的handler
	 default:return;
	 }
 //处理低压警告
 DisplayLowVoltageAlert();
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
	 case LightMode_CustomFlash:Freq=CurrentMode->CustomFlashSpeed;break;//自定义闪模式按照指定频率
	 case LightMode_BreathFlash:Freq=(float)2*CurrentMode->RandStrobeMinFreq;LinearFlashReset();break;//呼吸变频闪，进行处理
	 case LightMode_Ramp:Freq=BreathTIMFreq;break;
	 case LightMode_Breath:Freq=BreathTIMFreq;break;//呼吸和无极调光模式下，定时器生成的中断频率配置为指定值
	 case LightMode_Flash:Freq=(float)2*CurrentMode->StrobeFrequency;break;//按照爆闪频率的2倍去配置
	 case LightMode_RandomFlash:Freq=(float)2*CurrentMode->RandStrobeMaxFreq;break;//按照最高频率的2倍去配置
	 case LightMode_SOS:
	 case LightMode_MosTrans:Freq=(float)1/CurrentMode->MosTransferStep;break;//SOS和摩尔斯发送按照指定的每阶频率，求倒数
	 default:TimerHasStarted=true; return;
	 }
 //计算定时器内重载10秒的时间
 LVAlertReload=(short)(10*Freq); //计算GPTM1定时器按照设定频率运行时每10秒所产生的中断数量
 TimerMode=CurrentMode->Mode;//取当前模式
 //开始设置
 TM_Cmd(HT_GPTM1, DISABLE);//短时间关闭一下定时器
 TimeBaseInit.Prescaler = 4799;                      // 48MHz->10KHz
 TimeBaseInit.CounterReload = (int)(10000/Freq)-1;                  // 10KHz->指定频率
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
