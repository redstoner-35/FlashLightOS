#include "delay.h"
#include "Pindefs.h"
#include "modelogic.h"
#include "console.h"
#include "Cfgfile.h"
#include "LEDMgmt.h"
#include "AD5693R.h"
#include "PWMDIM.h"
#include "runtimelogger.h"
#include "ADC.h"

extern bool IsParameterAdjustMode;

//低功耗睡眠计时器、
int DeepSleepTimer=0;
bool DeepSleepTimerFlag=false;

void DeepSleepTimerCallBack(void)
  {
	if(AccountState!=Log_Perm_Guest)DeepSleepTimer=CfgFile.DeepSleepTimeOut;//console有用户登录
	else if(CfgFile.DeepSleepTimeOut==0)DeepSleepTimer=-1;//定时器关闭
	else if(DeepSleepTimer>0&&DeepSleepTimerFlag)
	    {
		  DeepSleepTimerFlag=false;
			DeepSleepTimer--; //开始计时
			}
	}

//进入低功耗休眠模式
void EnteredLowPowerMode(void)
  {
  CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
	DACInitStrDef DACPoffStr;
  int retry=0;
  //关闭系统外设
	LED_Reset();//复位LED状态机
  DACPoffStr.DACPState=DAC_Disable_HiZ;
	DACPoffStr.DACRange=DAC_Output_REF;
	DACPoffStr.IsOnchipRefEnabled=false; 
	if(!AD5693R_SetChipConfig(&DACPoffStr))return;//让DAC输出高阻关闭基准电压，进入掉电状态
	DisableHBTimer(); //关闭定时器
	ADC_DeInit(HT_ADC0);//将ADC复位
	USART_TxCmd(HT_USART1, DISABLE);
  USART_RxCmd(HT_USART1, DISABLE); //关闭shell串口	
	AFIO_GPxConfig(GPIO_PA,GPIO_PIN_4,AFIO_FUN_GPIO);
  AFIO_GPxConfig(GPIO_PA,GPIO_PIN_5,AFIO_FUN_GPIO); //配置为普通GPIO模式
	GPIO_PullResistorConfig(HT_GPIOA,GPIO_PIN_5,GPIO_PR_DISABLE);//关闭内部上拉电阻
	if(CfgFile.EnableLocatorLED)
	  {  
    GPIO_DirectionConfig(LED_Green_IOG,LED_Green_IOP,GPIO_DIR_IN);//配置为高阻输入
    GPIO_PullResistorConfig(LED_Green_IOG,LED_Green_IOP,GPIO_PR_UP);//打开绿色LED的上拉电阻，这样的话就可以让侧按微微发光指示手电位置
		}
	GPIO_DirectionConfig(HT_GPIOA,GPIO_PIN_4,GPIO_DIR_IN);
	GPIO_DirectionConfig(HT_GPIOA,GPIO_PIN_5,GPIO_DIR_IN); //将PA4-5配置为高阻GPIO避免TX和RX往外漏电
	GPIO_DirectionConfig(IIC_SCL_IOG,IIC_SCL_IOP,GPIO_DIR_IN);
	GPIO_DirectionConfig(IIC_SDA_IOG,IIC_SDA_IOP,GPIO_DIR_IN);//SCL SDA配置为高阻
	//关闭外设时钟
  CLKConfig.Bit.PB=1;
  CLKConfig.Bit.USART1=1;
  CLKConfig.Bit.PDMA=1;
  CLKConfig.Bit.MCTM0 = 1;
  CLKConfig.Bit.ADC0 = 1;
  CLKConfig.Bit.GPTM1 = 1;
  CKCU_PeripClockConfig(CLKConfig,DISABLE);
	//切换到32KHz时钟
	while(retry<500)//反复循环等待LSI稳定
	  {
		delay_ms(1);
		if(HT_CKCU->GCSR&0x20)break;
		retry++;
		}
	if(retry==500)return;
	retry=500;
  HT_CKCU->GCCR|=0x07; //令LSI作为系统时钟源
	while(--retry)if((HT_CKCU->CKST&0x07)==0x07)break; //等待LSI用于系统时钟源
	if(!retry)return;
	//按顺序关闭PLL，HSE，HSI
	HT_CKCU->GCCR&=0xFFFFFDFF;//关闭PLL
	while(HT_CKCU->GCSR&0x02);//等待PLL off		
	HT_CKCU->GCCR&=0xFFFFFBFF;//关闭HSE
	while(HT_CKCU->GCSR&0x04);//等待HSE off
	HT_CKCU->GCCR&=0xFFFFF7FF;//关闭HSI
	while(HT_CKCU->GCSR&0x08);//等待HSI off
	//进入休眠状态
	SysPstatebuf.Pstate=PState_DeepSleep;
	__wfi(); //进入暂停状态
	while(SysPstatebuf.Pstate==PState_DeepSleep);//等待
	}
