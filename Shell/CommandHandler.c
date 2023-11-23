#include <string.h>
#include "command.h"
#include "cfgfile.h"
#include <stdarg.h>
#include "AES256.h"

//global vars
permVerifyStatus Verifystat=ACC_No_Login;//权限验证状态
VerifyAccountLevel TargetAccount=VerifyAccount_None;//确认状态
PressYconfirmStatu YConfirmState=YConfirm_Idle;//确认状态
const char *YConfirmstr=NULL;//按Y确认输入的字符串

Conso1eLoginState AccountState=Log_Perm_Guest;//登录状态
CommandHandle CmdHandle=Command_None;//命令句柄

static CommandHandle LastCmdHandle=Command_None;//上一个命令句柄
char *CmdParamBuf[20]={NULL};//命令各参数的指针
int ArugmentCount=0;//命令的数目

//外部变量
extern bool IsForceStopByTimeOut;

/*
 下面的函数分别是处理命令行各个事件的模块。
 分别处理普通输入回显和删除事件，tab和回车开始执行事件。
 同时包括了处理用户登录，更改密码等等的事件。
 */
//处理用户的密码输入事件
void PasswordVerifyHandler(void)
  {
	int wordlen;
	char PasswordBuf[16];	
	//请求验证，提示用户名
	if(Verifystat==ACC_RequestVerify)
	  {
		ClearRecvBuffer();//清除接收缓冲
		Verifystat=ACC_Enter_UserName;
		}
	//用户名输入完毕，进行比对
	else if(Verifystat==ACC_Enter_UserName)
	  {
		wordlen=strlen(RXBuffer);
		if(wordlen==strlen("root")&&!strncmp(RXBuffer,"root",20))
			TargetAccount=VerifyAccount_Root;
		else if(wordlen==strlen(CfgFile.AdminAccountname)&&!strncmp(RXBuffer,CfgFile.AdminAccountname,20))
			TargetAccount=VerifyAccount_Admin;
		else TargetAccount=VerifyAccount_None;
		UARTPuts("\r\n请输入密码 : ");
		CurCmdField=PasswordField;//密码消隐
		ClearRecvBuffer();//清除接收缓冲
		Verifystat=ACC_Enter_Pswd;
		}
	//输入完毕，进行验证
	else if(Verifystat==ACC_Enter_Pswd)
	 {
	 wordlen=strlen(RXBuffer);//计算长度
	 if(TargetAccount==VerifyAccount_Admin)//验证管理员密码
	   {	
		 //进行解密
		 IsUsingOtherKeySet=false;//处理密文的时候使用第一组key
     memcpy(PasswordBuf,CfgFile.AdminAccountPassword,16);
     AES_EncryptDecryptData(PasswordBuf,0); //将密文复制过来，然后解密	
     //验证密码			 
		 if(wordlen==strlen(PasswordBuf)&&!strncmp(RXBuffer,PasswordBuf,16))
			 Verifystat=ACC_Verify_OK;  
		 else Verifystat=ACC_Verify_Error;//管理员密码错误
		 //验证完毕销毁密文
		 memset(PasswordBuf,0,16);
		 IsUsingOtherKeySet=true;//重新使用第二组key
		 }
	 else if(TargetAccount==VerifyAccount_Root)//验证root用户密码
	   {
		 //进行解密
		 IsUsingOtherKeySet=false;//处理密文的时候使用第一组key
     memcpy(PasswordBuf,CfgFile.RootAccountPassword,16);
     AES_EncryptDecryptData(PasswordBuf,0); //将密文复制过来，然后解密	
		 //验证密码		
		 if(wordlen==strlen(PasswordBuf)&&!strncmp(RXBuffer,PasswordBuf,16))
			 Verifystat=ACC_Verify_OK;  
		 else Verifystat=ACC_Verify_Error;//Root密码错误
		 //验证完毕销毁密文
		 memset(PasswordBuf,0,16);
		 IsUsingOtherKeySet=true;//重新使用第二组key
		 }
	 else Verifystat=ACC_Verify_Error;//其余情况，错误
	 ClearRecvBuffer();//清除接收缓冲	 
	 CurCmdField=TextField;//重新显示
	 }
	//改密码
	else if(Verifystat==ACC_ChangePassword)
	 {
	 wordlen=strlen(RXBuffer);//计算长度
	 if(wordlen<6||wordlen>15)Verifystat=ACC_ChgPswdErr_LenErr;//长度错误
   else if(AccountState==Log_Perm_Admin&&TargetAccount==VerifyAccount_Root)
		 Verifystat=ACC_ChgPswdErr_NoPerm;//权限不足，只有root用户自身可以改自己的密码
   else if(AccountState==Log_Perm_Admin&&TargetAccount==VerifyAccount_Admin)
	   {//ADMIN用户改自己密码
		 IsUsingOtherKeySet=false;//处理密文的时候使用第一组key
		 memset(PasswordBuf,0,16);
		 strncpy(PasswordBuf,RXBuffer,16);
     AES_EncryptDecryptData(PasswordBuf,1); //将明文复制过来，然后加密	
		 memcpy(CfgFile.AdminAccountPassword,PasswordBuf,16);//复制密码字段过去
		 //验证完毕销毁密文
		 memset(PasswordBuf,0,16);
		 IsUsingOtherKeySet=true;//重新使用第二组key
		 Verifystat=ACC_ChgPswdOK;
		 }		 
   else if(AccountState==Log_Perm_Root&&TargetAccount==VerifyAccount_Root)
	   {//Root用户改自己密码
		 IsUsingOtherKeySet=false;//处理密文的时候使用第一组key
		 memset(PasswordBuf,0,16);
		 strncpy(PasswordBuf,RXBuffer,16);
     AES_EncryptDecryptData(PasswordBuf,1); //将明文复制过来，然后加密	
		 strncpy(CfgFile.RootAccountPassword,PasswordBuf,16);//复制密码字段过去
     //验证完毕销毁密文
		 memset(PasswordBuf,0,16);
		 IsUsingOtherKeySet=true;//重新使用第二组key
		 Verifystat=ACC_ChgPswdOK;
		 }		 
	 else if(AccountState==Log_Perm_Root&&TargetAccount==VerifyAccount_Admin)
		 {//root用户改Admin用户的密码
		 IsUsingOtherKeySet=false;//处理密文的时候使用第一组key
		 memset(PasswordBuf,0,16);
		 strncpy(PasswordBuf,RXBuffer,16);
     AES_EncryptDecryptData(PasswordBuf,1); //将明文复制过来，然后加密	
		 memcpy(CfgFile.AdminAccountPassword,PasswordBuf,16);//复制密码字段过去
		 //验证完毕销毁密文
		 memset(PasswordBuf,0,16);
		 IsUsingOtherKeySet=true;//重新使用第二组key
		 Verifystat=ACC_ChgPswdOK;
		 }		
	 CurCmdField=TextField;//重新显示
	 }
	}
