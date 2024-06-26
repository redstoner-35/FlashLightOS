#include "console.h"
#include "modelogic.h"
#include "cfgfile.h"
#include "SideKey.h"
#include "LEDMgmt.h"
#include "logger.h"
#include "runtimelogger.h"
#include <string.h>
#include <math.h>

//常量
#define MoresIDCode "Example"
#ifndef DefaultRampMode
const char *ModeConst[5]={"极低亮","低亮","中亮","中高亮","高亮"};
const float regModeCurrent[4]={6,11,22,44};  //常规挡位电流百分比(100%为满量程)
#endif
const LightModeDef ModeCfgConst[4]={LightMode_RandomFlash,LightMode_SOS,LightMode_Breath,LightMode_MosTrans};
const char *SpecModeConst[4]={"随机闪","SOS","信标","识别码发送"};
const char *ModeGroupName[3]={"regular","double-click","special"};
//字符串
const char *ModeHasBeenSet="Default mode has been %s to %s mode NO.%d(name:%s)";
const char *OverwrittenStr="over-written";
const char *ConfigWriteFailed="Write_EEP::%s.cfg failed with error %d";
const char *ConfigWriteDone="%s config has been stored into %s config.";

//变量
int AutoOffTimer=-1; //定时关机延时器
static bool DCPressstatebuf=false;
unsigned char BlankTimer=0; //关闭手电时促使换挡模式消隐的定时器
static bool DisplayBattBuf=false;
extern unsigned char NotifyUserTIM; //指示定时器
CurrentModeStr CurMode;//当前模式的结构体

//电池电量和温度的显示
void DisplayBatteryValueHandler(void)
  {
	bool result,IsExecute;
  ModeConfStr *CurrentMode;
	//判断是否可以执行检测
	CurrentMode=GetCurrentModeConfig();//获取当前挡位配置
	if(CurrentMode==NULL||RunLogEntry.Data.DataSec.IsFlashLightLocked)return;//当前挡位为空或者处于锁定状态，不执行
	if(SysPstatebuf.Pstate==PState_Standby||SysPstatebuf.Pstate==PState_NonHoldStandBy)IsExecute=true;//手电筒处于待机状态，判断显示
	else if(SysPstatebuf.Pstate==PState_LEDOnNonHold||SysPstatebuf.Pstate==PState_Error)IsExecute=false; //战术模式或者遇到错误，不启用判断逻辑
	else if(CurrentMode->Mode!=LightMode_Ramp)IsExecute=true; //当前挡位模式不是无极调光，可以执行
  else IsExecute=false;		
	//开始显示
	if(!IsExecute)return;//不执行判断
	IsExecute=getSideKeyTripleClickAndHoldEvent();
	result=getSideKeyClickAndHoldEvent();//获取单击或三击+长按事件
	if(DisplayBattBuf!=(result||IsExecute))
	  {
		DisplayBattBuf=(result||IsExecute)?true:false;//同步结果
	  if(!DisplayBattBuf)return;//用户松开按键结束显示
		if(IsExecute)DisplayLEDTemp(); //显示温度
	  else if(!RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone)
			DisplayBattVoltage(); //库仑计未完成校准，显示电池电压
		else
			DisplayBatteryCapacity();//显示电池容量
		}
	}
//挡位复位时自动重置定时器
void ResetPowerOffTimerForPoff(void)
  {
	ModeConfStr *CurrentMode;
	if(AutoOffTimer==0)AutoOffTimer=-1;//定时器已经到时间了，关闭定时器
	if(AutoOffTimer==-1)return;//定时器关闭
	//定时器	
	CurrentMode=GetCurrentModeConfig();//获取目前挡位
	if(CurrentMode==NULL)return;//非法数值
	if(CurrentMode->PowerOffTimer>0)
		AutoOffTimer=CurrentMode->PowerOffTimer*480;//换挡的时候定时器重置时间
	else
		AutoOffTimer=-1;//否则定时器关闭
	}

//写主配置文件的函数
static void SaveMainConfig(void)
  {
	int errorcode;
	errorcode=WriteConfigurationToROM(Config_Main);
	if(errorcode)
	  {
	  UartPost(Msg_critical,"CfgEEP",(char *)ConfigWriteFailed,"Main",errorcode);
	  #ifndef FlashLightOS_Init_Mode
	  SelfTestErrorHandler();
		#endif
		}
	else UartPost(Msg_info,"ModeSel",(char *)ConfigWriteDone,"Corrected","Main");
	}
