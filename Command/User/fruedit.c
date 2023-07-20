#include "console.h"
#include "CfgFile.h"
#include "FRU.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//常量字符串指针
static const char *frueditstr[]=
 {
 "\r\n错误:系统尝试读取FRU时出现错误.", //0
 "\r\n错误:尝试更新FRU中%s时出现错误.这可能是驱动硬件问题.",//1
 "\r\n错误:FRU存储区已被永久写保护.",//2
 "系统FRU中的",//3
 "\r\nFRU写保护",//4
 };


//参数帮助entry
const char *frueditArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
		case 0:
		case 1:return "设置驱动FRU中的序列号字符串.";
		case 2:
		case 3:return "设置驱动FRU中指定的最大输出电流.";	
		case 4:
		case 5:return "编辑完FRU后，将FRU永久性锁定";
		}
	return NULL;
	} 

//命令主处理函数
void fruedithandler(void)
  {
	bool IsCmdParamOK=false;
	char *ParamPtr;
	char ParamOK;	
	int len;
	float buf;
	FRUBlockUnion FRU;
	//设置序列号
	ParamPtr=IsParameterExist("01",27,NULL);
  if(ParamPtr!=NULL)
	  {
		IsCmdParamOK=true;
		len=strlen(ParamPtr);
		//读取失败
	  if(ReadFRU(&FRU)||CheckFRUInfoCRC(&FRU))
			UARTPuts((char *)frueditstr[0]);
	  //FRU被锁定
		else if(M24C512_QuerySecuSetLockStat()!=LockState_Unlocked)
			UARTPuts((char *)frueditstr[2]);
		//字符串过短或者过长
		else if(len<1||len>31)
			UARTPuts("\r\n错误:序列号字符串应至少包含一个字符且小于32字节(10个中文字符或31个英文字符).");
		else //正常写入
		  {
			strncpy(FRU.FRUBlock.Data.Data.SerialNumber,ParamPtr,32);	//复制序列号信息
			if(!WriteFRU(&FRU))
				UartPrintf("\r\n%s序列号已被更新为'%s'.",frueditstr[3],ParamPtr);
			else
				UartPrintf((char *)frueditstr[1],"序列号");
			}
		}
	//设置最大电流
	ParamPtr=IsParameterExist("23",27,NULL);
  if(ParamPtr!=NULL)
	  {
		IsCmdParamOK=true;
		buf=atof(ParamPtr); //字符串转浮点
		//读取失败
	  if(ReadFRU(&FRU)||CheckFRUInfoCRC(&FRU))
			UARTPuts((char *)frueditstr[0]);
	  //FRU被锁定
		else if(M24C512_QuerySecuSetLockStat()!=LockState_Unlocked)
			UARTPuts((char *)frueditstr[2]);
		//电流超过范围
		else if(buf==NAN||buf<10||buf>MaxAllowedLEDCurrent)
			UartPrintf("\r\n错误:%s最大LED电流应该在10-%d(A)范围内",frueditstr[3],MaxAllowedLEDCurrent);
		else //正常写入
		  {
      FRU.FRUBlock.Data.Data.MaxLEDCurrent=buf;
			if(!WriteFRU(&FRU))
				UartPrintf("\r\n%s最大电流限制已被更新为%.2fA.",frueditstr[3],buf);
			else
				UartPrintf((char *)frueditstr[1],"最大电流限制");
			}
		}
	IsParameterExist("45",27,&ParamOK);
  if(ParamOK)
	  {
		IsCmdParamOK=true;
		#ifndef EnableSecureStor
		UARTPuts("\r\n固件编译时禁用了安全存储区,无法永久锁定FRU!");		
		#else
    if(M24C512_QuerySecuSetLockStat()!=LockState_Unlocked)
			UARTPuts((char *)frueditstr[2]);
 		else //启用写保护
		  {
			if(!M24C512_LockSecuSct())
			  UartPrintf("%s已被永久激活.",frueditstr[4]);
			else
				UartPrintf("%s激活失败.",frueditstr[4]); 
			}
	  #endif	
		}		
	if(!IsCmdParamOK)UartPrintCommandNoParam(27);//显示啥也没找到的信息 
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕
	}
