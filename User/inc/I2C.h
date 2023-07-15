#ifndef _I2C_
#define _I2C_

//内部包含
#include "Pindefs.h"
#include "I2CAddr.h"
#include "FirmwareConf.h"

/* EEPROM自动配置,禁止修改！ */
#ifdef UsingEE_24C512
  #define MaxByteRange 65535
  #define MaxPageSize 128   //24C512 (64K)
#endif

#ifdef UsingEE_24C256
  #define MaxByteRange 32767
  #define MaxPageSize 64    //24C256 (32K)
#endif

//检测是否定义了EEPROM
#ifndef MaxPageSize
  #error "You forgot to select which EEPROM you want to use!"
#endif

typedef enum
{
SMBUS_DIR_MOTS,//主机到从机
SMBUS_DIR_SOTM//从机到主机
}BDIR;

//函数
void SMBUS_Init(void);
void IIC_Start(void);
void IIC_Stop(void);
char IIC_Wait_Ack(void);
void IIC_Ack(void);
void IIC_NAck(void);
void IIC_Send_Byte(unsigned char txd);
unsigned char IIC_Read_Byte(unsigned char ack);

//M24C512
char M24C512_PageWrite(char *Data,int StartAddr,int len);//写数据
char M24C512_PageRead(char *Data,int StartAddr,int len);//读数据
char M24C512_Erase(int StartAddr,int len);//擦除

#endif
