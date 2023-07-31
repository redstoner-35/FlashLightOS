#include "console.h"
#include "modelogic.h"
#include "cfgfile.h"
#include "SideKey.h"
#include "LEDMgmt.h"
#include "logger.h"
#include "runtimelogger.h"
#include <string.h>

CurrentModeStr CurMode;//当前模式的结构体

//常量
#define MoresIDCode "DDH-D8B-Red35"
#ifndef DefaultRampMode
const char *ModeConst[5]={"月光","低亮","中亮","中高亮","高亮"};
const float regModeCurrent[4]={5,15,30,60};  //常规挡位电流百分比(100%为满量程)
#endif
const LightModeDef ModeCfgConst[4]={LightMode_Flash,LightMode_SOS,LightMode_Breath,LightMode_MosTrans};
const char *SpecModeConst[4]={"爆闪","SOS","信标","识别码发送"};
const char *ModeGroupName[3]={"regular","double-click","special"};

//变量
char LEDModeStr[64]; //LED模式的字符串
int AutoOffTimer=-1; //定时关机延时器
static bool DCPressstatebuf=false;
static bool DisplayBattBuf=false;


//电池电量的显示
void DisplayBatteryValueHandler(void)
  {
	bool result,IsExecute;
  ModeConfStr *CurrentMode;
	//判断是否可以执行检测
	CurrentMode=GetCurrentModeConfig();//获取当前挡位配置
	if(CurrentMode==NULL)return;//当前挡位为空
	if(SysPstatebuf.Pstate==PState_Standby||SysPstatebuf.Pstate==PState_NonHoldStandBy)IsExecute=true;//手电筒处于待机状态，判断显示
	else if(SysPstatebuf.Pstate==PState_LEDOnNonHold)IsExecute=false; //战术模式，不启用判断逻辑
	else if(CurrentMode->Mode!=LightMode_Ramp)IsExecute=true; //当前挡位模式不是无极调光，可以执行
  else IsExecute=false;		
	//开始显示
	if(!IsExecute)return;//不执行判断
	result=getSideKeyClickAndHoldEvent();//获取单击+长按事件
	if(DisplayBattBuf!=result)
	  {
		DisplayBattBuf=result;//同步结果
	  if(!DisplayBattBuf)return;//用户松开按键结束显示
		if(!RunLogEntry.Data.DataSec.BattUsage.IsCalibrationDone)
			DisplayBattVoltage(); //库仑计未完成校准，显示电池电压
		else
			DisplayBatteryCapacity();//显示电池容量
		}
	}
