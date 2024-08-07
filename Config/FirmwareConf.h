#ifndef _FirmwareConf_
#define _FirmwareConf_

//版本信息
#define HardwareMajorVer 3
#define HardwareMinorVer 0  //硬件版本号

#define MajorVersion 2
#define MinorVersion 4
#define HotfixVersion 15  //固件版本号

//固件模式配置
//#define FlashLightOS_Init_Mode //设定固件的编译模式，如果开启此注释则固件配置为初始化模式，此时将会允许工程师对FRU进行设置并允许校准功能

#define Firmware_DIY_Mode //是否启用DIY高级用户模式，在此模式下驱动所有功能可以使用，否则温控调整和日志清除和恢复功能会被禁用。
//#define Internal_Driver_Debug //是否启用驱动内部设备驱动的额外信息输出。
#define EnableFirmwareSecure //是否启用固件只读和CRC安全锁
//#define NoMoonDLCSelfTest //在debug模式下是否关闭月光小板的DAC自检（该选项仅debug模式有效）
#define HardwarePlatformString "Xtern Ripper" //硬件平台字符串信息，可以任意修改

//性能参数定义
#define HighCurrentRampSpeed 15 //在驱动进行爆闪等大电流阶跃的场合控制电流在切换边沿的斜率
#define MaximumBatteryPowerAllowed 200 //驱动允许的最大电池输入功率，单位W
#define MainBuckOffTimeOut 5 //主Buck在LED熄灭命令之后的延时值,输入8表示1秒
#define BreathTIMFreq 50 //负责生成平滑呼吸灯效果的定时器频率(单位Hz)，越高越平滑
#define GammaCorrectionValue 2.45   //亮度拟合曲线的gamma修正，根据不同LED有所不同
#define LVAlertCurrentLimit 4.3 //当低电压警告触发后，驱动最大的输出电流值(A)
#define ForceThermalControlCurrent 5.5 //当挡位的运行电流大于这个值时温控将会被强制启用不准关闭
#define MaxAllowedLEDCurrent 35 //驱动硬件熔断的最大输出电流设置(测得电流超过此电流的1.2倍将触发保护)
#define MinimumLEDCurrent 0.04 //最小的LED电流
#define BatteryCellCount 3 //默认情况下驱动使用的电池组中锂电池的串数(按照三元锂电池计算)
#define DeepsleepDelay 35 //驱动的出厂深度睡眠时间,35秒内没有操作则睡眠
#define ForceRequireLEDNTC //驱动的LED NTC热敏电阻强制要求存在,如果检测不到热敏电阻则驱动将自检失败
//#define SideKeyPolar_positive  //侧按的极性，保留此define表示侧按按键高有效，否则低有效

//FRU中LED类型的配置
#define CustomLEDName "DFEx-SuperLED+ FI1048D-Gen1" //如果指定驱动使用其他任意未指定型号的LED，则需要在此处填写LED名称
#define CustomLEDCode 0x7004 //如果指定驱动使用其他任意未指定型号的LED，则需要在此处填写该LED的类型代码

//#define Using_SBT90Gen2_LED //使用SBT90.2
//#define Using_SBT90R_LED //使用红色的SBT90
//#define Using_SBT70G_LED //使用绿色的SBT70
//#define Using_SBT70B_LED //使用蓝色的SBT70
//#define Using_Generic_3V_LED //使用其他任意未指定型号的3V LED
#define Using_Generic_6V_LED //使用其他任意未指定型号的6V LED

//挡位和操作逻辑配置
#define RESETPassword "12345" //固件的默认复位密码（在重置出厂时输入，可以单独更改）
#define EnableSideLocLED //启用定位LED，在驱动休眠过程中，侧按指示灯会发出微弱的绿光指示手电开关的位置
#define AMUTorchMode //保留此define则驱动的出厂开关机逻辑设置为阿木同款(单击开机长按关机)否则使用长按开关机
//#define DefaultRampMode //保留此define则驱动的出厂挡位配置为无极调光模式,否则为5挡位模式
#define EnableTurbo //出厂挡位组启用双击极亮挡位(此挡位按照100%电流输出)

//EEPROM配置(用于存储配置信息)
//#define Enable_Command_OTP_Lock  //是否启用系统对FRU区域进行永久写保护（OTP模式）的相关命令参数
#define EnableSecureStor //对于FM24C512 系统会使用Security sector存储FRU数据
#define UsingEE_24C512 //使用24C512
//#define UsingEE_24C1024 //使用24C1024

//自检运行参数
#define StartupAUXPSUPGCount 4 //启动过程中等待电源PG的等待时间
#define StartUpInitialVID 10 //启动过程中的初始VID值
#define StartupLEDVIDStep 1.0 //启动过程中LED VID上升的速度

//SPS功率级的温度和电流反馈参数配置
#define SPSIMONDiffOpGain 50 //SPS电流差分采样放大器的增益，单位*V/V
#define SPSIMONScale 5 //电流回报数值，单位uA/A
#define SPSIMONShunt 200 //SPS的电流回报分压电阻，单位欧姆
#define SPSTMONScale 8 //温度回报数值，单位mV/℃
#define SPSTMONStdVal 0.6 //SPS为0度时的温度，单位V

//温度测量设置
#define NTCT0ResK 10 //NTC电阻在标定温度(一般是25℃)x下面的阻值（单位KΩ）
#define NTCUpperResValueK 10 //NTC测温电路上面串联的电阻（单位KΩ）
#define NTCTRIMValue 0.5 //温度修正值，单位℃
#define NTCB 3950 //NTC热敏电阻的B值(V1.x 3450 V2.0 3950,注意:此处B值仅对debug模式下初始化FRU时有效，初始化后此处数值将被忽略)
#define NTCT0 25 //NTC电阻的标定温度，一般是25℃
#define SPSTRIMValue 0 //DrMOS测温的修正值，单位为℃

//电池功率计和输入电流环路限制参数设置
#define INA219ShuntOhm 0.5 //驱动的INA219功率级的分流电阻阻值(单位mΩ)
#define InputCurrentLimitTrip 29.5 //触发驱动电流输入的PID限制器的阈值
#define InputCurrentMaintain 15 //PID限制器目标维持的电流
#define InputCurrentLimitRelease 6 //除能驱动电流PID显示器的阈值
#define InplPIDKp 0.95 
#define InplPIDKi 1.8
#define InplPIDKd 0.65 //输入电流限制PID环

//辅助月光buck小板设置参数
#define AuxBuckIsensemOhm 15 //辅助月光buck小板的检流电阻阻值(单位mΩ) 
#define AuxBuckIsenADCGain PGA_Gain2to1 //设置输入增益为4X（fullscale=1.024V,Vsens=0.6V）

//#define AuxBuckADC_12BitMode //辅助小板的ADC使用12bit模式
#define AuxBuckADC_14BitMode //辅助小板的ADC使用14bit模式
//#define AuxBuckADC_16BitMode //辅助小板的ADC使用16bit模式(注意！以上三个define只能同时存在一个)

//ADC模拟电压参考电压
#define ADC_VRef 3.2990 //实际的ADC电压参考值

//试运行校准参数
#define TestRunAuxBuckLowMargen 3 //测试运行时辅助BUCK在低电流下的最大电流误差(单位%)
#define TestRunAuxBuckHighMargen 1 //测试运行时辅助BUCK在高电流下的最大电流误差(单位%)
#define TestRunMainBuckLowMargen 4 //测试运行时主BUCK在低电流下的最大电流误差(单位%)
#define TestRunMainBuckHighMargen 1 //测试运行时主BUCK在高电流下的最大电流误差(单位%)

#endif
