#include "console.h"

void clearHandler(void)
  {
	UARTPuts("\x0C\033[2J");
	ClearRecvBuffer();//清除接收缓冲
	CmdHandle=Command_None;//命令执行完毕				
	}