//挡位设置出厂配置
void RestoreFactoryModeCfg(void)
  {
	int i;
	//常规挡位的处理
	#ifndef DefaultRampMode
  for(i=0;i<5;i++)
	 {
	 strncpy(CfgFile.RegularMode[i].MosTransStr,MoresIDCode,32);//传输的字符串
	 CfgFile.RegularMode[i].IsModeEnabled=true;//挡位启用
	 CfgFile.RegularMode[i].CustomFlashSpeed=10;
	 CfgFile.RegularMode[i].PowerOffTimer=0;//自动关机定时器禁用
	 memset(CfgFile.RegularMode[i].CustomFlashStr,0x00,32);
	 strncpy(CfgFile.RegularMode[i].ModeName,ModeConst[i],16);//挡位名称
	 CfgFile.RegularMode[i].IsModeHasMemory=(i<4)?true:false;//高亮档不记忆
	 CfgFile.RegularMode[i].IsModeAffectedByStepDown=i<2?false:true;//除了月光和低中亮，其他挡位都受温控影响
	 CfgFile.RegularMode[i].LEDCurrentLow=0;//呼吸模式低电流为0
	 CfgFile.RegularMode[i].LEDCurrentHigh=(i==0)?MinimumLEDCurrent:((FusedMaxCurrent*regModeCurrent[i-1])/(float)100);//编程电流(百分比)
	 CfgFile.RegularMode[i].Mode=LightMode_On;//正常挡位全是常亮档
	 CfgFile.RegularMode[i].StrobeFrequency=10;//默认爆闪频率为10Hz
	 CfgFile.RegularMode[i].RandStrobeMaxFreq=16;
	 CfgFile.RegularMode[i].ThermalControlOffset=10; //温度向下偏移10摄氏度
	 CfgFile.RegularMode[i].RandStrobeMinFreq=5;		//随机爆闪频率 
	 CfgFile.RegularMode[i].MosTransferStep=0.5;//摩尔斯码发送的step为0.5秒1阶		 
	 CfgFile.RegularMode[i].RampModeSpeed=2.5; //无极调光模式下电流上升下降的速度(2.5秒1循环)
	 CfgFile.RegularMode[i].MaxMomtTurboCount=0;//不支持鸡血模式
	 CfgFile.RegularMode[i].MaxCurrentHoldTime=1;
	 CfgFile.RegularMode[i].MinCurrentHoldTime=10;
	 CfgFile.RegularMode[i].CurrentRampDownTime=2;
	 CfgFile.RegularMode[i].CurrentRampUpTime=2;  //信标模块，电流用2秒到最大电流，保持1秒，然后2秒到最小电流，保持10秒后循环
	 }
  for(i=5;i<8;i++)
	#else
	strncpy(CfgFile.RegularMode[0].MosTransStr,MoresIDCode,32);//传输的字符串
	CfgFile.RegularMode[0].IsModeEnabled=true;//挡位启用
	CfgFile.RegularMode[0].CustomFlashSpeed=10;
	CfgFile.RegularMode[0].PowerOffTimer=0;//自动关机定时器禁用
	memset(CfgFile.RegularMode[0].CustomFlashStr,0x00,32);
	strncpy(CfgFile.RegularMode[0].ModeName,"无极调光",16);//挡位名称
	CfgFile.RegularMode[0].IsModeHasMemory=true;//记忆
	CfgFile.RegularMode[0].IsModeAffectedByStepDown=true;//受温控影响
	CfgFile.RegularMode[0].LEDCurrentLow=MinimumLEDCurrent;//无极调光最低电流为设置值
	CfgFile.RegularMode[0].LEDCurrentHigh=(FusedMaxCurrent*70)/(float)100;//编程电流(百分比)
	CfgFile.RegularMode[0].Mode=LightMode_Ramp;//无极调光模式
	CfgFile.RegularMode[0].StrobeFrequency=10;//默认爆闪频率为10Hz
	CfgFile.RegularMode[0].ThermalControlOffset=10; //温度向下偏移10摄氏度
	CfgFile.RegularMode[0].MosTransferStep=0.5;//摩尔斯码发送的step为0.5秒1阶		 
	CfgFile.RegularMode[0].RampModeSpeed=2.5; //无极调光模式下电流上升下降的速度(多少秒1循环)
	CfgFile.RegularMode[0].MaxCurrentHoldTime=1;
	CfgFile.RegularMode[i].MaxMomtTurboCount=0;//不支持鸡血模式
	CfgFile.RegularMode[0].RandStrobeMaxFreq=16;
	CfgFile.RegularMode[0].RandStrobeMinFreq=5;		//随机爆闪频率 
	CfgFile.RegularMode[0].MinCurrentHoldTime=10;
	CfgFile.RegularMode[0].CurrentRampDownTime=2;
	CfgFile.RegularMode[0].CurrentRampUpTime=2;  //信标模块，电流用2秒到最大电流，保持1秒，然后2秒到最小电流，保持10秒后循环 
	for(i=1;i<8;i++) 
	#endif
	 {
	 strncpy(CfgFile.RegularMode[i].MosTransStr,MoresIDCode,32);//传输的字符串
	 CfgFile.RegularMode[i].IsModeEnabled=false;//挡位禁用
	 CfgFile.RegularMode[i].CustomFlashSpeed=10;
	 CfgFile.RegularMode[i].PowerOffTimer=0;//自动关机定时器禁用
	 memset(CfgFile.RegularMode[i].CustomFlashStr,0x00,32);
	 strncpy(CfgFile.RegularMode[i].ModeName,"保留挡位",16);//挡位名称
	 CfgFile.RegularMode[i].IsModeHasMemory=false;//不记忆
	 CfgFile.RegularMode[i].IsModeAffectedByStepDown=true;//受温控影响
	 CfgFile.RegularMode[i].LEDCurrentLow=0;//呼吸模式低电流为0
	 CfgFile.RegularMode[i].LEDCurrentHigh=10;//编程电流
	 CfgFile.RegularMode[i].MaxMomtTurboCount=0;//不支持鸡血模式
   CfgFile.RegularMode[i].Mode=LightMode_On;//正常挡位全是常亮档
	 CfgFile.RegularMode[i].StrobeFrequency=10;//默认爆闪频率为10Hz
	 CfgFile.RegularMode[i].MosTransferStep=0.1;//摩尔斯码发送的step为0.1秒1阶	 
	 CfgFile.RegularMode[i].ThermalControlOffset=10; //温度向下偏移10摄氏度
	 CfgFile.RegularMode[i].RandStrobeMaxFreq=16;
	 CfgFile.RegularMode[i].RandStrobeMinFreq=5;		//随机爆闪频率 
	 CfgFile.RegularMode[i].MaxCurrentHoldTime=1;
	 CfgFile.RegularMode[i].MinCurrentHoldTime=10;
	 CfgFile.RegularMode[i].CurrentRampDownTime=2;
	 CfgFile.RegularMode[i].CurrentRampUpTime=2;  //信标模块，电流用2秒到最大电流，保持1秒，然后2秒到最小电流，保持10秒后循环
	 }
	//双击挡位的处理（出厂设置是极亮）
	#ifdef EnableTurbo
	strncpy(CfgFile.DoubleClickMode.MosTransStr,MoresIDCode,32);//传输的字符串
	CfgFile.DoubleClickMode.IsModeEnabled=true;//挡位启用
  CfgFile.DoubleClickMode.CustomFlashSpeed=10;
	CfgFile.DoubleClickMode.PowerOffTimer=0;//自动关机定时器禁用
	memset(CfgFile.DoubleClickMode.CustomFlashStr,0x00,32);
	strncpy(CfgFile.DoubleClickMode.ModeName,"极亮",16);//挡位名称
  CfgFile.DoubleClickMode.IsModeHasMemory=false;//不记忆
	CfgFile.DoubleClickMode.IsModeAffectedByStepDown=true;//受温控影响
	CfgFile.DoubleClickMode.LEDCurrentLow=0;//呼吸模式低电流为0
	CfgFile.DoubleClickMode.StrobeFrequency=10;//默认爆闪频率为10Hz
	CfgFile.DoubleClickMode.LEDCurrentHigh=FusedMaxCurrent;//编程电流(配置为驱动允许的最大电流)
  CfgFile.DoubleClickMode.Mode=LightMode_On;//双击极亮也是常亮档
	CfgFile.DoubleClickMode.RandStrobeMaxFreq=16;
	CfgFile.DoubleClickMode.ThermalControlOffset=0; //极亮按照满血温控跑
	CfgFile.DoubleClickMode.RandStrobeMinFreq=5;		//随机爆闪频率
  CfgFile.DoubleClickMode.MaxMomtTurboCount=4;//支持4次鸡血模式	 
	CfgFile.DoubleClickMode.MosTransferStep=0.1;//摩尔斯码发送的step为0.1秒1阶		 
	CfgFile.DoubleClickMode.MaxCurrentHoldTime=1;
	CfgFile.DoubleClickMode.MinCurrentHoldTime=10;
	CfgFile.DoubleClickMode.CurrentRampDownTime=2;
	CfgFile.DoubleClickMode.CurrentRampUpTime=2;  //信标模块，电流用2秒到最大电流，保持1秒，然后2秒到最小电流，保持10秒后循环
	#else 
	//极亮挡位禁用
	strncpy(CfgFile.DoubleClickMode.MosTransStr,MoresIDCode,32);//传输的字符串
	CfgFile.DoubleClickMode.IsModeEnabled=false;//挡位禁用
	CfgFile.DoubleClickMode.CustomFlashSpeed=10;
	CfgFile.DoubleClickMode.PowerOffTimer=0;//自动关机定时器禁用
	memset(CfgFile.DoubleClickMode.CustomFlashStr,0x00,32);
	strncpy(CfgFile.DoubleClickMode.ModeName,"保留挡位",16);//挡位名称
	CfgFile.DoubleClickMode.IsModeHasMemory=false;//不记忆
	CfgFile.DoubleClickMode.MaxMomtTurboCount=0;//不支持鸡血
	CfgFile.DoubleClickMode.IsModeAffectedByStepDown=true;//受温控影响
	CfgFile.DoubleClickMode.LEDCurrentLow=0;//呼吸模式低电流为0
	CfgFile.DoubleClickMode.LEDCurrentHigh=10;//编程电流
  CfgFile.DoubleClickMode.Mode=LightMode_On;//常亮
	CfgFile.DoubleClickMode.ThermalControlOffset=0; //极亮按照满血温控跑
  CfgFile.DoubleClickMode.RandStrobeMaxFreq=16;
	CfgFile.DoubleClickMode.RandStrobeMinFreq=5;		//随机爆闪频率 
	CfgFile.DoubleClickMode.StrobeFrequency=10;//默认爆闪频率为10Hz
  CfgFile.DoubleClickMode.MosTransferStep=0.1;//摩尔斯码发送的step为0.1秒1阶	 
	CfgFile.DoubleClickMode.MaxCurrentHoldTime=1;
	CfgFile.DoubleClickMode.MinCurrentHoldTime=10;
	CfgFile.DoubleClickMode.CurrentRampDownTime=2;
	CfgFile.DoubleClickMode.CurrentRampUpTime=2;  //信标模块，电流用2秒到最大电流，保持1秒，然后2秒到最小电流，保持10秒后循环	 
	#endif 
	//特殊功能组的处理
	for(i=0;i<4;i++)
	 {
	 strncpy(CfgFile.SpecialMode[i].MosTransStr,MoresIDCode,32);//传输的字符串
	 CfgFile.SpecialMode[i].IsModeEnabled=true;//挡位启用
	 strncpy(CfgFile.SpecialMode[i].ModeName,SpecModeConst[i],16);//挡位名称
	 CfgFile.SpecialMode[i].CustomFlashSpeed=10;
	 CfgFile.SpecialMode[i].PowerOffTimer=0;//自动关机定时器禁用
	 memset(CfgFile.SpecialMode[i].CustomFlashStr,0x00,32);
   CfgFile.SpecialMode[i].IsModeHasMemory=false;//不记忆
	 CfgFile.SpecialMode[i].MaxMomtTurboCount=0;//不支持鸡血
	 CfgFile.SpecialMode[i].RandStrobeMaxFreq=15;
	 CfgFile.SpecialMode[i].RandStrobeMinFreq=7;		//随机爆闪频率(7-15Hz)
	 CfgFile.SpecialMode[i].ThermalControlOffset=0; //极亮按照满血温控跑
	 CfgFile.SpecialMode[i].IsModeAffectedByStepDown=true;//受温控影响
	 CfgFile.SpecialMode[i].LEDCurrentLow=0;//呼吸模式低电流为0A
	 CfgFile.SpecialMode[i].LEDCurrentHigh=i==0?FusedMaxCurrent:FusedMaxCurrent*0.9;//编程电流(除了爆闪100%，其他90%)
   CfgFile.SpecialMode[i].Mode=ModeCfgConst[i];//配置挡位
	 CfgFile.SpecialMode[i].MosTransferStep=0.15;//摩尔斯码发送的step为0.15秒1阶
   CfgFile.SpecialMode[i].StrobeFrequency=10;//默认爆闪频率为10Hz		 
   CfgFile.SpecialMode[i].MaxCurrentHoldTime=0;
	 CfgFile.SpecialMode[i].MinCurrentHoldTime=10;
	 CfgFile.SpecialMode[i].CurrentRampDownTime=2;
	 CfgFile.SpecialMode[i].CurrentRampUpTime=2;  //信标模块，电流用3秒到最大电流，保持0秒，然后3秒到最小电流，保持10秒后循环
	 }
	//选择默认启用的挡位组
	CfgFile.BootupModeNum=0;
	CfgFile.BootupModeGroup=ModeGrp_Regular;//使用普通挡位的第一个档
	}
