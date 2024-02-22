#include "selftestlogger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "Xmodem.h" //这里主要是为了利用校验和函数

static bool IsSelftestLoggerReady=false;
static char SelfTestLogEntry=0;

//根据日志消息类型打印对应的信息
static const char *GetHeaderFromLevel(Postmessagelevel Level)
  {
  switch(Level) //根据消息类型选择对应的输出
	  {
		case Msg_critical:return "\033[40;31m[FATAL]";
		case Msg_warning:return "\033[40;33m[WARN]";
		case msg_error:return "\033[40;35m[ERROR]";
		default:return "\033[40;32m[INFO]";
    }
	}
//根据上次自检成功与否检查记录器是否启动（避免覆盖掉上次的错误日志）
void CheckLastStartUpIsOK(void)
  {
	#ifndef FlashLightOS_Debug_Mode
  char Result;
	//读取数据
	if(M24C512_PageRead(&Result,SelftestLogControlByte,1))
	  {
		IsSelftestLoggerReady=false;
		return; //EEPROM读取失败，关闭记录器
		}
	//读取出来的数据如果是0x0E则说明上次自检出现异常，关闭记录器
	if(Result==0x0E)IsSelftestLoggerReady=false;
	else IsSelftestLoggerReady=true; //默认打开记录器
  #else
	IsSelftestLoggerReady=true; //debug模式下始终打开记录器	
  #endif
	}
//当自检通过后重置记录器的使能位
void ResetLogEnableAfterPOST(void)
  {
	char Result;
	//读取数据
	if(M24C512_PageRead(&Result,SelftestLogControlByte,1))return; //EEPROM读取失败
	//判断数据是否为0x1A (是1A就跳过否则写ROM)
	if(Result==0x1A)return;
	Result=0x1A;
	M24C512_PageWrite(&Result,SelftestLogControlByte,1);	//写入到EEPROM
	}

//显示本次启动的最后一条traceback信息
bool DisplayLastTraceBackMessage(void)
  {
	SelfTestLogUnion LogEntry;
	char ModCheckSum,Msgchecksum;
  char SBUF[32]={0};
	const char *Msg=NULL;
	int MessageCount;
	//读取数据
	MessageCount=SelfTestLogEntry>0?SelfTestLogEntry-1:0;//计算出最后一条消息的entry
	if(M24C512_PageRead(LogEntry.ByteBuf,SelfTestLogBase+(MessageCount*sizeof(SelfTestLogUnion)),sizeof(SelfTestLogUnion)))
		return false;//从ROM中取数据时失败了
	//检查消息内容是否合法
	ModCheckSum=(char)Checksum8(LogEntry.Log.ModuleName,strlen(LogEntry.Log.ModuleName));
	Msgchecksum=(char)Checksum8(LogEntry.Log.Message,strlen(LogEntry.Log.Message)); //计算校验和
	if(LogEntry.Log.LogContentCheckByte!=0x35)return false;  //检查byte错误
	if(ModCheckSum!=LogEntry.Log.ModuleNameChecksum)return false;
	if(Msgchecksum!=LogEntry.Log.MessageCheckSum)return false; //消息校验和错误
	//打印内容
	strncat(SBUF,"\r\n",32);
	Msg=GetHeaderFromLevel(LogEntry.Log.Level);
	strncat(SBUF,Msg,32-strlen(SBUF));
	strncat(SBUF,"\033[0m",32-strlen(SBUF));
	strncat(SBUF,LogEntry.Log.ModuleName,32-strlen(SBUF));
  strncat(SBUF,":",32-strlen(SBUF));
	UARTPuts(SBUF); 
	UARTPuts(LogEntry.Log.Message);//把头部信息和消息内容打印出来
	return true;
	}

//从ROM中仅读取5个字节的头部,提高速度
bool ReadSelfTestLogHeader(char EntryNum,SelfTestLogUnion *Dout)
  {
	if(M24C512_PageRead(Dout->ByteBuf,SelfTestLogBase+(EntryNum*sizeof(SelfTestLogUnion)),5))
	  return false; //读取失败
	return true;
	}

//向ROM中写入数据的函数
bool WriteSelfTestLog(char EntryNum,SelfTestLogUnion *Din)
  {
	if(M24C512_PageWrite(Din->ByteBuf,SelfTestLogBase+(EntryNum*sizeof(SelfTestLogUnion)),sizeof(SelfTestLogUnion)))
	  return false; //读取失败
	return true;
	}	

//在POST的同时将信息写入到ROM中
void UartPost(Postmessagelevel msglvl,const char *Modules,char *Format,...)
  {
	va_list arg;
  char SBUF[32]={0};
  SelfTestLogUnion LogEntry;
	const char *Msg=NULL;
	char ModCheckSum,Msgchecksum,chkbyte;
	bool IsLogNeedToWrite;
	//打印前面的提示信息
	strncat(SBUF,"\r\n",32);
	Msg=GetHeaderFromLevel(msglvl);
	strncat(SBUF,Msg,32-strlen(SBUF));
	strncat(SBUF,"\033[0m",32-strlen(SBUF));
	strncat(SBUF,Modules,32-strlen(SBUF));
  strncat(SBUF,":",32-strlen(SBUF));
	UARTPuts(SBUF); //打印出来
	//打印消息之前会先读取一次旧的log上来存储数据
  if(IsSelftestLoggerReady)
	  {
		//尝试读取
		if(!ReadSelfTestLogHeader(SelfTestLogEntry,&LogEntry))
		  IsSelftestLoggerReady=false;
		else
	   	{
			chkbyte=LogEntry.Log.LogContentCheckByte;
			ModCheckSum=LogEntry.Log.ModuleNameChecksum;
			Msgchecksum=LogEntry.Log.MessageCheckSum;
			}
		}
	//打印后部信息
	memset(LogEntry.Log.Message,0,236);//清空内存
	__va_start(arg,Format);
  vsnprintf(LogEntry.Log.Message,236,Format,arg);
  __va_end(arg);
  UARTPuts(LogEntry.Log.Message);
	//记录模块名和写入信息
	if(!IsSelftestLoggerReady)return;
  LogEntry.Log.Level=msglvl;
  strncpy(LogEntry.Log.ModuleName,Modules,16);
	LogEntry.Log.LogContentCheckByte=0x35;
	LogEntry.Log.MessageCheckSum=(char)Checksum8(LogEntry.Log.Message,strlen(LogEntry.Log.Message));
	LogEntry.Log.ModuleNameChecksum=(char)Checksum8(LogEntry.Log.ModuleName,strlen(LogEntry.Log.ModuleName));
	//出现致命错误，写入到log记录器通知log记录未完成，关闭下次自检的输出
  SBUF[0]=0x0E;
	if(msglvl==Msg_critical)M24C512_PageWrite(&SBUF[0],SelftestLogControlByte,1);
	//判断log是否需要写入
	if(chkbyte!=0x35)IsLogNeedToWrite=true;//需要写入
	else if(ModCheckSum!=LogEntry.Log.ModuleNameChecksum)IsLogNeedToWrite=true;
	else if(LogEntry.Log.MessageCheckSum!=Msgchecksum)IsLogNeedToWrite=true; //校验和不同说明日志内容发生变更需要写入
	else IsLogNeedToWrite=false; //不需要写入
	if(!IsLogNeedToWrite)return;
	if(!WriteSelfTestLog(SelfTestLogEntry,&LogEntry))
		 IsSelftestLoggerReady=false; //写入失败关闭写入功能
	else
		 SelfTestLogEntry=(SelfTestLogEntry+1)%SelftestLogDepth;//指针+1
	}
