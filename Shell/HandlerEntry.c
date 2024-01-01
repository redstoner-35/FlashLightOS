#include "cmdentry.h"
#include "console.h"

//每条命令响应的入口
void CommandHandler(void)
  {
	//没有句柄
	if(CmdHandle==Command_None)return;	
	//根据命令句柄找对应的Handler去执行
  else switch(CmdHandle)
	  {
		case Command_login:LoginHandler();break;//login
		case Command_logout:LogoutHandler();break;//logout	
		case Command_cfgmgmt:cfgmgmthandler();break;//cfgmgmt
	  case Command_UserMod:usrmodhandler();break;//usrmod   
		case Command_clear:clearHandler();break;//clear
	  case Command_ver:verHandler();break;//ver
		case Command_help:HelpHandler();break;//help
		case Command_modepofcfg:modepofcfghandler();break;//modepofcfg
		case Command_termcfg:termcfgHandler();break;//termcfg
		case Command_reboot:Reboothandler();break;//reboot
		case Command_imonadj:Imonadjhandler();break;//imonadj
		case Command_logview:logviewhandler();break;//logview
		case Command_logclr:logclrHandler();break;//logclear
		case Command_runlogview:runlogviewHandler();break;//runlogview
	  case Command_runlogcfg:runlogcfgHandler();break;//runlogcfg
		case Command_logbkup:logbkupHandler();break;//logbkup
		case Command_modeenacfg:ModeEnacfghandler();break;//modeenacfg
		case Command_modeadvcfg:modeadvcfgHandler();break;//modeadvcfg
		case Command_modecurcfg:modecurcfghandler();break;//modecurcfg
		case Command_strobecfg:strobecfghandler();break;//strobecfg
		case Command_mostranscfg:mostranscfghandler();break;//mostranscfg
		case Command_breathecfg:breathecfghandler();break;//breathecfg
		case Command_thermalcfg:thermalcfghandler();break;//thermalcfg
		case Command_modeporcfg:modeporcfghandler();break;//modeporcfg
		case Command_battcfg:battcfghandler();break;//battcfg
		case Command_customflashcfg:customflashcfgHandler();break;//customflashcfg
		case Command_modeview:modeviewhandler();break;//modeview
	  case Command_fruedit:fruedithandler();break;//fruedit
		case Command_thermaltripcfg:thermaltripcfgHandler();break;//thermaltripcfg
		case Command_rampcfg:rampcfghandler();break; //rampcfg
		#ifdef FlashLightOS_Debug_Mode
		case Command_eepedit:eepedithandler();break;//eepedit
		#endif
		//其余情况，显示错误或者直接退出
    default:
		  {
			#ifdef FlashLightOS_Debug_Mode
			UARTPuts("FlashLight OS CLI Internal Error:No matching handler for command '");
			UARTPuts(RXBuffer);
			UARTPuts("'.\r\nThis means you forget to register a handle when submit a new");
			UARTPuts("\r\ncommand entry. For developer,go to 'HandlerEntry.c' to check.");
			#endif
			ClearRecvBuffer();//清除接收缓冲
	    CmdHandle=Command_None;//命令执行完毕			
			return;
			} 
		}
	}
//shell所有分支事务的handler
void ShellProcUtilHandler(void)
  {
  //DMA环形缓冲区内容接收处理和用户登录超时计时器的处理
	RXDMACallback();	
  IdleTimerHandler();		 
	//处理用户操作的事件
	LoopbackAndDelHandler(); //用户输入内容（命令密码等）回显和退格事件的处理
	ArrowKeyHandler();  //用户按下方向键（上下左右）
	TabHandler();  //用户按下tab自动补全
	CtrlCHandler();  //用户按下CtrlC强制中断命令执行
	CmdExecuteHandler(); //用户按下回车执行命令
	DMAFullHandler();  //命令缓冲区满
  //处理shell注册的命令句柄		
	CommandHandler();
	}
