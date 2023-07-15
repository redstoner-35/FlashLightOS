#include "ht32.h"
#include "delay.h"
#include "console.h"
#include "cfgfile.h"
#include "I2C.h"
#include "INA219.h"
#include "SideKey.h"
#include "LEDMgmt.h"
#include "ADC.h"
#include "modelogic.h"
#include "PWMDIM.h"
#include "AD5693R.h"
#include "logger.h"
#include "runtimelogger.h"

bool SensorRefreshFlag=false;
bool EnteredMainApp=false;

int main(void)
 {
 //初始化系统时钟
 CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
 CLKConfig.Bit.PA=1;
 CLKConfig.Bit.PB=1;
 CLKConfig.Bit.PC=1;
 CLKConfig.Bit.AFIO=1;
 CLKConfig.Bit.EXTI=1;
 CLKConfig.Bit.USART1=1;
 CLKConfig.Bit.PDMA=1;
 CLKConfig.Bit.MCTM0 = 1;
 CLKConfig.Bit.ADC0 = 1;
 CLKConfig.Bit.GPTM1 = 1;
 CKCU_PeripClockConfig(CLKConfig,ENABLE);
 delay_init();//初始化systick
 //外设初始化
 ConsoleInit();//初始化串行 
 SMBUS_Init();//初始化SMBUS 
 EnableHBTimer();//启用系统心跳定时器
 CheckHBTimerIsEnabled();//对心跳定时器进行测试
 LED_Init();//启动LED管理器 
 INA219_POR();//初始化INA219
 PORConfHandler();//初始化配置文件
 InternalADC_Init();//初始化内部ADC
 SideKeyInit();//初始化侧按按钮
 ModeSwitchInit();//模式选择挡位的配置初始化
 PStateInit();//电源状态初始化
 PWMTimerInit();//PWM调光器初始化
 LinearDIM_POR();//DAC校准
 LoggerHeader_POR();//事件日志记录器初始化
 RunLogModule_POR();//运行日志模块POR
 DriverLockPOR();//初始化上电锁定状态
 ConsoleReconfigure();//自检完毕后输出配置信息
 //主循环
 EnteredMainApp=true;//标记已进入主APP,不在定时器中断内处理LED控制器
 while(1)
   {
	 //处理shell事务(为了保证性能,在手电筒运行时会直接跳过所有的shell事务)	 
   if(SysPstatebuf.Pstate!=PState_LEDOn&&SysPstatebuf.Pstate!=PState_LEDOnNonHold)
		  ShellProcUtilHandler();	 
	 //处理手电筒自身的运行逻辑
	 SideKey_LogicHandler();//处理侧按按键事务
	 ModeSwitchLogicHandler();//按侧按按键换挡的事务
	 PStateStateMachine();//处理电源状态切换的状态机 
   //传感器轮询模块
	 if(!SensorRefreshFlag)continue; //当前时间没到跳过下面的代码
	 LowVoltageIndicate();//低电压检测
	 LEDShortCounter();//LED短路检测积分函数
	 RunTimeBatteryTelemetry();//测量电池状态
	 RunTimeDataLogging();//运行时的记录
	 LoggerHeader_AutoUpdateHandler();//记录器自动更新头部数据
	 LEDMgmt_CallBack();//LED管理器
	 SensorRefreshFlag=false;
	 }
 return 0;
 }
