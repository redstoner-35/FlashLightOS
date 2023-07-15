#include "console.h"
#include "ht32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

//发送队列
char TXBUF[TxqueueLen];//发送缓冲区
int QueueFront;
int QueueRear; //队列头尾指针
UARTTXQueueState TDMAStatu;

#pragma push
#pragma Otime//优化该函数使用3级别优化
#pragma O3
//DMATXinit
void DMATxInit(void)
 {
 PDMA_IntConfig(PDMA_USART1_TX,PDMA_INT_TC|PDMA_INT_GE,ENABLE);
 NVIC_EnableIRQ(PDMACH2_5_IRQn);//启用PDMA 2-5中断
 QueueFront=0;
 QueueRear=0;//设置队列头指针和尾部指针
 TDMAStatu=Transfer_Idle;//待机状态
 }
//UARTPrintf(返回打印的字符数)
int UartPrintf(char *Format,...)
  {
	va_list arg;
	int sptr=0,len=0,IgnoreLen=0;
  char SBUF[256]={0};
  __va_start(arg,Format);
  vsnprintf(SBUF,256,Format,arg);
  __va_end(arg);
	UARTPuts(SBUF);
	//计算长度
	while(SBUF[sptr])
	  {		
		if(IgnoreLen>0)//在忽略长度范围内
		 {
		 IgnoreLen--;
		 sptr++;
		 }
		else //根据UTF-8编码处理内容
		 {
		 if((SBUF[sptr]&0x80)==0)//单字节ASCII字符
		   {
		   len++;
		   IgnoreLen=1;
			 }
     else if((SBUF[sptr]&0xE0)==0xC0)//双字节编码			 
		   {
		   len+=2;
		   IgnoreLen=2;			 
			 }
		 else if((SBUF[sptr]&0xF0)==0xE0)//三字节编码
		   {
		   len+=2;//两个ASCII字符长度
		   IgnoreLen=3;			 
			 }
		 else if((SBUF[sptr]&0xF8)==0xF0)//四字节编码
		   {
			 len+=2;//两个ASCII字符长度
		   IgnoreLen=4;			
			 }
		 else len++;//其余情况按照长度累加
		 }
		}
	return len;
	}
//将指定长度的数据放入循环队列中
void UARTPutd(char *Buf,int Len) 
 {
 int sptr=0;
 if(Len<1)return;//长度非法
 while(sptr<Len)
  {
	if((QueueRear+1)%TxqueueLen==QueueFront)TXDMA_StartFirstTransfer();//队列已满
	else//队列未满，继续塞数据
	  {
	  TXBUF[QueueRear]=Buf[sptr];//放入数据
	  QueueRear=(QueueRear+1)%TxqueueLen;//队列队头指针+1
	  sptr++;
		}
	}
 if(QueueRear!=QueueFront)TXDMA_StartFirstTransfer();//启动传输
 }

//将指定长度的重复单个字符放入循环队列中
void UARTPutc(char Data,int Len) 
 {
 int sptr=0;
 if(Len<1)return;//长度非法
 while(sptr<Len)
  {
	if((QueueRear+1)%TxqueueLen==QueueFront)TXDMA_StartFirstTransfer();//队列已满
	else//队列未满，继续塞数据
	  {
	  TXBUF[QueueRear]=Data;//放入数据
	  QueueRear=(QueueRear+1)%TxqueueLen;//队列队头指针+1
	  sptr++;
		}
	}
 if(QueueRear!=QueueFront)TXDMA_StartFirstTransfer();//启动传输
 } 
 
//将字符串放入循环队列内
void UARTPuts(char *string)
 {
 int sptr=0;
 while(string[sptr]!=0)
  {
	if((QueueRear+1)%TxqueueLen==QueueFront)TXDMA_StartFirstTransfer();//队列已满
	else//队列未满，继续塞数据
	  {
	  TXBUF[QueueRear]=string[sptr];//放入数据
	  QueueRear=(QueueRear+1)%TxqueueLen;//队列队头指针+1
	  sptr++;
		}
	}
 if(QueueRear!=QueueFront)TXDMA_StartFirstTransfer();//启动传输
 }
//具有自动换行功能的函数（包括对UTF-8字符的处理）
void UARTPutsAuto(char *string,int FirstlineLen,int OtherLineLen)
 {
 int sptr=0,len=0,i=0;
 bool IsFirstLen=true;
 int IgnoreLen=0;
 while(string[sptr]!=0)
  {
	if((QueueRear+1)%TxqueueLen==QueueFront)TXDMA_StartFirstTransfer();//队列已满
	else//队列未满，继续塞数据
	  {  
	  //自动换行逻辑处理
		if(IgnoreLen>0)//在忽略区域内，继续送数据
		 {
		 TXBUF[QueueRear]=string[sptr];//放入数据
		 if(string[sptr]=='\r')len=0;
		 IgnoreLen--;
		 sptr++;
		 QueueRear=(QueueRear+1)%TxqueueLen;//队列队头指针+1
		 }
		else 
		 {
		 //需要换行了
		 if((len>=OtherLineLen&&(!IsFirstLen))||(IsFirstLen&&(len>=FirstlineLen)))
		   {
			 //把换行符导进去
		   for(i=0;i<2;i++)
				 {
				 TXBUF[QueueRear]=(i==0)?'\r':'\n';//放入数据
				 QueueRear=(QueueRear+1)%TxqueueLen;//队列队头指针+1
				 }
		   len=0;
		   IsFirstLen=false;//第一行换行已完毕
		   }
		 //根据字符编码统计参数
		 if((string[sptr]&0x80)==0x00)//单字节ASCII字符
		   {
		   len++;
		   IgnoreLen=1;//送出1字节再处理
			 }
     else if((string[sptr]&0xE0)==0xC0)//双字节编码			 
		   {
		   len+=2;
		   IgnoreLen=2;	//等1字节处理		 
			 }
		 else if((string[sptr]&0xF0)==0xE0)//三字节编码
		   {
		   len+=2;//两个ASCII字符长度
		   IgnoreLen=3;		//等2字节处理		 	 
			 }
		 else if((string[sptr]&0xF8)==0xF0)//四字节编码
		   {
			 len+=2;//两个ASCII字符长度
		   IgnoreLen=4;	//等3字节处理		 
			 }
		 else len++;//其余情况按照长度累加
		 }
		}
	}
 if(QueueRear!=QueueFront)TXDMA_StartFirstTransfer();//启动传输
 }
