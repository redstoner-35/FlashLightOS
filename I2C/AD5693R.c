#include "PMBUS.h"
#include "delay.h"
#include "AD5693R.h"
#include "LEDMgmt.h"
#include "console.h"

//检测DAC是否就绪
bool AD5693R_Detect(char ADDR)
  {
	char i;
	//发送起始位，地址，NOP命令和2byte数据，如果都ACK则芯片已就绪
  IIC_Start();
  IIC_Send_Byte(ADDR);
  if(IIC_Wait_Ack())return false;//芯片无反应，返回false
	for(i=0;i<3;i++)
    {
	  IIC_Send_Byte(0x00); //NOP指令（空操作）
	  if(IIC_Wait_Ack())return false;//芯片无反应，返回false
		}
	//成功完成操作
	IIC_Stop();
	return true;
	}

//根据传入的结构体配置DAC的参数	
bool AD5693R_SetChipConfig(DACInitStrDef *DACInitStruct,char ADDR)
  {
	unsigned int buf=0; 
	//根据配置填写对应内容
	buf|=((unsigned short)DACInitStruct->DACPState)<<13;//设置PD1和PD0
	buf|=(DACInitStruct->IsOnchipRefEnabled)?0:0x1000;//设置Refbit
	buf|=DACInitStruct->DACRange==DAC_Output_REF?0:0x800;//设置GainBit
	#ifdef Internal_Driver_Debug
  UartPost(msg_error,"DACDrv","Control Register=0x%04X.",buf);
  #endif		 
	//发送配置
	if(!PMBUS_WordReadWrite(true,true,&buf,ADDR,0x40))return false;//向控制寄存器写入结果
	buf=0;
	return PMBUS_WordReadWrite(true,true,&buf,ADDR,0x30);//将DAC值POR为0
	}

/*设置DAC的输出电压（单位V）
注意！该函数不会针对实际DAC的允许摆幅进行检查。
例如DAC的Vref为2.5V,且启用了2倍增益模式而供电为3.3V。
此时DAC输出电压将达不到2倍Vref。*/
bool AD5693R_SetOutput(float VOUT,char ADDR)
  {
	unsigned int vid; 
	//计算VID Code
	float Buf=VOUT;
	Buf/=(DACVref/(float)65536);//将输出电压除以每阶的VID得到结果
	if(Buf>=(float)65535)Buf=(float)65535;
	if(Buf<0)Buf=0;//将算出的VID Code进行限幅
	#ifdef Internal_Driver_Debug
  UartPost(msg_error,"DACDrv","DAC VID Code=%d.",Buf);
  #endif		
	//开始发数据
	vid=(unsigned int)Buf;//把算出的VID Code转换一下
	return PMBUS_WordReadWrite(true,true,&vid,ADDR,0x30);//设置DAC值
	}
