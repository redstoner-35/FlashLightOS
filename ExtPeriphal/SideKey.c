#include "delay.h"
#include "SideKey.h"
#include "console.h"
#include "modelogic.h"
#include "Cfgfile.h"

//全局变量
static bool IsKeyPressed = false; //按键是否按下
static unsigned char KeyTimer[2];//计时器0用于按键按下计时，计时器1用于连按检测计时
static KeyEventStrDef Keyevent;

//侧按按键计时模块
void SideKey_TIM_Callback(void)
  {
	unsigned char buf;
	//定时器0（用于按键短按和长按计时）
	if(KeyTimer[0]&0x80)
	  {
		buf=KeyTimer[0]&0x7F;
		if(buf<(unsigned char)LongPressTime)buf++;
		KeyTimer[0]&=0x80;
		KeyTimer[0]|=buf; //将数值取出来，加1再写回去
		}
	else KeyTimer[0]=0; //定时器关闭
	//定时器1（用于按键连按检测）
  if(KeyTimer[1]&0x80)
	  {
		buf=KeyTimer[1]&0x7F;
		if(buf<(unsigned char)ContShortPressWindow)buf++;
		KeyTimer[1]&=0x80;
		KeyTimer[1]|=buf; //将数值取出来，加1再写回去
		}
	else KeyTimer[1]=0; //定时器关闭
	}

//初始化侧按键
void SideKeyInit(void)
  {
	CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
  EXTI_InitTypeDef EXTI_InitStruct;
	UartPost(Msg_info,"SideKey","Configuring Side-Key Process module...");
  //配置时钟打开GPIOC AFIO和EXTI系统
	CLKConfig.Bit.PC=1;
  CLKConfig.Bit.AFIO=1;
  CLKConfig.Bit.EXTI=1;
  CKCU_PeripClockConfig(CLKConfig,ENABLE);
	//配置GPIO
  AFIO_GPxConfig(ExtKey_IOB,ExtKey_IOP, AFIO_FUN_GPIO);//GPIO功能
  GPIO_DirectionConfig(ExtKey_IOG,ExtKey_IOP,GPIO_DIR_IN);//配置为输入
	GPIO_InputConfig(ExtKey_IOG,ExtKey_IOP,ENABLE);//启用IDR
	//配置GPIO中断
	AFIO_EXTISourceConfig(ExtKey_IOPN,ExtKey_IOB); //配置中断源
	EXTI_InitStruct.EXTI_Channel = ExtKey_EXTI_CHANNEL; //配置选择好的通道
  EXTI_InitStruct.EXTI_Debounce = EXTI_DEBOUNCE_ENABLE; 
  EXTI_InitStruct.EXTI_DebounceCnt = 5;  //启用去抖
  EXTI_InitStruct.EXTI_IntType = EXTI_BOTH_EDGE; //双边沿触发
  EXTI_Init(&EXTI_InitStruct);  
  EXTI_IntConfig(ExtKey_EXTI_CHANNEL, ENABLE); //启用对应的按键中断
  NVIC_EnableIRQ(ExtKey_EXTI_IRQn); //启用IRQ
	//初始化内容
	Keyevent.LongPressEvent=false;
	Keyevent.ShortPressCount=0;
	Keyevent.ShortPressEvent=false;
	Keyevent.PressAndHoldEvent=false;
	Keyevent.DoubleClickAndHoldEvent=false;
	UartPost(Msg_info,"SideKey","Key Process module has been started.");
	}

//侧按键回调处理函数
void ExitLowPowerMode(void);//负责退出低功耗模式的函数声明	
extern int DeepSleepTimer; //深度休眠计时器
	
