/*
本文件负责实现随机变频爆闪的功能，这个功能下，
每次都会随机的配置定时器生成随机频率的闪烁
*/

#include "modelogic.h"
#include "cfgfile.h"
#include "delay.h"
#include <math.h>
#include <stdlib.h>
#include "runtimelogger.h"

//外部变量和union
extern INADoutSreDef RunTimeBattTelemResult;


//随机数值
static int psc=1024;
static char RandomCount=0;

void RandomFlashHandler(void)
 {
 ModeConfStr *CurrentMode;
 unsigned int buf; 
 int maxreload,minreload;
 BinaryOpDef BINbuf;
 //获取挡位配置
 CurrentMode=GetCurrentModeConfig();//获取目前挡位信息
 if(CurrentMode==NULL)return; //字符串为NULL
 maxreload=(int)(10000/CurrentMode->RandStrobeMaxFreq); //计算最高允许频率
 minreload=(int)(10000/CurrentMode->RandStrobeMinFreq); 
 //获取最大频率然后开始延时
 if(RandomCount>0) //随机闪烁次数
   {
	 RandomCount--;
	 if(psc>maxreload)//当前频率比较慢，可以让频率逐渐变快
	   {
		 psc-=(psc>(maxreload+40))?40:1; //减去定时器ARR值
		 HT_GPTM1->CRR=(psc/2)-1;//写GPTM计时器
		 }
	 return;
	 }
 //设置随机数种子
 BINbuf.DataIN=RunTimeBattTelemResult.BusPower;
 buf=(unsigned int)(HT_GPTM0->CNTR^BINbuf.BINOut);//读取系统心跳定时器的计数器值然后和电池功率异或
 buf^=(unsigned int)psc; //加上PSC值
 srand(buf^RunLogEntry.CurrentDataCRC);//将算出来的值和运行日志当前的CRC32异或作为随机种子
 //生成定时器重载值
 do
   {
	 psc=(rand()%(minreload-maxreload))+maxreload;//生成随机数
	 RandomCount=(rand()%17); //随机决定执行次数
	 }
 while(psc<=0); //反复生成一个大于0的随机数
 //写GPTM寄存器
 HT_GPTM1->CRR=(psc/2)-1;//生成的频率需要/2才是真实值
 }
