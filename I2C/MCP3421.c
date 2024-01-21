#include "delay.h"
#include "I2C.h"
#include "MCP3421.h"
#include "PMBUS.h"

//配置MCP3421
bool MCP3421_SetChip(PGAGainDef Gain,SampleRateDef Rate,bool IsPowerDown)
  {
	unsigned char buf;
	//设置参数
  buf=IsPowerDown?0x00:0x10;
	buf|=(char)Gain&0x03; //设置增益
  buf|=((char)Rate&0x03)<<2;//设置转换位数和转换率
	//写参数
	IIC_Start();
  IIC_Send_Byte(MCP3421ADDR); //送出地址
  if(IIC_Wait_Ack())return false;
  IIC_Send_Byte(buf); //送出配置字节
  if(IIC_Wait_Ack())return false;
	IIC_Stop();//IIC停止
	//发送完毕，退出
  return true;
	}

//读取电压
bool MCP3421_ReadVoltage(float *VOUT)
  {
	unsigned short DBUF; 
	unsigned char conf,bitcount;
  float result;
	bool Isneg=false;
	//读参数
	IIC_Start();
  IIC_Send_Byte(MCP3421ADDR+1); //送出地址
  if(IIC_Wait_Ack())return false;
	DBUF=(unsigned short)IIC_Read_Byte(1)<<8;
	DBUF&=0xFF00;
  DBUF|=((unsigned short)IIC_Read_Byte(1))&0xFF; //取出word
	conf=IIC_Read_Byte(0); //最后一个字节的配置byte
	IIC_Stop(); //停止位
	//判断参数	
  if(DBUF&0x8000) //最高位为1，代表负数
	  {
		Isneg=true; 
		DBUF=~DBUF;//按位取反
		}
	switch((conf>>2)&0x03)//获取ADC位数去除符号位
	  {
		case 0:DBUF&=0x7FF;bitcount=12;break;//12bit模式
		case 1:DBUF&=0x1FFF;bitcount=14;break;//14bit模式
	  case 2:DBUF&=0x7FFF;bitcount=16;break;//16bit模式
		default:return false;
		}
	//转换数据
  if(Isneg)DBUF+=1; //取反+去除符号位之后还需要+1才是源码
  result=4.096/PMBUS_2NPowCalc((int)bitcount); //LSB=2*2.048V/2^位数
  result*=(float)DBUF; //LSB*code得到电压
	if(Isneg)result*=-1;//是负数，乘上-1
	switch(conf&0x03)//获取PGA增益并自动除以增益系数
	  {
		case 0:break;//PGA=1V/V
		case 1:result/=2;break;//PGA=2V/V
	  case 2:result/=4;break;//PGA=4V/V
		case 3:result/=8;break;//PGA=8V/V
		default:return false;
		}		
	//计算完毕，返回结果
		if(!(conf&0x80))*VOUT=result; //输出结果有效，更新数值
	return true;
	}