//初始化挡位设置
void ModeSwitchInit(void)
  {
	ModeConfStr *BootMode;//默认启用的结构体
  int i,errorcode;
	memset(LEDModeStr,0,sizeof(LEDModeStr));//清空自定义灯效模块的内存
	CurMode.ModeGrpSel=CfgFile.BootupModeGroup;
  if(CurMode.ModeGrpSel==ModeGrp_Regular)
		CurMode.RegularGrpMode=CfgFile.BootupModeNum;//如果默认启动的挡位是普通挡位则设置为普通挡位模式
	else
		CurMode.RegularGrpMode=0;
	if(CurMode.ModeGrpSel==ModeGrp_Special)
		CurMode.SpecialGrpMode=CfgFile.BootupModeNum;//如果默认启动的挡位是特殊挡位则设置为特殊挡位模式
	else
		CurMode.SpecialGrpMode=0;
	//查找设置的挡位是否被启用
	if(CurMode.ModeGrpSel==ModeGrp_Regular)BootMode=&CfgFile.RegularMode[CfgFile.BootupModeNum];
	else if(CurMode.ModeGrpSel==ModeGrp_Special)BootMode=&CfgFile.SpecialMode[CfgFile.BootupModeNum];
	else BootMode=&CfgFile.DoubleClickMode;
  if(BootMode->IsModeEnabled)//模式已经被启用，退出
	    {
			switch(CurMode.ModeGrpSel) 	
	      {
		    case ModeGrp_Regular:i=CurMode.RegularGrpMode;break;//常规挡位
		    case ModeGrp_DoubleClick:i=0;break;//双击挡位
		    case ModeGrp_Special:i=CurMode.SpecialGrpMode;break;//三击挡位
		    default: break;
				}	
		  UartPost(Msg_info,"ModeSel",(char *)ModeHasBeenSet,"set",ModeGroupName[(int)CurMode.ModeGrpSel],i,BootMode->ModeName);
		  return;
			}
	//模式被禁止
  UartPost(msg_error,"ModeSel","Specified default mode was disabled and not usable!");
	errorcode=WriteConfigurationToROM(Config_Backup);
	if(errorcode)
	  {
	  UartPost(Msg_critical,"CfgEEP",(char *)ConfigWriteFailed,"Backup",errorcode);
	  #ifndef FlashLightOS_Init_Mode
		SelfTestErrorHandler();
	  #endif
		}
	else UartPost(Msg_info,"ModeSel",(char *)ConfigWriteDone,"Illegal","Backup");
	if(CfgFile.BootupModeGroup==ModeGrp_Special)//从特殊挡位开始找
	  {
		//搜索每个挡位
		for(i=0;i<4;i++)if(CfgFile.SpecialMode[i].IsModeEnabled)
       {
			 UartPost(Msg_info,"ModeSel",(char *)ModeHasBeenSet,OverwrittenStr,ModeGroupName[2],i+1,CfgFile.SpecialMode[i].ModeName);
			 CfgFile.BootupModeNum=i;	 
			 SaveMainConfig();//保存修改好的配置文件
			 return;
			 }			
		CfgFile.BootupModeGroup=ModeGrp_DoubleClick;
		}
	if(CfgFile.BootupModeGroup==ModeGrp_DoubleClick)//从双击档开始找
	  {
		if(CfgFile.DoubleClickMode.IsModeEnabled)
		  {
			UartPost(Msg_info,"ModeSel",(char *)ModeHasBeenSet,OverwrittenStr,ModeGroupName[1],0,CfgFile.DoubleClickMode.ModeName);
			CfgFile.BootupModeNum=0;
			SaveMainConfig();//保存修改好的配置文件
			return;
			}	
		else CfgFile.BootupModeGroup=ModeGrp_DoubleClick;//从常规档开始找
		}
	if(CfgFile.BootupModeGroup==ModeGrp_Regular)//从普通挡位组
	  {
		//搜索每个挡位
		for(i=0;i<4;i++)if(CfgFile.RegularMode[i].IsModeEnabled)
       {
			 UartPost(Msg_info,"ModeSel",(char *)ModeHasBeenSet,OverwrittenStr,ModeGroupName[0],i+1,CfgFile.SpecialMode[i].ModeName);
			 CfgFile.BootupModeNum=i;
			 SaveMainConfig();
			 return;
			 }			
    //找完了都没有
	  UartPost(msg_error,"ModeSel","Illegal Mode Settings,reverb to default.");
		RestoreFactoryModeCfg();
		CurMode.ModeGrpSel=ModeGrp_Regular;
		CurMode.SpecialGrpMode=0;
	  CurMode.RegularGrpMode=0;
    SaveMainConfig();//保存修改好的配置文件
		}
	}
