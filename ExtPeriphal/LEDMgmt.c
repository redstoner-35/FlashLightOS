#include "delay.h"
#include "console.h"
#include "modelogic.h"
#include "Cfgfile.h"
#include "LEDMgmt.h"
#include "runtimelogger.h"
#include <string.h>

/*LED闪烁的pattern(每阶段0.25秒)
0=熄灭，
1=绿色，
2=红色  
E=强制结束当前pattern
*/
#define LEDPatternSize 33
const char *LEDPattern[LEDPatternSize]=
 {
   "00",//LED熄灭 0
   "1133220002010DE",//红黄绿分别亮一次然后绿色闪两次结束 1
	 "111",//绿灯常亮 2
	 "222",//红灯常亮 3 
	 "202020DD2020DDD",//快速闪烁三次，暂停两秒，快速闪烁两次 #INA219初始化异常 4
	 "20202DD202020DDD",//快速闪烁三次，暂停两秒，快速闪烁三次 #INA219测量失败异常 5
	 "2020DD20DDD",//快速闪烁两次，暂停两秒，闪一次  #EEPROM连接异常 6
	 "2020DD2020DDD",//快速闪烁两次，暂停两秒，闪两次 #EEPROM AES256解密失败 7
	 "1200210D",//红绿交替闪0.25秒，停0.5秒，绿红交替闪0.25秒，停1秒反复循环 #自检序列运行中 8
	 "2D0D",//红色灯慢闪1秒一循环，电力严重不足 9
	 "202020D10DD",//红色灯快速闪三次，绿色灯闪一次，驱动过热闭锁 10
	 "202020D1010DD",//红色灯快速闪三次，绿色灯闪两次，驱动输入过压保护 11
	 "202020D101010DD",//红色灯快速闪三次，绿色灯闪三次，驱动输出过流保护 12
	 "2020D202020DD",//快速闪烁两次，暂停1秒，闪三次，ADC异常 13
	 "202020D101001010DD",//红色灯快速闪三次，绿色灯闪4次，LED开路 14
	 "202020D101001010010DD",//红色灯快速闪三次，绿色灯闪5次，LED短路 15
	 "202020D10100101001010DD",//红色灯快速闪三次，绿色灯闪6次，LED过热闭锁 16
   "202020D3030D10DDD",//红色灯闪三次，橙色两次，绿色一次，驱动内部灾难性逻辑错误 17
	 "303010E",//橙色两次+绿色一次，进入战术模式 18
	 "30301010E",//橙色两次+绿色一次，退出战术模式 19
	 "20202020DD2020DDD",//快速闪烁四次，暂停两秒，闪两次 #DAC初始化失败 20
	 "2010201030103010DDD",//红绿色交替闪烁两次，橙绿交替闪烁两次然后暂停3秒重复 #内部挡位逻辑错误 21
	 "20102010301030103010DDD",//红绿色交替闪烁两次，橙绿交替闪烁3次然后暂停3秒重复 #电池过流保护 22
	 "333", //橙色常亮表示电池电量中等 23
	 "202020D10100101001010010DD", //红色灯快速闪三次，绿色灯闪7次，SPS自检异常 24
	 "22221111E", //红灯亮一下然后转到绿灯表示手电已经解锁 25
	 "11112222E", //绿灯亮一下然后转到红灯表示手电已被锁定 26
	 "202020E", //红灯快速闪三次表示手电被锁定 27
	 "202002020020DDD",//红色灯快速闪烁5次，停3秒循环 指示LED基板NTC异常 28
	 "1DD0DD", //绿灯慢闪表示进入调参模式 29
	 "20202020DD2020020DDD", //快速闪烁4次 暂停2秒,闪3次 PWM调光逻辑异常 30
	 "20202020DD202002020DDD",//快速闪烁四次，暂停2秒,闪4次 校准数据库异常 31
	 NULL//结束符
 };
