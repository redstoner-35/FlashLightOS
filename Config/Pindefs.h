#ifndef _PinDefs_
#define _PinDefs_

/***************************************************
整个工程所有的外设的引脚定义都在这里了，你可以按照
需要去修改。但是要注意，有些引脚是不可以修改的因为是
外设的输出引脚。
******* 目前已使用引脚  *******
PA0 1 2 3 4 5 9 12 13 14
PB 0 1 2 3 10 11

******* 不可修改的引脚 *******
PA4-5：预留给USART1
PA12-13：预留给SWDIO debug port
PA14：预留给MCTM的PWM output channel
***************************************************/

//侧按按键LED
#define LED_Green_IOBank B 
#define LED_Green_IOPinNum 1 //绿色LED引脚定义（PB1）

#define LED_Red_IOBank B
#define LED_Red_IOPinNum 0 //红色LED引脚定义（PB0）

//系统的软件I2C
#define IIC_SCL_IOBank B
#define IIC_SCL_IOPinNum 10 //SCL Pin=PB10

#define IIC_SDA_IOBank B
#define IIC_SDA_IOPinNum 11 //SDA Pin=PB11

//基于定时器的PWM输出引脚(用于月光档PWM调光)
#define PWMO_IOBank A
#define PWMO_IOPinNum 14 //PWM Pin=PA14（MT-CH0）

//按键
#define ExtKey_IOBank A
#define ExtKey_IOPN 9  //外部侧按按钮（PA9）

#define ExtKeyLatch_IOBank B 
#define ExtKeyLatch_IOPN 3 //外部侧按按钮锁存器使能引脚（PB3）这个脚主要是实现USB DFU相关电路所用

//3.3V辅助电源和LT3741芯片的使能
#define AUXPWR_EN_IOBank B
#define AUXPWR_EN_IOPinNum 2 //AUXPWR Pin=PB2

//负责测量LED电压电流，温度和SPS温度的ADC输入引脚
#define LED_Vf_ADC_Ch 0 //LED电压测量（PA0）
#define NTC_ADC_Ch 1  //LED温度测量模拟输入（PA1）
#define LED_If_Ch 2  //LED电流检测的IO（PA2）
#define SPS_Temp_Ch 3  //SPS温度检测的IO （PA3）

#endif