//进入三击的检查
static void EnteredTripleClick(void)
  {
	int i,j;
	j=CurMode.SpecialGrpMode;//获取当前模式值
	for(i=0;i<5;i++)
    {
		if(CfgFile.SpecialMode[j].IsModeEnabled)//找到可用的模式
			{
			CurMode.SpecialGrpMode=j;
			CurMode.ModeGrpSel=ModeGrp_Special; //设置挡位信息然后将模式组选择为三击			
			return;
			}
		j=(j+1)%4;//找下一个模式	
		}				
	}
//进入普通模式的检查
static void EnteredRegular(void)
  {
	int i,j;
	j=CurMode.RegularGrpMode;//获取当前模式值
	for(i=0;i<9;i++)
    {
		if(CfgFile.RegularMode[j].IsModeEnabled)//找到可用的模式
			{
			CurMode.RegularGrpMode=j;
			CurMode.ModeGrpSel=ModeGrp_Regular; //设置挡位信息然后将模式组选择常规		
			return;
			}
		j=(j+1)%8;//找下一个模式	
		}				
	}
//普通和特殊模式换挡
static void ModeSwitchGear(void)
  {
	int i,j,maxmodecount;
	ModeConfStr *Mode;
	if(CurMode.ModeGrpSel==ModeGrp_Regular)
	  {
	  maxmodecount=8;
		j=CurMode.RegularGrpMode; //当前模式组为普通模式
		}
	else 
	  {
		maxmodecount=4;
		j=CurMode.SpecialGrpMode;	//当前模式组为特殊模式
		}
	//寻找下一个模式
	for(i=0;i<(maxmodecount+1);i++)
    {
		j=(j+1)%maxmodecount;//找下一个模式	
	  if(CurMode.ModeGrpSel==ModeGrp_Regular)Mode=&CfgFile.RegularMode[j];
	  else Mode=&CfgFile.SpecialMode[j];  //获取模式结构体的指针
		if(Mode->IsModeEnabled) //该挡位已被启用	
		  {
			if(CurMode.ModeGrpSel==ModeGrp_Regular)CurMode.RegularGrpMode=j;//找到可用的普通模式
			else CurMode.SpecialGrpMode=j;//找到可用的特殊模式
		  break;
			}
		}
	}
