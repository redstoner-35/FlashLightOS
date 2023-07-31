#include "console.h"
#include "delay.h"
#include "cfgfile.h"
#include "PWMDIM.h"

extern bool IsParameterAdjustMode;

//设置占空比
void SetPWMDuty(float Duty)
 { 
 //计算reload Counter
 float buf;
 if(Duty>=100) //大于等于100%占空比
	 buf=(float)(SYSHCLKFreq / CfgFile.PWMDIMFreq);//载入100%占空比时的比较值
 else if(Duty>0) //大于0占空比
   {
	 buf=(float)(SYSHCLKFreq / CfgFile.PWMDIMFreq);//载入100%占空比时的比较值
	 buf*=Duty;
   buf/=(float)100;//计算reload值
	 }
 else buf=0;
 //写通道0比较寄存器后启用定时器
 HT_MCTM0->CH0CCR=(unsigned int)buf; 
 MCTM_CHMOECmd(HT_MCTM0,Duty>0?ENABLE:DISABLE);//根据占空比配置决定是否禁止定时器输出
 }

//检测主机USB是否连接(当主机连接时，PWM引脚会被主机的5V输入拉高,可以利用这个功能检测主机的存在)
FlagStatus IsHostConnectedViaUSB(void)
 {
 FlagStatus result;
 AFIO_GPxConfig(PWMO_IOB,PWMO_IOP, AFIO_FUN_GPIO);//GPIO功能
 GPIO_DirectionConfig(PWMO_IOG,PWMO_IOP,GPIO_DIR_IN);//配置为输入
 GPIO_InputConfig(PWMO_IOG,PWMO_IOP,ENABLE);//启用IDR 
 delay_ms(1);
 result=GPIO_ReadInBit(PWMO_IOG,PWMO_IOP);
 AFIO_GPxConfig(PWMO_IOB,PWMO_IOP, AFIO_FUN_MCTM_GPTM);//重新配置为定时器复用IO
 return result;
 }
 
//初始化定时器 
void PWMTimerInit(void)
 {
 TM_TimeBaseInitTypeDef MCTM_TimeBaseInitStructure;
 TM_OutputInitTypeDef MCTM_OutputInitStructure;
 MCTM_CHBRKCTRInitTypeDef MCTM_CHBRKCTRInitStructure;
 UartPost(Msg_info,"HostIf","Detecting host computer connection...."); 
 //通过PWM引脚检测主机的连接
 if(IsHostConnectedViaUSB())
   {
	 UartPost(Msg_info,"HostIf","Host computer connected,Driver will enter USB-Tuning mode."); 
	 IsParameterAdjustMode=true;
	 return;
	 } 
 //配置TBU(脉冲生成基本时基单元)
 UartPost(Msg_info,"MoonPWMDIM","Starting PWM Generator TIM...");
 MCTM_TimeBaseInitStructure.CounterReload = (SYSHCLKFreq / CfgFile.PWMDIMFreq) - 1; //计数分频值按照设置频率来
 MCTM_TimeBaseInitStructure.Prescaler = 0; //分频器禁用
 MCTM_TimeBaseInitStructure.RepetitionCounter = 0; //重复定时器禁用
 MCTM_TimeBaseInitStructure.CounterMode = TM_CNT_MODE_UP;
 MCTM_TimeBaseInitStructure.PSCReloadTime = TM_PSC_RLD_IMMEDIATE; //向上计数模式，计数器满后立即重载为0重新计数(CNT = (CNT + 1)%Maxvalue)
 TM_TimeBaseInit(HT_MCTM0, &MCTM_TimeBaseInitStructure);
 UartPost(Msg_info,"MoonPWMDIM","Time Base Unit has started,Dimming Freq is %.2fKHz.",(float)CfgFile.PWMDIMFreq/(float)1000);
 //配置通道0的输出级
 MCTM_OutputInitStructure.Channel = TM_CH_0; //定时器输出通道0
 MCTM_OutputInitStructure.OutputMode = TM_OM_PWM1; //PWM正输出模式（if CNT < CMP PWM=1 else PWM =0）
 MCTM_OutputInitStructure.Control = TM_CHCTL_ENABLE;
 MCTM_OutputInitStructure.ControlN = TM_CHCTL_DISABLE; //开启正极性输出，负极性禁用
 MCTM_OutputInitStructure.Polarity = TM_CHP_NONINVERTED;
 MCTM_OutputInitStructure.PolarityN = TM_CHP_NONINVERTED; //配置极性
 MCTM_OutputInitStructure.IdleState = MCTM_OIS_LOW;
 MCTM_OutputInitStructure.IdleStateN = MCTM_OIS_HIGH; //IDLE状态关闭
 MCTM_OutputInitStructure.Compare = 0; //默认输出0%占空比
 MCTM_OutputInitStructure.AsymmetricCompare = 0;  //非对称比较关闭
 TM_OutputInit(HT_MCTM0, &MCTM_OutputInitStructure);
 //配置刹车，锁定之类的功能
 MCTM_CHBRKCTRInitStructure.OSSRState = MCTM_OSSR_STATE_DISABLE; //OSSR(互补输出功能)关闭
 MCTM_CHBRKCTRInitStructure.OSSIState = MCTM_OSSI_STATE_ENABLE; //OSSI设置在定时器关闭后回到IDLE状态
 MCTM_CHBRKCTRInitStructure.LockLevel = MCTM_LOCK_LEVEL_OFF; //关闭锁定
 MCTM_CHBRKCTRInitStructure.Break0 = MCTM_BREAK_DISABLE;
 MCTM_CHBRKCTRInitStructure.Break0Polarity = MCTM_BREAK_POLARITY_LOW;  //禁用制动功能
 MCTM_CHBRKCTRInitStructure.AutomaticOutput = MCTM_CHAOE_ENABLE; //启用自动输出
 MCTM_CHBRKCTRInitStructure.DeadTime = 0; 
 MCTM_CHBRKCTRInitStructure.BreakFilter = 0; //死区时间和制动功能过滤器禁用
 MCTM_CHBRKCTRConfig(HT_MCTM0, &MCTM_CHBRKCTRInitStructure);
 //启动定时器TBU和比较通道1的输出功能
 TM_Cmd(HT_MCTM0, ENABLE); 
 MCTM_CHMOECmd(HT_MCTM0, ENABLE);
 }
