#include <string.h>
#include "FRU.h"
#include "CfgFile.h"
#include "SideKey.h"
#include "LEDMgmt.h"
#include "delay.h"

/*
本文件负责实现出厂设置重置功能，当用户意外忘记
配置系统的登录凭据时，可以在手电LED自检结束的
瞬间按下开关并根据提示输入5位的PassKey以重置
密码。
*/

//生成指定次数的提示操作
static void GenerateLEDInfoForInput(char *Color,int Time)
  {
	LED_Reset();//复位LED管理器
  memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
	LED_AddStrobe(Time,Color);
	strncat(LEDModeStr,"E",sizeof(LEDModeStr)-1);  //指示当前输入的是哪一位
	SubmitCustomLEDPattern();//提交编程好的pattern
	}

//接收用户的PassKey输入
static void AcceptPassKeyInput(char *PassKeyOut)
  {
  int PassKeyInputPtr=0; //当前用户输入的PassKey指针
  int ShortPressCount;
  while(PassKeyInputPtr<5)
	  {
		//用户单击+长按，直接结束输入过程
		if(getSideKeyClickAndHoldEvent())
		   {
		   memcpy(PassKeyOut,"USEND",5);
			 GenerateLEDInfoForInput("10",2); //绿色闪烁两次指示用户放弃输入
			 while(getSideKeyClickAndHoldEvent())SideKey_LogicHandler();//等待用户放开按键
			 break;
			 }
		//用户长按，清除本位数据并返回上一位
		if(getSideKeyHoldEvent())
		   {
			 CurrentLEDIndex=2; //绿灯亮起指示重置发生
			 while(getSideKeyHoldEvent())SideKey_LogicHandler(); //循环等待，直到用户松开按键
			 PassKeyInputPtr=PassKeyInputPtr>0?PassKeyInputPtr-1:0; //指针往前移动一位
			 PassKeyOut[PassKeyInputPtr]=0; //清除数据
			 CurrentLEDIndex=0; //让LED熄灭
			 GenerateLEDInfoForInput("30",PassKeyInputPtr+1); //生成黄色闪烁显示当前正在输入哪个字符
			 continue; //打断本次循环
			 }			
		//检测用户是否短按
	  SideKey_LogicHandler();//处理侧按按键事务
		ShortPressCount=getSideKeyShortPressCount(true);//获取短按次数 
		if(ShortPressCount<=0)continue; //用户没有按下按键，等待
		PassKeyOut[PassKeyInputPtr]=ShortPressCount+0x30;
		PassKeyInputPtr++; //存下当前数据，然后指针后移一位
    GenerateLEDInfoForInput("20",ShortPressCount); //生成红色闪烁显示用户输入的数字
		//如果用户还差一个字符则输出提示,否则进入等待用户确认的阶段
		if(PassKeyInputPtr<5)
		   {
			 delay_Second(1); //延迟1秒后
			 GenerateLEDInfoForInput("30",PassKeyInputPtr+1); //生成黄色闪烁显示当前正在输入哪个字符
			 }
		else //等待用户确认
		   {
			 LED_Reset();//复位LED管理器
       memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
			 strncat(LEDModeStr,"2233110D",sizeof(LEDModeStr)-1);  //红黄绿交替闪烁提示用户请按下按键确认
       ExtLEDIndex=&LEDModeStr[0];//传指针过去	
			 while(!getSideKeyHoldEvent()&&getSideKeyShortPressCount(true)<=0)SideKey_LogicHandler(); //循环等待用户按一次按键确认输入完毕	 
			 strncat(LEDModeStr,"E",sizeof(LEDModeStr)-1); //直接加入E打断循环
			 SubmitCustomLEDPattern();//提交编程好的pattern 
			 if(!getSideKeyHoldEvent())continue; //用户没有上输入长按，打断本次循环并重新判断条件	 
			 PassKeyInputPtr=4; //回到上一位
			 PassKeyOut[PassKeyInputPtr]=0; //清除数据
			 GenerateLEDInfoForInput("30",PassKeyInputPtr+1); //生成黄色闪烁显示当前正在输入哪个字符
			 while(getSideKeyHoldEvent())SideKey_LogicHandler(); //循环等待，直到LED序列播放完毕且用户松开按键
			 }
		}		
	}