//处理需要用户确认的命令事件
void YconfirmHandler(void)
  {
  int wordlen;
	if(YConfirmState ==YConfirm_Idle)return;//不需要任何确认
	if(YConfirmState == YConfirm_WaitInput)
	  {
	  //检测目标要比较的字符串内容是不是合法的
		if(YConfirmstr==NULL||!strlen(YConfirmstr))
		  {
			YConfirmState=YConfirm_Error;
		  return;
			}
		//开始比较
		wordlen=strlen(RXBuffer);
		if(wordlen==strlen(YConfirmstr)
			&&!strncmp(RXBuffer,YConfirmstr,128))//确认
		  YConfirmState=YConfirm_OK;	
    else YConfirmState=YConfirm_Error;
		}
	}
//处理用户输入的回显和删除内容的事件
void LoopbackAndDelHandler(void)
 {
 int i,j;
 int delcount,bscount,maxdelcount;
 short orgLastptr;
 char delbuf;
 bool IsActualDeleted;
 //Loopback
 if(ConsoleStat==BUF_LoopBackReq)
   {
	 //文字类型数据，回显
	 if(CurCmdField==TextField)UARTPutd((char *)&RXBuffer[RXLastLinBufPtr],RXLinBufPtr-RXLastLinBufPtr); 
	 //密码类型数据，显示'*'
	 else if(CurCmdField==PasswordField)UARTPutc('*',RXLinBufPtr-RXLastLinBufPtr);
	 ConsoleStat=BUF_Idle;//标记串口可以继续接收
	 }
 //删除数据(从输入序列的头部开始执行)
 if(ConsoleStat==BUF_Del_Req)
   {
	 //从头部开始遍历，如果检测到需要删除的内容，则搬数据并且将指针回退
	 orgLastptr=RXLinBufPtr;//存储原始的PTR
	 i=RXLastLinBufPtr;
	 while(i<RXLinBufPtr&&RXLinBufPtr>0)
		{	
    //删除
	  if(RXBuffer[i]==0x7F||RXBuffer[i]==0x08)
		 {
		 //确认是不是Unicode字符
		 if(RXBuffer[i>0?i-1:0]&0x80)
		   {
			 maxdelcount=0;
			 j=i>0?i-1:0; //复位编码长度统计然后指定指针部分
			 if(j>0)while(j>=0&&(RXBuffer[j]&0xC0)==0x80)//绕过后面10xx_xxxx的编码部分的同时计算总的编码长度
			     {
			     j--;
					 maxdelcount++;
					 }		
			 maxdelcount+=1;//实际编码部分除了被刚刚绕过的数据区还有头部因此要+1					 
			 //识别编码类型采取删除策略
			 if((RXBuffer[j]&0xE0)==0xC0)delcount=2;//双字节编码(110x_xxxx)
			 else if((RXBuffer[j]&0xF0)==0xE0)delcount=3;//三字节编码(1110_xxxx)
			 else if((RXBuffer[j]&0xF1)==0xF0)delcount=4;//四字节编码(1111_0xxx)
			 else delcount=0;//啥也不做
			 /*如果根据文件头读出的编码长度大于实际值(例如爆缓存编码数据不完整)
			 则确保只删除该字符的内容而不会删过头了*/
			 if(delcount>maxdelcount)delcount=maxdelcount;
			 }
     else	delcount=1;//执行一次删除一个ASCII字符			 
		 
     //设置退格次数
		 IsActualDeleted=delcount>0?true:false; //确认是不是真的有执行有效的删除操作
		 delbuf=RXBuffer[i];//存下主机送过来的删除缓存
		 bscount=delcount>1?2:1;//设置退格次数			 
		 while(RXLinBufPtr>0&&delcount) //循环执行指定次数的删除
		   {
		   for(j=i>0?i-1:0;j<RXLinBufPtr;j++)RXBuffer[j]=RXBuffer[j+1];//将后面的数据往前搬		 
		   if(RXLinBufPtr>0)RXLinBufPtr--;//删除1个字符
		   if(i>0)
			     {
				   if(bscount>0) //输出指定次数的空格
					   {
						 UARTPutc(delbuf,1);//将主机发的删除字符打印过去(这里是因为不同的终端软件删除字符不一样,有些终端不响应0x7F)
						 bscount--;
						 }
					 i--;//指针前移一个格子然后发删除删数据
					 }
		   RXBuffer[i]=0;//清空掉数据
		   for(j=i;j<RXLinBufPtr;j++)RXBuffer[j]=RXBuffer[j+1];
		   delcount--;
			 }
	   //执行指针的调整操作
		 if(RXLinBufPtr>0)RXLinBufPtr=IsActualDeleted?RXLinBufPtr-1:RXLinBufPtr;//在确实有执行删除操作的情况下，减掉del字符占的空间
		 else break;//已经没得删除了
		 }
		else //当前区域数据不是删除，打印并且继续搜索下一个数据
		   {
		   if(RXBuffer[i]>0x1F&&RXBuffer[i]<0X7F)UARTPutc(RXBuffer[i],1);//不是空字符就将字符打印过去
		   i++;
		   }	
		}
	 //将删除区域的后部填上0x00
	 for(i=RXLinBufPtr;i<orgLastptr;i++)RXBuffer[i]=0;
	 //重启DMA
	 ConsoleStat=BUF_Idle;//标记串口可以继续接收
	 }
 }

