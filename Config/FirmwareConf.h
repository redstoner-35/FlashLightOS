#ifndef _FirmwareConf_
#define _FirmwareConf_

//固件模式配置
#define Firmware_DIY_Mode //是否启用DIY高级用户模式，在此模式下驱动所有功能可以使用，否则温控调整和日志清除和恢复功能会被禁用。
//#define FlashLightOS_Debug_Mode //是否启用debug模式，此时驱动将会禁用部分自检项目以及低电量关机功能，并且强制使用工厂配置
//#define Internal_Driver_Debug //是否启用驱动内部设备驱动的额外信息输出。
#define HardwarePlatformString "DiffTorch Extreme(Xtern Ripper)" //硬件平台字符串信息，可以任意修改

//性能参数定义
#define BreathTIMFreq 50 //负责生成平滑呼吸灯效果的定时器频率(单位Hz)，越高越平滑
#define GammaCorrectionValue 2.45   //亮度拟合曲线的gamma修正，根据不同LED有所不同
#define LVAlertCurrentLimit 8 //当低电压警告触发后，驱动最大的输出电流值(A)
#define MaxAllowedLEDCurrent 32 //驱动硬件熔断的最大输出电流设置(测得电流超过此电流的1.2倍将触发保护)
#define BatteryCellCount 3 //默认情况下驱动使用的电池组中锂电池的串数(按照三元锂电池计算)
#define DeepsleepDelay 40 //驱动的出厂深度睡眠时间,40秒内没有操作则睡眠
//#define ForceRequireLEDNTC //驱动的LED NTC热敏电阻强制要求存在,如果检测不到热敏电阻则驱动将自检失败
#define SideKeyPolar_positive  //侧按的极性，保留此define表示侧按按键高有效，否则低有效
//#define Using_SBT90R //特殊模式，使用SBT90红光LED
#define UsingLED_3V
//#define UsingLED_6V  //选择驱动的LED额定电压是3V还是6V

//挡位配置
//#define DefaultRampMode //保留此define则驱动的出厂挡位配置为无极调光模式,否则为5挡位模式
#define EnableTurbo //出厂挡位组启用双击极亮挡位(此挡位按照100%电流输出)

//EEPROM配置(用于存储配置信息)
#define EnableSecureStor //对于FM24C512 系统会使用Security sector存储数据
#define UsingEE_24C512 //使用24C512
//#define UsingEE_24C256 //使用24C256

//SPS功率级的温度和电流反馈参数配置
#define SPSIMONDiffOpGain 50 //SPS电流差分采样放大器的增益，单位*V/V
#define SPSIMONScale 5 //电流回报数值，单位uA/A
#define SPSIMONShunt 200 //SPS的电流回报分压电阻，单位欧姆

#define SPSTMONScale 8 //温度回报数值，单位mV/℃
#define SPSTMONStdVal 0.6 //SPS为0度时的温度，单位V

//NTC温度测量设置
#define NTCUpperResValueK 10 //NTC测温电路上面串联的电阻（单位KΩ）
#define NTCTRIM 0.5 //温度修正值，单位℃
#define NTCB 3450 //NTC热敏电阻的B值
#define NTCT0 25 //NTC电阻的标定温度，一般是25℃

#endif
