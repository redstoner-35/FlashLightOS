#include <stdlib.h>
#include <string.h>
#include "console.h"
#include "I2C.h"
#include "Xmodem.h"
#include "AES256.h"
#include "delay.h"

//传输控制结构体和定时变量
bool RXTimerFilpFlag=false;
XmodemTransferCtrlStrDef XmodemTransferCtrl; //结构体，包含负责控制Xmodem传输的变量

//重置Xmodem的结构体
void XmodemTransferReset(void)
 {
 memset(XmodemTransferCtrl.XmodemRXBuf,0x00,sizeof(XmodemTransferCtrl.XmodemRXBuf));
 XmodemTransferCtrl.CurrentPacketNum=1;
 XmodemTransferCtrl.CurrentBUFRXPtr=0;
 XmodemTransferCtrl.RxRetryCount=0;
 XmodemTransferCtrl.RXTimer=0;
 XmodemTransferCtrl.XmodemTransferMode=Xmodem_Mode_NoTransfer;
 XmodemTransferCtrl.MaxTransferSize=0;
 XmodemTransferCtrl.ReadWriteBase=0;
 XmodemTransferCtrl.IsUseCRC16=false;
 XmodemTransferCtrl.XmodemRxState=Xmodem_RxStartWait;
 XmodemTransferCtrl.XmodemTxState=Xmodem_TxWaitFirstPack;
 }
 
//初始化Xmodem的RX Module，执行该函数后Xmodem将启动传输。
void XmodemInitRXModule(int MaximumRxSize,int ROMAddr)
 {
 memset(XmodemTransferCtrl.XmodemRXBuf,0x00,sizeof(XmodemTransferCtrl.XmodemRXBuf));
 XmodemTransferCtrl.CurrentPacketNum=1;
 XmodemTransferCtrl.CurrentBUFRXPtr=0;
 XmodemTransferCtrl.RxRetryCount=0;
 XmodemTransferCtrl.RXTimer=0;
 XmodemTransferCtrl.XmodemTransferMode=Xmodem_Mode_Rx;
 XmodemTransferCtrl.ReadWriteBase=ROMAddr;
 XmodemTransferCtrl.IsUseCRC16=false;
 XmodemTransferCtrl.MaxTransferSize=MaximumRxSize;////设置最大传输大小和基地址
 XmodemTransferCtrl.XmodemRxState=Xmodem_RxStartWait;
 }

//负责将DMA收到的数据搬运到缓冲区内
void XmodemDMARXHandler(void)
 {
 int CurrentQueueFrontPTR;
 //收取数据
 if((CmdBufLen-PDMA_GetRemainBlkCnt(PDMA_CH2))!=QueueRearPTR)do
		{
		CurrentQueueFrontPTR=CmdBufLen-PDMA_GetRemainBlkCnt(PDMA_CH2);//获取当前头指针的位置
		//Tx模式下接收收到的控制位
	  if(XmodemTransferCtrl.XmodemTransferMode==Xmodem_Mode_Tx&&XmodemTransferCtrl.CurrentBUFRXPtr<1)
		  {
			XmodemTxCtrlByte=RXRingBuffer[QueueRearPTR];//把收到的标志位写入到缓存的最后1字节
			XmodemTransferCtrl.CurrentBUFRXPtr++;//让指针等于1使得数据不得接收了
			}
		//正常复制数据(Xmodem接收模式)
		else if(XmodemTransferCtrl.XmodemTransferMode==Xmodem_Mode_Rx&&XmodemTransferCtrl.CurrentBUFRXPtr<133)
			{
			XmodemTransferCtrl.XmodemRXBuf[XmodemTransferCtrl.CurrentBUFRXPtr]=RXRingBuffer[QueueRearPTR];//载入已有的数据
			XmodemTransferCtrl.CurrentBUFRXPtr++;//指向下一个数据
			}
		QueueRearPTR=(QueueRearPTR+1)%CmdBufLen;//移动队尾指针
		}
 while(CurrentQueueFrontPTR!=QueueRearPTR);//反复循环复制数据
 }
