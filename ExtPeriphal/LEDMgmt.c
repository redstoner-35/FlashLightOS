#include "delay.h"
#include "console.h"
#include "LEDMgmt.h"


#pragma push
#pragma Otime//优化该函数使用3级别优化
#pragma O3

/*LED闪烁的pattern(每阶段0.25秒)
0=熄灭，
1=绿色，
2=红色  
E=强制结束当前pattern
*/
#define LEDPatternSize 29
const char *LEDPattern[LEDPatternSize]=
 {
   "00",//LED熄灭 0
   "111122220000E",//红绿交错闪一次然后结束 1
	 "11",//绿灯常亮 2
	 "22",//红灯常亮 3 
	 "20202000000000202000000000",//快速闪烁三次，暂停两秒，快速闪烁两次 #INA219初始化异常 4
	 "2020200000000020202000000000",//快速闪烁三次，暂停两秒，快速闪烁三次 #INA219测量失败异常 5
	 "2020000000002000000000",//快速闪烁两次，暂停两秒，闪一次  #EEPROM连接异常 6
	 "202000000000202000000000",//快速闪烁两次，暂停两秒，闪两次 #EEPROM AES256解密失败 7
	 "1200210000",//红绿交替闪0.25秒，停0.5秒，绿红交替闪0.25秒，停1秒反复循环 #自检序列运行中 8
	 "222222000000",//红色灯慢闪三秒一循环，电力严重不足 9
	 "202020000100000",//红色灯快速闪三次，绿色灯闪一次，驱动过热闭锁 10
	 "20202000010100000",//红色灯快速闪三次，绿色灯闪两次，驱动输入过压保护 11
	 "2020200001010100000",//红色灯快速闪三次，绿色灯闪三次，驱动输出过流保护 12
	 "20200000000020202000000000",//快速闪烁两次，暂停两秒，闪三次，ADC异常 13
	 "2020200001010010100000",//红色灯快速闪三次，绿色灯闪4次，LED开路 14
	 "2020200001010010100100000",//红色灯快速闪三次，绿色灯闪5次，LED短路 15
	 "202020000101001010010100000",//红色灯快速闪三次，绿色灯闪6次，LED过热闭锁 16
   "2020200003030000010",//红色灯闪三次，橙色两次，绿色一次，驱动内部灾难性逻辑错误 17
	 "303010E",//橙色两次+绿色一次，进入战术模式 18
	 "30301010E",//橙色两次+绿色一次，退出战术模式 19
	 "2020202000000000202000000000",//快速闪烁四次，暂停两秒，闪两次 #DAC初始化失败 20
	 "201020103010301000000000",//红绿色交替闪烁两次，橙绿交替闪烁两次然后暂停2秒重复 #内部挡位逻辑错误 21
	 "2010201030103010301000000000",//红绿色交替闪烁两次，橙绿交替闪烁3次然后暂停2秒重复 #电池过流保护 22
	 "33", //橙色常亮表示电池电量中等 23
	 "20202000010100101001010010000", // //红色灯快速闪三次，绿色灯闪7次，SPS自检异常 24
	 "22221111E", //红灯亮一下然后转到绿灯表示手电已经解锁 25
	 "11112222E", //绿灯亮一下然后转到红灯表示手电已被锁定 26
	 "202020E", //红灯快速闪三次表示手电被锁定 27
	 NULL//结束符
 };
//变量
static int ConstReadPtr;
static int LastLEDIndex;
int CurrentLEDIndex;
char *ExtLEDIndex = NULL; //用于传入的外部序列
static char *LastExtLEDIdx = NULL;

 //复位LED管理器从0开始读取
void LED_Reset(void)
  {
  GPIO_ClearOutBits(LED_Green_IOG,LED_Green_IOP);//输出设置为0
  GPIO_ClearOutBits(LED_Red_IOG,LED_Red_IOP);//输出设置为0	 	
	ConstReadPtr=0;//从初始状态开始读取
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
	 UartPost(Msg_info,"LEDMgt","Loaded Valid LED Pattern Count:%d",i+1);
	 UartPost(Msg_info,"LEDMgt","Target LED Pattern Size:%d",LEDPatternSize); 	
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
	//开始操作LED
	if(LEDPattern[LastLEDIndex<LEDPatternSize?LastLEDIndex:0]==NULL)return;//指针为空，不执行
	switch(LastExtLEDIdx==NULL?LEDPattern[LastLEDIndex<LEDPatternSize?LastLEDIndex:0][ConstReadPtr]:LastExtLEDIdx[ConstReadPtr])
	  {
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
#pragma pop
