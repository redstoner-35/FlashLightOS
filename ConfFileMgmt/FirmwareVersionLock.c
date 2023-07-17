#include "I2C.h"
#include "FirmwareConf.h"
#include "selftestlogger.h"
#include "console.h"
#include "Xmodem.h"
#include "LEDMgmt.h"
#include <string.h>

#ifdef UsingLED_3V
const char FRUVersion[3]={0x03,0x01,0x00}; //分别表示适用LED电压3V，硬件版本1.0
#else
const char FRUVersion[3]={0x06,0x01,0x01}; //分别表示适用LED电压6V，硬件版本1.1
#endif 

/**************************************************************
固件启动时检查EEPROM内的版本信息，如果版本信息不匹配则拒绝启动。
这是因为3V和6V版本的驱动固件和硬件之间互不兼容，会存在用户OTA更
新时意外将3V固件刷入6V驱动内的情况，这种情况就会导致不应出现的
LED开路和短路故障。
**************************************************************/

void FirmwareVersionCheck(void)
 {
 char buf[4];
 char checksumbuf[4];
 int i;
 //加载固件信息
 memcpy(checksumbuf,FRUVersion,3);
 checksumbuf[3]=Checksum8(checksumbuf,3)^0xAF; //计算校验和
 //读取	 
 if(M24C512_PageRead(buf,SelftestLogEnd,4))
   {
	 CurrentLEDIndex=6;//EEPROM不工作
	 UartPost(Msg_critical,"FRUChk","Failed to check Hardware FRU info in EEPROM.");
	 SelfTestErrorHandler();//EEPROM掉线
	 }
 //检查FRU信息是否匹配
 for(i=0;i<4;i++)if(buf[i]!=checksumbuf[i])break;
 if(i!=4) //信息不匹配
	  {
		#ifndef FlashLightOS_Debug_Mode
		CurrentLEDIndex=3;//红灯常亮
		UartPost(Msg_critical,"FRUChk","hardware platform mismatch!This firmware is for %dV LED platform and Hardware Rev. of V%d.%d.System halted!",FRUVersion[0],FRUVersion[1],FRUVersion[2]);
		SelfTestErrorHandler();//固件版本不匹配
	  #endif
		M24C512_PageWrite(checksumbuf,SelftestLogEnd,4);
		UartPost(Msg_warning,"FRUChk","hardware platform mismatch,FRU Record Has been refreshed.");
		UartPost(Msg_info,"FRUChk","New recoed is %dV LED platform,Hardware Rev. V%d.%d.",checksumbuf[0],checksumbuf[1],checksumbuf[2]);
		}
 }
