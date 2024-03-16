#include "console.h"
#include "CfgFile.h"
#include "runtimelogger.h"
#include "selftestlogger.h"
#include "Xmodem.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef enum
 {
 logbkup_XmodemInit,
 logbkup_XmodemTxInprogress,
 logbkup_XmodemRxInprogress, 
 }logbkupStatuDef;

//变量和常量
const char *LogRestoreResult="错误日志文件还原%s.";
logbkupStatuDef logbkupcmdstate=logbkup_XmodemInit; 
#ifndef Firmware_DIY_Mode
void ExitRunLogCmdHandler(void);
extern const char *NeedRoot;
#endif 
 
//参数帮助entry
const char *logbkupArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return "将错误日志以Xmodem的方式保存到电脑进行分析";
		case 2:
		case 3:return "从电脑端以Xmodem的方式接收并恢复已有的错误日志";
		case 4:
	  case 5:return "将系统的自检日志以Xmodem的方式下载到电脑";
		}
	return NULL;
	} 
//结束操作
void logbkupCmdEndHandler(void)
 {
 if(logbkupcmdstate!=logbkup_XmodemInit)
		XmodemTransferReset();//初始化之后重置状态机
 logbkupcmdstate=logbkup_XmodemInit;
 ClearRecvBuffer();//清除接收缓冲
 CmdHandle=Command_None;//命令执行完毕	
 }

 
//恢复错误日志的操作
void RestoreErrorLogFromXmodem(void)
 {
 int CarrySize,RWSize,Address,WriteAddress;
	//开始处理
	UartPrintf("%s系统正在验证文件完整性,请等待....   ",XmodemStatuString[4]);
	if(!CheckForErrLogStatu(XmodemConfRecvBase))
			UARTPuts("失败\r\n错误:您上传的错误日志文件无效.");					 
	else
			{
			UARTPuts("成功\r\n正在还原错误日志,请耐心等待...");
			CarrySize=LoggerAreaSize;
			Address=XmodemConfRecvBase;
			WriteAddress=LoggerBase;
			while(CarrySize>0)
				 {
			   //计算读写大小
			   RWSize=CarrySize>128?128:CarrySize;
			   //读数据,写数据,擦除
			   if(M24C512_PageRead(XmodemTransferCtrl.XmodemRXBuf,Address,RWSize))break;
			   if(M24C512_PageWrite(XmodemTransferCtrl.XmodemRXBuf,WriteAddress,RWSize))break;
			   memset(XmodemTransferCtrl.XmodemRXBuf,0xFF,RWSize);
			   if(M24C512_PageWrite(XmodemTransferCtrl.XmodemRXBuf,Address,RWSize))break; //将临时缓存擦除为0xFF
			   //计算新的地址,剩余大小
			   CarrySize-=RWSize;
			   Address+=RWSize;
			   WriteAddress+=RWSize;
			   }
			UartPrintf((char *)LogRestoreResult,(CarrySize>0||!fetchloggerheader(&LoggerHdr))?"失败":"成功");
			}
	logbkupCmdEndHandler();	 	
  }
 
//命令处理函数
void logbkupHandler(void)
 {
 char RxParamOK,TxParamOK,errlogOK;
 //状态机
 switch(logbkupcmdstate)
  {
	 case logbkup_XmodemInit:
	   {
		 IsParameterExist("01",15,&TxParamOK);
		 IsParameterExist("23",15,&RxParamOK);
		 IsParameterExist("45",15,&errlogOK);
		 if(errlogOK)  //自检日志
		    {
				#ifndef Firmware_DIY_Mode
	      if(AccountState!=Log_Perm_Root)
		      {
			    UartPrintf((char *)NeedRoot,"下载自检日志到电脑!"); 
			    ExitRunLogCmdHandler();
			    return;
			    }			 
	      #endif
				XmodemInitTxModule((SelftestLogDepth*sizeof(SelfTestLogUnion)),SelfTestLogBase);
		    logbkupcmdstate=logbkup_XmodemTxInprogress;
				DisplayXmodemBackUp("自检日志文件",false); //显示提示信息
				}
	   else if(TxParamOK) //发送
	      {
	      XmodemInitTxModule(LoggerAreaSize,LoggerBase);
		    logbkupcmdstate=logbkup_XmodemTxInprogress;
				DisplayXmodemBackUp("错误日志文件",false); //显示提示信息
		    }
	   else if(RxParamOK)  //接收
	      {
		    #ifndef Firmware_DIY_Mode
	      if(AccountState!=Log_Perm_Root)
		      {
			    UartPrintf((char *)NeedRoot,"从您的PC机上还原错误日志!"); 
			    ExitRunLogCmdHandler();
			    return;
			    }			 
	      #endif
	      XmodemInitRXModule(LoggerAreaSize,XmodemConfRecvBase);
		    logbkupcmdstate=logbkup_XmodemRxInprogress;
				DisplayXmodemBackUp("错误日志文件",true); //显示提示信息
				}
		 else //啥也没有
		    {
				UartPrintCommandNoParam(15);//显示啥也没找到的信息
				logbkupCmdEndHandler();
				}
		 break;
		 }
	 case logbkup_XmodemTxInprogress:XmodemTxDisplayHandler("日志文件",&logbkupCmdEndHandler,&logbkupCmdEndHandler);break; //发送中
	 case logbkup_XmodemRxInprogress:XmodemRxDisplayHandler("错误日志文件",&logbkupCmdEndHandler,&RestoreErrorLogFromXmodem);break;  //Xmodem接收进行中
	}
 }
