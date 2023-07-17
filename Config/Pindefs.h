#ifndef _PinDefs_
#define _PinDefs_

/*整个工程所有的外设的引脚
定义都在这里了，你可以按照
需要去修改。
目前已使用引脚：
PA0 1 2 3 4 5 9 10 12 13 14
PB 0 1 2 3
PC10 11 12 

其中PA4-5预留给串口，PA12-13预留给SWDIO debug port
*/

//侧按按键LED
#define LED_Green_IOB GPIO_PB
#define LED_Green_IOG HT_GPIOB
#define LED_Green_IOP GPIO_PIN_1 //绿色LED引脚定义（PB1）

#define LED_Red_IOB GPIO_PB
#define LED_Red_IOG HT_GPIOB
#define LED_Red_IOP GPIO_PIN_0 //红色LED引脚定义（PB0）

//系统的软件I2C
#define IIC_SCL_IOB GPIO_PB
#define IIC_SCL_IOG HT_GPIOB
#define IIC_SCL_IOP GPIO_PIN_10 //SCL Pin=PB10

#define IIC_SDA_IOB GPIO_PB
#define IIC_SDA_IOG HT_GPIOB
#define IIC_SDA_IOP GPIO_PIN_11 //SDA Pin=PB11

//基于定时器的PWM输出引脚(用于月光档PWM调光)
#define PWMO_IOG HT_GPIOA
#define PWMO_IOB GPIO_PA
#define PWMO_IOP GPIO_PIN_14 //PWM Pin=PA14（MT-CH0）

//按键
#define ExtKey_IOB GPIO_PA
#define ExtKey_IOG HT_GPIOA
#define ExtKey_IOPN 9  //外部侧按按钮（PA9）

#define ExtKey_EN_IOB GPIO_PB
#define ExtKey_EN_IOG HT_GPIOB
#define ExtKey_EN_IOP GPIO_PIN_3  //外部侧按按钮锁存器使能引脚（PB3）这个脚主要是实现USB DFU相关电路所用

//3.3V辅助电源和LT3741芯片的使能
#define AUXPWR_EN_IOB GPIO_PB
#define AUXPWR_EN_IOG HT_GPIOB
#define AUXPWR_EN_IOP GPIO_PIN_2 //AUXPWR Pin=PB2

//负责测量LED电压电流，温度和SPS温度的ADC输入引脚
#define LED_Vf_ADC_Ch 0 //LED电压测量（PA0）
#define NTC_ADC_Ch 1  //LED温度测量模拟输入（PA1）
#define LED_If_Ch 2  //LED电流检测的IO（PA2）
#define SPS_Temp_Ch 3  //SPS温度检测的IO （PA3）

#endif