//处理写入出错的函数
static void DisplayErrorHandler(void)
  {
	strncat(LEDModeStr,"E",sizeof(LEDModeStr)-1); //直接加入E打断循环
	SubmitCustomLEDPattern();//提交编程好的pattern
	CurrentLEDIndex=32; //指示数据库写入失败
	while(CurrentLEDIndex==32){}; //等待显示结束
  NVIC_SystemReset();
	while(1); //重启单片机
	}
	
//检测用户的PassKey输入
void PORResetFactoryDetect(void)
	{
	unsigned int crc,crcinbackup;
	char PassKeyBuf[5]={0};
	FRUBlockUnion FRU;
	//等待用户输入
	CurrentLEDIndex=1;//触发红绿交替闪一下
  while(CurrentLEDIndex==1)//循环等待直到自检序列结束
	  {
		SideKey_LogicHandler(); //读取按键数据
		if(getSideKeyShortPressCount(true)==2)break; //检测到用户双击，进入重置模式
		}	
  if(CurrentLEDIndex==0)return; //自检序列播放结束，用户仍然没有动作，退出
	//检测到重置开始，开始运行
  CurrentLEDIndex=0;//清除序列参数
	if(ReadFRU(&FRU))return;	
	if(!CheckFRUInfoCRC(&FRU))return; //尝试读取FRU，如果读取失败则直接跳过重置过程
  GenerateLEDInfoForInput("10",4);
  delay_Second(1);
	GenerateLEDInfoForInput("30",1); //侧按闪烁4次，停顿1秒后闪烁1次表示开始接收数据
	getSideKeyShortPressCount(true); //再度读取一次清除掉按键数据
	AcceptPassKeyInput(PassKeyBuf); //数据接收过程
	if(!strncmp("USEND",PassKeyBuf,5))return; //用户单击+长按取消数据输入，退出
	else if(!strncmp(FRU.FRUBlock.Data.Data.ResetPassword,PassKeyBuf,5)) //重置数据正确
	  {
		LED_Reset();//复位LED管理器
    memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
		strncat(LEDModeStr,"1010",sizeof(LEDModeStr)-1);  //绿色高频闪烁表示重置开始
		ExtLEDIndex=&LEDModeStr[0];//传指针过去	
		//开始对配置文件进行回滚	
		crc=ActiveConfigurationCRC();	
		CheckConfigurationInROM(Config_Backup,&crcinbackup);
		if(crc!=crcinbackup)	//进行当前配置和备份配置的数据内容匹配
		  {
			//内容不一致，保存当前配置到备份槽位
			if(WriteConfigurationToROM(Config_Backup))DisplayErrorHandler();
			}
		LoadDefaultConf(true);
	  CheckConfigurationInROM(Config_Main,&crcinbackup);
		crc=ActiveConfigurationCRC();	//加载配置后重新计算当前CRC值			
		if(crc==crcinbackup)//进行当前配置和备份配置的数据内容匹配,匹配则退出
		  {
			strncat(LEDModeStr,"E",sizeof(LEDModeStr)-1); //直接加入E打断循环
	    SubmitCustomLEDPattern();//提交编程好的pattern
			return;
	    }		
		//内容不一致，保存覆盖后的出厂配置到ROM
		if(WriteConfigurationToROM(Config_Main))DisplayErrorHandler();
		//操作完成，改为绿灯常亮，3秒后重启单片机
		strncat(LEDModeStr,"E",sizeof(LEDModeStr)-1); //直接加入E打断循环
	  SubmitCustomLEDPattern();//提交编程好的pattern
		CurrentLEDIndex=2;//绿灯常亮
    delay_Second(3); 
		NVIC_SystemReset();
	  while(1); //3秒后重启单片机		
		}
	//数据错误，提示重置失败并继续启动
	else
	  {
		strncat(LEDModeStr,"E",sizeof(LEDModeStr)-1); //直接加入E打断循环
		SubmitCustomLEDPattern();//提交编程好的pattern
  	CurrentLEDIndex=27;
	  while(CurrentLEDIndex==27){}; //等待显示结束	
		}
	}
