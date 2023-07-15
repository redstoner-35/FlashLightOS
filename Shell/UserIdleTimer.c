#include "console.h"
#include "CfgFile.h"

int IdleTimer=-1;

//Idle定时器callback
void IdleTimerCallback(void)
 {
 if(AccountState==Log_Perm_Guest)return;
 if(IdleTimer>0)IdleTimer--;
 }
//显示延时
void DisplayNoActTime(void)
 {
	int NoACTTime=CfgFile.IdleTimeout;
 	if(NoACTTime>28800)
	    UartPrintf("%d时%d分%d秒",NoACTTime/28800,(NoACTTime%28800)/480,((NoACTTime%28800)%480)/8);
	else if(NoACTTime>480)
	   UartPrintf("%d分%d秒",NoACTTime/480,(NoACTTime%480)/8);
	else	
	   UartPrintf("%d秒",NoACTTime/8);
 }
 
//定时handler
void IdleTimerHandler(void)
 {
 //登录超时功能被禁用(或者位于游客权限)
 if(CfgFile.IdleTimeout==0||AccountState==Log_Perm_Guest)IdleTimer=-1;
 //时间到,退出登录
 else if(IdleTimer==0)
   {
	 IdleTimer=-1;
	 UARTPuts("\r\n\r\n由于用户在");
	 DisplayNoActTime();
	 UARTPuts("内没有活动,已自动退出登录.");
	 AccountState=Log_Perm_Guest;
	 PrintShellIcon(); 
	 }
 }