//在正常和特殊循环档内，判断是否需要实现循环换挡的操作
static bool IsAllowToSwitchGear(void)
  {
	ModeConfStr *Mode;
	int i,maxmodecount,enabledcount;
  //当前用户位于双击功能组里面，不结束换挡操作
	if(CurMode.ModeGrpSel==ModeGrp_DoubleClick)return false;
	//开始获取该循环挡位组内的可用挡位数量
	enabledcount=0;
	if(CurMode.ModeGrpSel==ModeGrp_Regular)maxmodecount=8;
	else maxmodecount=4;
  for(i=0;i<maxmodecount;i++)
	 {
	 if(CurMode.ModeGrpSel==ModeGrp_Regular)Mode=&CfgFile.RegularMode[i];
	 else Mode=&CfgFile.SpecialMode[i];  //获取模式结构体的指针
	 if(Mode->IsModeEnabled)enabledcount++;//有额外可用的挡位，数目+1
	 }
	//返回结果 
	return enabledcount>1?false:true; //当前挡位组数量大于1个挡位，可以换过去
	}
//计算当前挡位实际运行电流的函数
float CalculateCurrentGearCurrent(void)
  {
	RampModeStaticStorDef *RampConfig;
  float CurrentDelta,Ratio;
	char Brightnessicon='0',MaxBrightCmd='0';
	int BrightnessCmdCount,LastBrightnessCmdCount,i;
	ModeConfStr *CurrentMode=GetCurrentModeConfig();//获取目前挡位
	if(CurrentMode==NULL)return -1; //挡位数据为空
	else if(CurrentMode->Mode==LightMode_On)return CurrentMode->LEDCurrentHigh;//常亮档，取出当前挡位的电流
	else if(CurrentMode->Mode==LightMode_Ramp) //无极调光模式，计算实际电流
	   {
		 RampConfig=&RunLogEntry.Data.DataSec.RampModeStor[GetRampConfigIndexFromMode()]; //取无极调光数据所在的位置
		 CurrentDelta=CurrentMode->LEDCurrentHigh-CurrentMode->LEDCurrentLow;//计算差值
		 return CurrentMode->LEDCurrentLow+(CurrentDelta*pow(RampConfig->RampModeConf,GammaCorrectionValue));//计算无极调光的额定输出电流
		 }
	else if(CurrentMode->Mode==LightMode_CustomFlash) //自定义闪模式，查找字符序列中使用最多的亮度命令作为平均值
	   {
		 LastBrightnessCmdCount=0;
		 while(Brightnessicon<='9'||Brightnessicon=='A')
		   {
			 //对字符串进行遍历统计
		   BrightnessCmdCount=0;
			 for(i=0;i<32;i++)if(CurrentMode->CustomFlashStr[i]==Brightnessicon)BrightnessCmdCount++;
			 //判断字符串数目
			 if(BrightnessCmdCount>LastBrightnessCmdCount)//当前的命令字符数大于上一次测试的，更新结果
			   {
				 MaxBrightCmd=Brightnessicon; 	 
			   LastBrightnessCmdCount=BrightnessCmdCount;//存储上一次遍历的结果
				 }					 
			 Brightnessicon++; //继续下一种字符
			 if(Brightnessicon==0x3A)Brightnessicon='A'; //当检索完0-9之后检索A
			 }
		 CurrentDelta=CurrentMode->LEDCurrentHigh-CurrentMode->LEDCurrentLow;//计算差值
		 Ratio=MaxBrightCmd!='A'?(float)(MaxBrightCmd-0x30)*0.1:1; //计算比值	
		 return CurrentMode->LEDCurrentLow+(CurrentDelta*Ratio);
		 }
  //返回其他值
	return -1;
	}
