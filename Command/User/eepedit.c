#include "cfgfile.h"
#include "console.h"
#include "I2C.h"
#include "Xmodem.h"
#include <string.h>
#include "delay.h"

#ifdef FlashLightOS_Init_Mode

//状态枚举
typedef enum
 {
 eepedit_GetParam,
 eepedit_reading,
 eepedit_writing
 }eepeditCmdStateDef;

//变量和字符串 
static eepeditCmdStateDef eepCmdState=eepedit_GetParam;
static const char *eepOpStr="\r\n系统将通过Xmodem完成整个EEPROM的%s操作,请打开您的内容传输软件.";
 
//强制停止处理
void eepedit_CtrlC_Handler(void)
  {
	//复位命令状态
	eepCmdState=eepedit_GetParam;	
	}

//Xmodem出错的处理
void eepedit_Xmodem_Error_Handler(void)
  {
	XmodemTransferReset();//复位传输状态机
	eepCmdState=eepedit_GetParam;	
	ClearRecvBuffer();//清除接收缓冲
	CmdHandle=Command_None;//命令执行完毕	
	}
	
//参数帮助entry
const char *eepeditargument(int ArgCount)
  {
	switch(ArgCount)
	  {
	  case 0:
		case 1:return "读取驱动的整个EEPROM的所有内容并通过Xmodem传输到电脑";
		case 2:
		case 3:return "将电脑发送的数据文件写入到驱动的EEPROM内";
		}
	return NULL;
	}
	
//命令处理函数
void eepedithandler(void)
  {
	char IsCmdOK;	
	//状态机
	switch(eepCmdState)
	  {
		//命令被执行，读取用户输入的参数
		case eepedit_GetParam:
			IsParameterExist("01",30,&IsCmdOK);
			if(IsCmdOK) //备份
				{
				eepCmdState=eepedit_reading; //进入读取阶段
				UartPrintf((char *)eepOpStr,"内容备份");
				XmodemInitTxModule(MaxByteRange+1,0x0000); //从0开始读取			
				break;
				}
			IsParameterExist("23",30,&IsCmdOK);
			if(IsCmdOK) //写入
				{
				eepCmdState=eepedit_writing; //进入读取阶段
				UartPrintf((char *)eepOpStr,"写入");
				XmodemInitRXModule(MaxByteRange+1,0x0000); //从0开始读取			
				break;
				}		
			//命令没有任何参数，显示未找到参数
			UartPrintCommandNoParam(30);//显示啥也没找到的信息
			eepCmdState=eepedit_GetParam;	
			ClearRecvBuffer();//清除接收缓冲
			CmdHandle=Command_None;//命令执行完毕
			break;
		//正在读取EEPROM内容
		case eepedit_reading:
			XmodemTxDisplayHandler("数据",&eepedit_Xmodem_Error_Handler,&eepedit_Xmodem_Error_Handler);
		  break;
	  //正在写入EEPROM内容
	  case eepedit_writing:
			XmodemRxDisplayHandler("数据",&eepedit_Xmodem_Error_Handler,&eepedit_Xmodem_Error_Handler);
		  break;
		}
	}
#endif