void SideKey_Callback(void)
  {
  unsigned char time;
	//处理深度休眠唤醒和定时器数值重置的部分
	if(SysPstatebuf.Pstate==PState_DeepSleep)ExitLowPowerMode();
	if(CfgFile.DeepSleepTimeOut>0)DeepSleepTimer=CfgFile.DeepSleepTimeOut;
  else DeepSleepTimer=-1;//复位定时器
	//给内部处理变量赋值
	#ifdef SideKeyPolar_positive	
	if(GPIO_ReadInBit(ExtKey_IOG,ExtKey_IOP))
	#else	
	if(!GPIO_ReadInBit(ExtKey_IOG,ExtKey_IOP))	
  #endif
	  {
		IsKeyPressed = true;//标记按键按下
		if(KeyTimer[1]&0x80)KeyTimer[1]=0x80;//复位
		if(!(KeyTimer[0]&0x80))KeyTimer[0]=0x80;//启动计时
    }
	//按键松开
  else
	  {
		IsKeyPressed = false;
		time=KeyTimer[0]&0x7F;//从计时器取出按键按下时间
		KeyTimer[0]=0;//复位并关闭定时器0
		if(Keyevent.LongPressDetected||
		Keyevent.PressAndHoldEvent||
		Keyevent.DoubleClickAndHoldEvent)//如果已经检测到长按事件则下面什么都不做
		  {
			Keyevent.DoubleClickAndHoldEvent=false;
			Keyevent.PressAndHoldEvent=false;
			Keyevent.LongPressDetected=false;//清除检测到的表示
		  return;
	    }
		if(time<(unsigned char)LongPressTime)//短按事件发生      
			{
		  if(Keyevent.ShortPressCount<10)Keyevent.ShortPressCount++;//累加有效的短按次数
		  KeyTimer[1]=0x80;//启动短按完毕等待统计的计时器
		  }			
		}
	}	

//侧按键逻辑处理函数
void SideKey_LogicHandler(void)
  {		
	//如果按键释放等待计时器在计时的话，则重置定时器
  if(IsKeyPressed&&(KeyTimer[1]&0x80))KeyTimer[1]=0x80;
	//长按3秒的时间到
	if(IsKeyPressed&&KeyTimer[0]==0x80+(unsigned char)LongPressTime)
		{
		if(Keyevent.ShortPressCount==2)//双击+长按
		  {
			KeyTimer[1]=0; //关闭后部检测定时器
			Keyevent.ShortPressEvent=false;
			Keyevent.LongPressDetected=false;
			Keyevent.LongPressEvent=false;//短按和长按事件没发生
			Keyevent.DoubleClickAndHoldEvent=true;//双击+长按事件发生
		  Keyevent.PressAndHoldEvent=false;//短按一下再按住的事件没发生
			Keyevent.ShortPressCount=0;
			}
		else if(Keyevent.ShortPressCount==1)//短按+长按事件
		  {
			KeyTimer[1]=0; //关闭后部检测定时器
			Keyevent.ShortPressEvent=false;
			Keyevent.LongPressDetected=false;
			Keyevent.LongPressEvent=false;//短按和长按事件没发生
		  Keyevent.PressAndHoldEvent=true;//短按一下再按住的事件发生
			Keyevent.DoubleClickAndHoldEvent=false; //双击再按住的事件没发生
			Keyevent.ShortPressCount=0;
			}
		else //长按事件
		  {
			Keyevent.DoubleClickAndHoldEvent=false;
      Keyevent.PressAndHoldEvent=false;
			Keyevent.LongPressEvent=true;//长按事件发生
	    Keyevent.LongPressDetected=true;//长按检测到了  
			}
		KeyTimer[0]=0;//关闭定时器
		}
	//连续短按序列已经结束
	if(!IsKeyPressed&&KeyTimer[1]==0x80+(unsigned char)ContShortPressWindow)
	  {
		KeyTimer[1]=0;//关闭定时器1
		if(!Keyevent.LongPressDetected)	
		  Keyevent.ShortPressEvent=true;//如果长按事件已经生效，则松开开关时短按事件不生效
		else 
			Keyevent.LongPressDetected=false; //清除长按检测到的结果
		}
	}
//获取侧按键点按次数的获取函数
int getSideKeyShortPressCount(bool IsRemoveResult)
  {
  short buf;
	if(Keyevent.LongPressDetected)return 0;
	if(!Keyevent.ShortPressEvent)return 0;
	buf=Keyevent.ShortPressCount;
  if(IsRemoveResult)
	  {
		Keyevent.ShortPressEvent=false; //获取了短按结果之后复位
	  Keyevent.ShortPressCount=0;  //获取了短按连击次数后清零结果
		}
  return buf;		
	}
//获取侧按按键长按2秒事件的函数
bool getSideKeyLongPressEvent(void)
  {
	if(!Keyevent.LongPressEvent)return false;
	else Keyevent.LongPressEvent=false;
  return true;
	}
//获取侧按按键一直按下的函数
bool getSideKeyHoldEvent(void)
  {
	return Keyevent.LongPressDetected;
	}
//获取侧按按键短按一下立刻长按的函数
bool getSideKeyClickAndHoldEvent(void)
  {
	return Keyevent.PressAndHoldEvent;
	}
//获取侧按按键是否有双击并长按的事件
bool getSideKeyDoubleClickAndHoldEvent(void)
  {
	return Keyevent.DoubleClickAndHoldEvent;
	}