//换挡逻辑的处理实现函数
void ModeSwitchLogicHandler(void)
  {
	int keycount;
  ModeConfStr *CurrentMode;
  float CurrentModeILED,NewModeILED;
  bool DoubleClickHoldDetected,IsNeedToSwitchGear;
	if(RunLogEntry.Data.DataSec.IsFlashLightLocked||SysPstatebuf.Pstate==PState_Error)return;//处于锁定或者错误状态，此时不处理
	//获取按键次数
  DoubleClickHoldDetected=getSideKeyDoubleClickAndHoldEvent();//获取用户是否使能操作
	keycount=getSideKeyShortPressCount(false);//获取短按按键次数
	//用户双击+长按,激活以及禁用定时器
	if(DCPressstatebuf!=DoubleClickHoldDetected)
	  {
		DCPressstatebuf=DoubleClickHoldDetected;//同步按键状态
	  if(DCPressstatebuf)//用户执行了操作
	    {
		  CurrentMode=GetCurrentModeConfig();//获取目前挡位
			if(CurrentMode->MaxMomtTurboCount==0&&CurrentMode->PowerOffTimer>0)//有时间设置且该挡位没有鸡血
		    {
		    if(AutoOffTimer==-1)
		      {
			    DisplayUserWhenTimerOn();
		      AutoOffTimer=CurrentMode->PowerOffTimer*480;//定时器激活
			    }
		    else
		      {		
          DisplayUserWhenTimerOff();				 
		      AutoOffTimer=-1;//定时器禁用
			    }
		    }			
		   }
		 }
	//根据用户的首选项选择是否长按换挡
	if(SysPstatebuf.Pstate==PState_LEDOnNonHold)//战术模式不允许换挡
		IsNeedToSwitchGear=false; 
	else if(CfgFile.IsHoldForPowerOn&&keycount==1)//长按开机，固定短按换挡
		IsNeedToSwitchGear=true; 
	else if(!CfgFile.IsHoldForPowerOn) //单击开机，逻辑比较复杂
	  {
		//开机和战术模式待机状态单击换挡
		if(SysPstatebuf.Pstate==PState_LEDOn||SysPstatebuf.Pstate==PState_NonHoldStandBy)
		  {
		  BlankTimer=0; //关闭消隐计时器
			IsNeedToSwitchGear=(keycount==1)?true:false;
			}
		//关机状态长按换挡
		else
		  {
			if(!getSideKeyHoldEvent()&&BlankTimer==0)BlankTimer=0x80; //按键松开，开始计时
			if(BlankTimer!=0x83)IsNeedToSwitchGear=false; //处于消隐状态，不换档
			else if(!getSideKeyHoldEvent())IsNeedToSwitchGear=false; //没有按下换挡
			else if(ExtLEDIndex!=NULL)IsNeedToSwitchGear=false; //按下换挡但是挡位指示还没结束
			else IsNeedToSwitchGear=true;//启动一次换挡
			}			
		}	  
	else IsNeedToSwitchGear=false;//不需要换挡
	//处理换挡逻辑 
	if(!IsNeedToSwitchGear&&(keycount<2||keycount>3))return;	//短按按键次数小于2次或者按了超过三次不处理
  if(IsNeedToSwitchGear&&IsAllowToSwitchGear())return; 		//如果当前用户操作需要换挡，则根据当前用户位于的挡位组进行判断是否允许换挡
	CurrentMode=GetCurrentModeConfig();//换挡前获取目前挡位的逻辑
	CurrentModeILED=CalculateCurrentGearCurrent(); //获取换挡前挡位的实际运行电流
  //检测到换挡操作时重置特殊功能的定时器和状态机
  ResetPowerOffTimerForPoff();//重置定时器
	ResetBreathStateMachine();//重置呼吸闪状态机
  MorseSenderReset();//重置摩尔斯代码的状态机
	ResetRampMode();//重置无极调光模块
  ResetCustomFlashControl();//复位自定义闪控制
  TimerHasStarted=false;//更换挡位之后重配置定时器
	//挡位组选择逻辑状态机
  switch(CurMode.ModeGrpSel)	
	  {
		case ModeGrp_Regular://常规挡位
		  {
			//在常规循环挡位组里面选择一个新的挡位
			if(IsNeedToSwitchGear)ModeSwitchGear();
			//双击，在有挡位启用的情况下进入双击挡位
			else if(keycount==2&&CfgFile.DoubleClickMode.IsModeEnabled)
				 CurMode.ModeGrpSel=ModeGrp_DoubleClick;//进入双击挡位
	  	//三击。进入特殊功能挡位组
      else if(keycount==3)EnteredTripleClick();
		  break;
			}
		case ModeGrp_DoubleClick://双击挡位
		  {
			//单击或双击回到常规挡位循环组
			if(keycount==1||keycount==2)EnteredRegular();
	  	//三击进入特殊挡位循环组
      else if(keycount==3)EnteredTripleClick();
			break;
			}
		case ModeGrp_Special://三击特殊挡位组
		  {
			//单击在特殊循环挡位组里面选择一个新的挡位
			if(IsNeedToSwitchGear)ModeSwitchGear();
			//双击，如果双击挡位启用则进入双击挡位
		  else if(keycount==2&&CfgFile.DoubleClickMode.IsModeEnabled)
				CurMode.ModeGrpSel=ModeGrp_DoubleClick;//进入双击挡位
			//再次三击回到常规挡位组
      else if(keycount==3)EnteredRegular();
			break;
			}
		default: break;
		}
	RampDimmingDirReConfig();//换挡完成之后重新配置无极调光方向
	//操作侧按LED提示当前生成的挡位
	if(SysPstatebuf.Pstate==PState_LEDOn||SysPstatebuf.Pstate==PState_LEDOnNonHold)
	  { 
		//主LED启动，如果当前挡位的电流变换不大则生成提示信息提示用户
		NewModeILED=CalculateCurrentGearCurrent(); //获取换挡前挡位的实际运行电流
	  if(NewModeILED==-1||CurrentModeILED==-1)return; //不满足执行条件或者电流无效，直接退出
	  CurrentModeILED=fabsf(1-(NewModeILED/CurrentModeILED)); //计算两个挡位之间的电流变化百分比
	  if(CurrentModeILED<=0.20)NotifyUserTIM=4; //挡位电流变化小于20%，调低电流指示用户
    }
	else
	  SideLED_GenerateModeInfoPattern(false); //手电筒处于关机状态，显示挡位数据
	}
