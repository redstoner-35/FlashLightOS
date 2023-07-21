#include "delay.h"
#include "console.h"
#include "modelogic.h"
#include "LEDMgmt.h"

/*LED闪烁的pattern(每阶段0.25秒)
0=熄灭，
1=绿色，
2=红色  
E=强制结束当前pattern
*/
#define LEDPatternSize 31
const char *LEDPattern[LEDPatternSize]=
 {
   "00",//LED熄灭 0
   "111122220000E",//红绿交错闪一次然后结束 1
	 "111",//绿灯常亮 2
	 "222",//红灯常亮 3 
	 "202020DD2020DD",//快速闪烁三次，暂停两秒，快速闪烁两次 #INA219初始化异常 4
	 "20202DD202020DD",//快速闪烁三次，暂停两秒，快速闪烁三次 #INA219测量失败异常 5
	 "2020DD20DD",//快速闪烁两次，暂停两秒，闪一次  #EEPROM连接异常 6
	 "2020DD2020DD",//快速闪烁两次，暂停两秒，闪两次 #EEPROM AES256解密失败 7
	 "1200210D",//红绿交替闪0.25秒，停0.5秒，绿红交替闪0.25秒，停1秒反复循环 #自检序列运行中 8
	 "2DD0DD",//红色灯慢闪1秒一循环，电力严重不足 9
	 "20202D10D",//红色灯快速闪三次，绿色灯闪一次，驱动过热闭锁 10
	 "20202D1010D",//红色灯快速闪三次，绿色灯闪两次，驱动输入过压保护 11
	 "20202D101010D",//红色灯快速闪三次，绿色灯闪三次，驱动输出过流保护 12
	 "2020D202020D",//快速闪烁两次，暂停两秒，闪三次，ADC异常 13
	 "20202D101001010D",//红色灯快速闪三次，绿色灯闪4次，LED开路 14
	 "20202D101001010010D",//红色灯快速闪三次，绿色灯闪5次，LED短路 15
	 "20202D10100101001010D",//红色灯快速闪三次，绿色灯闪6次，LED过热闭锁 16
   "202020D3030D10DD",//红色灯闪三次，橙色两次，绿色一次，驱动内部灾难性逻辑错误 17
	 "303010E",//橙色两次+绿色一次，进入战术模式 18
	 "30301010E",//橙色两次+绿色一次，退出战术模式 19
	 "20202020DD2020D",//快速闪烁四次，暂停两秒，闪两次 #DAC初始化失败 20
	 "2010201030103010DD",//红绿色交替闪烁两次，橙绿交替闪烁两次然后暂停2秒重复 #内部挡位逻辑错误 21
	 "20102010301030103010DD",//红绿色交替闪烁两次，橙绿交替闪烁3次然后暂停2秒重复 #电池过流保护 22
	 "333", //橙色常亮表示电池电量中等 23
	 "20202D10100101001010010D", // //红色灯快速闪三次，绿色灯闪7次，SPS自检异常 24
	 "22221111E", //红灯亮一下然后转到绿灯表示手电已经解锁 25
	 "11112222E", //绿灯亮一下然后转到红灯表示手电已被锁定 26
	 "202020E", //红灯快速闪三次表示手电被锁定 27
	 "202002020020DD",//红色灯快速闪烁5次，停1秒循环 28
	 "1DD0DD", //绿灯慢闪表示进入调参模式 29
	 NULL//结束符
 };
//变量
static int ConstReadPtr;
static int LastLEDIndex;
int CurrentLEDIndex;
static char LEDThermalBlinkTimer=20;//用于在温控介入时闪侧按指示灯
char *ExtLEDIndex = NULL; //用于传入的外部序列
static char *LastExtLEDIdx = NULL;
static char LEDDelayTimer=0; //侧按LED等待

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
	else if(SysPstatebuf.CurrentThrottleLevel<4)return false;//温控降档未介入
	else if(LastExtLEDIdx!=NULL)return false;//外部指定了LED闪烁内容
	else if(CurrentLEDIndex==2||CurrentLEDIndex==3)return true;
	else if(CurrentLEDIndex==23)return true; //电量提示
  //其余情况不执行
  return false;
	}

//LED控制器初始化
void LED_Init(void)
  {
	 int i;
	 UartPost(Msg_info,"LEDMgt","LED Controller Init Start...");
	 //配置GPIO(绿色LED)
   AFIO_GPxConfig(LED_Green_IOB,LED_Green_IOP, AFIO_FUN_GPIO);
   GPIO_DirectionConfig(LED_Green_IOG,LED_Green_IOP,GPIO_DIR_OUT);//配置为输出
   GPIO_ClearOutBits(LED_Green_IOG,LED_Green_IOP);//输出设置为0
	 //配置GPIO(红色LED)
   AFIO_GPxConfig(LED_Red_IOB,LED_Red_IOP, AFIO_FUN_GPIO);
   GPIO_DirectionConfig(LED_Red_IOG,LED_Red_IOP,GPIO_DIR_OUT);//配置为输出
   GPIO_ClearOutBits(LED_Red_IOG,LED_Red_IOP);//输出设置为0	 		
	 //初始化变量
	 ConstReadPtr=0;
	 LastLEDIndex=0;
	 CurrentLEDIndex=8;//自检序列运行中
	 for(i=0;i<LEDPatternSize;i++)if(LEDPattern[i]==NULL)break;
	 UartPost(Msg_info,"LEDMgt","Target LED Pattern Size:%d / %d",i+1,LEDPatternSize); 	
	 UartPost(Msg_info,"LEDMgt","LED Controller Has been started.");
	}
//在系统内控制LED的回调函数
void LEDMgmt_CallBack(void)
  {
	//检测到主机配置了新的LED状态
  if(CurrentLEDIndex!=LastLEDIndex||LastExtLEDIdx!=ExtLEDIndex)
	  {
		if(LastExtLEDIdx!=ExtLEDIndex)LastExtLEDIdx=ExtLEDIndex;
		if(CurrentLEDIndex!=LastLEDIndex)LastLEDIndex=CurrentLEDIndex;//同步index 
		LED_Reset();//复位LED状态机
		}
	//当手电开启，且温控介入后，侧按指示灯闪
	if(CheckForLEDOnStatus())	
	  {
		if(LEDThermalBlinkTimer==20)LEDThermalBlinkTimer=0; //时间到，翻转回去
		else if(LEDThermalBlinkTimer==0||LEDThermalBlinkTimer==2) //令所有LED熄灭后下个周期再动作
		  {
			LEDThermalBlinkTimer++;
			GPIO_ClearOutBits(LED_Green_IOG,LED_Green_IOP);
		  GPIO_ClearOutBits(LED_Red_IOG,LED_Red_IOP);
			return;
			}			
		else LEDThermalBlinkTimer++;//继续计时
		}
	//正常执行		
	if(LEDDelayTimer>0)//当前正在等待中
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
