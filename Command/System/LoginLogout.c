#include "cfgfile.h"
#include "console.h"
#include <string.h>
#include "runtimelogger.h"

unsigned int LogErrTimer=0;
unsigned int PunishSec=80;
extern int IdleTimer;
extern float MaximumBatteryPower;

//显示登录错误剩余时长
void DisplayLoginDelayTime(void)
  {
	if(LogErrTimer>28800)
	    UartPrintf("%d时%d分%d秒.",LogErrTimer/28800,(LogErrTimer%28800)/480,((LogErrTimer%28800)%480)/8);
	else if(LogErrTimer>480)
	   UartPrintf("%d分%d秒.",LogErrTimer/480,(LogErrTimer%480)/8);
	else	
	   UartPrintf("%d秒.",LogErrTimer/8);
	}

//强制终止处理
void login_ctrlC_handler(void)
  {
	TargetAccount=VerifyAccount_None;
	Verifystat=ACC_No_Login;
  CurCmdField=TextField;//强制恢复为文本，重新显示
	}
//登录操作
void LoginHandler(void)	
  {
	//还未登录，启动用户名输入
		if(Verifystat==ACC_No_Login)
		  {
			if(LogErrTimer>0)//时间未到
			  {
				UARTPuts("\r\n在重新登录前您仍需等待");
        DisplayLoginDelayTime();
				TargetAccount=VerifyAccount_None;
			  CmdHandle=Command_None;//命令执行完毕
			  Verifystat=ACC_No_Login;//复位状态
				}
			else if(AccountState!=Log_Perm_Guest)//已登录
			  {
				UartPrintf("\r\n你已经以%s身份登录到系统!",AccountState==Log_Perm_Admin?"管理员":"超级用户");
				UARTPuts("\r\n如果您想切换用户,请退出并重新登录.");
				TargetAccount=VerifyAccount_None;
			  CmdHandle=Command_None;//命令执行完毕
			  Verifystat=ACC_No_Login;//复位状态
				}
		  else //正常登录
			  {
				UARTPuts("\r\n\r\n请输入用户名 : ");
			  Verifystat=ACC_Enter_UserName;
				}
			ClearRecvBuffer();//清除接收缓冲
			}
		//认证结束
		if(Verifystat==ACC_Verify_OK)
		  {
			UARTPuts("\r\n登录完毕,您当前以");
			if(TargetAccount==VerifyAccount_Admin)
			  {
				UARTPuts("管理员");
				AccountState=Log_Perm_Admin;
				}
			else if(TargetAccount==VerifyAccount_Root)
			  {
				UARTPuts("超级用户");
				AccountState=Log_Perm_Root;							
				}
			UARTPuts("的身份登录.");
			if(RunLogEntry.Data.DataSec.IsLowQualityBattAlert) //电池质量警告生效
			  {
			  UARTPuts("\r\n\r\n警告:您使用的电池无法满足放电需求,为了保证电池和您的安全,手电筒的运行功率将");
			  UartPrintf("\r\n会被暂时限制为60%%.请尽快更换放电能力大于%.2fW的高性能动力电池并\r\n使用'battcfg -crst'命令消除告警.\r\n",MaximumBatteryPower);
				}
	    if(CfgFile.IdleTimeout>0)IdleTimer=CfgFile.IdleTimeout;//重置登录超时计时器
			TargetAccount=VerifyAccount_None;
			CmdHandle=Command_None;//命令执行完毕
			Verifystat=ACC_No_Login;//复位状态
			}
		else if(Verifystat==ACC_Verify_Error)
		  {
			UARTPuts("\r\n您提供的凭据无效,登录失败.\r\n为了保证安全,在重新尝试登录前您需等待");
			LogErrTimer=PunishSec;
			PunishSec*=2;
      DisplayLoginDelayTime();
			AccountState=Log_Perm_Guest;
			TargetAccount=VerifyAccount_None;
			CmdHandle=Command_None;//命令执行完毕
			Verifystat=ACC_No_Login;//复位状态
			}
	}
//退出登录操作
void LogoutHandler(void)
  {
		AccountState=Log_Perm_Guest;
		UARTPuts("\r\n登出完毕.");
		CmdHandle=Command_None;//命令执行完毕
	}
