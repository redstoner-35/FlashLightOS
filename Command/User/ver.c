#include "console.h"
#include "CfgFile.h"
#include <stdio.h>
#include <string.h>
#include "AES256.h"
#include "FirmwareConf.h"
#include "FRU.h"

extern const char FRUVersion[3];

//logo
const char *FlashLightOSIcon[7]=
 {
	" _______ ___     _______ _______ __   __ ___     ___ _______ __   __ _______    _______ _______ ", //0
	"|       |   |   |       |       |  | |  |   |   |   |       |  | |  |       |  |       |       |", //1
	"|    ___|   |   |   _   |  _____|  |_|  |   |   |   |    ___|  |_|  |_     _|  |   _   |  _____|", //2
	"|   |___|   |   |  |_|  | |_____|       |   |   |   |   | __|       | |   |    |  | |  | |_____ ", //3
	"|    ___|   |___|       |_____  |       |   |___|   |   ||  |       | |   |    |  |_|  |_____  |", //4
	"|   |   |       |   _   |_____| |   _   |       |   |   |_| |   _   | |   |    |       |_____| |", //5
	"|___|   |_______|__| |__|_______|__| |__|_______|___|_______|__| |__| |___|    |_______|_______|" //6
 };
//使用AES256加密的版权标识
//该版权标识可以使用'cptextgen'命令生成，然后替换掉下方数组的内容即可。
const char EncryptedCopyRight[48]=
{
0xDD,0x8C,0xBE,0x2F,0x3F,0xF3,0x40,0x24,
0x25,0x32,0x07,0x82,0x1D,0xAF,0x7A,0xE5,
0xBF,0x57,0xF6,0x80,0xD7,0x13,0x88,0x6C,
0x97,0x41,0x10,0x43,0x3E,0xE2,0x88,0xB4,
0x93,0xD9,0xE0,0x6C,0x4A,0x4C,0x13,0x4F,
0xDD,0x74,0xF4,0x3F,0xF0,0x2F,0xF9,0x4B
};
#pragma push
#pragma Otime//优化该函数使用3级别优化
#pragma O3
void verHandler(void)
  {
	int i;
	char EncryptBUF[48];
  const char *TextPtr;
	FRUBlockUnion FRU;
	UARTPuts("\r\n");
	for(i=0;i<7;i++)//打印logo
		 {
		 UARTPuts("\r\n");
		 UARTPuts((char *)FlashLightOSIcon[i]);
		 }
	UartPrintf("\r\n\r\nPowered by FlashLight OS version-%d.%d.%d,终端波特率:%dbps",MajorVersion,MinorVersion,HotfixVersion,CfgFile.USART_Baud);
  #ifndef EnableSecureStor
  if(!M24C512_PageRead(FRU.FRUBUF,SelftestLogEnd,sizeof(FRUBlockUnion))&&CheckFRUInfoCRC(&FRU))
  #else
  if(!M24C512_ReadSecuSct(FRU.FRUBUF,0,sizeof(FRUBlockUnion))&&CheckFRUInfoCRC(&FRU)) //FRU读取成功
  #endif	 
	   {
     TextPtr=DisplayLEDType(&FRU);
		 UartPrintf("\r\n硬件平台:%s V%d.%d for %s LED.",HardwarePlatformString,FRU.FRUBlock.Data.Data.FRUVersion[1],FRU.FRUBlock.Data.Data.FRUVersion[2],TextPtr);
		 UartPrintf("\r\n产品序列号:%s",FRU.FRUBlock.Data.Data.SerialNumber);
		 }
	else //读取失败
     UARTPuts("\r\n硬件平台:未知平台\r\n产品序列号:未知");
	switch(AccountState)//根据当前登录状态显示信息
	   {
		 case Log_Perm_Guest:TextPtr="游客";break;
		 case Log_Perm_Admin:TextPtr="管理员";break;
		 case Log_Perm_Root:TextPtr="超级用户";break;
		 default:TextPtr="未知";
		 }
	UartPrintf("\r\n当前登录身份:%s                主机名:%s",TextPtr,CfgFile.HostName);
	UartPrintf("\r\n固件构建日期:%s %s\r\n",__DATE__,__TIME__);	 
	//安全操作，处理密文
	IsUsingOtherKeySet=false;//处理密文的时候使用第一组key
	memcpy(EncryptBUF,EncryptedCopyRight,48);//将密文复制进来	
	AES_EncryptDecryptData(&EncryptBUF[0],0);	 
	AES_EncryptDecryptData(&EncryptBUF[16],0);//解密被加密的文字
	AES_EncryptDecryptData(&EncryptBUF[32],0);	 	
	UARTPuts(EncryptBUF);
	memset(EncryptBUF,0x00,48);//销毁原文
  IsUsingOtherKeySet=true;//重新使用第二组key
	//显示其余内容
	UARTPuts("       All rights reserved.\r\n警告:本驱动的硬件和固件(FlashLight OS)基于'CC-BY-NC-SA-4.0'");
		 UARTPuts("\r\n许可证发布并且受《著作权法》的保护.未经允许不得商用!\r\n");
	ClearRecvBuffer();//清除接收缓冲
	CmdHandle=Command_None;//命令执行完毕				
	}
#pragma pop