//手电筒关闭切换挡位时,生成一组跳档后指示当前挡位的序列让侧按LED显示
void SideLED_GenerateModeInfoPattern(bool IsSwitchOff)
  {
	int flashCount;
	char Strbuf[7]={0};
	LED_Reset();//复位LED管理器
  memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
	if(IsSwitchOff)strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1); //开关关闭时加入额外的延时
  switch(CurMode.ModeGrpSel) 	
	  {
		case ModeGrp_Regular:flashCount=1;break;//常规挡位
		case ModeGrp_DoubleClick:flashCount=2;break;//双击挡位
		case ModeGrp_Special:flashCount=3;break;//三击挡位
		default: return;
		}
  LED_AddStrobe(flashCount,"10");
	strncat(LEDModeStr,"0000",sizeof(LEDModeStr)-1);	
  switch(CurMode.ModeGrpSel) 	
	  {
		case ModeGrp_Regular:flashCount=CurMode.RegularGrpMode+1;break;//常规挡位
		case ModeGrp_DoubleClick:flashCount=0;break;//双击挡位
		case ModeGrp_Special:flashCount=CurMode.SpecialGrpMode+1;break;//三击挡位
		default: break;
		}		
	LED_AddStrobe(flashCount,"30");
	if(!IsSwitchOff&&!CfgFile.IsHoldForPowerOn)		
	  {
	  memset(Strbuf,'0',sizeof(Strbuf)-1);
		strncat(LEDModeStr,Strbuf,sizeof(LEDModeStr)-1); //加上后面的值
		}
	strncat(LEDModeStr,"E",sizeof(LEDModeStr)-1);
	ExtLEDIndex=&LEDModeStr[0];//传指针过去
	}
