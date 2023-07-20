#ifndef _I2CADDR_
#define _I2CADDR_

/****************************************************
这是系统中所有I2C从器件的地址。您可以根据需要去更改。
需要注意的是，这里写的所有地址都是设R/W位=0的8位地址。
地址格式如下：

MSB  [A7 A6 A5 ... A3 A2 A1 R/W(=0)]  LSB

*****************************************************/

#define INA219ADDR 0x84 //电池端功率计，负责估算容量消耗数  
#define M24C512ADDR 0xA0  //64K EEPROM负责存储系统的配置参数
#define M24C512SecuADDR 0xB0 //FM24C512独有的安全sector的地址
#define AD5693ADDR 0x98 //负责线性调光的16bit DAC

#endif
