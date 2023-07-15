#include "delay.h"
#include "I2C.h"

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
   //
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
