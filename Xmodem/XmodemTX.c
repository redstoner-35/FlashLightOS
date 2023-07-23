#include <stdlib.h>
#include <string.h>
#include "console.h"
#include "I2C.h"
#include "Xmodem.h"
#include "AES256.h"
#include "delay.h"

//外部变量
extern bool RXTimerFilpFlag;

//初始化Xmodem的发送模块，执行该函数后Xmodem将启动传输。
void XmodemInitTxModule(int MaximumTxSize,int ROMAddr)
 {
 memset(XmodemTransferCtrl.XmodemRXBuf,0x00,sizeof(XmodemTransferCtrl.XmodemRXBuf));
 XmodemTransferCtrl.CurrentPacketNum=1;
 XmodemTransferCtrl.CurrentBUFRXPtr=0;
 XmodemTransferCtrl.RxRetryCount=0;
 XmodemTransferCtrl.RXTimer=0;
 XmodemTransferCtrl.XmodemTransferMode=Xmodem_Mode_Tx;
 XmodemTransferCtrl.ReadWriteBase=ROMAddr;
 XmodemTransferCtrl.IsUseCRC16=false;
 XmodemTransferCtrl.MaxTransferSize=MaximumTxSize;//设置最大传输大小和基地址
 XmodemTransferCtrl.XmodemRxState=Xmodem_RxStartWait;
 }

