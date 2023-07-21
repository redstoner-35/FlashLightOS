#include "console.h"
#include "ht32.h"
#include "delay.h"
#include <string.h>
#include "cfgfile.h"
#include "LEDMgmt.h"
#include "AES256.h"
#include "Xmodem.h"

//变量
UARTBUFState ConsoleStat;
ConsoleOperateFelid CurCmdField;
extern CommandHandle CmdHandle;//命令句柄
extern int IdleTimer;
char RXRingBuffer[CmdBufLen];//循环写入缓存
char RXBuffer[CmdBufLen+1]={0};//解码操作缓存
short QueueRearPTR=0;
short RXLinBufPtr=0;//当前RX操作的指针
short RXLastLinBufPtr=0;//上一轮RX操作的指针
char UARTIdleCNT=30;//串口DMA收数据空闲计数器

//搬运数据的时候不想写入到缓存里面的黑名单字符
bool InputDataBlackList(char DIN)
  {
	switch(DIN)
	 {
	 //屏蔽掉所有Xmodem的控制字符
	 case SOH:
   case STX:
   case EOT:
   case ACK:
   case NACK:
   case CAN:
	 case CTRLZ:return false;
	 }
	return true;
	}
//重配置console
void ConsoleReconfigure(void)
  {
	USART_InitTypeDef USART1InitStr;
	UARTPuts("\r\n智能驱动的上电自检已经完毕，驱动将进入用户操作模式。");
	UartPrintf("\r\n请将您的终端软件的波特率配置为%dbps以访问驱动的终端。",CfgFile.USART_Baud);
	PrintShellIcon();
	USART1InitStr.USART_BaudRate=CfgFile.USART_Baud;
	USART1InitStr.USART_Mode=USART_MODE_NORMAL;
	USART1InitStr.USART_Parity=USART_PARITY_NO;
	USART1InitStr.USART_WordLength=USART_WORDLENGTH_8B;
	USART1InitStr.USART_StopBits=USART_STOPBITS_1; //115200BPS 8bit数据位，关闭校验
	USART_Init(HT_USART1,&USART1InitStr);
	//进行LED自检
	CurrentLEDIndex=1;//红绿交替闪一下
	}
//初始化console
void ConsoleInit(void)
	{
	USART_InitTypeDef USART1InitStr;
	char EncryptBUF[48];
	//配置USART1 GPIO
  AFIO_GPxConfig(GPIO_PA,GPIO_PIN_4,AFIO_FUN_USART_UART);
  AFIO_GPxConfig(GPIO_PA,GPIO_PIN_5,AFIO_FUN_USART_UART); //将PA4-5配置为USART1复用IO
  GPIO_PullResistorConfig(HT_GPIOA,GPIO_PIN_5,GPIO_PR_UP);//启用上拉
	//配置USART1波特率，数据位数，校验，停止位
	USART1InitStr.USART_BaudRate=BISTBaud;
	USART1InitStr.USART_Mode=USART_MODE_NORMAL;
	USART1InitStr.USART_Parity=USART_PARITY_NO;
	USART1InitStr.USART_WordLength=USART_WORDLENGTH_8B;
	USART1InitStr.USART_StopBits=USART_STOPBITS_1; //115200BPS 8bit数据位，关闭校验
	USART_Init(HT_USART1,&USART1InitStr);
	//启动串行口DMA操作                                                         
	USART_PDMACmd(HT_USART1,USART_PDMAREQ_TX,ENABLE);
  USART_PDMACmd(HT_USART1,USART_PDMAREQ_RX,ENABLE);//打开串行口PDMA
  //设置串行口内置FIFO触点
  USART_TXRXTLConfig(HT_USART1,USART_CMD_TX,USART_TXTL_00);
	USART_TXRXTLConfig(HT_USART1,USART_CMD_RX,USART_RXTL_01);
	//启用DMA
  PDMAConfig((u32)(&RXRingBuffer),CmdBufLen);
	DMATxInit();//初始化DMA中断和IRQ
  //启用串口		
  USART_TxCmd(HT_USART1, ENABLE);
  USART_RxCmd(HT_USART1, ENABLE);	
	ConsoleStat=BUF_Idle;//标记当前串口状态为Buf idle
	CurCmdField=TextField;//默认为文本模式
	//console启动，输出自检信息
	InitHistoryBuf();//初始化历史缓存
	UARTPuts("\x0C\033[2JWelcome to Project:Diff-Torch Extreme Smart Driver for DDH-D8B.");
	UartPrintf("\r\nPowered by FlashLight OS version-%d.%d.%d,BIST Baudrate:%dbps\r\n",MajorVersion,MinorVersion,HotfixVersion,BISTBaud);
	//安全操作区，解密原文，显示之后销毁
  IsUsingFMCUID=false;//处理密文的时候关闭FMC随机加盐 	 
	memcpy(EncryptBUF,EncryptedCopyRight,48);//将密文复制进来	
	AES_EncryptDecryptData(&EncryptBUF[0],0);	 
	AES_EncryptDecryptData(&EncryptBUF[16],0);
	AES_EncryptDecryptData(&EncryptBUF[32],0);//解密被加密的文字	 	
	UARTPuts(EncryptBUF);
	memset(EncryptBUF,0x00,48);//销毁原文
  IsUsingFMCUID=true;//重新打开FMC随机加盐
	//显示其余内容
	UARTPuts("   All rights reserved.\r\n");
	#ifdef FlashLightOS_Debug_Mode
	UARTPuts("\r\n\r\nWarning:Debug mode has been enabled during FlashLight OS firmware compilation!");
	UARTPuts("\r\nThis is a special mode only for internal development and SHOULD BE DISABLED in");
	UARTPuts("\r\nproduction firmware! For developer, remove 'FlashLightOS_Debug_Mode' global\r\ndefinition in firmware project settings to disable it.\r\n");
	#endif
	UARTPuts("\r\nPlease wait for Smart Driver to perfrom power-on selftest...\r\n\r\n");
	XmodemTransferReset();//复位Xmodem模块
	}