//TX DMA配置
void TXDMAConfig(u32 TargetBufAddr,u16 Size)
 {
  PDMACH_InitTypeDef PDMAInitStr;
  PDMA_TranSizeConfig(PDMA_USART1_TX,TxqueueLen,1);//随便往寄存器里面写入数据，否则因为硬件问题会导致DMA打开失败。
	PDMAInitStr.PDMACH_AdrMod=SRC_ADR_LIN_INC | DST_ADR_FIX;
	PDMAInitStr.PDMACH_BlkCnt=Size;//指定块数量
	PDMAInitStr.PDMACH_BlkLen=1; //每个块1个字节大小
	PDMAInitStr.PDMACH_DataSize=WIDTH_8BIT;
	PDMAInitStr.PDMACH_DstAddr=(u32)(&HT_USART1->DR);
	PDMAInitStr.PDMACH_Priority=M_PRIO; //1个数据块，块大小128字节,中优先级
	PDMAInitStr.PDMACH_SrcAddr=TargetBufAddr;
	PDMA_Config(PDMA_USART1_TX,&PDMAInitStr);//初始化TX DMA
	PDMA_EnaCmd(PDMA_USART1_TX,ENABLE);//启动DMA通道3
 } 

#pragma pop 
 
#pragma push
#pragma O0 //优化该函数使用0级别优化
 
void TXDMA_StartFirstTransfer(void)
 {
 int DMASendLen=0;
 u32 DMASendBase;
 while(TDMAStatu==Transfer_Busy);//正在繁忙状态，wait
 /*
 队头指针在队尾后面，要发送的数据长度=队尾-队头，发送起始点为队头
 1 2 3 4 5 6 7 8 9 10 11 12 13 14	 
 	 F = = = = = = = == == R 
 */	 	 
 if(QueueFront<QueueRear)
   {
	 DMASendLen=QueueRear-QueueFront;//发送长度为队头送到队尾
	 DMASendBase=(u32)&TXBUF[QueueFront];
	 }
 /*
 队尾指针在队头后面，要发送的数据长度=队尾-队头，发送起始点为队头
 1 2 3 4 5 6 7 8 9 10 11 12 13 14	 
 = R               F  == == == == 
 */	 
 else if(QueueFront>QueueRear)	 
	 {
	 DMASendLen=TxqueueLen-QueueFront;//发送长度为队列末尾指针送到队列尽头
	 DMASendBase=(u32)&TXBUF[QueueFront];
	 }
 //送出数据	 
 if(DMASendLen>0)
   {
	 TDMAStatu=Transfer_Busy;//进入繁忙状态
   TXDMAConfig(DMASendBase,DMASendLen);//启动DMA
	 }
 }	
 
 //发送DMA完成传输中断的回调函数(自动启动下一轮传输)
void TXDMAIntCallBack(void)
 {
 int CurrentTransferLen,DMASendLen=0;
 u32 DMASendBase;
 HT_PDMACH_TypeDef *PDMACH;
 //判断传输进度和队列情况
 PDMACH = (HT_PDMACH_TypeDef *)(HT_PDMA_BASE + PDMA_CH3 * 6 * 4);//求PDMA通道3的结构体指针所在位置 
 CurrentTransferLen=(PDMACH->TSR>>16)&0xFFFF;//求DMA通道3上次实际目标送出的数量
 CurrentTransferLen=CurrentTransferLen-PDMA_GetRemainBlkCnt(PDMA_CH3);//减去计数器剩余算出实际tx的数量
 QueueFront=(QueueFront+CurrentTransferLen)%TxqueueLen;//根据上次传输完毕的结果计算队头指针
 /*
 队头指针在队尾后面，要发送的数据长度=队尾-队头，发送起始点为队头
 1 2 3 4 5 6 7 8 9 10 11 12 13 14	 
 	 F = = = = = = = == == R 
 */	 	 
 if(QueueFront<QueueRear)
   {
	 DMASendLen=QueueRear-QueueFront;//发送长度为队头送到队尾
	 DMASendBase=(u32)&TXBUF[QueueFront];
	 }
 /*
 队尾指针在队头后面，要发送的数据长度=队尾-队头，发送起始点为队头
 1 2 3 4 5 6 7 8 9 10 11 12 13 14	 
 = R               F  == == == == 
 */	 
 else if(QueueFront>QueueRear)	 
	 {
	 DMASendLen=TxqueueLen-QueueFront;//发送长度为队列末尾指针送到队列尽头
	 DMASendBase=(u32)&TXBUF[QueueFront];
	 }
 else TDMAStatu=Transfer_Idle;//Front=Rear，队列空，传输完毕
 //送出数据	 
 if(DMASendLen>0)TXDMAConfig(DMASendBase,DMASendLen);
 }
#pragma pop
