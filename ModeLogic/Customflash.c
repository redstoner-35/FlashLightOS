/*
该文件允许用户使用1-9和A以及空格的组合创建自定义
闪烁的pattern.这个功能对于某些领域特殊的需要非常
有用。
//字符串释义如下:
1-9分别对应挡位的10-90%电流
'A'对应100%电流。（这些字符串会让灯珠点亮并按照指定电流工作）
'B'表示随机决定是否继续执行后面的指令
'-'表示让灯珠熄灭1个周期
'R'表示让灯珠以随机的亮度点亮
'S'表示让灯珠强制熄灭并且永久停止执行此序列，直到换挡
'WXYZ'分别延时0.5-1秒-2秒,4秒
'T'表示序列在奇数次执行时不执行后面的内容，偶数次则执行
*/

#include "modelogic.h"
#include "cfgfile.h"
#include "delay.h"
#include "runtimelogger.h"
#include "console.h"
#include <math.h>
#include <stdlib.h>

static bool IsEndToggle=false;//循环结束操作除能
static char StrPtr=0;
static short CustomFlashTimer;//实现延时功能用于计时的变量
extern float BreathCurrent; //调用呼吸电流标志位控制电流

//复位状态机
void ResetCustomFlashControl(void)
  {
	IsEndToggle=false;
	SysPstatebuf.ToggledFlash=true;//恢复点亮
	BreathCurrent=0; 
	StrPtr=0; //复位状态机
	}

//检查传入的字符串数据是否合法
int CheckForCustomFlashStr(char *Str)
  {
	int i;
	bool result;
	if(Str==NULL)return 0;
	for(i=0;i<32;i++)
		{
		if(Str[i]=='U'||Str[i]=='B')result=true;
		else if(Str[i]=='S')result=true;
		else if(Str[i]=='R'||Str[i]=='T')result=true;
    else if(Str[i]=='A'||Str[i]=='-')result=true;
		else if(Str[i]>='W'&&Str[i]<='Z')result=true;
		else if(Str[i]>='1'&&Str[i]<='9')result=true;
		else if(Str[i]=='\0')return -1;//检查到末尾空字符，检查成功
    else result=false;//非法字符
		//检测到非法字符,立即跳出循环
		if(!result)break;
		}
	return i;
	}	
	
//处理模块
void CustomFlashHandler(void)
  {
	float buf,CurrentRatio;
	ModeConfStr *CurrentMode;
  BinaryOpDef BINbuf;
	//实现延时功能
	if(CustomFlashTimer>0)
	  {
		CustomFlashTimer--;
		return;
		}	
	//获取控制字符
	CurrentMode=GetCurrentModeConfig();//获取目前挡位信息
	if(CurrentMode==NULL)return; //字符串为NULL
	//设置随机数种子
  BINbuf.DataIN=RunTimeBattTelemResult.BusPower;
  buf=(unsigned int)(HT_GPTM0->CNTR^BINbuf.BINOut);//读取系统心跳定时器的计数器值然后和电池功率异或
	buf+=BreathCurrent*500; //加上呼吸电流值
	srand(((unsigned int)buf)^RunLogEntry.CurrentDataCRC);//将算出来的值和运行日志当前的CRC32异或作为随机种子
  //解析字符
	switch(CurrentMode->CustomFlashStr[StrPtr]) 
	  {
		case 'B': //随机分支功能
		if((rand()%99+1)<50) //1-50,直接跳回序列开头
		    {
				StrPtr=0; 
				return;//标志位为0，执行到该指令后直接结束本次循环从头开始
				}
		else StrPtr++;//其他值，跳过本条指令执行后续内容
		case 'T': //类似T触发的功能
			if(!IsEndToggle)
			  {
				IsEndToggle=true;
				StrPtr=0; 
				return;//标志位为0，执行到该指令后直接结束本次循环从头开始
				}
			else
			  {
				IsEndToggle=false;
				StrPtr++; //跳过本条指令，继续执行后面的内容
				}
		case 'W':StrPtr++;CustomFlashTimer=(short)(CurrentMode->CustomFlashSpeed/2);return;//0.5秒延时	
		case 'X':StrPtr++;CustomFlashTimer=(short)(CurrentMode->CustomFlashSpeed);return;//1秒延时
		case 'Y':StrPtr++;CustomFlashTimer=(short)(CurrentMode->CustomFlashSpeed*2);return;//2秒延时
		case 'Z':StrPtr++;CustomFlashTimer=(short)(CurrentMode->CustomFlashSpeed*4);return;//4秒延时	
		case 'U':StrPtr++;CustomFlashTimer=(short)(((rand()%29)+1)*CurrentMode->CustomFlashSpeed);return;//随机在1-30秒内的延时
		case '1':buf=10;break;
		case '2':buf=20;break;
		case '3':buf=30;break;	
		case '4':buf=40;break;
		case '5':buf=50;break;
		case '6':buf=60;break;
		case '7':buf=70;break;
		case '8':buf=80;break;
		case '9':buf=90;break;
		case 'A':buf=100;break; //10-100%亮度控制
		case 'R':buf=(float)((rand()%90)+10);break; //随机亮度
		case '-':buf=0;break;  //熄灭
		case 'S':buf=-2;break; //ADSR包络功能的停止符，此时手电执行到这里后将会停止
		default:buf=-1;  //非法字符
		}
	//开始处理
	if(buf==-1)StrPtr=0;//碰到非法字符，指针复位到第一个字符处循环
	else //其余字符
	  {
		if(buf!=-2)StrPtr++;//指向下一个字符
	  if(buf<0)buf=0; //限幅
		SysPstatebuf.ToggledFlash=(buf>0)?true:false; //根据亮度控制是否点亮LED
		buf/=(float)100;
	  CurrentRatio=pow(buf,GammaCorrectionValue);//使用幂函数模拟人眼亮度曲线算出电流取值
		CurrentRatio*=CurrentMode->LEDCurrentHigh;//算出最终的LED电流
		BreathCurrent=CurrentRatio;//送出LED电流
		}
	}
