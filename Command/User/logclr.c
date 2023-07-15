#include "console.h"
#include "logger.h"

//主处理函数
void logclrHandler(void)
 {
 UARTPuts("\r\n正在重新初始化日志区域并清除所有数据,这会需要一些时间,请等待...");
 ResetLoggerHeader_AutoUpdateTIM();
 if(ReInitLogArea())
   UARTPuts("\r\n错误日志已经成功清除.");
 else
   UARTPuts("\r\n错误日志清除失败,请重试.");
 ClearRecvBuffer();//清除接收缓冲
 CmdHandle=Command_None;//命令执行完毕		
 }