//RX PDMA配置
void PDMAConfig(u32 TargetBufAddr,u16 Size)
 {
  PDMACH_InitTypeDef PDMAInitStr={0};
	PDMAInitStr.PDMACH_AdrMod=SRC_ADR_FIX | DST_ADR_LIN_INC | AUTO_RELOAD;
	PDMAInitStr.PDMACH_BlkCnt=Size;//指定块数量
	PDMAInitStr.PDMACH_BlkLen=1; //每个块1个字节大小
	PDMAInitStr.PDMACH_DataSize=WIDTH_8BIT;
	PDMAInitStr.PDMACH_DstAddr=TargetBufAddr;
	PDMAInitStr.PDMACH_Priority=VH_PRIO; //指定的块数量，单块1Byte
	PDMAInitStr.PDMACH_SrcAddr=(u32)(&HT_USART1->DR);
	PDMA_Config(PDMA_CH2,&PDMAInitStr);//初始化RX DMA
	PDMA_EnaCmd(PDMA_CH2,ENABLE);//启动DMA通道2
 }
//接收DMA完成操作中断回调函数
void RXDMACallback(void)
 {
 int CurrentQueueFrontPTR,i;
 /***********************************************
 这部分负责在Xmodem传输模式激活时直接重定向DMA环
 形队列的输出数据到Xmodem的接收缓存，把shell给屏
 蔽掉不允许shell参加任何的数据处理
 ***********************************************/
 if(XmodemTransferCtrl.XmodemTransferMode!=Xmodem_Mode_NoTransfer)
   {
	 XmodemDMARXHandler();//接收处理
	 if(XmodemTransferCtrl.XmodemTransferMode==Xmodem_Mode_Rx)
	    XmodemRXStateMachine();		 
	 else
		  XmodemTxStateMachine(); //根据接收器的模式选择是Tx还是Rx状态机去执行
	 return;
	 }
 /***********************************************
 这部分是负责直接在缓冲区内查询Ctrl+C所对应的特殊
 控制字符(0x03)然后直接令ctrl+C的处理函数完成相应
 操作。这样可以保证即使有命令在运行也可以强制打断
 模仿linux的操作
 ***********************************************/
 if((CmdBufLen-PDMA_GetRemainBlkCnt(PDMA_CH2))!=QueueRearPTR)
  {
	 i=QueueRearPTR;//取队尾指针数值
	 do
		{
		CurrentQueueFrontPTR=CmdBufLen-PDMA_GetRemainBlkCnt(PDMA_CH2);//获取当前头指针的位置
		//收到Ctrl+C，强制打断当前命令	
		if(RXRingBuffer[i]==0x03)
		  {
			ConsoleStat=BUF_Force_Stop;//强制打断命令执行
			if(ConsoleStat!=BUF_Idle)//执行阶段，强制设置队列尾部指针跳过该参数，然后退出
			  {
				QueueRearPTR=CmdBufLen-PDMA_GetRemainBlkCnt(PDMA_CH2);
			  break;
				}
			else RXRingBuffer[i]=0x00;//空闲状态下将该字节替换，这样允许将其他字节复制过去缓冲区
			}
		i=(i+1)%CmdBufLen;//移动队尾指针
		}
	 while(CurrentQueueFrontPTR!=i);//反复循环检测数据
	}
 if(ConsoleStat!=BUF_Idle||ConsoleStat==BUF_Force_Stop)return;//不是接收状态或者检测到ctrl+C，退出
 /***********************************************
 正常情况下将数据从环形缓冲搬运到命令缓冲的处理。
 在不是ctrl+C强制停止和执行阶段，这部分函数会把
	数据搬入命令缓冲区内。
 ***********************************************/
 if((CmdBufLen-PDMA_GetRemainBlkCnt(PDMA_CH2))!=QueueRearPTR)
   {
   //经历了长时间idle后第一次进入这个函数，同步指针
   if(UARTIdleCNT>=RXDMAIdleCount)RXLastLinBufPtr=RXLinBufPtr;
   UARTIdleCNT=0;//计数器清零
   do
		{
		CurrentQueueFrontPTR=CmdBufLen-PDMA_GetRemainBlkCnt(PDMA_CH2);//获取当前头指针的位置
		//正常复制数据(缓冲区未满且搬运的数据不在黑名单内)
		if(InputDataBlackList(RXRingBuffer[QueueRearPTR])&&RXLinBufPtr<CmdBufLen-1)
			{
			RXBuffer[RXLinBufPtr]=RXRingBuffer[QueueRearPTR];//载入已有的数据
			RXLinBufPtr++;//指向下一个数据
			}
		//缓冲区数据已满，剩下的数据丢弃掉并且标记缓冲满
		else if(RXLinBufPtr>=CmdBufLen-1)
			ConsoleStat=BUF_DMA_Full;
		QueueRearPTR=(QueueRearPTR+1)%CmdBufLen;//移动队尾指针
		}
	 while(CurrentQueueFrontPTR!=QueueRearPTR);//反复循环复制数据
   }
 else if(UARTIdleCNT<RXDMAIdleCount)UARTIdleCNT++;
 /***********************************************
 这部分是负责解析新搬来的数据的，当主函数执行5个
 周期内DMA都没有把新数据放入环形缓冲区，则程序将
 执行该区域解析新搬来的数据实现回显等操作
 ***********************************************/
 if(UARTIdleCNT==RXDMAIdleCount&&ConsoleStat!=BUF_DMA_Full&&(RXLinBufPtr-RXLastLinBufPtr)>0)
   {
	 //重置登录超时和深度休眠计时器
	 if(CfgFile.DeepSleepTimeOut>0)DeepSleepTimer=CfgFile.DeepSleepTimeOut;
   else DeepSleepTimer=-1;
	 if(CfgFile.IdleTimeout>0)IdleTimer=CfgFile.IdleTimeout;
	 //处理方向键序列
	 for(i=RXLastLinBufPtr;i<RXLinBufPtr;i++)
			 {
		   if(!strncmp("\x1B[A",&RXBuffer[i],CmdBufLen-i))ConsoleStat=BUF_Up_Key;//向上按键按下
		   else if(!strncmp("\x1B[B",&RXBuffer[i],CmdBufLen-i))ConsoleStat=BUF_Down_Key;//向下按键按下
		   else if(!strncmp("\x1B[D",&RXBuffer[i],CmdBufLen-i))ConsoleStat=BUF_Left_Key;//向左按键按下
		   else if(!strncmp("\x1B[C",&RXBuffer[i],CmdBufLen-i))ConsoleStat=BUF_Right_Key;//向右按键按下
			 //找到特殊按键
       if(ConsoleStat!=BUF_Idle)
		     {
			   for(i=RXLinBufPtr;i>=RXLastLinBufPtr;i--)RXBuffer[i]=0;//清除掉NULL
			   RXLinBufPtr=RXLastLinBufPtr;//将指针调回去
		     UARTIdleCNT++;//计数器加1使得该模块只会执行一次
			   return;
			   }		
			 }	
	 for(i=RXLinBufPtr-1;i>=RXLastLinBufPtr;i--)
		{
		//从后往前扫描输入的数据
		switch(RXBuffer[i])
			{
			case '\r':ConsoleStat=BUF_Exe_Req;break;
			case '\n':ConsoleStat=BUF_Exe_Req;break;//执行命令
			case 0x7F:ConsoleStat=BUF_Del_Req;break;
			case 0x08:ConsoleStat=BUF_Del_Req;break;//退格
			case 0x09:ConsoleStat=BUF_Tab_Req;break;//补全
			}		
		//找到匹配项，使能对应的操作
		if(ConsoleStat!=BUF_Idle)//特殊命令格式
		  {
      if(ConsoleStat==BUF_Exe_Req||ConsoleStat==BUF_Tab_Req)
			  {
				RXLinBufPtr--;//指针回退一格
				RXBuffer[i]=0;//去掉所有的回车和tab换成\0
				}
			break;
			}
		}
	 //扫描完毕，判断序列内有没有特殊内容
	 if(ConsoleStat==BUF_Idle)ConsoleStat=BUF_LoopBackReq;
	 UARTIdleCNT++;//计数器加1使得该模块只会执行一次
	 }
 //一个很重要的延时，保证串口的DMA可以在一长串数据进来的时候不会以外的把数据分成多段处理
 delay_ms(5);
 }	
