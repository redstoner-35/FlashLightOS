#include "cfgfile.h"
#include "console.h"
#include "Xmodem.h"
#include <string.h>

//字符串
static const char *PleaseEnterStr="请在下方输入 '我知道我在干什么' 然后回车以继续.";
static const char *EnterInputSnippet="\r\n\r\n?";
static const char *ConfigIsSame="\r\n当前配置和%s配置文件中的配置一致,无需加载.";
static const char *ConfigFileCorrDetected="\r\n检测到%s配置文件损毁,系统将尝试自动修复.";

//enum
typedef enum
{
CfgMgmtNoneOp,//没有操作
CfgMgmtRequestWrite,//写入请求
CfgMgmtWriting,//正在写入中
CfgMgmtRequestRead,//读取请求
CfgMgmtReading,//正在验证中
CfgMgmtRequstRestoreFactory,//请求恢复到工厂设置 
CfgMgmtConfirmOverridePassword, //是否覆盖密码
CfgMgmtRequestBackup,
CfgMgmtWaitXmodemConfirm,
CfgMgmtRequestRestore,
CfgMgmtBackupInProgress,
CfgMgmtRestoreInProgress,
}cfgMgmtState;

cfgMgmtState CfgmgmtState;
bool IsUsingBackupFiles;

//提示操作已经被用户终止
static void OperationStopByUserInfo(void)
  {
	UARTPuts("\r\n操作已被用户阻止.");
	CfgmgmtState=CfgMgmtNoneOp;
	ClearRecvBuffer();//清除接收缓冲
	CmdHandle=Command_None;//命令执行完毕
	}
//显示允许的参数
static void DisplayParam(void)
  {
	UARTPuts("\r\n允许的参数如下：");
	UARTPuts("\r\n'Main':指示程序对主用配置文件进行操作");
	UARTPuts("\r\n'Backup':指示程序对备用配置文件进行操作");
	}
//显示损坏结果
static bool DisplayConfError(int checkresult,bool IgnoreCorrError)
  {
	 switch(checkresult)
      {
			case 1:UARTPuts("\n\n错误:配置存储器离线。");break;
      case 2:UARTPuts("\n\n错误:内部加解密模块异常");break;
	    case 3:UARTPuts("\n\n错误:配置文件已损坏或被篡改");
			}
	 if(checkresult)//内部错误
			{
			if(IgnoreCorrError&&checkresult==3)return false;
			UARTPuts("\r\n由于上方所列出的致命系统错误，指定的配置文件操作无法完成。");
			ClearRecvBuffer();//清除接收缓冲
			CmdHandle=Command_None;//命令执行完毕
			return true;
			}
	return false;
	}
//显示文件状态
static void DisplayFileState(const char *name,int checkresult,unsigned int MainCRC)
{
UARTPuts("\r\n----------- 配置文件状态 ----------");
UartPrintf("\r\n文件名称 : %s",name);
UartPrintf("\r\n文件大小 : %dBytes",sizeof(CfgFile));
UartPrintf("\r\n文件CRC-32校验和 : 0x%08X",MainCRC);
UartPrintf("\r\n文件完整性 : %s",(checkresult?"损坏":"正常"));
}
//保存函数在保存配置前检测到配置相同的提示
static void SaveFileDisplay(const char *name)
{
UartPrintf("\r\n似乎%s配置文件和当前的配置文件包含相同的配置。如果您需要强制覆",name);
UartPrintf("\r\n盖%s配置文件,%s%s",name,PleaseEnterStr,EnterInputSnippet);
ClearRecvBuffer();//清除接收缓冲
YConfirmstr="我知道我在干什么";
YConfirmState=YConfirm_WaitInput;
}