//处理用户按下方向键之后的事件（翻到上一条历史命令等）
void ArrowKeyHandler(void)
  {
	if(ConsoleStat==BUF_Up_Key)
	  {
    CommandHistoryUpward();//命令历史往上翻
		ConsoleStat=BUF_Idle;//标记串口可以继续接收
		}
	if(ConsoleStat==BUF_Down_Key)
	  {
    CommandHistoryDownward();//命令历史往下翻
		ConsoleStat=BUF_Idle;//标记串口可以继续接收
		}	
	if(ConsoleStat==BUF_Left_Key)
	  {
		//ActionsHere.
		ConsoleStat=BUF_Idle;//标记串口可以继续接收
		}		
	if(ConsoleStat==BUF_Right_Key)
	  {
		//ActionsHere.
		ConsoleStat=BUF_Idle;//标记串口可以继续接收
		}			
	}
//处理用户按下回车后的执行命令事件
void CmdExecuteHandler(void)
  {
	int i,argc,j;
	char *argv[20];
	int cmdlen,matchcmdindex;	
	//判断命令是否执行完毕
	if(LastCmdHandle!=CmdHandle)
	  {
		LastCmdHandle=CmdHandle;
		if(LastCmdHandle!=Command_None)return;//还有命令没有执行完毕
	  PrintShellIcon();//打印shell图标
    ClearRecvBuffer();//清除接收缓冲
		}
	if(ConsoleStat!=BUF_Exe_Req)return;
	//当前有命令要执行,开始对缓冲区进行处理
	if(CmdHandle==Command_None)
	  { 
		CommandHistoryWrite();//将目前需要执行的命令写入到缓冲区内
		UARTPuts("\r\n");//打印回车
		argc=Str2Argv(argv,RXBuffer,20);//进行命令反序列化
		}
	else argc=0;
	PasswordVerifyHandler();//密码验证处理
	YconfirmHandler();//按Y验证
	if(CmdHandle==Command_None&&argc>0)
	  {
		//先对命令进行查找找到目标的参数entry
		i=0;
		j=strlen(argv[0]);//get到目标的命令长度
    while(i<TotalCommandCount)
		  { 
			cmdlen=strlen(Commands[i].CommandName);//获取目标比较的命令长度
			if(j==cmdlen&&!strncmp(Commands[i].CommandName,argv[0],cmdlen))break;//找到完全匹配的命令
			i++;
			}
		matchcmdindex=i;//存储匹配的命令参数
		if(i<TotalCommandCount)//找到匹配的命令
			{
			//判断权限节点是否匹配
			j=0;
			i=0;
			while(j<4&&Commands[matchcmdindex].PermNode[j]!=Log_Perm_End)j++;//判断整个权限节点总长度
			while(i<j)//遍历权限节点内的权限检测是否有该命令的权限
			 {
			 if(Commands[matchcmdindex].PermNode[i]==AccountState)break;
			 i++;
			 }
			if(i<j)//权限检查通过，注册节点开始执行
			  {
				if(Commands[matchcmdindex].AllowInputDuringExecution==true)ConsoleStat=BUF_Idle;//标记串口可以继续接收
				CmdHandle=Commands[matchcmdindex].TargetHandle;
				LastCmdHandle=CmdHandle;//注册句柄
				for(i=0;i<20;i++)CmdParamBuf[i]=argv[i];//复制参数
				ArugmentCount=argc;//存储参数数量
				return;
				}
			//命令对于当前权限无效
			else 
			  {
				UARTPuts("\r\n\r\n\033[40;31m对不起,您没有权限使用");
				UARTPuts("\"");	
				UARTPuts(argv[0]);	
        UARTPuts("\"命令.\033[0m");						
				}
			}
    //命令无效
    else
		  {
			UARTPuts("\r\n\r\n\033[40;31m命令 ");
			UARTPuts("\"");	
			UARTPuts(argv[0]);	
			UARTPuts("\" 无法识别.您可尝试使用'\033[40;32mhelp\033[40;31m'命令显示帮助.\033[0m");					
			}			
		}
	//命令执行过程中，不允许声明新的命令
	else if(CmdHandle!=Command_None)return;
  ClearRecvBuffer();//清除接收缓冲
	PrintShellIcon();//打印shell图标
	}
