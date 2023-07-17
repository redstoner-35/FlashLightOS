#include "delay.h"
#include "Pindefs.h"
#include "modelogic.h"
#include "console.h"
#include "Cfgfile.h"
#include "LEDMgmt.h"
#include "AD5693R.h"
#include "ADC.h"

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
	USART_TxCmd(HT_USART1, DISABLE);
  USART_RxCmd(HT_USART1, DISABLE); //关闭shell串口	
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
	HT_CKCU->GCCR&=0xFFFFF1FF;//关闭HSI HSE PLL
	//进入休眠状态
	SysPstatebuf.Pstate=PState_DeepSleep;
	__wfi(); //进入暂停状态
	while(SysPstatebuf.Pstate==PState_DeepSleep);
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
	while((HT_CKCU->CKST&0xF00)!=0x100){};//等待48MHz的PLL输出切换为时钟源
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
	USART_TxCmd(HT_USART1, ENABLE);
  USART_RxCmd(HT_USART1, ENABLE); //关闭shell串口	
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
	//启动完毕，回到待机模式
	SysPstatebuf.Pstate=PState_Standby;
	}
