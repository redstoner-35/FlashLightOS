#include "console.h"
#include "logger.h"

//主处理函数
void logclrHandler(void)
 {
 bool IsLogCleared;
 UARTPuts("\r\n正在重新初始化日志区域并清除所有数据,这会需要一些时间,请等待...");
 ResetLoggerHeader_AutoUpdateTIM();
 IsLogCleared=ReInitLogArea(); //清除
 UartPrintf("\r\n错误日志清除操作%s.",IsLogCleared?"成功完成":"完成失败");
 ClearRecvBuffer();//清除接收缓冲
 CmdHandle=Command_None;//命令执行完毕		
 }
