#include "console.h"
#include "CfgFile.h"
#include "FRU.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//函数声明
unsigned int atoh(char *Text,bool *IsError);

//常量字符串指针
static const char *TempOffsetstr="\r\n%s温度修正值 : %.2f'C";
static const char *frueditstr[]=
 {
 "\r\n错误:系统尝试读取FRU时出现错误.", //0
 #ifdef FlashLightOS_Debug_Mode
 "\r\n错误:尝试更新FRU中%s时出现异常.",//1
 "\r\n错误:FRU存储区已被永久写保护.",//2
 "系统FRU中的",//3
 "\r\nFRU写保护",//4
 #endif
 };


//参数帮助entry
const char *frueditArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
	  case 0:
		case 1:return "查看驱动FRU信息.";	
		#ifdef FlashLightOS_Debug_Mode
		case 2:return "设置驱动FRU中的序列号字符串.";
		case 3:return "设置驱动FRU中指定的最大输出电流.";		
		case 4:return "设置驱动FRU中指定的LED平台.";
		case 5:return "编辑完FRU后，将FRU永久性锁定";
		case 6:return "设置驱动负责采集基板温度的NTC B值.";
		case 7:return "设置基板温度的修正值.";
		case 8:return "设置驱动MOSFET温度的修正值.";
		case 9:return "设置驱动ADC的参考电压值.";
		case 10:return "当使用自定义LED时，设置驱动的LED识别代码";
		case 11:return "设置驱动的硬件大版本信息";
		case 12:return "设置驱动的硬件小版本信息";
	  case 13:return "当使用自定义LED时,设置LED的名称";
		#endif
		}
	return NULL;
	} 