//当用户除能自动关机定时器时提示用户
void DisplayUserWhenTimerOff(void)
{
 //清空内存
 LED_Reset();//复位LED管理器
 memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
 strncat(LEDModeStr,"D303010DE",sizeof(LEDModeStr)-1);//黄色闪两次绿色一次表示定时器除能
 ExtLEDIndex=&LEDModeStr[0];//传指针过去	
}
//当用户使能自动关机定时器时，指示用户定时器的时间
void DisplayUserWhenTimerOn(void)
 {
 ModeConfStr *CurrentMode;
 int time,decval,gapcnt;
 //获取当前挡位信息
 CurrentMode=GetCurrentModeConfig();
 if(CurrentMode==NULL)return;//当前挡位为空
 //清空内存
 LED_Reset();//复位LED管理器
 memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
 strncat(LEDModeStr,"D30301010D",sizeof(LEDModeStr)-1);//填充头部 
 //生成字符串
 if(CurrentMode->PowerOffTimer>=60)decval=30;//每次闪烁表示30分钟
 else decval=5; //否则每次闪烁表示5分钟
 gapcnt=0;
 time=CurrentMode->PowerOffTimer;//初始化gap计数器
 do
  {
	//附加闪烁次数
	if(CurrentMode->PowerOffTimer>=60)
	  strncat(LEDModeStr,"20",sizeof(LEDModeStr)-1);	//大于60秒时使用红色闪烁
	else
		strncat(LEDModeStr,"30",sizeof(LEDModeStr)-1);	//否则使用黄色
	//每2次闪烁之间插入额外停顿方便用户计数
	if(gapcnt==1)
	  {
		gapcnt=0;
		strncat(LEDModeStr,"0",sizeof(LEDModeStr)-1);
		}
	else gapcnt++;
	//处理完一轮，减去时间
	time-=decval;
	}	 
 while(time>0);//如果时间大于0则继续	 
 strncat(LEDModeStr,"DE",sizeof(LEDModeStr)-1);//结束闪烁前加多1秒延时
 ExtLEDIndex=&LEDModeStr[0];//传指针过去	
 }	
	
//当手电筒关闭时，检查当前挡位是否带记忆，如果不带则调回去默认档
void ModeNoMemoryRollBackHandler(void)
 {
 ModeConfStr *CurrentMode;
 int ModeOffset;
 //获取当前挡位信息
 CurrentMode=GetCurrentModeConfig();
 if(CurrentMode==NULL)return;//当前挡位为空
 //执行跳档操作(循环找到使能的档)
 ModeOffset=CfgFile.BootupModeNum;//记录下目标要跳过去的挡位
 if(!CurrentMode->IsModeHasMemory)do
  {
	CurMode.ModeGrpSel=CfgFile.BootupModeGroup;//选择挡位组
  switch(CfgFile.BootupModeGroup)//将对应的挡位号写过去
    {
	  case ModeGrp_Regular:CurMode.RegularGrpMode=ModeOffset;//常规挡位组
		case ModeGrp_DoubleClick:break;//双击挡位,什么也不做
	  case ModeGrp_Special:CurMode.SpecialGrpMode=ModeOffset;//特殊功能挡位
	  default: break;
		}
	//获取跳档之后的结果，如果挡位已经使能则退出查找，否则令挡位组+1继续找
  CurrentMode=GetCurrentModeConfig();
	if(CurrentMode->IsModeEnabled)
  	{
		SideLED_GenerateModeInfoPattern(true);//生成侧按LED序列提示当前在哪个档
	  return;
		}
	ModeOffset=(ModeOffset+1)%CfgFile.BootupModeGroup==ModeGrp_Regular?8:4;
	}while(ModeOffset!=CfgFile.BootupModeNum);
 else return;//否则直接退出
 //在对应挡位组找了一圈都没有使能的挡位，报错		 
 SysPstatebuf.Pstate=PState_Error;
 SysPstatebuf.ErrorCode=Error_Mode_Logic;
 CollectLoginfo("挡位变换",NULL);
 }
//从结构体获取当前的挡位
ModeConfStr *GetCurrentModeConfig(void)
 {
	//获取挡位信息
 switch(CurMode.ModeGrpSel)
  {
	case ModeGrp_Regular:return &CfgFile.RegularMode[CurMode.RegularGrpMode];//常规挡位组
	case ModeGrp_DoubleClick:return &CfgFile.DoubleClickMode;//双击功能
	case ModeGrp_Special:return &CfgFile.SpecialMode[CurMode.SpecialGrpMode];//特殊功能挡位
	default: break;
	}
 return NULL;
 }
//根据传入的挡位设置得到对应挡位的结构体指针
ModeConfStr *GetSelectedModeConfig(ModeGrpSelDef ModeGrpSel,int index)
 {
 	//获取挡位信息
 switch(ModeGrpSel)
  {
	case ModeGrp_Regular:return &CfgFile.RegularMode[index];//常规挡位组
	case ModeGrp_DoubleClick:return &CfgFile.DoubleClickMode;//双击功能
	case ModeGrp_Special:return &CfgFile.SpecialMode[index];//特殊功能挡位
	default: break;
	}
 return NULL;
 }
