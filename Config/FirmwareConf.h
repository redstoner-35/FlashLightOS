#ifndef _FirmwareConf_
#define _FirmwareConf_

//性能参数定义
#define BreathTIMFreq 50 //负责生成平滑呼吸灯效果的定时器频率(单位Hz)，越高越平滑
#define GammaCorrectionValue 2.45   //亮度拟合曲线的gamma修正，根据不同LED有所不同
#define LVAlertCurrentLimit 8 //当低电压警告触发后，驱动最大的输出电流值(A)
#define FusedMaxCurrent 32 //驱动硬件熔断的最大输出电流设置(测得电流超过此电流的1.2倍将触发保护)
#define BatteryCellCount 3 //默认情况下驱动使用的电池组中锂电池的串数(按照三元锂电池计算)
#define DeepsleepDelay 40 //驱动的出厂深度睡眠时间,40秒内没有操作则睡眠

//挡位配置
//#define DefaultRampMode //保留此define则驱动的出厂挡位配置为无极调光模式,否则为5挡位模式
#define EnableTurbo //出厂挡位组启用双击极亮挡位(此挡位按照100%电流输出)

//EEPROM配置(用于存储配置信息)
#define UsingEE_24C512 //使用24C512
//#define UsingEE_24C256 //使用24C256

#endif
