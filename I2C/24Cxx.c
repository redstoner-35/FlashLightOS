#include "delay.h"
#include "I2C.h"

//检查FM24C512的安全扇区是否被锁定
SecuLockState M24C512_QuerySecuSetLockStat(void)
 {
 #ifndef EnableSecureStor
 return LockState_Unlocked; //没使能安全存储，此功能无效	 
 #else
 char buf;	 
 //发送地址，目标要读写的位置
 IIC_Start();
 IIC_Send_Byte(M24C512SecuADDR);
 if(IIC_Wait_Ack())return LockState_EEPROM_NACK;
 IIC_Send_Byte(0x04);	 //高位地址XXXX_X10X 
 if(IIC_Wait_Ack())return LockState_EEPROM_NACK;
 IIC_Send_Byte(0x00); //低位地址XXXX_XXXX
 if(IIC_Wait_Ack())return LockState_EEPROM_NACK;	 
 //读取
 IIC_Start();
 IIC_Send_Byte(M24C512SecuADDR+1);
 if(IIC_Wait_Ack())return LockState_EEPROM_NACK; //重复启动一次开始读数据	 
 buf=IIC_Read_Byte(0);//读一字节之后NACK表示不读了
 IIC_Stop();
 //如果锁定字节的第2位为1则表示已锁定	 
 if(buf&0x02)return LockState_Locked;
 return LockState_Unlocked;
 #endif
 }
//给安全存储区上锁
char M24C512_LockSecuSct(void)
 {
  //发送地址，目标要读写的位置
 IIC_Start();
 IIC_Send_Byte(M24C512SecuADDR);
 if(IIC_Wait_Ack())return 1;
 IIC_Send_Byte(0x04);	 //高位地址XXXX_X10X 
 if(IIC_Wait_Ack())return 1;
 IIC_Send_Byte(0x00); //低位地址XXXX_XXXX
 if(IIC_Wait_Ack())return 1;
 IIC_Send_Byte(0x02); //锁定字节XXXX_XX1X
 if(!IIC_Wait_Ack())
   {
	 IIC_Stop();
	 return 1;//发送完毕后回复ACK，锁定失败 
	 }
 return 0;
 }
//写入FM24C512的Security Sector
char M24C512_WriteSecuSct(char *Data,int StartAddr,int len)
 {
 int i;
 //判断参数
 if(StartAddr>127)return 1; //安全扇区固定128字节大小
 if(len+StartAddr>127)len=127-StartAddr;//写入超长度，限制长度 
 //发送地址，目标要读写的位置
 IIC_Start();
 IIC_Send_Byte(M24C512SecuADDR);
 if(IIC_Wait_Ack())return 1;
 IIC_Send_Byte(0x00);	 //高位地址XXXX_X00X 
 if(IIC_Wait_Ack())return 1;
 IIC_Send_Byte(StartAddr&0x7F); //低位地址
 if(IIC_Wait_Ack())return 1;
 //发送数据
 for(i=0;i<len;i++)
    {
	  IIC_Send_Byte(Data[i]);
	  if(IIC_Wait_Ack())return 1;
	  }	 
 IIC_Stop();
 //发送完毕后等待器件处理
 i=0;
 while(i<50)
    {
	  IIC_Start();
	  IIC_Send_Byte(M24C512SecuADDR);
	  if(!IIC_Wait_Ack())
	    {
		  IIC_Stop();
		  break;
		  }
	  delay_ms(1);
	  i++;
	  }
 if(i==50)return 1;//等待超时
 //写入成功，退出
 return 0;
 }
//读取FM24C512的Security Sector
char M24C512_ReadSecuSct(char *Data,int StartAddr,int len)
 {
 int i;
 //判断参数
 if(StartAddr>127)return 1; //安全扇区固定128字节大小
 if(len+StartAddr>128)len=128-StartAddr;//写入超长度，限制长度 
 //发送地址，目标要读写的位置
 IIC_Start();
 IIC_Send_Byte(M24C512SecuADDR);
 if(IIC_Wait_Ack())return 1;
 IIC_Send_Byte(0x00);	 //高位地址XXXX_X00X 
 if(IIC_Wait_Ack())return 1;
 IIC_Send_Byte(StartAddr&0x7F); //低位地址
 if(IIC_Wait_Ack())return 1;
 //读取数据
 IIC_Start();
 IIC_Send_Byte(M24C512SecuADDR+1);
 if(IIC_Wait_Ack())return 1; //重复启动一次开始读数据
 for(i=0;i<len;i++)Data[i]=IIC_Read_Byte(i<(len-1)?1:0);//循环读数据，直到最后发送NACK
 IIC_Stop();
 //读取成功，退出
 return 0;
 }

//读取FM24C512的UID
char M24C512_ReadUID(char *Data,int len)
 {
 int i;
 //判断参数
 if(len>128)return 1;//写入超长度，限制长度 
 //发送地址，目标要读写的位置
 IIC_Start();
 IIC_Send_Byte(M24C512SecuADDR);
 if(IIC_Wait_Ack())return 1;
 IIC_Send_Byte(0x02);	 //高位地址XXXX_XX1X 
 if(IIC_Wait_Ack())return 1;
 IIC_Send_Byte(0x00); //低位地址XXXX_0000
 if(IIC_Wait_Ack())return 1;
 //读取数据
 IIC_Start();
 IIC_Send_Byte(M24C512SecuADDR+1);
 if(IIC_Wait_Ack())return 1; //重复启动一次开始读数据
 for(i=0;i<len;i++)Data[i]=IIC_Read_Byte(i<(len-1)?1:0);//循环读数据，直到最后发送NACK
 IIC_Stop();
 //读取成功，退出
 return 0;
 } 
 
