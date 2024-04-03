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
static char TinyShellKey[]={'x','e','r','m'};
static const char *TinyShellText[]=
{
"\r\n在这个环境下您可输入如下命令进行排查.",//0
"自检",//1
"错误", //2
"手动重启驱动",//3
"查看驱动输出的最后一条自检日志信息",//4
"\r\n\r\nRescue:>",//5
"\r\n您请求了下载%s日志,请打开Xmodem传输软件以接收日志.\r\n\r\n",//6
};

//显示Shell操作内容
static void PrintShellContent(void)
 {
 int i;
 for(i=0;i<6;i++)
	 {
	 if(i!=0&&i<5)UartPrintf("\r\n'%c'或'%c' : ",TinyShellKey[i-1]-0x20,TinyShellKey[i-1]);
	 if(i!=0&&i<3)UartPrintf("通过xmodem方式将%s日志下载到电脑进行分析",TinyShellText[i]);
	 else UARTPuts((char *)TinyShellText[i]);
	 }
 }

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
 XmodemTransferReset();//复位Xmodem状态机
 delay_Second(3);
 UARTPuts("\x0C\033[2J");
 UARTPuts("\r\n驱动在POST时发现了致命硬件错误.为避免故障扩大,自我保护模式已激活.");
 UARTPuts("\r\n如您作为消费者看到此提示,请输入'X'下载日志并发给商家分析问题."); 
 PrintShellContent();//打印操作提示
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
				UARTPuts("\r\n正在读取最后一条自检的日志信息...");
			if(!DisplayLastTraceBackMessage())
			  UARTPuts("\r\n系统读取该信息时遇到了错误,请重试.");
			UARTPuts((char *)TinyShellText[5]);//显示shell字符
			}
		//重启系统
		else if(TinyShellBuf=='R'||TinyShellBuf=='r')
		  {
			UARTPuts("\r\n系统将在3秒后重启...\r\n\r\n");
			delay_Second(3);
			NVIC_SystemReset();
			while(1);
			}
		//发送错误日志
		else if(TinyShellBuf=='E'||TinyShellBuf=='e')
		  {
		  UartPrintf((char *)TinyShellText[6],"错误");
			XmodemInitTxModule(LoggerAreaSize,LoggerBase);
			}
		//发送错误日志
		else if(TinyShellBuf=='X'||TinyShellBuf=='x')
		  {
		  UartPrintf((char *)TinyShellText[6],"自检");
			XmodemInitTxModule((SelftestLogDepth*sizeof(SelfTestLogUnion)),SelfTestLogBase);
			}
		//输入了非法内容
		else if(InputDataBlackList(TinyShellBuf))
		  {
			UARTPuts("\x0C\033[2J");
			UARTPuts("\r\n您输入的命令不合法,请重试.");
      PrintShellContent();//打印操作提示
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
