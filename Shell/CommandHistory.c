#include <string.h>
#include "console.h"
#include "CfgFile.h"
#include <stdlib.h>
#include <stdio.h>

//内部声明
#define MaxHistoryDepth 5 //最大历史命令数目5

char HistoryBuf[MaxHistoryDepth+1][128];//命令历史缓存
char CurrentHistoryDepth;
signed char CurrentViewHistory;

//命令历史初始化
void InitHistoryBuf(void)
 {
 int i;
 CurrentHistoryDepth=0;
 CurrentViewHistory=-1;//-1表示当前内容
 for(i=0;i<MaxHistoryDepth+1;i++)
	 memset(HistoryBuf[i],0x00,128);//清除缓存
 }
//命令历史上翻
void CommandHistoryUpward(void)
 {
 if(CurrentViewHistory==-1)//从当前内容翻上去
   {
	 CurrentViewHistory++;
	 strncpy(HistoryBuf[MaxHistoryDepth],RXBuffer,128);//保存当前内容 	 
	 UARTPutc('\x7F',ustrlen(RXBuffer));//发送退格键
	 RXLastLinBufPtr=0;//last设置为0	
	 ClearRecvBuffer();//清空缓冲区
	 strncpy(RXBuffer,HistoryBuf[0],128);
	 RXLinBufPtr=strlen(RXBuffer);//设置指针
	 UARTPuts(RXBuffer);//将RX Buffer内的内容打印出去
	 }
 //不是当前内容
 else if(CurrentViewHistory<(CurrentHistoryDepth-1))
   {
	 CurrentViewHistory++;
	 UARTPutc('\x7F',ustrlen(RXBuffer));//发送退格键
	 RXLastLinBufPtr=0;//last设置为0	
	 ClearRecvBuffer();//清空缓冲区
	 strncpy(RXBuffer,HistoryBuf[CurrentViewHistory],128);
	 RXLinBufPtr=strlen(RXBuffer);//设置指针
	 UARTPuts(RXBuffer);//将RX Buffer内的内容打印出去	 
	 }
 }
//命令历史往下翻
void CommandHistoryDownward(void)
 {
 //从倒数第一条内容翻回来
 if(CurrentViewHistory==0)
   {
	 CurrentViewHistory=-1; 
	 UARTPutc('\x7F',ustrlen(RXBuffer));//发送退格键
	 RXLastLinBufPtr=0;//last设置为0	
	 ClearRecvBuffer();//清空缓冲区
	 strncpy(RXBuffer,HistoryBuf[MaxHistoryDepth],128);
	 RXLinBufPtr=strlen(RXBuffer);//设置指针
	 UARTPuts(RXBuffer);//将RX Buffer内的内容打印出去
	 }
 //往回翻
  else if(CurrentViewHistory>0)
   {
	 CurrentViewHistory--;
	 UARTPutc('\x7F',ustrlen(RXBuffer));//发送退格键
	 RXLastLinBufPtr=0;//last设置为0	
	 ClearRecvBuffer();//清空缓冲区
	 strncpy(RXBuffer,HistoryBuf[CurrentViewHistory],128);
	 RXLinBufPtr=strlen(RXBuffer);//设置指针
	 UARTPuts(RXBuffer);//将RX Buffer内的内容打印出去	 
	 }
 }
//命令历史写入
void CommandHistoryWrite(void)
 {
 int i;
 //搬运数据
 for(i=MaxHistoryDepth-1;i>0;i--)
   memcpy(HistoryBuf[i],HistoryBuf[i-1],128);	 
 //写入内容然后复位指针
 memcpy(HistoryBuf[0],RXBuffer,128);
 if(CurrentHistoryDepth<MaxHistoryDepth)
	 CurrentHistoryDepth++;
 CurrentViewHistory=-1;//复位为当前视图
 }
