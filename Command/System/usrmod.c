#include "cfgfile.h"
#include "console.h"
#include <string.h>

//字符串
static const char *InputTooLong="错误:系统的%s应在3-19字符长度的范围内且仅能包含ASCII字符.";

//enum
typedef enum
{
UsrMod_None_Operation,
UsrMod_ChgHostName,
UsrMod_ChgUsrName,
UsrMod_ChgPassword
}UsrmodState;

UsrmodState UsrcmdState;

//强制关闭处理
void usrmod_ctrlC_handler(void)
  {
	UsrcmdState=UsrMod_None_Operation;
	TargetAccount=VerifyAccount_None;
	Verifystat=ACC_No_Login;
	}
//具体的参数帮助
const char *usrmodArgument(int ArgCount)
  {
	switch(ArgCount)
	 {
		case 0:
		case 1:return "请求更改管理员或超级用户的密码";
	  case 2:
		case 3:return "更改管理员用户的名称";
		case 4:
		case 5:return "更改驱动终端的主机名";
	 }
	return "";
	}
//实际的命令处理
void usrmodhandler(void)
  {
	char ParamExist;
	int namelen;
	char *NamePtr;
	//开始扫描用户输入
	if(UsrcmdState==UsrMod_None_Operation)
	  {
		if(IsParameterExist("45",3,&ParamExist)!=NULL)
			  UsrcmdState=UsrMod_ChgHostName;//找到了改主机名的命令
		else if(IsParameterExist("23",3,&ParamExist)!=NULL)
			  UsrcmdState=UsrMod_ChgUsrName;//找到了改用户名的命令
		else //没找到需要改用户名的选项。
		  {
			IsParameterExist("01",3,&ParamExist);
			if(ParamExist)UsrcmdState=UsrMod_ChgPassword;//找到了改密码的命令
			else //啥也没有找到
				{
				UartPrintCommandNoParam(3);//显示啥也没找到的信息
				ClearRecvBuffer();//清除接收缓冲
				CmdHandle=Command_None;//命令执行完毕
				}
		  }
		}
	 //更改主机名的操作
	 if(UsrcmdState==UsrMod_ChgHostName)
	  {
		//获取名称指针
	  NamePtr=IsParameterExist("45",3,&ParamExist);
		if(NamePtr!=NULL)namelen=strlen(NamePtr);
	  else namelen=0;
		//检查名称是否合法
		if(NamePtr==NULL)UARTPuts("\r\n错误:系统的主机名不得为空!");	 
		else if(namelen<3||namelen>19)UartPrintf((char *)InputTooLong,"主机名");	 
		else //执行更新名字的操作	
			{
			strncpy(CfgFile.HostName,NamePtr,20);
			UartPrintf("\r\n系统的主机名已更新为:%s.",CfgFile.HostName);	 
			}
		//更新完毕，检测是否需要设置用户名，密码等
		if(IsParameterExist("23",3,&ParamExist)!=NULL)UsrcmdState=UsrMod_ChgUsrName;//需要设置用户名（需不需要设置密码在设置用户名时检查）
		else if(IsParameterExist("01",3,&ParamExist)!=NULL)UsrcmdState=UsrMod_ChgPassword;//只需要设置密码
		else //操作完毕了
		  { 			
			ClearRecvBuffer();//清除接收缓冲
			CmdHandle=Command_None;//命令执行完毕
		  UsrcmdState=UsrMod_None_Operation;//啥都不需要去设置
			}
		}
	 //更改用户名的操作
	 if(UsrcmdState==UsrMod_ChgUsrName)
	  {
		NamePtr=IsParameterExist("23",3,&ParamExist);
		if(NamePtr!=NULL)namelen=strlen(NamePtr);
	  else namelen=0;
		if(AccountState==Log_Perm_Root)UARTPuts("\r\n错误:超级用户的名称不允许被更改!");
		else if(NamePtr==NULL)UARTPuts("\r\n错误:管理员用户的名称不得为空!");	 
		else if(namelen<3||namelen>19)UartPrintf((char *)InputTooLong,"管理员用户的名称");	 
		else //执行更新名字的操作
		  {
			strncpy(CfgFile.AdminAccountname,NamePtr,20);
			UartPrintf("\r\n管理员的用户名已更新为:%s.",CfgFile.AdminAccountname);	 
			}
		//判断需不需要设置密码，不需要的话本次命令就已经完成了
		IsParameterExist("01",3,&ParamExist);
		if(ParamExist)UsrcmdState=UsrMod_ChgPassword;//找到了改密码的命令
		else 
		  {
			ClearRecvBuffer();//清除接收缓冲
			CmdHandle=Command_None;//命令执行完毕
			}
		}
   //判断是否需要更改用户密码
	 if(UsrcmdState==UsrMod_ChgPassword)
	  {
		//还未登录，启动用户名输入
		if(Verifystat==ACC_No_Login)
		  {
			UARTPuts("\r\n\r\n请输入需要更改密码的用户名称 : ");
			Verifystat=ACC_Enter_UserName;
			ClearRecvBuffer();//清除接收缓冲
			}
		//root用户无需输入密码即可改ADMIN的账户
		if(AccountState==Log_Perm_Root&&TargetAccount==VerifyAccount_Admin&&Verifystat==ACC_Enter_Pswd)
		  {
			Verifystat=ACC_Verify_OK;
			ClearRecvBuffer();//清除接收缓冲
			}
		//认证结束
		if(Verifystat==ACC_Verify_OK)
		  {
			UARTPuts("\r\n请输入该用户的新密码 : ");
			Verifystat=ACC_ChangePassword;
			ClearRecvBuffer();//清除接收缓冲				
			CurCmdField=PasswordField;//密码消隐
			}
		//认证结束且失败。
		if(Verifystat==ACC_Verify_Error)
		  {
			UARTPuts("\r\n原始密码不匹配，无法更新用户凭据信息。");
		  TargetAccount=VerifyAccount_None;
			CmdHandle=Command_None;//命令执行完毕
			Verifystat=ACC_No_Login;//复位状态
			ClearRecvBuffer();//清除接收缓冲		
			}
		//改密码成功
		if(Verifystat==ACC_ChgPswdOK)
		  {
			UARTPuts("\r\n\r\n用户凭据信息已更新，请使用新的凭据进行登录。");
			AccountState=Log_Perm_Guest;
			TargetAccount=VerifyAccount_None;
			CmdHandle=Command_None;//命令执行完毕
			Verifystat=ACC_No_Login;//复位状态
			ClearRecvBuffer();//清除接收缓冲		
			}
		if(Verifystat==ACC_ChgPswdErr_NoPerm||Verifystat==ACC_ChgPswdErr_LenErr)
		  {
			UARTPuts("\r\n\r\n错误:");
			if(Verifystat==ACC_ChgPswdErr_NoPerm)UARTPuts("您当前没有更改超级用户密码的权限.");
			else UARTPuts("新密码应在6-15字符长度的范围内且仅能包含ASCII字符。");
			TargetAccount=VerifyAccount_None;
			CmdHandle=Command_None;//命令执行完毕
			Verifystat=ACC_No_Login;//复位状态
			ClearRecvBuffer();//清除接收缓冲		
			}		
		}
	}