static void ReadFileDisplay(const char *name)
{
UartPrintf("\r\n警告:你确定要将当前配置覆盖为%s配置吗?此操作将会摧毁当前配置且无法",name);
UartPrintf("\r\n撤销!如果你希望继续,%s%s",PleaseEnterStr,EnterInputSnippet);
ClearRecvBuffer();//清除接收缓冲
YConfirmstr="我知道我在干什么";
YConfirmState=YConfirm_WaitInput;
}
//参数帮助entry
const char *cfgmgmtArgument(int ArgCount)
  {
	switch(ArgCount)
	 {
		case 0:
		case 1:return "将当前配置恢复到出厂设置";
	  case 2:
	  case 3:return "将当前配置保存到ROM中。";
		case 4:
	  case 5:return "检查ROM中配置文件的完整性";
		case 6:
		case 7:return "从ROM中加载配置。";
		case 8:
		case 9:return "将ROM中指定的配置通过Xmodem方式备份至电脑";
		case 10:
		case 11:return "通过Xmodem协议接收配置并应用到当前配置";
	 }
	return "";
	}
//ctrl+c entry
void cfgmgmt_ctrlc_handler(void)
  {
	CfgmgmtState=CfgMgmtNoneOp;
	YConfirmState=YConfirm_Idle;
  IsUsingBackupFiles=false;
	}
//在执行这个命令的时候如果Xmodem传输失败的故障处理
void cfgmgmtCmdXmodemErrorHandler(void)	
  {
	cfgmgmt_ctrlc_handler();
	XmodemTransferReset();//复位传输状态机
	ClearRecvBuffer();//清除接收缓冲
	CmdHandle=Command_None;//命令执行完毕
	}
//恢复配置
void CheckAndRestoreConfigFromXmodem(void)
  {
  unsigned int CRC32Result;
	char result;
  UartPrintf("%s系统正在验证文件完整性,请等待....   ",XmodemStatuString[4]);
	result=CheckConfigurationInROM(Config_XmodemRX,&CRC32Result);
	if(result)
	  UartPrintf("失败\r\n错误:您上传的配置文件无效,文件已损坏或被篡改.系统错误码:0x%02X",result);					 
	else
	  {
		UartPrintf("成功\r\n您上传的配置文件已通过验证,该文件的CRC32为0x%08X.",CRC32Result);
		UARTPuts("\r\n请耐心等待系统对文件进行预处理...");		
    result=ReadConfigurationFromROM(Config_XmodemRX);
		if(result)
	    UartPrintf("错误:系统在尝试应用您的配置文件时出现问题,错误码:0x%02X",result);	
		else //恢复成功
		  {
      if(M24C512_Erase(XmodemConfRecvBase,CfgFileSize))
				UARTPuts("错误:配置文件恢复成功,但是试图清除缓存时出现问题,请重试.");
			else 
				UARTPuts("\r\n配置文件恢复成功,请输入'cfgmgmt -s <配置文件类型>'进行固化.");	
			}				
	  }
	cfgmgmtCmdXmodemErrorHandler();
	}