//M24C512 写入数据
char M24C512_PageWrite(char *Data,int StartAddr,int len)
 {
 int i,txlen,offset;
 //判断参数
 if(StartAddr>MaxByteRange)return 1;	
 if(StartAddr+len>MaxByteRange)len=MaxByteRange-StartAddr;
 offset=0;
 //开始发送
 while(len>0)
   {
	 //计算欲发送的长度
	 txlen=StartAddr%MaxPageSize; 
   txlen=MaxPageSize-txlen;
	 if(txlen>len)txlen=len; 
   //发送地址，目标要读写的位置
   IIC_Start();
   IIC_Send_Byte(M24C512ADDR);
   if(IIC_Wait_Ack())return 1;
   IIC_Send_Byte((StartAddr>>8)&0xFF);	 
   if(IIC_Wait_Ack())return 1;
   IIC_Send_Byte(StartAddr&0xFF);
   if(IIC_Wait_Ack())return 1;
   //发送数据
   for(i=0;i<txlen;i++)
    {
	  IIC_Send_Byte(Data[i+offset]);
	  if(IIC_Wait_Ack())return 1;
	  }	 
   IIC_Stop();
   //发送完毕后等待器件处理
   i=0;
   while(i<50)
    {
	  IIC_Start();
	  IIC_Send_Byte(M24C512ADDR);
	  if(!IIC_Wait_Ack())
	    {
		  IIC_Stop();
		  break;
		  }
	  delay_ms(1);
	  i++;
	  }
	 if(i==50)return 1;//等待超时
	 //计算新的缓冲区读取，ROM写入和长度
   offset+=txlen;
	 StartAddr+=txlen;
	 len-=txlen; 
	 }
 return 0;
 }

//M24C512读取数据
char M24C512_PageRead(char *Data,int StartAddr,int len)
 {
 int i,rxlen,offset;
 //判断参数
 if(StartAddr>MaxByteRange)return 1;	
 if(StartAddr+len>MaxByteRange)len=MaxByteRange-StartAddr; 
 offset=0;
 //开始读数据
 while(len>0)
   {
	 //计算欲发送的长度
	 rxlen=StartAddr%MaxPageSize; 
   rxlen=MaxPageSize-rxlen;
	 if(rxlen>len)rxlen=len; 
	 //发送地址，目标要读写的位置
   IIC_Start();
   IIC_Send_Byte(M24C512ADDR);
   if(IIC_Wait_Ack())return 1;
   IIC_Send_Byte((StartAddr>>8)&0xFF);	 
   if(IIC_Wait_Ack())return 1;
   IIC_Send_Byte(StartAddr&0xFF);
   if(IIC_Wait_Ack())return 1;	
   //读取内容
   IIC_Start();
   IIC_Send_Byte(M24C512ADDR+1);
   if(IIC_Wait_Ack())return 1;//鑺墖鏃犲弽搴旓紝杩斿洖1
   for(i=0;i<rxlen;i++)Data[i+offset]=IIC_Read_Byte(i<(rxlen-1)?1:0); 
   IIC_Stop();
	 //一轮读取完毕，计算新的缓冲区读取，ROM写入和长度
	 offset+=rxlen;
	 StartAddr+=rxlen;
	 len-=rxlen; 
	 }
 //读取操作完毕
 return 0;
 }
//擦除EEPROM指定区域内的数据内容
char M24C512_Erase(int StartAddr,int len)
 {
 int i,txlen,offset;
 //判断参数
 if(StartAddr>MaxByteRange)return 1;	
 if(StartAddr+len>MaxByteRange)len=MaxByteRange-StartAddr;
 offset=0;
 //开始发送
 while(len>0)
   {
	 //计算欲发送的长度
	 txlen=StartAddr%MaxPageSize; 
   txlen=MaxPageSize-txlen;
	 if(txlen>len)txlen=len; 
   //发送地址，目标要读写的位置
   IIC_Start();
   IIC_Send_Byte(M24C512ADDR);
   if(IIC_Wait_Ack())return 1;
   IIC_Send_Byte((StartAddr>>8)&0xFF);	 
   if(IIC_Wait_Ack())return 1;
   IIC_Send_Byte(StartAddr&0xFF);
   if(IIC_Wait_Ack())return 1;
   //
   for(i=0;i<txlen;i++)
    {
	  IIC_Send_Byte(0xFF);
	  if(IIC_Wait_Ack())return 1;
	  }	 
   IIC_Stop();
   //发送完毕后等待器件处理
   i=0;
   while(i<50)
    {
	  IIC_Start();
	  IIC_Send_Byte(M24C512ADDR);
	  if(!IIC_Wait_Ack())
	    {
		  IIC_Stop();
		  break;
		  }
	  delay_ms(1);
	  i++;
	  }
	 if(i==50)return 1;//等待超时
	 //计算新的缓冲区读取，ROM写入和长度
   offset+=txlen;
	 StartAddr+=txlen;
	 len-=txlen; 
	 }
 return 0;
 }