//变量
char LEDModeStr[64]; //LED模式的字符串
static int ConstReadPtr;
static int LastLEDIndex;
static bool IsLEDInitOK=false; //LED是否初始化完毕
volatile int CurrentLEDIndex;
static char LEDThermalBlinkTimer=20;//用于在温控介入时闪侧按指示灯
char *ExtLEDIndex = NULL; //用于传入的外部序列
static char *LastExtLEDIdx = NULL;
static char LEDDelayTimer=0; //侧按LED等待
bool IsRedLED=false;//对于SBT90-R，侧按会亮红灯
extern bool ReverseTactalEnabled; //反向战术模式是否启用
 
//在无极调光启动的时候显示方向
void LED_DisplayRampDir(bool IsDirUp)
  {
	if(ExtLEDIndex!=NULL)return; //当前序列正在运行中
	LED_Reset();//复位LED管理器
  memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
  strncat(LEDModeStr,IsDirUp?"1030200E":"2030100E",sizeof(LEDModeStr)-1);//显示方向
	ExtLEDIndex=&LEDModeStr[0];//传指针过去		
	}
 
//往自定义LED缓存里面加上闪烁字符
void LED_AddStrobe(int count,const char *ColorStr) 
  {
	int gapcnt;
	if(count<=0)return; //输入的次数非法
	//开始显示
	gapcnt=0;
  while(count>0)
   {
	 //附加闪烁次数
	 strncat(LEDModeStr,ColorStr,sizeof(LEDModeStr)-1);	//使用黄色代表电压个位数
	 //每2次闪烁之间插入额外停顿方便用户计数
	 if(gapcnt==1)
	    {
		  gapcnt=0;
		  strncat(LEDModeStr,"0",sizeof(LEDModeStr)-1);
		  }
	 else gapcnt++;
	 //处理完一轮，减去闪烁次数
	 count--;
	 }
	}
//复位LED管理器从0开始读取
void LED_Reset(void)
  {
	LEDThermalBlinkTimer=20; //复位定时器
  GPIO_ClearOutBits(LED_Green_IOG,LED_Green_IOP);//输出设置为0
  GPIO_ClearOutBits(LED_Red_IOG,LED_Red_IOP);//输出设置为0	 	
	ConstReadPtr=0;//从初始状态开始读取
	}
	//执行温控介入提示逻辑前的检查
static bool CheckForLEDOnStatus(void)	
  {
	if(SysPstatebuf.Pstate!=PState_LEDOn&&SysPstatebuf.Pstate!=PState_LEDOnNonHold)return false;//LED熄灭
	else if(LEDPattern[LastLEDIndex<LEDPatternSize?LastLEDIndex:0][ConstReadPtr]=='\0')return false;//目前已经运行到序列的末尾，不能熄灭
	else if(SysPstatebuf.CurrentThrottleLevel<4&&!RunLogEntry.Data.DataSec.IsLowQualityBattAlert&&!ReverseTactalEnabled)return false;//反向战术模式温控和输入低电流警告未介入
	else if(LastExtLEDIdx!=NULL)return false;//外部指定了LED闪烁内容
	else if(CurrentLEDIndex==2||CurrentLEDIndex==3)return true;
	else if(CurrentLEDIndex==23)return true; //电量提示
  //其余情况不执行
  return false;
	}

//LED控制器初始化
const char *LEDControllerInitMsg="Side switch LED controller init %s.";	
	