//挡位自动关机定时器的累减处理
void AutoPowerOffTimerHandler(void)
  {
	if(SysPstatebuf.Pstate!=PState_LEDOn&&SysPstatebuf.Pstate!=PState_LEDOnNonHold)return;//LED未开启不进行计时
	if(AutoOffTimer>0)AutoOffTimer--;//每0.125秒减一次自动关机计时器
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
	  UartPost(Msg_critical,"CfgEEP","Write_EEP::Main.config failed with error code %d",errorcode);
	  #ifndef FlashLightOS_Debug_Mode
	  SelfTestErrorHandler();
		#endif
		}
	else UartPost(Msg_info,"ModeSel","Corrected config has been stored into main config files.");
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
	 CfgFile.RegularMode[i].LEDCurrentLow=0.5;//呼吸模式低电流为0.5
	 CfgFile.RegularMode[i].LEDCurrentHigh=(i==0)?0.5:((FusedMaxCurrent*regModeCurrent[i-1])/(float)100);//编程电流(百分比)
	 CfgFile.RegularMode[i].Mode=LightMode_On;//正常挡位全是常亮档
	 CfgFile.RegularMode[i].StrobeFrequency=10;//默认爆闪频率为10Hz
	 CfgFile.RegularMode[i].RandStrobeMaxFreq=16;
	 CfgFile.RegularMode[i].RandStrobeMinFreq=5;		//随机爆闪频率 
	 CfgFile.RegularMode[i].MosTransferStep=0.5;//摩尔斯码发送的step为0.5秒1阶		 
	 CfgFile.RegularMode[i].RampModeSpeed=2.5; //无极调光模式下电流上升下降的速度(2.5秒1循环)
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
	CfgFile.RegularMode[0].LEDCurrentLow=0.5;//无极调光最低电流为0.5
	CfgFile.RegularMode[0].LEDCurrentHigh=(FusedMaxCurrent*80)/(float)100;//编程电流(百分比)
	CfgFile.RegularMode[0].Mode=LightMode_Ramp;//无极调光模式
	CfgFile.RegularMode[0].StrobeFrequency=10;//默认爆闪频率为10Hz
	CfgFile.RegularMode[0].MosTransferStep=0.5;//摩尔斯码发送的step为0.5秒1阶		 
	CfgFile.RegularMode[0].RampModeSpeed=2.5; //无极调光模式下电流上升下降的速度(多少秒1循环)
	CfgFile.RegularMode[0].MaxCurrentHoldTime=1;
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
   CfgFile.RegularMode[i].Mode=LightMode_On;//正常挡位全是常亮档
	 CfgFile.RegularMode[i].StrobeFrequency=10;//默认爆闪频率为10Hz
	 CfgFile.RegularMode[i].MosTransferStep=0.1;//摩尔斯码发送的step为0.1秒1阶	 
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
	CfgFile.DoubleClickMode.RandStrobeMinFreq=5;		//随机爆闪频率 
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
	CfgFile.DoubleClickMode.IsModeAffectedByStepDown=true;//受温控影响
	CfgFile.DoubleClickMode.LEDCurrentLow=0;//呼吸模式低电流为0
	CfgFile.DoubleClickMode.LEDCurrentHigh=10;//编程电流
  CfgFile.DoubleClickMode.Mode=LightMode_On;//常亮
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
	 strncpy(CfgFile.SpecialMode[i].MosTransStr,"DDH-D8B-Red35",32);//传输的字符串
	 CfgFile.SpecialMode[i].IsModeEnabled=true;//挡位启用
	 strncpy(CfgFile.SpecialMode[i].ModeName,SpecModeConst[i],16);//挡位名称
	 CfgFile.SpecialMode[i].CustomFlashSpeed=10;
	 CfgFile.SpecialMode[i].PowerOffTimer=0;//自动关机定时器禁用
	 memset(CfgFile.SpecialMode[i].CustomFlashStr,0x00,32);
   CfgFile.SpecialMode[i].IsModeHasMemory=false;//不记忆
	 CfgFile.SpecialMode[i].RandStrobeMaxFreq=16;
	 CfgFile.SpecialMode[i].RandStrobeMinFreq=5;		//随机爆闪频率 
	 CfgFile.SpecialMode[i].IsModeAffectedByStepDown=true;//受温控影响
	 CfgFile.SpecialMode[i].LEDCurrentLow=0;//呼吸模式低电流为0A
	 CfgFile.SpecialMode[i].LEDCurrentHigh=i==0?FusedMaxCurrent:FusedMaxCurrent*0.9;//编程电流(除了爆闪100%，其他90%)
   CfgFile.SpecialMode[i].Mode=ModeCfgConst[i];//配置挡位
	 CfgFile.SpecialMode[i].MosTransferStep=0.15;//摩尔斯码发送的step为0.15秒1阶
   CfgFile.SpecialMode[i].StrobeFrequency=16;//默认爆闪频率为16Hz		 
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
		  UartPost(Msg_info,"ModeSel","Default mode has been set to %s mode NO.%d(name:%s)",ModeGroupName[(int)CurMode.ModeGrpSel],i,BootMode->ModeName);
		  return;
			}
	//模式被禁止
  UartPost(msg_error,"ModeSel","Specified default mode was disabled by user and not usable!");
	errorcode=WriteConfigurationToROM(Config_Backup);
	if(errorcode)
	  {
	  UartPost(Msg_critical,"CfgEEP","Write_EEP::Backup.config failed with error code %d",errorcode);
	  #ifndef FlashLightOS_Debug_Mode
		SelfTestErrorHandler();
	  #endif
		}
	else UartPost(Msg_info,"ModeSel","Old Config has been stored in backup config files.");
	if(CfgFile.BootupModeGroup==ModeGrp_Special)//从特殊挡位开始找
	  {
		//搜索每个挡位
		for(i=0;i<4;i++)if(CfgFile.SpecialMode[i].IsModeEnabled)
       {
			 UartPost(Msg_info,"ModeSel","Default mode has been over-written to special mode NO.%d(name:%s)",i+1,CfgFile.SpecialMode[i].ModeName);
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
			UartPost(Msg_info,"ModeSel","Default mode has been over-written to double-click mode(name:%s)",CfgFile.DoubleClickMode.ModeName);
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
			 UartPost(Msg_info,"ModeSel","Default mode has been over-written to regular mode NO.%d(name:%s)",i+1,CfgFile.SpecialMode[i].ModeName);
			 CfgFile.BootupModeNum=i;
			 SaveMainConfig();
			 return;
			 }			
    //找完了都没有
	  UartPost(msg_error,"ModeSel","Seems Mode Setting is corrupted,driver will restore to factory default settings.");
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
//普通模式换挡
static void RegularModeSwitchGear(void)
  {
	int i,j;
	j=CurMode.RegularGrpMode;//获取当前模式值
	for(i=0;i<9;i++)
    {
		j=(j+1)%8;//找下一个模式	
		if(CfgFile.RegularMode[j].IsModeEnabled)//找到可用的模式
			{
			CurMode.RegularGrpMode=j;
			return;
			}
		}
	}
//特殊模式换挡
static void SpecialModeSwitchGear(void)
  {
	int i,j;
	j=CurMode.SpecialGrpMode;//获取当前模式值
	for(i=0;i<5;i++)
    {
		j=(j+1)%4;//找下一个模式	
		if(CfgFile.SpecialMode[j].IsModeEnabled)//找到可用的模式
			{
			CurMode.SpecialGrpMode=j;
			return;
			}
		}
	}
//短按的挡位选择逻辑
void ModeSwitchLogicHandler(void)
  {
	int keycount;
	ModeConfStr *CurrentMode;
  bool DoubleClickHoldDetected;
	DoubleClickHoldDetected=getSideKeyDoubleClickAndHoldEvent();//获取用户是否使能操作
	keycount=getSideKeyShortPressCount(false);//获取短按按键次数
	if(SysPstatebuf.Pstate==PState_Locked||SysPstatebuf.Pstate==PState_Error)return;//处于锁定或者错误状态，此时不处理
	//用户双击+长按,激活以及禁用定时器
	if(DCPressstatebuf!=DoubleClickHoldDetected)
	  {
		DCPressstatebuf=DoubleClickHoldDetected;//同步按键状态
	  if(DCPressstatebuf)//用户执行了操作
	    {
		  CurrentMode=GetCurrentModeConfig();//获取目前挡位
		  if(CurrentMode->PowerOffTimer>0)//有时间设置
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
	//处理换挡逻辑
  if(!keycount||keycount>3)return;	//啥也没按或者短按按键次数按了超过三次不处理
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
			//单击在常规循环挡位组里面选择一个新的挡位
			if(keycount==1)RegularModeSwitchGear();
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
			if(keycount==1)SpecialModeSwitchGear();
			//双击，如果双击挡位启用则进入双击挡位
		  else if(keycount==2&&CfgFile.DoubleClickMode.IsModeEnabled)
				CurMode.ModeGrpSel=ModeGrp_DoubleClick;//进入双击挡位
			//再次三击回到常规挡位组
      else if(keycount==3)EnteredRegular();
			break;
			}
		default: break;
		}
	//操作侧按LED提示当前生成的挡位
	if(SysPstatebuf.Pstate==PState_LEDOn||SysPstatebuf.Pstate==PState_LEDOnNonHold)
	  SetAUXPWR(true); //换挡的时候,如果LED是启动的，那就需要重新使能辅助电源否则会出现信标模式无光的问题
	else
	  SideLED_GenerateModeInfoPattern(); //手电筒处于关机状态，显示挡位数据
	}
//手电筒关闭切换挡位时,生成一组跳档后指示当前挡位的序列让侧按LED显示
void SideLED_GenerateModeInfoPattern(void)
  {
	int flashCount,i,j;
	LED_Reset();//复位LED管理器
  memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
  switch(CurMode.ModeGrpSel) 	
	  {
		case ModeGrp_Regular:flashCount=1;break;//常规挡位
		case ModeGrp_DoubleClick:flashCount=2;break;//双击挡位
		case ModeGrp_Special:flashCount=3;break;//三击挡位
		default: break;
		}
  j=0;		
  for(i=0;i<flashCount;i++)
		{
		strncat(LEDModeStr,"10",sizeof(LEDModeStr)-1);
 	  if(j==1)//追加内容，每2次多一点停顿
		  {
			j=0;
			strncat(LEDModeStr,"0",sizeof(LEDModeStr)-1);
			}
		else j++;
		}		
	strncat(LEDModeStr,"00000",sizeof(LEDModeStr)-1);	
  switch(CurMode.ModeGrpSel) 	
	  {
		case ModeGrp_Regular:flashCount=CurMode.RegularGrpMode+1;break;//常规挡位
		case ModeGrp_DoubleClick:flashCount=0;break;//双击挡位
		case ModeGrp_Special:flashCount=CurMode.SpecialGrpMode+1;break;//三击挡位
		default: break;
		}		
	j=0;
	if(flashCount)for(i=0;i<flashCount;i++)//根据目前选择的挡位组，选择闪烁的数量
		{
		strncat(LEDModeStr,"30",sizeof(LEDModeStr)-1);	
		if(j==1)//追加内容，每2次多一点停顿
		  {
			j=0;
			strncat(LEDModeStr,"0",sizeof(LEDModeStr)-1);
			}
		else j++;
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
		SideLED_GenerateModeInfoPattern();//生成侧按LED序列提示当前在哪个档
	  return;
		}
	ModeOffset=(ModeOffset+1)%CfgFile.BootupModeGroup==ModeGrp_Regular?8:4;
	}while(ModeOffset!=CfgFile.BootupModeNum);
 else return;//否则直接退出
 //在对应挡位组找了一圈都没有使能的挡位，报错		 
 SysPstatebuf.Pstate=PState_Error;
 SysPstatebuf.ErrorCode=Error_ADC_Logic;
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
