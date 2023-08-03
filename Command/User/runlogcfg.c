#include "console.h"
#include "CfgFile.h"
#include "runtimelogger.h"
#include "Xmodem.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef enum
 {
 RunLogCmd_DetectParam, //检查命令里面的参数
 RunLogCmd_OneShotParam, //只有需要一次执行的东西
 RunLogCmd_RequestXmodem, //请求Xmodem执行
 RunLogCmd_XmodemTxInProgress, //Xmodem发送进行中
 RunLogCmd_XmodemRxInProgress  //Xmodem接收进行中
 }runlogCfgStateDef;

static const char *RunLogOpStr="\r\n运行日志记录器已被%s,您需要使用'reboot'命令重启驱动以使更改生效.";
static const char *RunLogFileName="运行日志文件";

runlogCfgStateDef RunLogCmdState;
#ifndef Firmware_DIY_Mode
const char *NeedRoot="\r\n错误:您需要超级用户(厂家售后工程师)的用户权限方可%s";
#endif

//参数帮助entry
const char *runlogcfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return "清除系统中的运行日志";	
		case 2:
		case 3:return "设置运行日志记录器是否启用(需要重启方可生效)";
		case 4:
		case 5:return "将运行日志以Xmodem的方式保存到电脑进行分析";
		case 6:
		case 7:return "从电脑端以Xmodem的方式接收并恢复已有的运行日志";
		}
	return NULL;
	}
//退出前复位的操作
void ExitRunLogCmdHandler(void)	
  {
  if(RunLogCmdState!=RunLogCmd_OneShotParam)XmodemTransferReset();//不是Xmodem状态机调用不执行复位
	RunLogCmdState=RunLogCmd_DetectParam;//复位标志位
  ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕	
	}
//内部使用的函数，负责处理日志文件
void RestoreRunLog(void)
  {
	int CarrySize,RWSize,Address,WriteAddress;
	//开始处理
	UartPrintf("%s系统正在验证文件完整性,请等待....   ",XmodemStatuString[4]);
	if(!IsRunLogAreaOK(XmodemConfRecvBase))
			UARTPuts("失败\r\n错误:您上传的日志文件无效,文件已损坏.");					 
	else
			{
			UARTPuts("成功\r\n日志文件检查通过,正在搬运至ROM区域,请耐心等待...");
			CarrySize=RunTimeLogSize;
			Address=XmodemConfRecvBase;
			WriteAddress=RunTimeLogBase;
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
			if(CarrySize>0)UARTPuts("\r\n系统在编程运行日志文件时出现了错误.请重试.");
			else UARTPuts("\r\n运行日志文件已经还原成功,请手动重启驱动以读取新的日志.");
			}
	ExitRunLogCmdHandler();
	}	