void LED_Init(void)
  {
	 UartPost(Msg_info,"LEDMgt",(char *)LEDControllerInitMsg,"Start..");
	 //配置GPIO(绿色LED)
   AFIO_GPxConfig(LED_Green_IOB,LED_Green_IOP, AFIO_FUN_GPIO);
   GPIO_DirectionConfig(LED_Green_IOG,LED_Green_IOP,GPIO_DIR_OUT);//配置为输出
   GPIO_ClearOutBits(LED_Green_IOG,LED_Green_IOP);//输出设置为0
	 GPIO_DriveConfig(LED_Green_IOG,LED_Green_IOP,GPIO_DV_16MA);	//设置为16mA最大输出保证指示灯亮度足够
	 //配置GPIO(红色LED)
   AFIO_GPxConfig(LED_Red_IOB,LED_Red_IOP, AFIO_FUN_GPIO);
   GPIO_DirectionConfig(LED_Red_IOG,LED_Red_IOP,GPIO_DIR_OUT);//配置为输出
   GPIO_ClearOutBits(LED_Red_IOG,LED_Red_IOP);//输出设置为0	 		
	 GPIO_DriveConfig(LED_Red_IOG,LED_Red_IOP,GPIO_DV_16MA);	//设置为16mA最大输出保证指示灯亮度足够
	 //初始化变量
	 ConstReadPtr=0;
	 LastLEDIndex=0;
	 CurrentLEDIndex=8;//自检序列运行中
	 IsLEDInitOK=true; //LED GPIO初始化已完毕
	 UartPost(Msg_info,"LEDMgt",(char *)LEDControllerInitMsg,"done");
	}
//控制侧按LED实现LED微微发光的控制函数
void SideLEDWeakLitControl(bool IsEnabled)
  {
	if(IsEnabled)
	 {
	 if(!IsRedLED) //绿色LED
		 {		
		 GPIO_DirectionConfig(LED_Green_IOG,LED_Green_IOP,GPIO_DIR_IN);//配置为高阻输入
     GPIO_PullResistorConfig(LED_Green_IOG,LED_Green_IOP,GPIO_PR_UP);//打开LED的上拉电阻，这样的话就可以让侧按微微发光指示手电位置
		 }
	 else //红色LED
		 {
		 GPIO_DirectionConfig(LED_Red_IOG,LED_Red_IOP,GPIO_DIR_IN);//配置为高阻输入
     GPIO_PullResistorConfig(LED_Red_IOG,LED_Red_IOP,GPIO_PR_UP);//打开LED的上拉电阻，这样的话就可以让侧按微微发光指示手电位置						
		 }
	 }
	else //侧按定位LED关闭
	 {
	 GPIO_DirectionConfig(LED_Green_IOG,LED_Green_IOP,GPIO_DIR_OUT);
	 GPIO_DirectionConfig(LED_Red_IOG,LED_Red_IOP,GPIO_DIR_OUT);//配置为输出
	 GPIO_PullResistorConfig(LED_Green_IOG,LED_Green_IOP,GPIO_PR_DISABLE);	 
   GPIO_PullResistorConfig(LED_Red_IOG,LED_Red_IOP,GPIO_PR_DISABLE);	//关闭上拉电阻	 
	 }
	}
