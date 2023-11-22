#include "modelogic.h"
#include "cfgfile.h"
#include "delay.h"

/*
本文件负责实现线性变频爆闪的功能，这个功能下，
每次都会配置定时器生成频率变化的闪烁
*/

#define MaxFreqHoldTime 3
#define MinFreqHoldTime 2

static int TimerReloadValue=999; //定时器重装载值
static int IncreseValue=20;
static char MaxFreqHoldTimer;
static bool IncreseDirection=true; //方向控制变量

//线性闪复位函数
void LinearFlashReset(void)
 {
 ModeConfStr *CurrentMode=GetCurrentModeConfig();//获取目前挡位信息
 if(CurrentMode!=NULL)//挡位配置不为NULL，执行配置
	 TimerReloadValue=(int)(10000/(CurrentMode->RandStrobeMinFreq*2))-1;//计算定时器重装值
 else
	 TimerReloadValue=999; //随机填个值
 IncreseDirection=true;
 IncreseValue=20;
 MaxFreqHoldTimer=0;
 }

//线性闪处理函数
void LinearFlashHandler(void)
 {
 ModeConfStr *CurrentMode; 
 int maxreload,minreload;
 //获取挡位配置
 CurrentMode=GetCurrentModeConfig();//获取目前挡位信息
 if(CurrentMode==NULL)return; //字符串为NULL
 maxreload=(int)(10000/(CurrentMode->RandStrobeMaxFreq*2)); 
 minreload=(int)(10000/(CurrentMode->RandStrobeMinFreq*2)); //计算最低定时器重装值
 //在闪烁的消隐阶段逐步减少频率
 if(!SysPstatebuf.ToggledFlash)
   {
	 if(MaxFreqHoldTimer>0)MaxFreqHoldTimer--; //最高频率定时器保持
	 else if(!IncreseDirection)//频率向下减少(定时器重装值增加)
	   {
		 if(TimerReloadValue==minreload) //已到频率下限
		   {
			 IncreseValue=30;
			 MaxFreqHoldTimer=MinFreqHoldTime*CurrentMode->RandStrobeMinFreq;//计时
		   IncreseDirection=true;
			 }
		 else if(TimerReloadValue>=minreload-IncreseValue) //距离顶部值还剩下30以内，加上剩余的数值
		   TimerReloadValue+=minreload-TimerReloadValue;
		 else //还有很大数值，继续加30
		   TimerReloadValue+=IncreseValue;
		 //逐步递减increase值
	   if(IncreseValue>20)IncreseValue-=4;
		 else IncreseValue=20;
		 }
	 else //频率向上增加(定时器重装值减少)
	   {
		 if(TimerReloadValue==maxreload) //已到频率上限
			 {
			 IncreseValue=30;
			 MaxFreqHoldTimer=MaxFreqHoldTime*CurrentMode->RandStrobeMaxFreq;//计时
		   IncreseDirection=false;
			 }
		 else if(TimerReloadValue<=maxreload+IncreseValue)//距离顶部值还剩下30以内，加上剩余的数值
		   TimerReloadValue-=TimerReloadValue-maxreload;
		 else //还有很大数值，继续-30
		   TimerReloadValue-=IncreseValue;	 
		 //逐步递增increase值
	   if(IncreseValue<100)IncreseValue+=4;
		 else IncreseValue=100;
		 }
	 //写GPTM定时器的寄存器
	 HT_GPTM1->CRR=TimerReloadValue-1;
	 }
 //控制输出
 SysPstatebuf.ToggledFlash=SysPstatebuf.ToggledFlash?false:true;
 }