//负责处理清除日志和控制记录器运行等只需要跑一次的功能
void runlogclrAndrunloggerEnCtrl(void)
 {
 char *ParamPtr;	
 char ParamOK;
 bool CmdParamOK=false;
 //清除日志的模块
 IsParameterExist("01",14,&ParamOK); 
 #ifndef Firmware_DIY_Mode
 if(ParamOK&&AccountState!=Log_Perm_Root)
   {
	 UartPrintf((char *)NeedRoot,"清除运行日志!"); 
	 CmdParamOK=true;
	 }
 else if(ParamOK)
 #else
 if(ParamOK)//清除记录
 #endif
   {
	 UARTPuts("\r\n正在清除系统中的运行日志,请等待...");
   if(ResetRunTimeLogArea())
     UARTPuts("\r\n运行日志已经成功清除.");
   else
     UARTPuts("\r\n运行日志清除失败,请重试.");
	 CmdParamOK=true;
	 }	 
 //控制记录器运行的模块
 ParamPtr=IsParameterExist("23",14,&ParamOK);
 #ifndef Firmware_DIY_Mode
 if(ParamPtr!=NULL&&AccountState!=Log_Perm_Root)
   {
	 UartPrintf((char *)NeedRoot,"控制记录器的运行!"); 
	 CmdParamOK=true;
	 }
 else if(ParamPtr!=NULL)switch(CheckUserInputIsTrue(ParamPtr)) 
 #else
 if(ParamPtr!=NULL)switch(CheckUserInputIsTrue(ParamPtr))
 #endif
   {
	 //用户输入了true
	 case UserInput_True:
	   {
		 CmdParamOK=true;
		 if(CfgFile.EnableRunTimeLogging)
		   {
			 UARTPuts("\r\n运行日志记录器已在工作中,无需再次启用.");
			 break;
			 }
		 UartPrintf((char *)RunLogOpStr,"启用");
		 CfgFile.EnableRunTimeLogging=true;
		 break;
		 }
	 //用户输入了false
	 case UserInput_False:
	   {
		 CmdParamOK=true;
		 if(!CfgFile.EnableRunTimeLogging)
		   {
			 UARTPuts("\r\n运行日志记录器已处于关闭状态,无需再次关闭.");
			 break;
			 }
		 UartPrintf((char *)RunLogOpStr,"禁用");
		 CfgFile.EnableRunTimeLogging=false;
		 break;
		 }	 
	 //其余情况
	 case UserInput_Nothing:
	   {
		 CmdParamOK=true;
		 DisplayIllegalParam(ParamPtr,14,2);//显示用户输入了非法参数
		 UARTPuts("\r\n如您需要关闭记录器,请输入'false'参数.对于开启,则输入'true'参数.");
		 break;
		 }
	 } 
 //命令运行完毕
 if(!CmdParamOK)UartPrintCommandNoParam(14);//显示啥也没找到的信息	
 ExitRunLogCmdHandler();
 }
//命令主函数
void runlogcfgHandler(void)
 {
 char ParamOK;
 //状态机，负责实现命令功能
 switch(RunLogCmdState)	 
   {
	 case RunLogCmd_DetectParam: //检查命令里面的参数
	      {
	      IsParameterExist("4567",14,&ParamOK); 
        if(!ParamOK)RunLogCmdState=RunLogCmd_OneShotParam;
	      else RunLogCmdState=RunLogCmd_RequestXmodem; //检查程序是否需要调用Xmodem传输
				break;
				}
   case RunLogCmd_OneShotParam:runlogclrAndrunloggerEnCtrl();break; //只有需要一次执行的东西
   case RunLogCmd_RequestXmodem: //请求Xmodem执行
	      {
				IsParameterExist("45",14,&ParamOK);
	      if(ParamOK) //发送
	         {
	         XmodemInitTxModule(RunTimeLogSize,RunTimeLogBase);
		       RunLogCmdState=RunLogCmd_XmodemTxInProgress;
		       }
	       else //接收
	         {
		       #ifndef Firmware_DIY_Mode
	         if(AccountState!=Log_Perm_Root)
		           {
			         UartPrintf((char *)NeedRoot,"从您的PC机上还原运行日志!"); 
			         ExitRunLogCmdHandler();
			         return;
			         }			 
	         #endif
		       RunLogCmdState=RunLogCmd_XmodemRxInProgress;
	         XmodemInitRXModule(RunTimeLogSize,XmodemConfRecvBase);
				   }
				 DisplayXmodemBackUp(RunLogFileName,!ParamOK); //显示提示信息
				 break;
				 }
   case RunLogCmd_XmodemTxInProgress:XmodemTxDisplayHandler(RunLogFileName,&ExitRunLogCmdHandler,&ExitRunLogCmdHandler);break; //Xmodem发送进行中
   case RunLogCmd_XmodemRxInProgress:XmodemRxDisplayHandler(RunLogFileName,&ExitRunLogCmdHandler,&RestoreRunLog);break;  //Xmodem接收进行中
	 }
 }