//命令主处理函数
void fruedithandler(void)
  {
	bool IsCmdParamOK=false;
	FRUBlockUnion FRU;
	char ParamOK;
	#ifdef FlashLightOS_Debug_Mode
	char *ParamPtr;
	int len;
	float buf;	
	unsigned int hout;
	bool IsError;
	#endif
	//查看内容
	IsParameterExist("01",27,&ParamOK);
  if(ParamOK)
	  {
		IsCmdParamOK=true;
		//读取失败
	  if(ReadFRU(&FRU)||!CheckFRUInfoCRC(&FRU))
			UARTPuts((char *)frueditstr[0]);
		//正常显示
		else 
		  {
			UartPrintf("\r\n序列号 : %s",FRU.FRUBlock.Data.Data.SerialNumber);
			UARTPuts("\r\nLED类型 : ");
			if(FRU.FRUBlock.Data.Data.FRUVersion[0]==0x03||FRU.FRUBlock.Data.Data.FRUVersion[0]==0x06)
        UARTPuts("(自定义)");
			
			UARTPuts((char *)DisplayLEDType(&FRU)); //打印LED内容					
			UartPrintf((char *)TempOffsetstr,"LED基板",FRU.FRUBlock.Data.Data.NTCTrim);
			UartPrintf((char *)TempOffsetstr,"驱动MOS",FRU.FRUBlock.Data.Data.SPSTrim);
		  UartPrintf("\r\n最大LED电流 : %.2fA",FRU.FRUBlock.Data.Data.MaxLEDCurrent);
		  UartPrintf("\r\n硬件版本 : V%d.%d",FRU.FRUBlock.Data.Data.FRUVersion[1],FRU.FRUBlock.Data.Data.FRUVersion[2]);
			UartPrintf("\r\nNTC B值 : %d",FRU.FRUBlock.Data.Data.NTCBValue);
			#ifdef FlashLightOS_Debug_Mode					
			UartPrintf("\r\n自定义LED标识码 : 0x%04X",FRU.FRUBlock.Data.Data.CustomLEDIDCode);
			UartPrintf("\r\nADC参考电压 : %.4fV",FRU.FRUBlock.Data.Data.ADCVREF);
			#endif
			}
		}
	#ifdef FlashLightOS_Debug_Mode
	//设置序列号
	ParamPtr=IsParameterExist("2",27,NULL);
  if(ParamPtr!=NULL)
	  {
		IsCmdParamOK=true;
		len=strlen(ParamPtr);
		//读取失败
	  if(ReadFRU(&FRU)||!CheckFRUInfoCRC(&FRU))
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
	ParamPtr=IsParameterExist("3",27,NULL);
  if(ParamPtr!=NULL)
	  {
		IsCmdParamOK=true;
		buf=atof(ParamPtr); //字符串转浮点
		//读取失败
	  if(ReadFRU(&FRU)||!CheckFRUInfoCRC(&FRU))
			UARTPuts((char *)frueditstr[0]);
	  //FRU被锁定
		else if(M24C512_QuerySecuSetLockStat()!=LockState_Unlocked)
			UARTPuts((char *)frueditstr[2]);
		//用户输入的数值非法
	  else if(buf==NAN||buf<5||buf>QueryMaximumCurrentLimit(&FRU))
		  {
			DisplayIllegalParam(ParamPtr,27,3);//显示用户输入了非法参数
			UartPrintf("\r\n错误:您指定的LED电流限制应在5.0-%.1fA之间.",QueryMaximumCurrentLimit(&FRU));
			}
		else //写入数据
		  {
			FRU.FRUBlock.Data.Data.MaxLEDCurrent=buf; //更新电流值条目
			if(!WriteFRU(&FRU))
				UartPrintf("\r\n%s的最大电流限制值已被更新为%.1fA.",frueditstr[3],buf);
			else
				UartPrintf((char *)frueditstr[1],"最大电流限制");
			}
		}
	
	//设置LED类型
	ParamPtr=IsParameterExist("4",27,NULL);
  if(ParamPtr!=NULL)
	  {
		IsCmdParamOK=true;
	  ParamOK=GetLEDTypeFromUserInput(ParamPtr);
		//读取失败
	  if(ReadFRU(&FRU)||!CheckFRUInfoCRC(&FRU))
			UARTPuts((char *)frueditstr[0]);
	  //FRU被锁定
		else if(M24C512_QuerySecuSetLockStat()!=LockState_Unlocked)
			UARTPuts((char *)frueditstr[2]);
		//用户输入的FRU字符串参数非法
		else if(ParamOK==0)
		  {
			DisplayIllegalParam(ParamPtr,27,4);//显示用户输入了非法参数
			DisplayCorrectLEDType(); 
			}
		else //正常写入
		  {
      FRU.FRUBlock.Data.Data.FRUVersion[0]=ParamOK;
			if(ParamOK==0x03||ParamOK==0x03)
			  {
				UARTPuts("\r\n注意:您已将LED类型设置为通用LED,您需要指定LED识别码和名称!");
				FRU.FRUBlock.Data.Data.CustomLEDIDCode=0x5AA5; //自定义LED Code保留数值
        strncpy(FRU.FRUBlock.Data.Data.GeneralLEDString,"Undefined",32); //复制LED名称	 	
				}
			else //使用指定型号，覆盖数据
        {
        FRU.FRUBlock.Data.Data.CustomLEDIDCode=0xFFFF; //自定义LED Code保留数值
        strncpy(FRU.FRUBlock.Data.Data.GeneralLEDString,"Empty",32); //复制LED名称	 
	      }
			if(FRU.FRUBlock.Data.Data.MaxLEDCurrent>QueryMaximumCurrentLimit(&FRU))//非法的电流设置
		    {
				UARTPuts("\r\n警告:原有的最大电流值已被覆盖.");
				FRU.FRUBlock.Data.Data.MaxLEDCurrent=QueryMaximumCurrentLimit(&FRU); //设置最大电流
			  }
			if(!WriteFRU(&FRU))
				UartPrintf("\r\n%s的目标LED类型已被更新为%s.",frueditstr[3],DisplayLEDType(&FRU));
			else
				UartPrintf((char *)frueditstr[1],"目标LED类型");
			}
	  }
	//锁定FRU
	IsParameterExist("5",27,&ParamOK);
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
			M24C512_LockSecuSct();//上锁
			if(M24C512_QuerySecuSetLockStat()==LockState_Locked)
			  UartPrintf("%s已被永久激活.",frueditstr[4]);
			else
				UartPrintf("%s激活失败.",frueditstr[4]); 
			}
		#endif		
		}
	//设置NTC B值
	ParamPtr=IsParameterExist("6",27,NULL);
  if(ParamPtr!=NULL)
	  {
		IsCmdParamOK=true;
		len=atoi(ParamPtr); //字符串转整数
		//读取失败
	  if(ReadFRU(&FRU)||!CheckFRUInfoCRC(&FRU))
			UARTPuts((char *)frueditstr[0]);
	  //FRU被锁定
		else if(M24C512_QuerySecuSetLockStat()!=LockState_Unlocked)
			UARTPuts((char *)frueditstr[2]); 
		//写入数据
		else
		  {
			FRU.FRUBlock.Data.Data.NTCBValue=len; //更新电流值条目
			if(!WriteFRU(&FRU))
				UartPrintf("\r\n%s的NTC B值已被更新为%d.",frueditstr[3],len);
			else
				UartPrintf((char *)frueditstr[1],"NTC B值");
			}
		}
  //设置LED基板的温度修正值
	ParamPtr=IsParameterExist("7",27,NULL);
  if(ParamPtr!=NULL)
	  {
		IsCmdParamOK=true;
		buf=atof(ParamPtr); //字符串转浮点
		//读取失败
	  if(ReadFRU(&FRU)||!CheckFRUInfoCRC(&FRU))
			UARTPuts((char *)frueditstr[0]);
	  //FRU被锁定
		else if(M24C512_QuerySecuSetLockStat()!=LockState_Unlocked)
			UARTPuts((char *)frueditstr[2]);
		//用户输入的数值非法
	  else if(buf==NAN||buf<-40||buf>40)
		  {
			DisplayIllegalParam(ParamPtr,27,7);//显示用户输入了非法参数
			UARTPuts("\r\n错误:您指定的LED基板温度修正值应在-40到40'C之间.");
			}
		else //写入数据
		  {
			FRU.FRUBlock.Data.Data.NTCTrim=buf; //更新电流值条目
			if(!WriteFRU(&FRU))
				UartPrintf("\r\n%s的LED基板温度修正值已被更新为%.1f'C.",frueditstr[3],buf);
			else
				UartPrintf((char *)frueditstr[1],"LED基板温度修正值");
			}
     }
  //设置SPS的温度修正值
	ParamPtr=IsParameterExist("8",27,NULL);
  if(ParamPtr!=NULL)
	  {
		IsCmdParamOK=true;
		buf=atof(ParamPtr); //字符串转浮点
		//读取失败
	  if(ReadFRU(&FRU)||!CheckFRUInfoCRC(&FRU))
			UARTPuts((char *)frueditstr[0]);
	  //FRU被锁定
		else if(M24C512_QuerySecuSetLockStat()!=LockState_Unlocked)
			UARTPuts((char *)frueditstr[2]);
		//用户输入的数值非法
	  else if(buf==NAN||buf<-40||buf>40)
		  {
			DisplayIllegalParam(ParamPtr,27,8);//显示用户输入了非法参数
			UARTPuts("\r\n错误:您指定的驱动MOS温度修正值应在-40到40'C之间.");
			}
		else //写入数据
		  {
			FRU.FRUBlock.Data.Data.SPSTrim=buf; //更新电流值条目
			if(!WriteFRU(&FRU))
				UartPrintf("\r\n%s的驱动MOS温度修正值已被更新为%.1f'C.",frueditstr[3],buf);
			else
				UartPrintf((char *)frueditstr[1],"LED基板温度修正值");
		  }
		}
  //设置ADC的参考值
	ParamPtr=IsParameterExist("9",27,NULL);
  if(ParamPtr!=NULL)
	  {
		IsCmdParamOK=true;
		buf=atof(ParamPtr); //字符串转浮点
		//读取失败
	  if(ReadFRU(&FRU)||!CheckFRUInfoCRC(&FRU))
			UARTPuts((char *)frueditstr[0]);
	  //FRU被锁定
		else if(M24C512_QuerySecuSetLockStat()!=LockState_Unlocked)
			UARTPuts((char *)frueditstr[2]);
		//用户输入的数值非法
	  else if(buf==NAN||buf<3.2||buf>3.35)
		  {
			DisplayIllegalParam(ParamPtr,27,9);//显示用户输入了非法参数
			UARTPuts("\r\n错误:您指定的ADC参考电压值应在3.2到3.35V之间.");
			}
		else //写入数据
		  {
			FRU.FRUBlock.Data.Data.ADCVREF=buf; //更新电流值条目
			if(!WriteFRU(&FRU))
				UartPrintf("\r\n%s的ADC参考电压值已被更新为%.4fV.",frueditstr[3],buf);
			else
				UartPrintf((char *)frueditstr[1],"ADC参考电压值");
		  }
		}
	//设置LED的类型代码
	ParamPtr=IsParameterExist("A",27,NULL);
  if(ParamPtr!=NULL)
	  {		
	  IsCmdParamOK=true;
		hout=atoh(ParamPtr,&IsError); //转换16进制
		//读取失败
	  if(ReadFRU(&FRU)||!CheckFRUInfoCRC(&FRU))
			UARTPuts((char *)frueditstr[0]);
	  //FRU被锁定
		else if(M24C512_QuerySecuSetLockStat()!=LockState_Unlocked)
			UARTPuts((char *)frueditstr[2]);
		//用户输入的数值非法
		else if(IsError||(hout&0xFFFF0000)!=0)
		  {
			DisplayIllegalParam(ParamPtr,27,10);//显示用户输入了非法参数
			UARTPuts("\r\n错误:您输入的LED类型代码无效,有效的代码应为16位无符号16进制数并以'0x'开头.");
			}
    else //写入数据
		  {
			FRU.FRUBlock.Data.Data.CustomLEDIDCode=(unsigned short)hout; //更新电流值条目
			if(!WriteFRU(&FRU))
				UartPrintf("\r\n%s的LED类型代码已被更新为0x%04X.",frueditstr[3],hout);
			else
				UartPrintf((char *)frueditstr[1],"LED类型代码");
		  }
		}
	//设置大版本号
	ParamPtr=IsParameterExist("B",27,NULL);
  if(ParamPtr!=NULL)
	  {
		IsCmdParamOK=true;
		len=atoi(ParamPtr); //字符串转整数
		//读取失败
	  if(ReadFRU(&FRU)||!CheckFRUInfoCRC(&FRU))
			UARTPuts((char *)frueditstr[0]);
	  //FRU被锁定
		else if(M24C512_QuerySecuSetLockStat()!=LockState_Unlocked)
			UARTPuts((char *)frueditstr[2]); 
		//写入数据
		else
		  {
			FRU.FRUBlock.Data.Data.FRUVersion[1]=(char)len; //更新数据
			if(!WriteFRU(&FRU))
				UartPrintf("\r\n%s的硬件大版本号已被更新为%d.",frueditstr[3],len);
			else
				UartPrintf((char *)frueditstr[1],"硬件大版本号");
			}
		}
	//设置小版本号
	ParamPtr=IsParameterExist("C",27,NULL);
  if(ParamPtr!=NULL)
	  {
		IsCmdParamOK=true;
		len=atoi(ParamPtr); //字符串转整数
		//读取失败
	  if(ReadFRU(&FRU)||!CheckFRUInfoCRC(&FRU))
			UARTPuts((char *)frueditstr[0]);
	  //FRU被锁定
		else if(M24C512_QuerySecuSetLockStat()!=LockState_Unlocked)
			UARTPuts((char *)frueditstr[2]); 
		//写入数据
		else
		  {
			FRU.FRUBlock.Data.Data.FRUVersion[2]=(char)len; //更新数据
			if(!WriteFRU(&FRU))
				UartPrintf("\r\n%s的硬件小版本号已被更新为%d.",frueditstr[3],len);
			else
				UartPrintf((char *)frueditstr[1],"硬件小版本号");
			}
		}
	//设置自定义LED名称
	ParamPtr=IsParameterExist("D",27,NULL);
  if(ParamPtr!=NULL)
	  {
		IsCmdParamOK=true;
		len=strlen(ParamPtr);
		//读取失败
	  if(ReadFRU(&FRU)||!CheckFRUInfoCRC(&FRU))
			UARTPuts((char *)frueditstr[0]);
	  //FRU被锁定
		else if(M24C512_QuerySecuSetLockStat()!=LockState_Unlocked)
			UARTPuts((char *)frueditstr[2]);
		//字符串过短或者过长
		else if(len<1||len>31)
			UARTPuts("\r\n错误:LED名称字符串应至少包含一个字符且小于32字节(10个中文字符或31个英文字符).");
		else //正常写入
		  {
			strncpy(FRU.FRUBlock.Data.Data.GeneralLEDString,ParamPtr,32);	//复制序列号信息
			if(!WriteFRU(&FRU))
				UartPrintf("\r\n%sLED名称已被更新为'%s'.",frueditstr[3],ParamPtr);
			else
				UartPrintf((char *)frueditstr[1],"LED名称");
			}
		}
	#endif
	if(!IsCmdParamOK)UartPrintCommandNoParam(27);//显示啥也没找到的信息 
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕
	}