//在系统内控制LED的回调函数
void LEDMgmt_CallBack(void)
  {
	bool IsNeedToOffLED;
	//安全措施，保证GPIO配置后才执行LED操作
	if(!IsLEDInitOK)return;
	//检测到主机配置了新的LED状态
  if(CurrentLEDIndex!=LastLEDIndex||LastExtLEDIdx!=ExtLEDIndex)
	  {
		if(LastExtLEDIdx!=ExtLEDIndex)LastExtLEDIdx=ExtLEDIndex;
		if(CurrentLEDIndex!=LastLEDIndex)LastLEDIndex=CurrentLEDIndex;//同步index 
		if(LastExtLEDIdx==NULL&&LastLEDIndex==0&&CfgFile.EnableLocatorLED) //如果当前LED为熄灭状态且侧按定位功能开启，则让侧按LED微微闪烁
      SideLEDWeakLitControl(true);
		else //这些条件不满足，关闭侧按LED
		  {
      SideLEDWeakLitControl(false);
			LEDDelayTimer=0; //复位变量
			}
		LED_Reset();//复位LED状态机
		}
	//当手电开启，且温控介入后，侧按指示灯闪
	if(CheckForLEDOnStatus())	
	  {
		//根据温度控制指示
		if(LEDThermalBlinkTimer==0||(LEDThermalBlinkTimer==2&&SysPstatebuf.CurrentThrottleLevel>=4))IsNeedToOffLED=true;
		else if(SysPstatebuf.CurrentThrottleLevel>=4)IsNeedToOffLED=false; //如果温控指示灯触发则屏蔽后面的电池质量警告
		else if(RunLogEntry.Data.DataSec.IsLowQualityBattAlert&&LEDThermalBlinkTimer==4)IsNeedToOffLED=true;
		else IsNeedToOffLED=false;
		//指示灯闪的计时器
	  if(LEDThermalBlinkTimer==20)LEDThermalBlinkTimer=0; //时间到，翻转回去
		else if(IsNeedToOffLED) //令所有LED熄灭后下个周期再动作
		  {
			LEDThermalBlinkTimer++;
			GPIO_ClearOutBits(LED_Green_IOG,LED_Green_IOP);
		  GPIO_ClearOutBits(LED_Red_IOG,LED_Red_IOP);
			return;
			}			
		else LEDThermalBlinkTimer++;//继续计时
		}	
		//如果侧按LED启用则让侧按LED反复发光
	if(LastExtLEDIdx==NULL&&LastLEDIndex==0&&CfgFile.EnableLocatorLED)
	  {
		if(LEDDelayTimer<16)
		  {
			LEDDelayTimer++;
			if(!IsRedLED) //根据是否为红色LED控制上拉电阻
			  GPIO_PullResistorConfig(LED_Green_IOG,LED_Green_IOP,LEDDelayTimer<8?GPIO_PR_DISABLE:GPIO_PR_UP); 
			else
				GPIO_PullResistorConfig(LED_Red_IOG,LED_Red_IOP,LEDDelayTimer<8?GPIO_PR_DISABLE:GPIO_PR_UP); //控制上拉电阻
			}
		else LEDDelayTimer=0;
		}
	//正常执行		
	else if(LEDDelayTimer>0)//当前正在等待中
	  {
		LEDDelayTimer--;
		return;
		}
	if(LEDPattern[LastLEDIndex<LEDPatternSize?LastLEDIndex:0]==NULL)return;//指针为空，不执行	
	switch(LastExtLEDIdx==NULL?LEDPattern[LastLEDIndex<LEDPatternSize?LastLEDIndex:0][ConstReadPtr]:LastExtLEDIdx[ConstReadPtr])
	  {
		case 'D'://延时1秒
		  {
			ConstReadPtr++;//指向下一个数字
			LEDDelayTimer=8;
			break;
			}
		case '0'://LED熄灭
		  {
			GPIO_ClearOutBits(LED_Green_IOG,LED_Green_IOP);
		  GPIO_ClearOutBits(LED_Red_IOG,LED_Red_IOP);
			ConstReadPtr++;//指向下一个数字
			break;
			}
		case '1'://绿色
		  {
			GPIO_SetOutBits(LED_Green_IOG,LED_Green_IOP);
		  GPIO_ClearOutBits(LED_Red_IOG,LED_Red_IOP);
			ConstReadPtr++;//指向下一个数字
			break;
			}		
		case '2'://红色
		  {
			GPIO_ClearOutBits(LED_Green_IOG,LED_Green_IOP);
		  GPIO_SetOutBits(LED_Red_IOG,LED_Red_IOP);
			ConstReadPtr++;//指向下一个数字
			break;
			}	
		case '3'://红绿一起亮(橙色)
		  {
			GPIO_SetOutBits(LED_Green_IOG,LED_Green_IOP);
		  GPIO_SetOutBits(LED_Red_IOG,LED_Red_IOP);
			ConstReadPtr++;//指向下一个数字
			break;				
			}
		case 'E'://结束显示
		  {
			GPIO_ClearOutBits(LED_Green_IOG,LED_Green_IOP);
		  GPIO_ClearOutBits(LED_Red_IOG,LED_Red_IOP);
			if(LastExtLEDIdx!=NULL)ExtLEDIndex=NULL;//使用的是外部传入的pattern，所以说需要清除指针
			else CurrentLEDIndex=0;//回到熄灭状态
			ConstReadPtr=0;//回到第一个参数
			break;
			}	
		default:ConstReadPtr=0;//其他任何非法值，直接回到开始点
		}
	}