//Xmodem Tx状态机
void XmodemTxStateMachine(void)
 {
 int PacketCRC16;
 int ROMAddrBase,ROMReadSize,i;
 //状态机
 switch(XmodemTransferCtrl.XmodemTxState)
   {
	 /*************************************
	 传输开始，此时从机耐心等待主机准备好,
	 然后发送'C'表示以CRC-16校验的方式让从
	 机送出第一个数据包
   *************************************/
	 case Xmodem_TxWaitFirstPack:
	   {
		 if(XmodemTxCtrlByte==0x03||XmodemTxCtrlByte==CAN)  //收到了Ctrl+C或者CAN,停止
		   {
			 XmodemTransferReset();//复位状态机
		   XmodemTransferCtrl.XmodemTxState=Xmodem_Error_TxManualStop;
			 }
		 else if(XmodemTxCtrlByte=='C'||(!ForceUseCRC16&&XmodemTxCtrlByte==NACK)) //收到了C或者NACK,跳转到准备包裹阶段开始传输
		   {
			 if(ForceUseCRC16)XmodemTransferCtrl.IsUseCRC16=true;
			 else XmodemTransferCtrl.IsUseCRC16=XmodemTxCtrlByte=='C'?true:false;//根据收到的是C还是NACK决定使用什么校验协议
			 XmodemTransferCtrl.XmodemTxState=Xmodem_PreparePacket;
			 }
		 else if(RXTimerFilpFlag)
		   {
       RXTimerFilpFlag=false;
			 if(XmodemTransferCtrl.RXTimer==8*MaximumRxWait)
				 XmodemTransferCtrl.XmodemTxState=Xmodem_Error_TxTimeOut;//超时
			else 
				 XmodemTransferCtrl.RXTimer++;//时间没到，继续计时
			 }
		 break;
		 }
	 /*************************************
	 从机收到主机的ACK或者'C'，准备新一轮
	 需要送出的数据包，准备完毕后，从机开始
	 发送。对于这个驱动而言，需要送出的数据
	 是从ROM里面读取的。
   *************************************/ 
	 case Xmodem_PreparePacket:
	   {
		 //复位指针和计时器
	   XmodemTransferCtrl.RXTimer=0;
		 XmodemTransferCtrl.CurrentBUFRXPtr=0; 
		 //读取数据
		 memset(&XmodemTransferCtrl.XmodemRXBuf[3],0x1A,128);//根据Xmodem的规范，不满128字节的数据包后面需要用0x1A填充
		 ROMAddrBase=XmodemTransferCtrl.ReadWriteBase+((XmodemTransferCtrl.CurrentPacketNum-1)*128);
		 ROMReadSize=XmodemTransferCtrl.MaxTransferSize>128?128:XmodemTransferCtrl.MaxTransferSize; //计算写入的地址offset和大小
		 if(M24C512_PageRead(&XmodemTransferCtrl.XmodemRXBuf[3],ROMAddrBase,ROMReadSize))
		    {
			  XmodemTransferCtrl.XmodemTxState=Xmodem_Error_PreparePacket;
			  UARTPutc(EOT,1);//发送EOT，结束传输跳转到数据处理错误状态
			  }
		 IsUsingOtherKeySet=false;
		 for(i=0;i<8;i++)AES_EncryptDecryptData(&XmodemTransferCtrl.XmodemRXBuf[3+(i*16)],1);
		 IsUsingOtherKeySet=true; //加密数据
		 //写帧头部和尾部的数据包校验
		 XmodemTransferCtrl.XmodemRXBuf[0]=SOH; //帧头
		 XmodemTransferCtrl.XmodemRXBuf[1]=XmodemTransferCtrl.CurrentPacketNum&0xFF;
		 XmodemTransferCtrl.XmodemRXBuf[2]=~XmodemTransferCtrl.XmodemRXBuf[1]; //数据包号
		 if(!XmodemTransferCtrl.IsUseCRC16)//使用校验和
		   {
			 PacketCRC16=Checksum8(&XmodemTransferCtrl.XmodemRXBuf[3],128)&0xFF;
		   XmodemTransferCtrl.XmodemRXBuf[131]=PacketCRC16;
			 }
		 else //使用CRC-16校验
		   {
			 PacketCRC16=PEC16CheckXModem(&XmodemTransferCtrl.XmodemRXBuf[3],128);
		   PacketCRC16&=0xFFFF;
		   XmodemTransferCtrl.XmodemRXBuf[132]=PacketCRC16&0xFF;//LSB
		   XmodemTransferCtrl.XmodemRXBuf[131]=(PacketCRC16>>8)&0xFF;//MSB
			 }
		 //准备完毕，开始发送
		 UARTPutd(XmodemTransferCtrl.XmodemRXBuf,XmodemTransferCtrl.IsUseCRC16?133:132);//发送数据包
		 XmodemTransferCtrl.XmodemTxState=Xmodem_WaitForHostRespond; //发送完毕后等待
		 break;
		 }
	 /*************************************
	 从机收到主机的ACK或者NACK以及CAN和Ctrl
	 +C根据操作去处理。
   *************************************/ 	 
	 case Xmodem_WaitForHostRespond:
	   {
		 if(XmodemTxCtrlByte==0x03||XmodemTxCtrlByte==CAN)  //收到了Ctrl+C或者CAN,停止
		   {
			 XmodemTransferReset();//复位状态机
		   XmodemTransferCtrl.XmodemTxState=Xmodem_Error_TxManualStop;
			 }
		 else if(XmodemTxCtrlByte==ACK) //收到了ACK,说明本次传输有效，此时可以计算新的传输大小和包数开始第二轮传输
		   {
		   memset(XmodemTransferCtrl.XmodemRXBuf,0x00,sizeof(XmodemTransferCtrl.XmodemRXBuf));//清空缓冲区
			 XmodemTransferCtrl.CurrentPacketNum++;  //处理完毕的数据包+1，此时可以接收下一个了
			 XmodemTransferCtrl.MaxTransferSize-=128;
			 if(XmodemTransferCtrl.MaxTransferSize<0)XmodemTransferCtrl.MaxTransferSize=0;//计算传输完毕后的大小
			 //检查传输是否完成，完成的话发EOT然后等待主机最后ACK一次，否则继续准备数据包并发出
			 if(XmodemTransferCtrl.MaxTransferSize==0)
			    {		 
	        XmodemTransferCtrl.RXTimer=0;
		      XmodemTransferCtrl.CurrentBUFRXPtr=0; //复位指针和计时器
					UARTPutc(EOT,1);//发送EOT
					delay_ms(200);//这里需要等200mS,让主机认为EOT以及后面的其他字符，不是一个数据包
					XmodemTransferCtrl.XmodemTxState=Xmodem_TxTransferDone;		
					}				 
			 else XmodemTransferCtrl.XmodemTxState=Xmodem_PreparePacket;
			 }
		 else if(XmodemTxCtrlByte==NACK) //说明数据出错，需要重发
		   {
			 XmodemTxCtrlByte=0;
			 XmodemTransferCtrl.RXTimer=0;
			 XmodemTransferCtrl.CurrentBUFRXPtr=0; //清除NACK字符复位指针
			 if(XmodemTransferCtrl.RxRetryCount<MaximumPackRetry) //还在重试次数范围内
				 {
				 UARTPutd(XmodemTransferCtrl.XmodemRXBuf,XmodemTransferCtrl.IsUseCRC16?133:132);//发送数据包
				 XmodemTransferCtrl.RxRetryCount++;
				 }
			 else XmodemTransferCtrl.XmodemTxState=Xmodem_Error_TxRetryTooMuch; //重试次数过多
			 }
		 else if(RXTimerFilpFlag) //主机没响应，超时
		   {
       RXTimerFilpFlag=false;
			 if(XmodemTransferCtrl.RXTimer==8*MaximumRxWait)
				 XmodemTransferCtrl.XmodemTxState=Xmodem_Error_TxTimeOut;//超时
		 else 
				 XmodemTransferCtrl.RXTimer++;//时间没到，继续计时
			 }
		 break;
		 }	 
	 /*************************************
   其余的错误状态我们只需要关闭Xmodem传输
	 即可。剩下的事情丢给其他的东西去处理。
   *************************************/
	 default : XmodemTransferCtrl.XmodemTransferMode=Xmodem_Mode_NoTransfer;
	 }
 }
