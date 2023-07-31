/*
本文件负责实现随机变频爆闪的功能，这个功能下，
每次都会随机的配置定时器生成随机频率的闪烁
*/

#include "modelogic.h"
#include "cfgfile.h"
#include "delay.h"
#include "PWMDIM.h"
#include <math.h>
#include <stdlib.h>
#include "runtimelogger.h"

//随机数值
static int psc=1024;

void RandomFlashHandler(void)
 {
 ModeConfStr *CurrentMode;
 unsigned int buf; 
 int maxreload,minreload;
 //获取挡位配置
 CurrentMode=GetCurrentModeConfig();//获取目前挡位信息
 if(CurrentMode==NULL)return; //字符串为NULL
 //设置随机数种子
 buf=(unsigned int)(HT_MCTM0->CNTR^HT_GPTM0->CNTR);//读取系统心跳和PWM定时器的计数器值
 buf^=(unsigned int)psc;
 srand(buf^RunLogEntry.CurrentDataCRC);//将算出来的值和运行日志当前的CRC32异或作为随机种子
 //生成定时器重载值
 maxreload=(int)(10000/CurrentMode->RandStrobeMaxFreq);
 minreload=(int)(10000/CurrentMode->RandStrobeMinFreq); 
 do
   {
	 psc=(rand()%(minreload-maxreload))+maxreload;//生成随机数
	 }
 while(psc<=0); //反复生成一个大于0的随机数
 //写入到GPTM1的寄存器里面
 HT_GPTM1->CRR=(psc/2)-1;//生成的频率需要/2才是真实值
 }
