#include "console.h"
#include "Xmodem.h"
#include "delay.h"
#include "selftestlogger.h"

/*
这个文件主要负责处理当驱动自检失败后进入的tinyshell环境
这个环境允许用户下载错误日志和手动重启驱动的固件再次尝试。
*/

//外部函数声明
bool InputDataBlackList(char DIN);

static char TinyShellPtr=0;
static char TinyShellBuf=0;
static const char *TinyShellText[]=
{
"\r\n在这个环境下您可以通过输入如下的命令来指定操作.",//0
"\r\n'X'或'x' : 通过xmodem方式将自检日志下载到电脑进行分析.",//1
"\r\n'E'或'e' : 通过xmodem方式将错误日志下载到电脑进行分析.", //2
"\r\n'R'或'r' : 手动重启驱动.",//3
"\r\n'M'或'm' : 查看驱动在自检失败时输出的最后一条自检日志信息.",//4
"\r\n\r\nRescue:>"//5
};


//简易shell收取数据的流程
void TinyshellGetData(void)
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
		//将数据写到tinyshell的缓冲区里面取
		else if(TinyShellPtr==0)
		  {
			TinyShellBuf=RXRingBuffer[QueueRearPTR];//载入已有的数据
			TinyShellPtr=1;	
			}
		QueueRearPTR=(QueueRearPTR+1)%CmdBufLen;//移动队尾指针
		}
 while(CurrentQueueFrontPTR!=QueueRearPTR);//反复循环复制数据
 }

//tinyshell的Xmodem handler 
void TinyshellXmodemHandler(void)
 {
 XmodemTransferReset();//复位Xmodem状态机
 TinyShellPtr=0;
 TinyShellBuf=0;// 完成处理开始处理别的	
 delay_ms(100);
 UARTPuts((char *)TinyShellText[5]);
 }	
 
//自检错误的时候进入的简易shell环境
void SelfTestErrorHandler(void)
 {
 int i;
 XmodemTransferReset();//复位Xmodem状态机
 delay_Second(3);
 UARTPuts("\x0C\033[2J");
 UARTPuts("\r\n驱动在POST时发现了致命硬件错误.为了保证安全,自我保护模式已被激活.\r\n此模式在问题解决前会导致手电筒将不可用.");
 UARTPuts("\r\n对于消费者而言,您可准备好SecureCRT软件,连接到驱动后输入'X'然后选择文件->");
 UARTPuts("\r\n接收Xmodem选项接收自检日志,接着将收到的自检日志文件发送给商家以分析\r\n问题所在.");
 for(i=0;i<6;i++)UARTPuts((char *)TinyShellText[i]);
 while(1)//主循环
  {
	TinyshellGetData();//收取数据
	if(XmodemTransferCtrl.XmodemTransferMode==Xmodem_Mode_NoTransfer&&TinyShellPtr)
	  {
		//回显，检查确认字符是合法的方可执行回显
		if(InputDataBlackList(TinyShellBuf))
		  {	
		  UARTPutc(TinyShellBuf,1);
		  UARTPuts("\r\n");
		  }
		//显示最后一条自检信息
		if(TinyShellBuf=='M'||TinyShellBuf=='m')
		  {
			UARTPuts("\r\n您请求了显示最后一条自检的日志信息,请等待系统从存储器内取出该信息...");
			if(!DisplayLastTraceBackMessage())
			  UARTPuts("\r\n系统取出该信息时遇到了错误,请重试.");
			else
				UARTPuts("\r\n\r\n系统已成功取出该信息,该信息内容如上.");
			UARTPuts((char *)TinyShellText[5]);//显示shell字符
			}
		//重启系统
		else if(TinyShellBuf=='R'||TinyShellBuf=='r')
		  {
			UARTPuts("\r\n您请求了手动重启,系统将在3秒后重启...\r\n\r\n");
			delay_Second(3);
			NVIC_SystemReset();
			while(1);
			}
		//发送错误日志
		else if(TinyShellBuf=='E'||TinyShellBuf=='e')
		  {
			UARTPuts("\r\n您请求了下载错误日志,请打开Xmodem传输软件进行错误日志的接收.\r\n\r\n");
			XmodemInitTxModule(LoggerAreaSize,LoggerBase);
			}
		//发送错误日志
		else if(TinyShellBuf=='X'||TinyShellBuf=='x')
		  {
			UARTPuts("\r\n您请求了下载自检日志,请打开Xmodem传输软件进行自检日志的接收.\r\n\r\n");
			XmodemInitTxModule((SelftestLogDepth*sizeof(SelfTestLogUnion)),SelfTestLogBase);
			}
		//输入了非法内容
		else if(InputDataBlackList(TinyShellBuf))
		  {
			UARTPuts("\x0C\033[2J");
			UARTPuts("\r\n您输入的命令不合法,请重试.");
      for(i=0;i<6;i++)UARTPuts((char *)TinyShellText[i]);
			}
	  TinyShellPtr=0;
		TinyShellBuf=0;// 完成处理开始处理别的	
		}
	else if(XmodemTransferCtrl.XmodemTransferMode!=Xmodem_Mode_NoTransfer) //xmodem传输中
	  {
	  XmodemTxStateMachine();//执行状态机逻辑
		XmodemTxDisplayHandler("日志文件",&TinyshellXmodemHandler,&TinyshellXmodemHandler); 	
		}
	}
 }
