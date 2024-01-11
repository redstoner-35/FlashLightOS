#include "console.h"
#include "FirmwareConf.h"
#include "ht32.h"
#include "delay.h"
#include "I2C.h"

#define ProgramSize 0x1FBFF  //程序的大小
#define CRCWordAddress 0x1FC00  //存储CRC字的存储器

//计算主程序区域的CRC-32
unsigned int MainProgramRegionCRC(void)
 {
 unsigned int DATACRCResult;
 #ifdef EnableSecureStor
 char buf[64];
 #endif 
 int i;
 CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
 //初始化CRC32      
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,ENABLE);//启用CRC-32时钟  
 CRC_DeInit(HT_CRC);//清除配置
 HT_CRC->SDR = 0x0;//CRC-32 poly: 0x04C11DB7  
 HT_CRC->CR = CRC_32_POLY | CRC_BIT_RVS_WR | CRC_BIT_RVS_SUM | CRC_BYTE_RVS_SUM | CRC_CMPL_SUM;
 //开始校验
 for(i=0;i<ProgramSize;i+=4)HT_CRC->DR=*(u32 *)i;//将内容写入到CRC寄存器内
 for(i=4;i<8;i++)HT_CRC->DR=i<4?*(u32*)(0x40080310+(i*4)):~*(u32*)(0x40080310+((i-4)*4));//写入FMC UID
 #ifdef EnableSecureStor
 if(!M24C512_ReadUID(buf,64))for(i=0;i<64;i++)
   {
	 buf[i]^=(i+3);
	 wb(&HT_CRC->DR,buf[i]); //读取UID成功，写入UID
	 }
 #endif
 //校验完毕计算结果
 DATACRCResult=HT_CRC->CSR;
 CRC_DeInit(HT_CRC);//清除CRC结果
 CLKConfig.Bit.CRC = 1;
 CKCU_PeripClockConfig(CLKConfig,DISABLE);//禁用CRC-32时钟节省电力
 DATACRCResult^=0x351A53DD;//输出结果异或一下
 return DATACRCResult;
 }	

//启用flash锁定
void CheckForFlashLock(void)
 {
 int i=0;
 FLASH_OptionByte Option;
 unsigned int ProgramAreaCRC;
 #ifdef FlashLightOS_Debug_Mode
 UartPost(Msg_warning,"FWSec","Debug mode Enabled,firmware security feature will be disabled.");	 
 return;
 #endif	 
 //检查option byte是否开启
 FLASH_GetOptionByteStatus(&Option);
 if(Option.MainSecurity != 0)
    {
    UartPost(Msg_info,"FWSec","Checking firmware code integrity...");
    ProgramAreaCRC=MainProgramRegionCRC();
		if(*(u32 *)CRCWordAddress!=ProgramAreaCRC) //检查不通过
	    {
	    UartPost(Msg_critical,"FWSec","firmware code was corrupted or being modified by attackers,system halted!");
	    SelfTestErrorHandler();
	    }
    return;
    }
 UartPost(Msg_warning,"FWSec","Firmware security feature is not enabled.System will implement this feature for security reason.");
 //启用HSI(给flash设置option byte需要HSI启用)
 CKCU_HSICmd(ENABLE);
 while(CKCU_GetClockReadyStatus(CKCU_FLAG_HSIRDY) != SET)
   {
	 delay_ms(1);
	 i++;
	 if(i==50)break;
	 }
 if(i==50)
   {
	 UartPost(msg_error,"FMC","Failed to enable HSI for Flash Programming!");
	 return;
	 }
 //编程程序的CRC32值
 ProgramAreaCRC=MainProgramRegionCRC();
 if(FLASH_ErasePage(CRCWordAddress)!=FLASH_COMPLETE)
	 {
	 UartPost(msg_error,"FWSec","Flash erase failed when try to program checksum.");
	 return;
	 }
 if(FLASH_ProgramWordData(CRCWordAddress, ProgramAreaCRC)!=FLASH_COMPLETE)
   {
	 UartPost(msg_error,"FWSec","Flash program failed when try to program checksum.");
	 return;
	 }
 if(*((u32 *)CRCWordAddress)!=ProgramAreaCRC)
	 {
	 UartPost(msg_error,"FWSec","Flash verify failed when try to program checksum.");
	 return;
	 }
 UartPost(Msg_info,"FWSec","Firmware signature has been programmed,value is 0x%08X.",ProgramAreaCRC);
 //打开主安全功能
 Option.OptionProtect=1; //锁定选项byte
 Option.MainSecurity=1;  //打开ROP
 for(i=0;i<4;i++)Option.WriteProtect[i]=0xFFFFFFFF;//所有ROM上锁
 FLASH_EraseOptionByte();
 if(FLASH_ProgramOptionByte(&Option)!=FLASH_COMPLETE)
   {
	 UartPost(msg_error,"FWSec","Option byte program failed.");
	 return;
	 }
 UartPost(Msg_info,"FWSec","Firmware security feature has been enabled.System will restart now.");
 NVIC_SystemReset();  //刷完之后重启
 while(1);
 }