//退出低功耗模式
void ExitLowPowerMode(void)
  {
	DACInitStrDef DACInitStr;
	CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
	//启用HSE并使用HSE通过PLL作为单片机的运行时钟
	HT_CKCU->GCCR|=0x400;//启用HSE
  while(!(HT_CKCU->GCSR&0x04));//等待HSE就绪
  HT_CKCU->PLLCFGR&=0xF81FFFFF;
  HT_CKCU->PLLCFGR|=0x3000000;	//设置Fout=8Mhz*6/1=48MHz
	HT_CKCU->GCCR|=0x200;//启用PLL
	while(!(HT_CKCU->GCSR&0x02));//等待PLL就绪
	HT_CKCU->GCCR&=0xFFFFFFF8; //令PLL作为系统时钟源	
	while((HT_CKCU->CKST&0x100)!=0x100){};//等待48MHz的PLL输出切换为时钟源
	//打开外设时钟
  CLKConfig.Bit.PB=1;
  CLKConfig.Bit.USART1=1;
  CLKConfig.Bit.PDMA=1;
  CLKConfig.Bit.MCTM0 = 1;
  CLKConfig.Bit.ADC0 = 1;
  CLKConfig.Bit.GPTM1 = 1;
  CKCU_PeripClockConfig(CLKConfig,ENABLE);
	//打开外设
	delay_init();//重新初始化Systick
	EnableHBTimer(); //打开定时器
  AFIO_GPxConfig(GPIO_PA,GPIO_PIN_4,AFIO_FUN_USART_UART);
  AFIO_GPxConfig(GPIO_PA,GPIO_PIN_5,AFIO_FUN_USART_UART); //将PA4-5配置为USART1复用IO
  GPIO_PullResistorConfig(HT_GPIOA,GPIO_PIN_5,GPIO_PR_UP);//启用上拉
  if(CfgFile.EnableLocatorLED) //仅在启用侧按后执行
	  {  
    GPIO_DirectionConfig(LED_Green_IOG,LED_Green_IOP,GPIO_DIR_OUT);//配置为输出
	  GPIO_PullResistorConfig(LED_Green_IOG,LED_Green_IOP,GPIO_PR_DISABLE);//关闭上拉电阻
		}
	USART_TxCmd(HT_USART1, ENABLE);
  USART_RxCmd(HT_USART1, ENABLE); //重新启用shell的串口	
	GPIO_DirectionConfig(IIC_SCL_IOG,IIC_SCL_IOP,GPIO_DIR_OUT);
	GPIO_DirectionConfig(IIC_SDA_IOG,IIC_SDA_IOP,GPIO_DIR_OUT);//SCL SDA配置为低阻输出
	//重新复位并初始化ADC
  InternalADC_QuickInit();
	//复位DAC
  DACInitStr.DACPState=DAC_Normal_Mode;
  DACInitStr.DACRange=DAC_Output_REF;
  DACInitStr.IsOnchipRefEnabled=true; //正常运行，基准启动
	if(!AD5693R_SetChipConfig(&DACInitStr))//重新初始化DAC
	  {
		SysPstatebuf.Pstate=PState_Error;
		SysPstatebuf.ErrorCode=Error_DAC_Logic;//出错了
		return;
		}
	//启动完毕，检查是否进入调参模式，否则回到待机模式
	if(IsHostConnectedViaUSB())IsParameterAdjustMode=true;
	if(RunLogEntry.Data.DataSec.IsFlashLightLocked)SysPstatebuf.Pstate=PState_Locked;//如果睡眠前手电筒被上锁则恢复到锁定模式
	else if(SysPstatebuf.ErrorCode==Error_None)SysPstatebuf.Pstate=PState_Standby;
	else SysPstatebuf.Pstate=PState_Error; //如果有没消除的错误则跳回Error状态
	}
