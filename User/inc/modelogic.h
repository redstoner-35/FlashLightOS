#ifndef _ModeLogic_
#define _ModeLogic_

#include <stdbool.h>
#include "INA219.h"
#include "FirmwareConf.h"

/*安全保护用的define，不允许修改！*/
#if (BreathTIMFreq < 20 || BreathTIMFreq >50)
 #error "Breath Timer Frequency should be set in range of 20-50Hz!"
#endif

//挡位模式enum
typedef enum
 {
 LightMode_On,//常亮
 LightMode_Flash,//爆闪
 LightMode_SOS,//SOS
 LightMode_MosTrans,//摩尔斯代码发送
 LightMode_Breath,//呼吸模式(参数调整正确时为信标模式)
 LightMode_Ramp, //无极调光模式
 LightMode_CustomFlash, //自定义闪模式
 LightMode_None   //无模式,不会出现在挡位配置里面，程序用于检测错误的
 }LightModeDef;

//挡位组的enum
typedef enum
 {
 ModeGrp_Regular, //普通挡位组
 ModeGrp_DoubleClick, //双击挡位组
 ModeGrp_Special,  //三击进入的特殊挡位功能组
 ModeGrp_None //不会在实际的挡位里面存在但是会被程序使用用来做错误标记
 }ModeGrpSelDef;	
//电源状态的enum
typedef enum
 {
 PState_DeepSleep, //深度睡眠（fcpu=32Khz,关闭所有外设）
 PState_Error, //错误状态
 PState_Standby,  //待机状态
 PState_NonHoldStandBy,  //战术模式待机状态
 PState_LEDOnNonHold,
 PState_LEDOn,  //LED点亮的模式(保持)
 PState_Locked  //锁定状态
 }PStateDef;	
//错误代码Enum
typedef enum
 {
 Error_None, //无错误
 Error_SPS_TMON_Offline, //智能功率级温度检测下线
 Error_SPS_CATERR, //智能功率级反馈灾难性错误
 Error_SPS_ThermTrip,  //智能功率级过热保护
 Error_LED_Open,  //灯珠开路
 Error_LED_Short,  //灯珠短路
 Error_LED_OverCurrent,  //灯珠过流保护
 Error_LED_ThermTrip, //灯珠过热保护
 Error_PWM_Logic,  //PWM逻辑异常	 
 Error_DAC_Logic,  //DAC异常
 Error_ADC_Logic,  //ADC异常
 Error_Thremal_Logic,//温度控制逻辑异常
 Error_Input_OVP,  //输入过压保护
 Error_Input_UVP,  //输入欠压保护
 Error_Mode_Logic,  //挡位逻辑错误
 Error_Input_OCP  //输入过流故障
 }SystemErrorCodeDef;	
 
//每个挡位的结构体
typedef struct
 {
 char ModeName[16];//挡位名称
 char MosTransStr[32];//摩尔斯代码发送的字符串
 char CustomFlashStr[32];//自定义闪的字符串序列
 float LEDCurrentHigh;//该常亮挡位的LED电流(对于呼吸闪烁，则是最大电流)
 float LEDCurrentLow;//信标和呼吸闪烁模式的最低电流
 float CurrentRampUpTime;//呼吸模式时，电流从最小到最大的爬升时间，单位(s)
 float CurrentRampDownTime;//呼吸模式时，电流从最大到最小的爬升时间，单位(s)
 float MaxCurrentHoldTime;//呼吸模式时，电流在最大值的保持时间，单位(s)
 float MinCurrentHoldTime;//呼吸模式时，电流在最小值的保持时间，单位(s)
 float RampModeSpeed; //无极调光模式时电流上升下降的速度，单位(s) 
 float MosTransferStep;//摩尔斯代码发送的Step,单位(秒)
 float StrobeFrequency;//爆闪频率
 float CustomFlashSpeed;//自定义闪速度
 short PowerOffTimer;//定时关机计时器
 bool IsModeEnabled;//挡位是否启用
 bool IsModeAffectedByStepDown;//挡位是否受温控影响
 bool IsModeHasMemory;//该挡位是否记忆(如果该选项为false，则关闭手电筒后挡位会自动回到你指定的第一挡位)
 LightModeDef Mode;//该挡位的操作模式
 }ModeConfStr;

//挡位切换控制的结构体
typedef struct
 {
 unsigned char RegularGrpMode; //标准挡位组当前的数值
 unsigned char SpecialGrpMode; //特殊挡位组的记忆值
 ModeGrpSelDef ModeGrpSel;  //模式组的选择
 }CurrentModeStr;