//Xmodem接收的状态机
void XmodemRXStateMachine(void)
 {
 int CRC16Result,PacketCRC16;
 char FrameHeadChk;
 int ROMAddrBase,ROMWriteSize,i;
 //状态机
 switch(XmodemTransferCtrl.XmodemRxState)
  {
	/*************************************
	传输开始，第一个帧还没过来。这时候接收
	机发送'C'请求发送机以CRC-16的方式发送
	第一帧的数据过来。		
  *************************************/
	case Xmodem_RxStartWait:
	  {
	  //如果主机发送Ctrl+C过来，则强制结束传输
		if(XmodemTransferCtrl.CurrentBUFRXPtr==1&&XmodemTransferCtrl.XmodemRXBuf[0]==0x03)	
		  {
			XmodemTransferReset();//复位状态机
			XmodemTransferCtrl.XmodemRxState=Xmodem_Error_ManualStop;//输入Ctrl+C重置
			}
		//如果主机发送EOT，则检查传输是否完毕，没完成则报错，否则跳转到完成
		else if(XmodemTransferCtrl.CurrentBUFRXPtr==1&&XmodemTransferCtrl.XmodemRXBuf[0]==EOT)
		  {
			UARTPutc(ACK,1); //最后发送一次ACK结束传输
			if(XmodemTransferCtrl.MaxTransferSize>0)
				XmodemTransferCtrl.XmodemRxState=Xmodem_Error_TransferSize;
			else
			  {
				delay_ms(200);//这里需要等200mS,让主机认为后面的其他字符，不是一个数据包
				XmodemTransferCtrl.XmodemRxState=Xmodem_TransferDone; //如果传输大小和预期一致(最大传输大小为0)则传输完毕
				}
			}
	  //如果主机已经发来了132/133字节的第一帧，则开始处理
		else if(XmodemTransferCtrl.CurrentBUFRXPtr==(XmodemTransferCtrl.IsUseCRC16?133:132))
			XmodemTransferCtrl.XmodemRxState=Xmodem_PacketRxDone;
		//主机还没反应，计时等待
		else if(RXTimerFilpFlag)
		  {
			RXTimerFilpFlag=false;
			if(XmodemTransferCtrl.RXTimer==8*MaximumRxWait)
				XmodemTransferCtrl.XmodemRxState=Xmodem_Error_RxTimeOut;//超时
			else 
				XmodemTransferCtrl.RXTimer++;//时间没到，继续计时
		  if((XmodemTransferCtrl.RXTimer%8)==0)
			    {
					if(ForceUseCRC16)XmodemTransferCtrl.IsUseCRC16=true;//强制使用CRC-16
					else XmodemTransferCtrl.IsUseCRC16=XmodemTransferCtrl.RXTimer<(4*MaximumRxWait)?true:false;//优先传输'C'
				  UARTPutc(XmodemTransferCtrl.IsUseCRC16?'C':NACK,1);//每一秒发送一次启动传输的字符
					}
			}
		break;
		}
	/*************************************
  主机已经顺利的发完了1帧，此时从机开始
  对数据进行处理，首先检查包裹顺序和CRC
	-16校验和是否对的上，如果对不上，则请
	求重发
  *************************************/	
	case Xmodem_PacketRxDone:
	  {
		//如果主机在传输大小已经达到的情况仍然继续传输，则说明文件大小错误，此时报错
			if(XmodemTransferCtrl.MaxTransferSize<=0)	
			{
		  XmodemTransferCtrl.XmodemRxState=Xmodem_Error_TransferSize;
			UARTPutc(CAN,1);//发送CAN，结束传输跳转到大小错误状态
			}
		//计算帧头
		FrameHeadChk=~XmodemTransferCtrl.XmodemRXBuf[2]; //将第二字节的包序号反码取反用于比对包序号
		//计算错误检测结果
		if(!XmodemTransferCtrl.IsUseCRC16) //CRC-16
		   {
		   CRC16Result=Checksum8(&XmodemTransferCtrl.XmodemRXBuf[3],128);
		   PacketCRC16=XmodemTransferCtrl.XmodemRXBuf[131]&0xFF;
			 CRC16Result&=0xFFFF;//清除无效的不需要被比较的位
			 }
		else //checksum-8
	  	{
			CRC16Result=PEC16CheckXModem(&XmodemTransferCtrl.XmodemRXBuf[3],128);//取数据并校验		
		  PacketCRC16=XmodemTransferCtrl.XmodemRXBuf[132]&0xFF;//LSB
		  PacketCRC16|=XmodemTransferCtrl.XmodemRXBuf[131]<<8;//MSB
			PacketCRC16&=0xFFFF;//清除无效的不需要被比较的位
			}
		//检查是否需要重发
	  if(CRC16Result!=PacketCRC16||XmodemTransferCtrl.XmodemRXBuf[0]!=SOH||FrameHeadChk!=XmodemTransferCtrl.XmodemRXBuf[1])
		  {
      if(XmodemTransferCtrl.RxRetryCount==MaximumPackRetry)
				XmodemTransferCtrl.XmodemRxState=Xmodem_Error_PacketData;  //重试次数过多，直接结束
			else 
			  {
				XmodemTransferCtrl.RXTimer=0;
				XmodemTransferCtrl.CurrentBUFRXPtr=0;
				XmodemTransferCtrl.RxRetryCount++;                         //重试次数+1
			  XmodemTransferCtrl.XmodemRxState=Xmodem_PacketNACK;        //包头或者内部数据损坏，要求重发
				}
			break;
			}
		else XmodemTransferCtrl.RxRetryCount=0;                        //没有出错,重试次数清零
		//比较数据包的位序和目前接收的位序是否匹配
		if(XmodemTransferCtrl.XmodemRXBuf[1]!=XmodemTransferCtrl.CurrentPacketNum)
		  {
		  XmodemTransferCtrl.XmodemRxState=Xmodem_Error_PacketOrder;
			UARTPutc(CAN,1);//发送CAN，结束传输跳转到包错误状态
			}
		else //数据包检查通过，可以开始处理了。
		  {
		  XmodemTransferCtrl.RXTimer=0;
			XmodemTransferCtrl.CurrentBUFRXPtr=0; //复位指针和计时器
			XmodemTransferCtrl.XmodemRxState=Xmodem_SlaveProcessData;
			}
	  break;
		}
	/*************************************
	从机完成了对数据的检查确认数据合法，此
	时从机将在这里处理数据。处理完毕后继续
	接收。
	对于这个驱动而言，接收数据的处理是将数
	据写入到ROM内的临时缓冲中，整个文件写完
	然后交给其他程序进行校验等。
  *************************************/
	case Xmodem_SlaveProcessData:
	  {
		//将收到的数据写入EEPROM的cache area
		ROMAddrBase=XmodemTransferCtrl.ReadWriteBase+((XmodemTransferCtrl.CurrentPacketNum-1)*128);
		ROMWriteSize=XmodemTransferCtrl.MaxTransferSize>128?128:XmodemTransferCtrl.MaxTransferSize; //计算写入的地址offset和大小
	  IsUsingFMCUID=false;
		for(i=0;i<8;i++)AES_EncryptDecryptData(&XmodemTransferCtrl.XmodemRXBuf[3+(i*16)],0);
		IsUsingFMCUID=true; //对数据进行解密操作
		if(M24C512_PageWrite(&XmodemTransferCtrl.XmodemRXBuf[3],ROMAddrBase,ROMWriteSize))
		  {
			XmodemTransferCtrl.XmodemRxState=Xmodem_Error_DataProcess;
			UARTPutc(CAN,1);//发送CAN，结束传输跳转到数据处理错误状态
			}
		else 
		  {
			memset(XmodemTransferCtrl.XmodemRXBuf,0x00,sizeof(XmodemTransferCtrl.XmodemRXBuf));//清空RX缓冲区
			XmodemTransferCtrl.CurrentPacketNum++;  //处理完毕的数据包+1，此时可以接收下一个了
			XmodemTransferCtrl.MaxTransferSize-=128;
			if(XmodemTransferCtrl.MaxTransferSize<0)XmodemTransferCtrl.MaxTransferSize=0;//计算传输完毕后的大小
			XmodemTransferCtrl.XmodemRxState=Xmodem_SlaveWaitForNextFrame;
			}
		break;
		}
	/*************************************
	从机完成了对数据的检查，但是发现数据
	出现了错误，此时从机通过发送NAK请求主
	机重发数据
  *************************************/		
	case Xmodem_PacketNACK:
	  {
		//如果主机发送Ctrl+C过来，则强制结束传输
		if(XmodemTransferCtrl.CurrentBUFRXPtr==1&&XmodemTransferCtrl.XmodemRXBuf[0]==0x03)	
		  {
			XmodemTransferReset();//复位状态机
			XmodemTransferCtrl.XmodemRxState=Xmodem_Error_ManualStop;//输入Ctrl+C重置
			}
		//如果主机发送EOT，则检查传输是否完毕，没完成则报错，否则跳转到完成
		else if(XmodemTransferCtrl.CurrentBUFRXPtr==1&&XmodemTransferCtrl.XmodemRXBuf[0]==EOT)
		  {
			UARTPutc(ACK,1); //最后发送一次ACK结束传输
			if(XmodemTransferCtrl.MaxTransferSize>0)
				XmodemTransferCtrl.XmodemRxState=Xmodem_Error_TransferSize;
			else
			  {
				delay_ms(200);//这里需要等200mS,让主机认为后面的其他字符，不是一个数据包
				XmodemTransferCtrl.XmodemRxState=Xmodem_TransferDone; //如果传输大小和预期一致(最大传输大小为0)则传输完毕
				}
			}
	  //如果主机已经发来了133字节的一帧，则开始处理
		else if(XmodemTransferCtrl.CurrentBUFRXPtr==(XmodemTransferCtrl.IsUseCRC16?133:132))
			XmodemTransferCtrl.XmodemRxState=Xmodem_PacketRxDone;
		//主机还没反应，计时等待
		else if(RXTimerFilpFlag)
		  {
			RXTimerFilpFlag=false;
			if(XmodemTransferCtrl.RXTimer==8*MaximumRxWait)
				XmodemTransferCtrl.XmodemRxState=Xmodem_Error_RxTimeOut;//超时
			else
			  {				
				if((XmodemTransferCtrl.RXTimer%16)==0)UARTPutc(NACK,1);//每2秒发送一次NACK
				XmodemTransferCtrl.RXTimer++;//时间没到，继续计时
				}
			}
		break;
	  }
	/*************************************
	从机完成了对收到数据的处理，此时从机发
	送ACK请求继续传送数据
  *************************************/	
	case Xmodem_SlaveWaitForNextFrame:
		{
		//如果主机发送Ctrl+C过来，则强制结束传输
		if(XmodemTransferCtrl.CurrentBUFRXPtr==1&&XmodemTransferCtrl.XmodemRXBuf[0]==0x03)	
		  {
			XmodemTransferReset();//复位状态机
			XmodemTransferCtrl.XmodemRxState=Xmodem_Error_ManualStop;//输入Ctrl+C重置
			}
		//如果主机发送EOT，则检查传输是否完毕，没完成则报错，否则跳转到完成
		else if(XmodemTransferCtrl.CurrentBUFRXPtr==1&&XmodemTransferCtrl.XmodemRXBuf[0]==EOT)
		  {
			UARTPutc(ACK,1); //最后发送一次ACK结束传输
			if(XmodemTransferCtrl.MaxTransferSize>0)
				XmodemTransferCtrl.XmodemRxState=Xmodem_Error_TransferSize;
			else
			  {
				delay_ms(200);//这里需要等200mS,让主机认为后面的其他字符，不是一个数据包
				XmodemTransferCtrl.XmodemRxState=Xmodem_TransferDone; //如果传输大小和预期一致(最大传输大小为0)则传输完毕
				}
			}
	  //如果主机已经发来了一帧，则开始处理
		else if(XmodemTransferCtrl.CurrentBUFRXPtr==(XmodemTransferCtrl.IsUseCRC16?133:132))
			XmodemTransferCtrl.XmodemRxState=Xmodem_PacketRxDone;
		//主机还没反应，计时等待
		else if(RXTimerFilpFlag)
		  {
			RXTimerFilpFlag=false;
			if(XmodemTransferCtrl.RXTimer==8*MaximumRxWait)
				XmodemTransferCtrl.XmodemRxState=Xmodem_Error_RxTimeOut;//超时
			else
			  {				
				if((XmodemTransferCtrl.RXTimer%16)==0)UARTPutc(ACK,1);//每2秒发送一次ACK
				XmodemTransferCtrl.RXTimer++;//时间没到，继续计时
				}
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
