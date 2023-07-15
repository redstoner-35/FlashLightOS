#include "ht32.h"
#include "I2C.h"
#include "delay.h"
#include "console.h"
#include "PMBUS.h"

//I2C延时
static void IIC_delay(void)
  {
	int i=7;
	while(--i);
	}

//SMBUS初始化
extern bool IsSelftestLoggerReady;	
	
void SMBUS_Init(void)
  {
	 unsigned char i,slavecount;
	 UartPost(Msg_info,"SMBUSDrv","Start to Configure SMBUS Controller...");
	 //配置GPIO(SCL)
   AFIO_GPxConfig(IIC_SCL_IOB,IIC_SCL_IOP, AFIO_FUN_GPIO);//I2C SCL(用来做时钟)
   GPIO_DirectionConfig(IIC_SCL_IOG,IIC_SCL_IOP,GPIO_DIR_OUT);//配置为输出
   GPIO_SetOutBits(IIC_SCL_IOG,IIC_SCL_IOP);//输出设置为1
	 GPIO_PullResistorConfig(IIC_SCL_IOG,IIC_SCL_IOP,GPIO_PR_UP);//启用上拉
	 //配置GPIO(SDA)
   AFIO_GPxConfig(IIC_SDA_IOB,IIC_SDA_IOP, AFIO_FUN_GPIO);//I2C SDA(用来做数据)
   GPIO_DirectionConfig(IIC_SDA_IOG,IIC_SDA_IOP,GPIO_DIR_OUT);//配置为输出
   GPIO_SetOutBits(IIC_SDA_IOG,IIC_SDA_IOP);//输出设置为1	 
	 GPIO_PullResistorConfig(IIC_SDA_IOG,IIC_SDA_IOP,GPIO_PR_UP);//启用上拉
	 //配置完毕，启用logger然后开始扫描设备
	 IsSelftestLoggerReady=true;
	 delay_ms(10);
	 UartPost(Msg_info,"SMBUSDrv","SMBUS Controller is ready with %dKbps speed.",480);
	 //扫描设备
	 UartPost(Msg_info,"SMBUSDrv","--- Probing all accessible SMBUS slave devices ---\r\n");
	 slavecount=0;
	 for(i=0;i<0x7F;i++)
     {
		 IIC_Start();
		 IIC_Send_Byte((i<<1)&0xFE);
     if(IIC_Wait_Ack())continue;
		 IIC_Send_Byte(0x01);
		 if(IIC_Wait_Ack())continue;	 
		 UartPost(Msg_info,"SMBUSDrv","Found smbus slave at Addr:0x%02X(%dd)",(i<<1)&0xFE,(i<<1)&0xFE);
		 slavecount++;
		 IIC_Stop();
		 }		
	UartPost(Msg_info,"SMBUSDrv","Device Probing Done,found %d Slave Devices.\r\n",slavecount); 
	}
//设置传输方向
static void SetTransferDir(BDIR DIR)
  {
	GPIO_DirectionConfig(IIC_SDA_IOG,IIC_SDA_IOP,(DIR==SMBUS_DIR_MOTS)?GPIO_DIR_OUT:GPIO_DIR_IN);//配置IO方向
	GPIO_InputConfig(IIC_SDA_IOG,IIC_SDA_IOP,(DIR==SMBUS_DIR_MOTS)?DISABLE:ENABLE);//启用或禁用IDR
	}
//产生IIC起始信号
void IIC_Start(void)
{
	SetTransferDir(SMBUS_DIR_MOTS);//主机->从机
	IIC_SDA_Set();	  	  
	IIC_SCL_Set();
	IIC_delay();
 	IIC_SDA_Clr();//START:when CLK is high,DATA change form high to low 
	IIC_delay();
	IIC_SCL_Clr();//钳住I2C总线，准备发送或接收数据 
}	  
//产生IIC停止信号
void IIC_Stop(void)
{
	SetTransferDir(SMBUS_DIR_MOTS);//主机->从机
	IIC_SCL_Clr();
	IIC_SDA_Clr();//STOP:when CLK is high DATA change form low to high
 	IIC_delay();
	IIC_SCL_Set(); 
	IIC_delay();
	IIC_SDA_Set();//发送I2C总线结束信号
	IIC_delay();							   	
}
//等待应答信号到来
//返回值：1，接收应答失败
//        0，接收应答成功
char IIC_Wait_Ack(void)
{
	u8 ucErrTime=0;
	IIC_SDA_Set();//释放数据线
	IIC_delay();	   
	IIC_SCL_Set();//时钟拉高
	IIC_delay();	
	SetTransferDir(SMBUS_DIR_SOTM);//从机->主机
	while(READ_SDA)
	{
		ucErrTime++;
		if(ucErrTime>250)
		{
			IIC_Stop();
			return 1;
		}
	}
	IIC_SCL_Clr();//时钟输出0 	   
	return 0;  
} 
//产生ACK应答
void IIC_Ack(void)
{
	SetTransferDir(SMBUS_DIR_MOTS);//主机->从机
	IIC_SCL_Clr();
	IIC_SDA_Clr();
	IIC_delay();
	IIC_SCL_Set();
	IIC_delay();
	IIC_SCL_Clr();
}
//不产生ACK应答		    
void IIC_NAck(void)
{
	SetTransferDir(SMBUS_DIR_MOTS);//主机->从机
	IIC_SCL_Clr();
	IIC_SDA_Set();
	IIC_delay();
	IIC_SCL_Set();
	IIC_delay();
	IIC_SCL_Clr();
}					 				     
//IIC发送一个字节	  
void IIC_Send_Byte(unsigned char txd)
{                        
    u8 t;      
	  SetTransferDir(SMBUS_DIR_MOTS);//主机->从机
    IIC_SCL_Clr();//拉低时钟开始数据传输
    IIC_delay();//等一等再开始传输
    for(t=0;t<8;t++)
    {              
    if(txd&0x80)IIC_SDA_Set();
		else IIC_SDA_Clr();
    txd<<=1; 	  
		IIC_delay();   //对TEA5767这三个延时都是必须的
		IIC_SCL_Set();
		IIC_delay(); 
		IIC_SCL_Clr();	
		IIC_delay();
    }	 
} 	    
//读1个字节，ack=1时，发送ACK，ack=0，发送nACK   
unsigned char IIC_Read_Byte(unsigned char ack)
{
	unsigned char i,receive=0;
	  IIC_SDA_Set();//释放总线
	  SetTransferDir(SMBUS_DIR_SOTM);//从机->主机
    for(i=0;i<8;i++ )
	{
        IIC_SCL_Clr(); 
        IIC_delay();
		    IIC_SCL_Set();
        receive<<=1;
        if(READ_SDA)receive++;   
		    IIC_delay(); 
    }					 
    if (!ack)
        IIC_NAck();//发送nACK
    else
        IIC_Ack(); //发送ACK   
    return receive;
}