//用户按下Ctrl+C强制中断命令执行的处理事件
void CtrlCHandler(void)
	{
	int i;
	if(ConsoleStat!=BUF_Force_Stop)return;	//不是强制打断	
	//复位一些系统命令的指针
	for(i=0;i<TotalCommandCount;i++)
		if(Commands[i].TargetHandle==CmdHandle)//找到当前正在跑的命令
		 {
		 if(Commands[i].CtrlCProc!=NULL)Commands[i].CtrlCProc();//执行强制打断处理的槽函数
		 break;
		 }
  //打断命令	
  ConsoleStat=BUF_Idle;//标记串口可以继续接收			
  ClearRecvBuffer();//清除接收缓冲	
	if(!IsForceStopByTimeOut) //由于超时而被打断
	  {
	  UARTPuts("\r\n^C");	 
	  if(CmdHandle==Command_None)PrintShellIcon();//没有命令在执行，重新打印shell图标
		}
	else IsForceStopByTimeOut=false;
	CmdHandle=Command_None;	//停止当前命令
	}
//DMA缓冲区溢出事件处理函数
void DMAFullHandler(void)
  {
	if(ConsoleStat!=BUF_DMA_Full)return;
	ClearRecvBuffer();//清除接收缓冲
	PrintShellIcon();
	ConsoleStat=BUF_Idle;//标记串口可以继续接收	
	}	
