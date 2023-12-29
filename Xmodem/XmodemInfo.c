#include "Xmodem.h"
#include "console.h"
#include "delay.h"

//字符串常量指针
const char *XmodemStatuString[]=
{
"\r\n主机在发送期间没有响应,传输终止.",
"\r\n传输已被您或主机手动终止.",
"\r\n传输过程中出现了过多的错误.",
"\r\n系统在试图处理您的数据请求时出现了问题,请重试.",
"\r\n传输已完成,",
"\r\n传输过程中主机和从机之间的数据包同步异常,请重试.",
"\r\n您传输的文件大小不匹配.请检查上传的文件是否正确"
};

//显示备份和读取操作提示的字符串
void DisplayXmodemBackUp(const char *Operation,bool IsRestore)
  {
  //显示
	UartPrintf("\r\n系统将使用'Xmodem'协议完成%s的%s,请将文件传输软件配置为相应参数.\r\n\r\n",Operation,!IsRestore?"备份":"恢复");
	}

//Xmodem发送字符串
static void XmodemDisplayStrAfterTransfer(char *Str)
  {
	UARTPutc(ACK,1);
	delay_ms(20);
	UARTPuts(Str);
	XmodemTransferReset();
	}
	
//在Xmodem发送内容期间显示提示字符串	
void XmodemTxDisplayHandler(const char *FinishContentString,void (*ThingsToDoInError)(void),void (*ThingsToDoWhenDone)(void))
  {
	if(FinishContentString==NULL||ThingsToDoInError==NULL||ThingsToDoWhenDone==NULL)return;
	switch(XmodemTransferCtrl.XmodemTxState)
	     {
			 case Xmodem_Error_TxTimeOut:
				   XmodemDisplayStrAfterTransfer((char *)XmodemStatuString[0]);
			     ThingsToDoInError();
			     return;
			 case Xmodem_Error_TxManualStop:
				   XmodemDisplayStrAfterTransfer((char *)XmodemStatuString[1]);
			     ThingsToDoInError();
			     return;
			 case Xmodem_Error_TxRetryTooMuch:
				   XmodemDisplayStrAfterTransfer((char *)XmodemStatuString[2]);
			     ThingsToDoInError();
			     return; 
			 case Xmodem_Error_PreparePacket:
				   XmodemDisplayStrAfterTransfer((char *)XmodemStatuString[3]);
					 ThingsToDoInError();
			     return;
			 case Xmodem_TxTransferDone:
				   UartPrintf("%s您可通过电脑对%s进行查看操作.",(char *)XmodemStatuString[4],FinishContentString);
			     ThingsToDoWhenDone();
			     return;
			 default: return; //其余状态是内部状态，啥也不做
			 }
	 }
//在Xmodem接收期间显示字符串的处理
void XmodemRxDisplayHandler(const char *FinishContentString,void (*ThingsToDoInError)(void),void (*ThingsToDoWhenDone)(void))
   {
	 if(FinishContentString==NULL||ThingsToDoInError==NULL||ThingsToDoWhenDone==NULL)return;
	 switch(XmodemTransferCtrl.XmodemRxState)
		   {
			 case Xmodem_Error_RxTimeOut://接收超时
				   XmodemDisplayStrAfterTransfer((char *)XmodemStatuString[0]);
			     ThingsToDoInError();
			     return;
       case Xmodem_Error_PacketOrder://包裹顺序错误
				   XmodemDisplayStrAfterTransfer((char *)XmodemStatuString[5]);
			     ThingsToDoInError();
			     return; 
       case Xmodem_Error_TransferSize: //传输大小出错
				   XmodemDisplayStrAfterTransfer((char *)XmodemStatuString[6]);
			     ThingsToDoInError();
			     return; 
       case Xmodem_Error_PacketData: //包裹出错的错误次数超过允许值
				   XmodemDisplayStrAfterTransfer((char *)XmodemStatuString[2]);
			     ThingsToDoInError();
			     return; 
       case Xmodem_Error_DataProcess: //数据处理过程(EEPROM编程)出错
				   XmodemDisplayStrAfterTransfer((char *)XmodemStatuString[3]);
					 ThingsToDoInError();
			     return;
       case Xmodem_Error_ManualStop: //用户输入Ctrl+C强制结束传输
				   XmodemDisplayStrAfterTransfer((char *)XmodemStatuString[1]);
			     ThingsToDoInError();
			     return;			 
			 case Xmodem_TransferDone: //传输完毕  
					 UARTPutc(ACK,1);
				   delay_ms(10);
				   UartPrintf("%s%s",(char *)XmodemStatuString[4],FinishContentString);
			     ThingsToDoWhenDone();
			     XmodemTransferReset();//复位传输状态机
			     return;		 
			 //其余状态是内部状态，啥也不做
			 default: break;	
		   }
		 }
