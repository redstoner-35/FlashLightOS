#include "Xmodem.h"

//Xmodem使用的CRC-16多项式是0x1021
#define CRC16Poly 0x1021

/*******************************
在Xmodem的循环里面往密钥加值顺序
值，使得加密更难以对付。
*******************************/
unsigned int GenerateSeqCodeForAES(void)
 {
 unsigned int buf;
 int i;
 if(XmodemTransferCtrl.XmodemTransferMode==Xmodem_Mode_NoTransfer)return 0;//仅在XModem需要的时候加值密钥
 buf=0;
 for(i=0;i<8;i++)
	 {
	 buf<<=4;
	 buf|=(XmodemTransferCtrl.CurrentPacketNum+i)&0x0F;
	 }	
 return buf;	 
 }

//软件CRC16
int PEC16CheckXModem(char *DIN,long Len)
 {
 unsigned short crc;
 char i;
 long j=0;
 //开始计算
 crc=0;
 while(j<Len)
  {
	crc=crc^(DIN[j]<<8);
	for(i=0;i<8;i++)
    {
	  if(crc&0x8000)
		 crc=(crc<<1)^CRC16Poly;
	  else
		 crc=crc<<1;
	}
	j++;
  }
 return ((int)crc)&0xFFFF;
 }

//校验和(checksum-8)计算
int Checksum8(char *DIN,long Len) 
 {
 long CalcBuf=0;
 int i;
 for(i=0;i<Len;i++)CalcBuf+=((int)DIN[i])&0xFF;
 CalcBuf&=0xFF;
 return (int)CalcBuf;
 }