//命令处理
void cfgmgmthandler(void)
  {
	char ParamExist,IsWantToXmodemRestore;
	int namelen,checkresult;
	char *NamePtr;
  unsigned int MainCRC;
	//命令处理的状态机
  switch(CfgmgmtState)
	 { 
	 case CfgMgmtNoneOp://识别参数
	  {
		IsParameterExist("01",4,&ParamExist);
		IsParameterExist("AB",4,&IsWantToXmodemRestore);
		if(IsWantToXmodemRestore)//使用Xmodem恢复配置
		   {
			 CfgmgmtState=CfgMgmtWaitXmodemConfirm;
			 YConfirmstr="我知道我在干什么";
			 ReadFileDisplay("由Xmodem传输过来的");
			 YConfirmState=YConfirm_WaitInput;
			 ClearRecvBuffer();//清除接收缓冲
			 }
    else if(ParamExist)//恢复工厂设置
		   {
		   CfgmgmtState=CfgMgmtRequstRestoreFactory;
			 ReadFileDisplay("出厂默认值");
			 }
		else if(IsParameterExist("23",4,&ParamExist)!=NULL)//处理保存函数
		   {
			 NamePtr=IsParameterExist("23",4,&ParamExist);//抓取参数
		   switch(getCfgTypeFromUserInput(NamePtr))
			   {
				 case UserInput_NoCfg://参数不合法
           DisplayIllegalParam(NamePtr,4,2);//显示用户输入了非法参数
				   DisplayParam();
				   ClearRecvBuffer();//清除接收缓冲
			     CmdHandle=Command_None;//命令执行完毕
				   break;
				 case UserInput_MainCfg://目标是主操作块
					 IsUsingBackupFiles=false;
				   checkresult=CheckConfigurationInROM(Config_Main,&MainCRC);
           if(DisplayConfError(checkresult,true))checkresult=0;//配置文件损坏
				   else if(checkresult==3)//配置文件损坏，直接覆盖
					  {
						UartPrintf((char *)ConfigFileCorrDetected,"主用");
						CfgmgmtState=CfgMgmtWriting;
						}
				   else //其余情况
					  {
					  if(MainCRC==ActiveConfigurationCRC())//包含相同配置
						  {
              SaveFileDisplay("主用");
						  CfgmgmtState=CfgMgmtRequestWrite;//请求写入
							}
						else //直接启动写入
							CfgmgmtState=CfgMgmtWriting;
						}
					 break;
				 case UserInput_BackupCfg:
					 IsUsingBackupFiles=true;
				   checkresult=CheckConfigurationInROM(Config_Backup,&MainCRC);
           if(DisplayConfError(checkresult,true))checkresult=0;//配置文件损坏
				   else if(checkresult==3)//配置文件损坏，直接覆盖
					  {
						UartPrintf((char *)ConfigFileCorrDetected,"备用");
						CfgmgmtState=CfgMgmtWriting;
						}
				   else //其余情况
					  {
					  if(MainCRC==ActiveConfigurationCRC())//包含相同配置
						  {
              SaveFileDisplay("备用");
						  CfgmgmtState=CfgMgmtRequestWrite;//请求写入
							}
						else //直接启动写入
							CfgmgmtState=CfgMgmtWriting;
						}
				   break;
				   }
			 }
		else if(IsParameterExist("45",4,&ParamExist)!=NULL)//验证函数
		   {
			 NamePtr=IsParameterExist("45",4,&ParamExist);//抓取参数
		   switch(getCfgTypeFromUserInput(NamePtr))
			   {
				 case UserInput_NoCfg://参数不合法
				   DisplayIllegalParam(NamePtr,4,4);//显示用户输入了非法参数
				   DisplayParam();
				   break;
			   case UserInput_MainCfg://目标是主操作块
				   checkresult=CheckConfigurationInROM(Config_Main,&MainCRC);
           if(DisplayConfError(checkresult,true))//配置文件损坏
					   {
					   ClearRecvBuffer();//清除接收缓冲
			       CmdHandle=Command_None;//命令执行完毕
						 }
				   else DisplayFileState("主用配置",checkresult,MainCRC);//输出结果
					 break;
				 case UserInput_BackupCfg://目标是备用操作块
				   checkresult=CheckConfigurationInROM(Config_Backup,&MainCRC);
           if(DisplayConfError(checkresult,true))//配置文件损坏
					   {
					   ClearRecvBuffer();//清除接收缓冲
			       CmdHandle=Command_None;//命令执行完毕
						 }
				   else DisplayFileState("备用配置",checkresult,MainCRC);//输出结果
					 break;
				 }
			  ClearRecvBuffer();//清除接收缓冲
			  CmdHandle=Command_None;//命令执行完毕
			  CfgmgmtState=CfgMgmtNoneOp;
			  }
		else if(IsParameterExist("67",4,&ParamExist)!=NULL)//处理读取函数
		   {
			 NamePtr=IsParameterExist("67",4,&ParamExist);//抓取参数
       switch(getCfgTypeFromUserInput(NamePtr))
			   {
				 case UserInput_NoCfg://参数不合法
            DisplayIllegalParam(NamePtr,4,6);//显示用户输入了非法参数
				    DisplayParam();
				    ClearRecvBuffer();//清除接收缓冲
			      CmdHandle=Command_None;//命令执行完成
				    break;
			 case UserInput_MainCfg://目标是主操作块
				    IsUsingBackupFiles=false;
				    checkresult=CheckConfigurationInROM(Config_Main,&MainCRC);
            if(DisplayConfError(checkresult,false))checkresult=0;//出错了
				    else //可以正常读取
					    {
				      if(MainCRC==ActiveConfigurationCRC())
						     {
						     CfgmgmtState=CfgMgmtNoneOp;
						     UartPrintf((char *)ConfigIsSame,"主用");
							   ClearRecvBuffer();//清除接收缓冲
			           CmdHandle=Command_None;//命令执行完毕
							   }
					  	else  //需要加载
						     {
                 ReadFileDisplay("主用");
						     CfgmgmtState=CfgMgmtRequestRead;//请求读取
							   }
						  }
						break;
       case UserInput_BackupCfg://目标是备用操作块
					  IsUsingBackupFiles=true;
				    checkresult=CheckConfigurationInROM(Config_Backup,&MainCRC);
            if(DisplayConfError(checkresult,false))checkresult=0;//出错了
				    else //其余情况
					    {
					    if(MainCRC==ActiveConfigurationCRC())
						     {
						     CfgmgmtState=CfgMgmtNoneOp;
						     UartPrintf((char *)ConfigIsSame,"备用");
							   ClearRecvBuffer();//清除接收缓冲
			           CmdHandle=Command_None;//命令执行完毕
							   }
						  else
						     {
							   ReadFileDisplay("备用");
						     CfgmgmtState=CfgMgmtRequestRead;//请求读取
							   }
						  }
						break;
			   }
			 }
		else if(IsParameterExist("89",4,&ParamExist)!=NULL)//Xmodem备份函数
		   {
			 NamePtr=IsParameterExist("89",4,&ParamExist);//抓取参数
       switch(getCfgTypeFromUserInput(NamePtr))
			   {
				 case UserInput_NoCfg://参数不合法
				    DisplayIllegalParam(NamePtr,4,4);//显示用户输入了非法参数
				    DisplayParam();
				    ClearRecvBuffer();//清除接收缓冲
			      CmdHandle=Command_None;//命令执行完毕
				    break;
			   case UserInput_MainCfg://目标是主操作块
				    checkresult=CheckConfigurationInROM(Config_Main,&MainCRC);
            if(DisplayConfError(checkresult,false))//配置文件损坏
					    {
					    ClearRecvBuffer();//清除接收缓冲
			        CmdHandle=Command_None;//命令执行完毕
						  }
				    else CfgmgmtState=CfgMgmtRequestBackup;//开始备份
						break;
         case UserInput_BackupCfg://目标是备用操作块
				    checkresult=CheckConfigurationInROM(Config_Backup,&MainCRC);
            if(DisplayConfError(checkresult,false))//配置文件损坏
					    {
					    ClearRecvBuffer();//清除接收缓冲
			        CmdHandle=Command_None;//命令执行完毕
						  }
				    else 
					    {
					    IsUsingBackupFiles=true;
						  CfgmgmtState=CfgMgmtRequestBackup;//开始备份
						  }
				  }
			 }
		else //啥也没有找到
			{
			UartPrintCommandNoParam(4);//显示啥也没找到的信息
			CfgmgmtState=CfgMgmtNoneOp;
			ClearRecvBuffer();//清除接收缓冲
			CmdHandle=Command_None;//命令执行完毕
			}
		break;
		}
	 case CfgMgmtConfirmOverridePassword:
	  {
		if(YConfirmState!=YConfirm_Error&&YConfirmState!=YConfirm_OK)break; //等待用户确认
	  UartPrintf("\r\n系统的凭据将被%s.\r\n",YConfirmState==YConfirm_OK?"重置":"保留");
	  LoadDefaultConf(YConfirmState==YConfirm_OK?true:false);//加载默认配置
		UARTPuts("\r\n当前配置已恢复为出厂默认值.");
		CfgmgmtState=CfgMgmtNoneOp;
		ClearRecvBuffer();//清除接收缓冲
		CmdHandle=Command_None;//命令执行完毕
		break;
		}
	 case CfgMgmtRequstRestoreFactory://处理恢复工厂设置
	  {
		//验证成功
		if(YConfirmState==YConfirm_OK)
		  {
			CfgmgmtState=CfgMgmtConfirmOverridePassword; //等待用户告知是否需要覆盖密码			
			UARTPuts("\r\n\r\n请确认是否重置系统的凭据.如输入'yes'则重置凭据.");
			UARTPuts("\r\n? ");	
			ClearRecvBuffer();//清除接收缓冲
      YConfirmstr="yes"; 
      YConfirmState=YConfirm_WaitInput;	
			}
		else if(YConfirmState==YConfirm_Error)
      OperationStopByUserInfo();//用户停止操作
		break;
		}
	 case CfgMgmtWaitXmodemConfirm://处理从Xmodem读取配置并恢复前的确认工序
	  {
		//验证成功
		if(YConfirmState==YConfirm_OK)
			CfgmgmtState=CfgMgmtRequestRestore; //跳转到恢复阶段
		else if(YConfirmState==YConfirm_Error)
      OperationStopByUserInfo();//用户停止操作
		break;
		}
	 case CfgMgmtRequestWrite://写入请求前面的确认工序
	  {
		//验证成功
		if(YConfirmState==YConfirm_OK)CfgmgmtState=CfgMgmtWriting;
    else if(YConfirmState==YConfirm_Error)OperationStopByUserInfo();//用户停止操作
		break;
		}
	 case CfgMgmtWriting://启动写入操作
	  {
		UartPrintf("\r\n开始将当前配置写入到%s配置文件内,请等待...  ",IsUsingBackupFiles==true?"备用":"主用");
		namelen=WriteConfigurationToROM(IsUsingBackupFiles==true?Config_Backup:Config_Main);
		if(!namelen)
			 UartPrintf("写入完毕\r\n文件的CRC-32值为0x%08X.",ActiveConfigurationCRC());
		else 
			 DisplayConfError(namelen,true);
		CfgmgmtState=CfgMgmtNoneOp;
		ClearRecvBuffer();//清除接收缓冲
	  CmdHandle=Command_None;//命令执行完毕
		break;
		}
 	 case CfgMgmtRequestRead://读取请求前面的确认工序 
	  {
		//验证成功
		if(YConfirmState==YConfirm_OK)CfgmgmtState=CfgMgmtReading;
		else if(YConfirmState==YConfirm_Error)OperationStopByUserInfo();//用户停止操作
		break;
		}
	 case CfgMgmtReading://启动读取操作
	  {
		UartPrintf("\r\n开始从%s配置文件内读入配置并应用,请等待...  ",IsUsingBackupFiles==true?"备用":"主用");
		namelen=ReadConfigurationFromROM(IsUsingBackupFiles==true?Config_Backup:Config_Main);
		if(!namelen)
			 UartPrintf("读入完毕，配置已生效\r\n文件的CRC-32值为0x%08X.",ActiveConfigurationCRC());
    else 
			 DisplayConfError(namelen,false);//出错了
		CfgmgmtState=CfgMgmtNoneOp;
		ClearRecvBuffer();//清除接收缓冲
	  CmdHandle=Command_None;//命令执行完毕
		break;
		}
	 case CfgMgmtRequestBackup: //请求Xmodem备份操作
	  {
		CfgmgmtState=CfgMgmtBackupInProgress;
		DisplayXmodemBackUp("配置文件",false);
		XmodemInitTxModule(CfgFileSize,IsUsingBackupFiles?CfgFileSize:0);
		break;
		}
	 case CfgMgmtRequestRestore: //请求Xmodem恢复操作
	  {		
    CfgmgmtState=CfgMgmtRestoreInProgress;
		DisplayXmodemBackUp("配置文件",true);
    XmodemInitRXModule(CfgFileSize,XmodemConfRecvBase);
		break;
		}
	 case CfgMgmtBackupInProgress:XmodemTxDisplayHandler("配置文件",&cfgmgmtCmdXmodemErrorHandler,&cfgmgmtCmdXmodemErrorHandler);break;
	 case CfgMgmtRestoreInProgress:XmodemRxDisplayHandler("配置文件",&cfgmgmtCmdXmodemErrorHandler,&CheckAndRestoreConfigFromXmodem);break;//备份和恢复操作进行中，执行对应的处理
	 }
	}