//库仑计结构体
typedef struct 
 {
 bool IsLearningEnabled; //是否开启自学习模式
 bool IsCalibrationDone; //当前电池的数据是否有效
 float DesignedCapacity; //电池的设计容量
 }BatteryCapacityGaugeStrDef;
//驱动内部遥测的结构体
typedef struct
 {
 float BattVoltage;
 float BattCurrent;
 float BattPower;  //电池端参数
 float LEDTemp;
 float LEDVf;
 float LEDIf;   //LED参数
 float SPSTemp;   //SPS温度
 bool BatteryVoltageAlert;
 bool BatteryVoltageUVLO;
 bool BatteryVoltageOVLO;
 bool BatteryOverCurrent; //电池过电流
 }DrivertelemStr;
//开关切换控制的结构体
typedef struct
 {
 PStateDef Pstate; //电源状态
 SystemErrorCodeDef ErrorCode; //错误代码
 bool ToggledFlash; //是否点亮主灯(由定时器控制)
 float Duty;//PWM调光模式的占空比
 float TargetCurrent;//目标电流
 bool IsLinearDim;//是否用的线性调光
 bool IsLEDShorted;//LED是否短路
 float CurrentDACVID;//当前DAC的VID
 float CurrentThrottleLevel;//当前降频等级
 }SYSPStateStrDef;	

//外部函数
SystemErrorCodeDef TurnLightONLogic(INADoutSreDef *BattOutput);//开灯自检操作
void TurnLightOFFLogic(void);//关灯操作
void RuntimeModeCurrentHandler(void);//运行过程中的挡位电流计算
void DisableFlashTimer(void);//禁用闪烁定时器 
void FlashTimerInitHandler(void);//配置特殊功能定时器 
void BreatheStateMachine(void);//呼吸信标闪的状态机 
void MorseSenderStateMachine(void);//摩尔斯代码发送的状态机
void ResetBreathStateMachine(void);//重置呼吸闪状态机
void MorseSenderReset(void);//重置摩尔斯代码的状态机
void RunTimeBatteryTelemetry(void);//电池状态遥测的模块
void EnteredLowPowerMode(void);//进入深度睡眠
void DeepSleepTimerCallBack(void);//进入深度睡眠的定时器
void RampModeHandler(void);//无极调光模式的handler
void ResetRampMode(void);//重置无极调光状态机
int CheckForStringCanBeSentViaMorse(char *StringIN);//检查传入的自定义字符串是否包含非法内容 
void ResetCustomFlashControl(void);//复位自定义闪模块
void CustomFlashHandler(void);//自定义闪处理模块
int CheckForCustomFlashStr(char *Str);//自定义闪检测字符串是否合法
void LEDShortCounter(void);//短路检测积分函数 
void LowVoltageIndicate(void);//低电压检测

//外部引用
extern CurrentModeStr CurMode;//当前模式的结构体 
extern SYSPStateStrDef SysPstatebuf;//当前系统电源状态的结构体
extern bool TimerHasStarted; //标志位，GPTM1是否启用
extern float BreathCurrent;//由呼吸状态机生成的呼吸电流
extern INADoutSreDef RunTimeBattTelemResult;//电池反馈结果
extern bool TimerCanTrigger;//内部flag 保证检测逻辑跑完定时器才能触发 
extern int DeepSleepTimer;//深度睡眠定时器
extern float FusedMaxCurrent;//熔断的最大电流

//函数
void ResetPowerOffTimerForPoff(void);//挡位复位时自动重置定时器
void AutoPowerOffTimerHandler(void);//自动关机计时器的处理
void DisplayUserWhenTimerOff(void);
void DisplayUserWhenTimerOn(void);//当用户使能或者除能自动关机定时器时，指示用户定时器的时间
void DriverLockPOR(void);//初始化上电锁定设置
void RunTimeErrorReportHandler(SystemErrorCodeDef ErrorCode);//系统运行中出现错误时负责提交日志并关闭输出的模块
void SideLED_GenerateModeInfoPattern(void);//控制侧按LED生成提示序列的模块
void ModeNoMemoryRollBackHandler(void);//关闭手电后执行非记忆逻辑
void ModeSwitchInit(void);//初始化挡位设置
void RestoreFactoryModeCfg(void);//复位挡位设置
void ModeSwitchLogicHandler(void);//模式开关配置
void PStateInit(void);//初始化PState
void PStateStateMachine(void);//电源状态的状态机
void SetAUXPWR(bool IsEnabled);//设置辅助电源的输出
void EnteredLowPowerMode(void);//
ModeConfStr *GetCurrentModeConfig(void);//获取当前挡位设置 
ModeConfStr *GetSelectedModeConfig(ModeGrpSelDef ModeGrpSel,int index);//根据传入的挡位设置得到对应挡位的结构体指针 

#endif
