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
#include "LinearDIM.h"
#include "AD5693R.h"
#include "logger.h"
#include "selftestlogger.h"
#include "runtimelogger.h"
#include "CurrentReadComp.h"

bool SensorRefreshFlag=false;
bool EnteredMainApp=false;
bool IsParameterAdjustMode=false;
extern int AutoOffTimer; //自动关机定时器

int main(void)
 {
 //初始化系统时钟
 CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
 CLKConfig.Bit.PA=1;
 CLKConfig.Bit.PB=1;
 CLKConfig.Bit.AFIO=1;
 CLKConfig.Bit.EXTI=1;
 CLKConfig.Bit.USART1=1;
 CLKConfig.Bit.PDMA=1;
 CLKConfig.Bit.ADC0 = 1;
 CLKConfig.Bit.GPTM1 = 1;
 CLKConfig.Bit.BKP = 1;
 CKCU_PeripClockConfig(CLKConfig,ENABLE);
 //初始化LSI和RTC
 while((PWRCU_CheckReadyAccessed() != PWRCU_OK)){}; //等待BKP可以访问
 RTC_LSILoadTrimData();//加载LSI的修正值
 RTC_DeInit();//复位RTC确保RTC不要在运行
 //外设初始化
 delay_init();//初始化systick
 ConsoleInit();//初始化串行
 SMBUS_Init();//初始化SMBUS 
 #ifdef EnableFirmwareSecure
 GPIO_DisableDebugPort();//关闭debug口
 CheckForFlashLock();//安全功能，检查程序区是否被锁定
 #endif
 EnableHBTimer();//启用系统心跳定时器
 CheckHBTimerIsEnabled();//对心跳定时器进行测试
 LED_Init();//启动LED管理器 
 FirmwareVersionCheck();//检查固件版本信息
 INA219_POR();//初始化INA219
 PORConfHandler();//初始化配置文件
 InternalADC_Init();//初始化内部ADC
 LoadCalibrationConfig();//加载电流补偿设置
 SideKeyInit();//初始化侧按按钮
 ModeSwitchInit();//模式选择挡位的配置初始化
 PStateInit();//电源状态初始化
 IsParameterAdjustMode=IsHostConnectedViaUSB();//检测电脑是否连接
 if(!IsParameterAdjustMode)LinearDIM_POR();//DAC校准(仅在正常运行模式下启动)
 LoggerHeader_POR();//事件日志记录器初始化
 RunLogModule_POR();//运行日志模块POR
 DriverLockPOR();//初始化上电锁定状态 
 ConsoleReconfigure();//自检完毕后输出配置信息
 PORResetFactoryDetect();//检测重置状态
 getSideKeyShortPressCount(true); //在进入主APP前清除按键信息
 if(!IsParameterAdjustMode)ResetLogEnableAfterPOST(); //非调参模式正常启动,重新打开记录器
 EnteredMainApp=true;//标记已进入主APP,不在定时器中断内处理LED控制器
 //主循环
 while(1)
   {	 
	 //处理shell事务(为了保证性能,在手电筒运行时会直接跳过所有的shell事务)	 
   if(SysPstatebuf.Pstate!=PState_LEDOn&&SysPstatebuf.Pstate!=PState_LEDOnNonHold) 
	   {
	   ShellProcUtilHandler();
		 //调参模式启用，禁用手电筒的所有运行逻辑，绿灯慢闪
	   if(IsParameterAdjustMode)
		    {
				#ifdef FlashLightOS_Init_Mode	
				SideKey_LogicHandler();//处理侧按按键事务
		    SideKeyTestDisplay();//侧按测试打印
				#endif
		    CurrentLEDIndex=29;//绿灯慢闪提示进入调参模式
	      if(!SensorRefreshFlag)continue;
				LEDMgmt_CallBack();//LED管理器
			  SensorRefreshFlag=false;
		    continue;
				}
		 }
   #ifndef FlashLightOS_Init_Mode	 
	 //处理手电筒自身的运行逻辑
	 DisplayBatteryValueHandler();//处理显示电池电量操作的事务
	 SideKey_LogicHandler();//处理侧按按键事务
	 ModeSwitchLogicHandler();//按侧按按键换挡的事务
	 PStateStateMachine();//处理电源状态切换的状态机 
   //传感器轮询模块
	 if(!SensorRefreshFlag)continue; //当前时间没到跳过下面的代码
	 GetAuxBuckCurrent();//获取辅助buck的电流
	 LowVoltageIndicate();//低电压检测
	 LEDShortCounter();//LED短路检测积分函数
	 RunTimeBatteryTelemetry();//测量电池状态
	 RunTimeDataLogging();//运行时的记录
	 LEDMgmt_CallBack();//LED管理器
	 NotifyUserForGearChgHandler();//指示用户挡位发生小幅度变动的函数
	 //传感器轮询结束，手电如果正在运行则跳过不需要的代码
	 SensorRefreshFlag=false;
	 //手电筒处于运行状态，跳过一些运行时不需要处理的浪费时间的函数
   if(SysPstatebuf.Pstate==PState_LEDOn||SysPstatebuf.Pstate==PState_LEDOnNonHold)continue;
   SystemRunLogProcessHandler(); //负责管理将日志写入到ROM的管理函数
	 AuroPowerOffTIM();//处理自动关机定时器
	 LoggerHeader_AutoUpdateHandler();//记录器自动更新头部数据	   	 
	 #else
	 SideKey_LogicHandler();//处理侧按按键事务
	 DoSelfCalibration(); //处理按键自身的校准操作
	 if(!SensorRefreshFlag)continue;
	 LEDMgmt_CallBack();//LED管理器
	 SensorRefreshFlag=false; //复位标志位
	 #endif
	 }
 return 0;
 }
