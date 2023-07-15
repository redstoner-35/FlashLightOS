#include "console.h"
#include "CfgFile.h"
#include <stdio.h>
#include <string.h>
#include "AES256.h"  
	
//可重用字符串
static const char *TextLengthErrStr="\r\n错误:输入的文字'%s'长度过%s!输入文字的长度应在4到%d字符之间。";

//参数帮助entry
const char *cptxgenArgument(int ArgCount)
  {
	switch(ArgCount)
	 {
		case 0:
		case 1:return "为加密过程指定原文输入";
	  case 2:
	  case 3:return "添加此参数以启用版权文字输出模式";
	 }
	return "";
	}

void CopyRightTextGenerator(void)
  {	
  int i,j;
  int wordlen;
	char EncryptBUF[48];	
	char *TextContent;
	char IsReqtoAddCopyRight;	
	//检查是否传入了目标文本
  if(IsParameterExist("01",7,NULL)!=NULL)
	  {
		IsParameterExist("23",7,&IsReqtoAddCopyRight);//检查是否需要获取copyright
		TextContent=IsParameterExist("01",7,NULL);//获取参数
		if(IsReqtoAddCopyRight)
			wordlen=47-strlen("Copyright to ")-strlen(TextContent);
		else 
			wordlen=47-strlen(TextContent); //判断长度
		if(strlen(TextContent)<4)//输入长度太短
			 UartPrintf((char *)TextLengthErrStr,TextContent,"短",47-strlen("Copyright to ")); 
		else if(wordlen<0)//输入长度太长
			 UartPrintf((char *)TextLengthErrStr,TextContent,"长",47-strlen("Copyright to ")); 
		else //开始加密
		  { 			
			memset(EncryptBUF,0x00,48);//初始化内存
			if(IsReqtoAddCopyRight)//附加copyright
			  {
		    strncpy(EncryptBUF,"Copyright to ",47);				
				strncat(EncryptBUF,TextContent,47-strlen("Copyright to "));
				}
			else strncpy(EncryptBUF,TextContent,47);//直接处理文字内容
			//开始加密
			IsUsingFMCUID=false;//处理密文的时候关闭FMC随机加盐
	    AES_EncryptDecryptData(&EncryptBUF[0],1);	 
	    AES_EncryptDecryptData(&EncryptBUF[16],1);
	    AES_EncryptDecryptData(&EncryptBUF[32],1);//加密输入的内容
      //打印内容和后处理
			UartPrintf("\r\n[Text2Enc]生成模式 : 48字节%s模式",IsReqtoAddCopyRight?"版权信息":"非版权源文本");
		  UartPrintf("\r\n[Text2Enc]输入文本长度 : %d字节",strlen(TextContent));
			UARTPuts("\r\n[Text2Enc]如需更改版权信息，请将下方生成的加密版权信息内容复制到固件工程");
			UARTPuts("\r\n[Text2Enc]目录下'ver.c'文件内的一个名为EncryptedCopyRight的常量字符数组中");	
			UARTPuts("\r\n[Text2Enc]再重新编译固件即可。\r\n");
			if(IsReqtoAddCopyRight)//附加copyright
			   UartPrintf("\r\n/* AES256加密的版权信息 'Copyright to %s' */\r\n",TextContent);
		  else
				 UartPrintf("\r\n/* AES256加密的文本 '%s' */\r\n",TextContent);
			j=0;
		  for(i=0;i<48;i++)//输出内容
		    {
				UartPrintf(i<47?"0x%02X,":"0x%02X",EncryptBUF[i]);
				if(j==7)
				 {
				 UARTPuts("\r\n");
				 j=0; //每8个字符换行一次
				 }
				else j++;
				}
			memset(EncryptBUF,0x00,48);//销毁密文
      IsUsingFMCUID=true;//重新打开FMC随机加盐
			}
		}
	//非法参数
	else
    {
		IsParameterExist("23",7,&IsReqtoAddCopyRight);//检查是否需要获取copyright
		UARTPuts("\r\n错误:您应该使用'-t'或'--text'定义待处理的文本内容！");
		UartPrintf("\r\n与此同时，输入文字的长度应在4到%d字符之间。",47-(IsReqtoAddCopyRight?strlen("Copyright to "):0)); 
		}	
  //命令执行完成
  ClearRecvBuffer();//清除接收缓冲
	CmdHandle=Command_None;//命令执行完毕						
	}