//Tab自动补全事件处理
void TabHandler(void)
  {
	char *argv[20];
	const char *paramptr[20];
	const char *ParamUsage[20];
	char *ParameterPtr;
	char *ParameterEndPtr;
	int argc,cmdlen,matchedcmdcount,i,j,matchcmdindex;
	int paramcount,ParamUsagecount;
	int CurrentMatchCMDlen,refmatchcmdcount;
	bool ResendRxbuf;
	//判断tab生效条件
	if(CmdHandle!=Command_None&&ConsoleStat==BUF_Tab_Req)ConsoleStat=BUF_Idle; //用户在命令执行期间按下tab，不生效
	if(ConsoleStat!=BUF_Tab_Req)return; //用户没有按下Tab
	//首先对命令参数进行解码
	ResendRxbuf=false;//默认不需要送出内容
	argc=Str2Argv(argv,RXBuffer,20);
	if(argc==1)//只有1个选项
	  {
		matchedcmdcount=0;
		cmdlen=strlen(argv[0]);//获取命令的最大长度开始查找
		for(i=0;i<TotalCommandCount;i++)
			if(IsCmdExecutable(i)&&!strncasecmp(Commands[i].CommandName,argv[0],cmdlen))matchedcmdcount++;//查找匹配的命令
		//如果匹配的命令数值等于1，代表只有一条命令匹配，此时删除旧的命令然后补全出新的命令
	  if(matchedcmdcount==1)
		 {
		 i=0;
     while(i<TotalCommandCount)
		  { 
			if(IsCmdExecutable(i)&&!strncasecmp(Commands[i].CommandName,argv[0],cmdlen))break;//找到匹配的命令
			i++;
			}
		 if(i==TotalCommandCount)//没找到匹配的命令
			{
			ConsoleStat=BUF_Idle;//标记串口可以继续接收
			return;
			}
		 matchcmdindex=i;//存储匹配的命令参数
		 ParameterPtr=&argv[argc-2][strlen(argv[argc-2])];
		 UARTPutc('\x08',RXLinBufPtr);//发送退格键
		 for(i=0;i<RXLinBufPtr;i++)RXBuffer[i]=0;//清除所有数据
		 RXLastLinBufPtr=0;//last设置为0
		 strncpy(RXBuffer,Commands[matchcmdindex].CommandName,CmdBufLen-RXLinBufPtr);//将匹配的命令复制进去
		 RXLinBufPtr=strlen(Commands[matchcmdindex].CommandName);//设置指针
		 UARTPuts((char *)Commands[matchcmdindex].CommandName);//将命令复制过去
		 ConsoleStat=BUF_Idle;//标记串口可以继续接收
		 return;//直接退出
		 }
		//匹配到的命令数值不等于1 打印每条命令
		else if(matchedcmdcount>1)
		 {
		 i=0;
		 j=0;
		 UARTPuts("\r\n\r\n");
     while(i<TotalCommandCount)
		  { 
			if(IsCmdExecutable(i)&&!strncasecmp(Commands[i].CommandName,argv[0],cmdlen))
			   {
				 UARTPuts((char *)Commands[i].CommandName);//找到匹配的命令，打印出来
				 UARTPuts("  ");//加上空格
				 j++;
				 }
			if(j==5)//每5个命令换行一次
			  {
				UARTPuts("\r\n");
				j=0;	
				}
			i++;
			}
     //将匹配的第一条命令复制进去开始查找  
		 i=0;
		 while(i<TotalCommandCount)
		  { 
			if(IsCmdExecutable(i)&&!strncasecmp(Commands[i].CommandName,argv[0],cmdlen))
			  {
		    memcpy(RXBuffer,Commands[i].CommandName,strlen(Commands[i].CommandName));//把内容复制进去
				break;//找到第一条匹配的指令，跳出
				}
			i++;
		  }
		 //进行查找匹配
		 CurrentMatchCMDlen=0;
		 while(CurrentMatchCMDlen<strlen(RXBuffer))
		  {
			//对每个命令进行遍历，检查匹配的值
		  i=0;
			refmatchcmdcount=0;
		  while(i<TotalCommandCount)
		    { 
			  if(IsCmdExecutable(i)&&!strncasecmp(Commands[i].CommandName,RXBuffer,CurrentMatchCMDlen+1))
				  refmatchcmdcount++;
			  i++;
			  }
			if(refmatchcmdcount<matchedcmdcount)break;//当前命令已经超过范围了
			CurrentMatchCMDlen++;
			}
		 memset(&RXBuffer[CurrentMatchCMDlen],0,CmdBufLen-CurrentMatchCMDlen);//清空后面的内容，仅保留最大成度匹配的字符串
		 ResendRxbuf=true;//指示后面的函数需要送出内容
		 RXLinBufPtr=strlen(RXBuffer);//设置指针值
		 RXLastLinBufPtr=0;//设置上一个指针值
		 }	
		//一个命令都没找到，没反应
		else
		 {
		 ConsoleStat=BUF_Idle;//标记串口可以继续接收
		 return;
		 }
		}
	//有多个选项，处理最后的一个
  else if(argc>1)
	  {
		//先对命令进行查找找到目标的参数entry
		i=0;
		j=strlen(argv[0]);//get到目标的命令长度
    while(i<TotalCommandCount)
		  { 
			cmdlen=strlen(Commands[i].CommandName);//获取目标比较的命令长度
			if(!IsCmdExecutable(i))cmdlen=0;//当前没有权限去执行
			else if(j==cmdlen&&!strncmp(Commands[i].CommandName,argv[0],cmdlen))break;//找到完全匹配的命令
			i++;
			}
		if(i==TotalCommandCount)//没找到匹配的命令
			{
			ConsoleStat=BUF_Idle;//标记串口可以继续接收
			return;
			}
		matchcmdindex=i;//存储匹配的命令参数
		if(matchcmdindex<TotalCommandCount)//有有效的命令
		  {
			if(Commands[matchcmdindex].IsModeCommand)	//是模式组相关命令
			 {
			 paramcount=ParamToConstPtr(paramptr,ModeRelCommandParam,4);
			 paramcount+=ParamToConstPtr(&paramptr[4],Commands[matchcmdindex].Parameter,16);//对指定的参数字符串进行解码
			 ParamUsagecount=ParamToConstPtr(ParamUsage,ModeRelCommandStr,4); 
			 ParamUsagecount+=ParamToConstPtr(&ParamUsage[4],Commands[matchcmdindex].ParamUsage,16);//取出后面的其余参数
			 }
		  else //不是模式字符串，正常取参数
			 {				
			 paramcount=ParamToConstPtr(paramptr,Commands[matchcmdindex].Parameter,20);
			 ParamUsagecount=ParamToConstPtr(ParamUsage,Commands[matchcmdindex].ParamUsage,20);
		   }
			if(paramcount==0||ParamUsagecount==0)//该命令没有可用参数
			 {
			 ConsoleStat=BUF_Idle;//标记串口可以继续接收
			 return;
			 }
			cmdlen=strlen(argv[argc-1]);
			matchedcmdcount=0;//清空以匹配的命令数量
			for(i=0;i<paramcount;i++)
				if(!strncasecmp(paramptr[i],argv[argc-1],cmdlen))matchedcmdcount++;//统计找到的参数个数
			//参数大于一个，打印所有的参数	
			if(matchedcmdcount>1)
			  {				
			  i=0;
		    j=0;
				UARTPuts("\r\n\r\n");
        while(i<paramcount)
			  	{ 
		  		if(!strncasecmp(paramptr[i],argv[argc-1],cmdlen))
		  			{
	  				UARTPuts((char *)paramptr[i]);
	  				if(i<ParamUsagecount)UARTPuts((char *)ParamUsage[i]);//找到匹配的参数，打印出来
						UARTPuts("  ");//加上空格
		  			j++;
						}
	  			if(j==2)//每3个参数换行一次
	  				{
  					UARTPuts("\r\n");
	  				j=0;	
	  				}
	  			i++;
	  			}
				PrintShellIcon();
	      ConsoleStat=BUF_Idle;//标记串口可以继续接收
				for(i=0;i<argc;i++)
					{
					if(i<argc-1)ParameterPtr=argv[i+1];				
				  ParameterPtr--;//指向指针前面的一个字符
					UARTPuts(argv[i]);
					if(i<argc-1)
					  {
						UARTPuts(" ");//把命令打印出来
						*ParameterPtr=' ';//获取最后一个参数的上一个地址，然后填上空格
					  }
					}
				return;
				}
			 //只有一个参数，直接替换
			 else if(matchedcmdcount==1)
			  {
				i=0;
				while(i<paramcount)
					{ 
					if(!strncasecmp(paramptr[i],argv[argc-1],cmdlen))break;//找到匹配的命令
					i++;
					}
				if(i==paramcount)
				  {
					ConsoleStat=BUF_Idle;//标记串口可以继续接收
					return;
					}
				matchcmdindex=i;//存储匹配的命令参数
				UARTPutc('\x08',strlen(argv[argc-1]));//发送退格键
				ParameterEndPtr=&argv[argc-1][strlen(argv[argc-1])+1];
				ParameterPtr=argv[argc-1];//get地址
				while(ParameterPtr!=ParameterEndPtr)//清除缓冲区内已有的参数
				  {
					RXLinBufPtr--;
					*ParameterPtr=0;
					ParameterPtr++;//指向下一个数据
					}
        for(i=0;i<argc;i++)//将序列化的命令重新进行处理
					{
					if(i<argc-1)ParameterPtr=argv[i+1];				
				  ParameterPtr--;//指向指针前面的一个字符
					if(i<argc-1)*ParameterPtr=' ';//获取最后一个参数的上一个地址，然后填上空格
					}
				ParameterPtr=argv[argc-1];
				ParameterPtr+=strlen(argv[argc-1]);
				strncpy(ParameterPtr," ",CmdBufLen-RXLinBufPtr);//加空格
				RXLinBufPtr++;
				strncpy(&RXBuffer[RXLinBufPtr],paramptr[matchcmdindex],CmdBufLen-RXLinBufPtr);//把命令参数复制进去
				RXLinBufPtr+=strlen(paramptr[matchcmdindex]);
				UARTPuts((char *)paramptr[matchcmdindex]);//输出命令参数
				strncpy(&RXBuffer[RXLinBufPtr],ParamUsage[matchcmdindex],CmdBufLen-RXLinBufPtr);//把命令用法复制进去
				RXLinBufPtr+=strlen(ParamUsage[matchcmdindex]);
				UARTPuts((char *)ParamUsage[matchcmdindex]);//输出命令参数		
        RXLastLinBufPtr=RXLinBufPtr;//设置上一个指针值
				ConsoleStat=BUF_Idle;//标记串口可以继续接收					
				return;//操作结束，退出
				}
			 //没找到任何匹配的命令
			 else 
				{
				for(i=0;i<argc;i++)//将序列化的命令重新进行处理
					{
					if(i<argc-1)ParameterPtr=argv[i+1];				
				  ParameterPtr--;//指向指针前面的一个字符
					if(i<argc-1)*ParameterPtr=' ';//获取最后一个参数的上一个地址，然后填上空格
					}
				ConsoleStat=BUF_Idle;//标记串口可以继续接收
				return;
				}
			 }
		//没有合法命令，退出
		else 
		   {
			 ConsoleStat=BUF_Idle;//标记串口可以继续接收
			 return;
			 }
		}		
	//处理完毕，打印shell提示符
	if(argc>0)PrintShellIcon();
	if(ResendRxbuf)UARTPuts(RXBuffer);//发出内容
	ConsoleStat=BUF_Idle;//标记串口可以继续接收
	}
