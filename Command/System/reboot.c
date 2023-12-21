#include "console.h"
#include "delay.h"

typedef enum
{
reboot_init,
reboot_waitconfirm,
reboot_wait
}rebootstatdef;

//局部变量
static rebootstatdef rebootstat=reboot_init;
static unsigned char rebootcounter=0;

//reboot计时器
void reboot_TIM_Callback(void)
  {
	unsigned char buf;
	if(!(rebootcounter&0x80))return;
	buf=rebootcounter&0x7F;
  if(buf<15)buf++;
  else
	  {
		NVIC_SystemReset();
			while(1);//强制重启
		}		
	rebootcounter&=0x80;
	rebootcounter|=buf;//将数值加1再写回去
	}

//使用ctrlc强制打断的操作
void reboot_CtrlC_Handler(void)
  {
	rebootcounter=0;//强制停止计时器
	rebootstat=reboot_init;
	ClearRecvBuffer();//清除接收缓冲
	CmdHandle=Command_None;//命令执行完毕
	}

void Reboothandler(void)
  {
	//等待操作
	if(rebootstat==reboot_init)
	  {
		UARTPuts("\r\n警告:强制重启固件并重新初始化将会清除当前所有未保存的配置.如您确定");
		UARTPuts("\r\n需要进行强制重启,请在下方输入'我知道我在干什么,请继续'并按下回车.");
    UARTPuts("\r\n\r\n?");
    ClearRecvBuffer();//清除接收缓冲
    YConfirmstr="我知道我在干什么,请继续";
    YConfirmState=YConfirm_WaitInput;
		rebootstat=reboot_waitconfirm;
		}
	else if(rebootstat==reboot_waitconfirm)//等待认证
	  {
		if(YConfirmState==YConfirm_OK)//确认重启
		  {
			UARTPuts("\r\n系统将在3秒后自动重启.如果您想打断此操作,请按下ctrl+c");	
      rebootstat=reboot_wait;
			rebootcounter=0x80;//启动计时器
			}
		else if(YConfirmState==YConfirm_Error)
		  {
			UARTPuts("\r\n重启操作已被用户阻止.");
	    rebootstat=reboot_init;
	    ClearRecvBuffer();//清除接收缓冲
	    CmdHandle=Command_None;//命令执行完毕
			}
		}
	}
