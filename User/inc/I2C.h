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

#ifdef UsingEE_24C1024
  //24C1024没有支持安全扇区的功能因此要禁用安全扇区
  #ifdef EnableSecureStor
	  #error "Security Sector did not compatible with 24C1024 EEPROM!Remove 'EnableSecureStor' define in FirmwareConf.h file."
	#endif
	//设置地址范围和页大小
  #define MaxByteRange 1048575
  #define MaxPageSize 256    //24C1024 (128K)
#endif

//检测是否定义了EEPROM
#ifndef MaxPageSize
  #error "You forgot to select which EEPROM you want to use!"
#endif

//I2C总线操作自动define，禁止修改！
#define IIC_SCL_IOB STRCAT2(GPIO_P,IIC_SCL_IOBank)
#define IIC_SCL_IOG STRCAT2(HT_GPIO,IIC_SCL_IOBank)
#define IIC_SCL_IOP STRCAT2(GPIO_PIN_,IIC_SCL_IOPinNum) //SCL自动Define

#define IIC_SDA_IOB STRCAT2(GPIO_P,IIC_SDA_IOBank)
#define IIC_SDA_IOG STRCAT2(HT_GPIO,IIC_SDA_IOBank)
#define IIC_SDA_IOP STRCAT2(GPIO_PIN_,IIC_SDA_IOPinNum) //SDA自动Define

#define IIC_SDA_Set() GPIO_SetOutBits(IIC_SDA_IOG,IIC_SDA_IOP)
#define IIC_SDA_Clr() GPIO_ClearOutBits(IIC_SDA_IOG,IIC_SDA_IOP)
#define IIC_SCL_Set() GPIO_SetOutBits(IIC_SCL_IOG,IIC_SCL_IOP)
#define IIC_SCL_Clr() GPIO_ClearOutBits(IIC_SCL_IOG,IIC_SCL_IOP)
#define READ_SDA GPIO_ReadInBit(IIC_SDA_IOG,IIC_SDA_IOP)

typedef enum
{
SMBUS_DIR_MOTS,//主机到从机
SMBUS_DIR_SOTM//从机到主机
}BDIR;

typedef enum
{
LockState_EEPROM_NACK, //EEPROM没有回复
LockState_Locked,  //已上锁
LockState_Unlocked  //未上锁	
}SecuLockState;

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
char M24C512_WriteSecuSct(char *Data,int StartAddr,int len);//写安全扇区
char M24C512_ReadSecuSct(char *Data,int StartAddr,int len);//读安全扇区
char M24C512_ReadUID(char *Data,int len);//读取128Bit UID
SecuLockState M24C512_QuerySecuSetLockStat(void);//检查FM24C512的安全扇区是否被锁定
char M24C512_LockSecuSct(void);//给安全存储区上锁

#endif
