#ifndef _CfgFile_
#define _CfgFile_

#include "I2C.h"
#include "modelogic.h"
#include "FirmwareConf.h"

//电流补偿曲线节点设置
#define SPSCompensateTableSize 10 //SPS电流检测补偿表的节点数

//配置文件类型
typedef enum
{
Config_Main,
Config_Backup,
Config_XmodemRX
}cfgfiletype;

//配置文件结构体
typedef struct
{
 char HostName[20];
 char AdminAccountname[20];
 char AdminAccountPassword[16];
 char RootAccountPassword[16];
 int USART_Baud;
 int IdleTimeout;
 int DeepSleepTimeOut;
 //驱动硬件层面设定
 bool IsHoldForPowerOn; //选择是长按开机还是单击开机
 bool EnableLocatorLED;//是否启用侧按定位LED
 bool EnableRunTimeLogging;//是否启用运行时记录
 unsigned short PWMDIMFreq;//PWM调光频率
 bool IsDriverLockedAfterPOR;//驱动上电后是否保持自锁
 float LEDIMONCalThreshold[SPSCompensateTableSize];
 float LEDIMONCalGain[SPSCompensateTableSize]; //LED电流测量补偿	
 //挡位组配置
 ModeConfStr RegularMode[8];  //普通挡位
 ModeConfStr DoubleClickMode; //双击挡位
 ModeConfStr SpecialMode[4];  //特殊功能挡位
 char BootupModeNum;  //不记忆的情况下默认的挡位
 ModeGrpSelDef BootupModeGroup;//不记忆的情况下默认选择的模式组
 //温控设置
 char MOSFETThermalTripTemp;
 char LEDThermalTripTemp; //MOS管和LED的过热拉闸温度
 float ThermalPIDKp; 
 float ThermalPIDKi;
 float ThermalPIDKd; //PID控制的Kp Ki Kd
 float PIDTriggerTemp; //PID温控触发温度
 float PIDRelease; //PID温控释放温度
 float PIDTargetTemp; //PID目标温度
 float LEDThermalWeight;//LED温度和MOS温度对于PID输入的影响
 //电池电量保护设置
 float VoltageFull;
 float VoltageAlert;
 float VoltageTrip;  //电池满电，警告和自动关机点
 float VoltageOverTrip; //电池过压保护点
 float OverCurrentTrip;  //电池电流过流保护点
 //无极调光参数设置
 float DefaultLevel[13]; //默认等级
 bool IsRememberBrightNess[13]; //是否记忆亮度等级
 bool IsNoteLEDEnabled; //是否启用调光方向提示LED
}ConfFileStr;
//用来读写配置文件的联合体
typedef union
{
char StrBUF[sizeof(ConfFileStr)];
ConfFileStr ConfFileStr;
}ConfUnionDef;

//外部声明
#define CfgFileSize (((sizeof(CfgFile)+4)/16)+(((sizeof(CfgFile)+4)%16)?1:0))*16
#define CfgFile CfgFileUnion.ConfFileStr
extern ConfUnionDef CfgFileUnion;

//函数
void PORConfHandler(void);//上电初始化
void LoadDefaultConf(void);//加载默认配置
int WriteConfigurationToROM(cfgfiletype cfgtyp);//写配置
int CheckConfigurationInROM(cfgfiletype cfgtyp,unsigned int *CRCResultO);//检查ROM中的配置是否损毁
int ReadConfigurationFromROM(cfgfiletype cfgtyp);//读取配置
unsigned int ActiveConfigurationCRC(void);
void CheckForFlashLock(void);//检查程序区是否被锁定
void FirmwareVersionCheck(void);//检查固件版本

//硬件安全保护define
#if (FusedMaxCurrent > 50)
#error "Maxmium fused current should NOT exceed 50 amps!"
#endif

#endif
