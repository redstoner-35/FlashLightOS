#include "console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modelogic.h"

//显示用户将反向战术模式配置为了什么
//输入：字符串指针，打印的字符内存空间 输出：无
void DisplayReverseTacModeName(char *ParamO,int size,ReverseTacModeDef ModeIn)
  {
  int BrightLevel;
  memset(ParamO,0,size);
	switch(ModeIn)
	  {
		case RevTactical_InstantTurbo:strncpy(ParamO,"瞬时极亮",size);return;
	  case RevTactical_NoOperation:strncpy(ParamO,"禁用",size);return;
		case RevTactical_Off:strncpy(ParamO,"关闭手电",size);return;
		case RevTactical_DimTo30:BrightLevel=30;break;
		case RevTactical_DimTo50:BrightLevel=50;break;	
		case RevTactical_DimTo70:BrightLevel=70;break; //调整亮度
		default: return ;
		}
	snprintf(ParamO,size,"将手电亮度设置为%d%%",BrightLevel);
	}

//获取用户选择的反向战术模式的操作
//输入：字符串指针 输出：枚举类型指示用户输入了什么
ReverseTacModeDef getReverseTacModeFromUserInput(char *Param)
  {
	int InputLen,i,result;
	if(Param==NULL)return RevTactical_InputError; //用户输入内容非法
	//开始获取
	InputLen=strlen(Param);
  if(InputLen==5&&!strcmp("turbo",Param))return RevTactical_InstantTurbo; //瞬时极亮
	if(InputLen==3&&!strcmp("off",Param))return RevTactical_Off; //关闭手电
	if(InputLen==7&&!strcmp("disable",Param))return RevTactical_NoOperation;//无动作
	//调光到指定的亮度
	if(InputLen==6&&!strncmp("dim",&Param[0],3))
	  {
		for(i=0;i<InputLen;i++)if(Param[i]=='%')Param[i]=0; //替换百分号
		result=atoi(&Param[3]); //从数字这里开始识别
		switch(result)
		 {
		 case 30:return RevTactical_DimTo30; //30%
		 case 50:return RevTactical_DimTo50; //50%
		 case 70:return RevTactical_DimTo70; //70%
		 }
		}
	//没有解析到合法数值
	return RevTactical_InputError;
	}

//获取用户选择的配置文件类型
//输入：字符串指针 输出：枚举类型指示用户输入了什么
const char *CfgFileTypeStr[]={"Main","Backup"};

userSelectConfigDef getCfgTypeFromUserInput(char *Param)
  {
	int i;
  int InputLen;
	if(Param==NULL)return UserInput_NoCfg; //用户输入内容非法
  InputLen=strlen(Param);
	for(i=0;i<2;i++)//找到匹配的字符串
  if(InputLen==strlen(CfgFileTypeStr[i])&&!strcmp(CfgFileTypeStr[i],Param))switch(i)
	 {
		case 0:return UserInput_MainCfg;
	  case 1:return UserInput_BackupCfg;
	 }
	return UserInput_NoCfg; //啥也没找到
	}

//获取用户选择的FRU LED类型字符串
//输入：字符串指针 输出：FRU的LED类型代码，具体请前往FRU.c内查看
const char *LEDStr[]={"G3V","SBT90R","SBT70G","G6V","SBT70B","SBT90G2"};
const char *LEDInfoStr[]={"任意3V","Luminus SBT-90-R","Luminus SBT-70-G","任意6V","Luminus SBT-70-B","Luminus SBT90.2"};

char GetLEDTypeFromUserInput(char *Param)
  {
	int i;
  int InputLen;
	if(Param==NULL)return 0; //传入指针为空
  InputLen=strlen(Param);
  for(i=0;i<6;i++)//找到匹配的字符串
  if(InputLen==strlen(LEDStr[i])&&!strcmp(LEDStr[i],Param))
	 return i+0x03; //将序列数值加3得到FRU内的LEDCode
	//啥也没找到返回0
	return 0;
	}

//打印可选的LED类型字符串让用户知道应该选什么
void DisplayCorrectLEDType(void) 
 {
 int i;
 UARTPuts("\r\n有效的FRU LED类型参数如下:");
 for(i=0;i<6;i++)UartPrintf("\r\n%s : 使用%sLED.",LEDStr[i],LEDInfoStr[i]);
 }	 
//获取用户输入的模式组和挡位编号
//输入 命令序号,输出选择的模式组数据和模式的指针
extern const char *ModeSelectStr[];

bool GetUserModeNum(int cmdindex,ModeGrpSelDef *UserSelect,int *modenum)
  {
  char *ParamPtr;
	int maxmodenum;
	//识别用户输入的模式组
  ParamPtr=IsParameterExist("01",cmdindex,NULL);  
	*UserSelect=CheckUserInputForModeGroup(ParamPtr);
	if(*UserSelect==ModeGrp_None)
	  {
	  DisplayIllegalParam(ParamPtr,cmdindex,0);//显示用户输入了非法参数
	  DisplayCorrectModeGroup();//显示正确的模式组	  
		ClearRecvBuffer();//清除接收缓冲
    CmdHandle=Command_None;//命令执行完毕	
	  return false;
		}
	//识别用户输入的挡位编号
  if(*UserSelect!=ModeGrp_DoubleClick)
	  {
	  ParamPtr=IsParameterExist("23",cmdindex,NULL);  
    if(!CheckIfParamOnlyDigit(ParamPtr))*modenum=atoi(ParamPtr);
    else *modenum=-1;
    switch(*UserSelect)
	    {
			case ModeGrp_Regular:maxmodenum=8;break;
		  case ModeGrp_Special:maxmodenum=4;break;
		  default:maxmodenum=0;break;
			}
		if(*modenum==-1||*modenum>=maxmodenum)
		  {
			if(*modenum==-1)
				UARTPuts((char *)ModeSelectStr[0]);
			else
				UartPrintf((char *)ModeSelectStr[1],maxmodenum-1);
			ClearRecvBuffer();//清除接收缓冲
      CmdHandle=Command_None;//命令执行完毕	
			return false;
			}
		}
	else *modenum=0;//双击挡位组编号为0
	return true;
	}
//检查用户选择了哪个挡位模式
//输入：字符串  输出:枚举方式分别表示用户输入了什么
const char *LightModeString[9]={"常亮","爆闪","随机爆闪","SOS","摩尔斯发送","呼吸","无极调光","自定义闪","线性变频闪"};
static const char *ModeUsageString[9]=
 {
 "一直保持点亮",
 "按照用户设置的爆闪频率闪烁",
 "按照随机的频率和占空比爆闪",
 "按照用户指定的速度发送SOS求救光学信号",
 "通过摩尔斯电码以光学方式发送用户设置的自定义字符串",
 "用户指定的参数平滑的增高亮度,等待一会后亮度平滑下跌,再度等待并反复循环模拟呼吸效果.",
 "保持点亮,用户可自由设定亮度.",
 "依据用户指定的字符串所指定的闪烁模式闪烁",
 "按照指定的爆闪速度从低频逐步升到高频爆闪然后再降回低频,反复循环"	 
 };

LightModeDef CheckUserInputForLightmode(char *Param)
 {
 int i;
 int InputLen;
 if(Param==NULL)return LightMode_None;
 InputLen=strlen(Param);
 for(i=0;i<9;i++)//找到匹配的字符串
	 if(InputLen==strlen(LightModeString[i])&&!strcmp(LightModeString[i],Param))switch(i)
	  {
		 case 0:return LightMode_On;
		 case 1:return LightMode_Flash;
		 case 2:return LightMode_RandomFlash;
		 case 3:return LightMode_SOS;
		 case 4:return LightMode_MosTrans;
		 case 5:return LightMode_Breath;
		 case 6:return LightMode_Ramp;
		 case 7:return LightMode_CustomFlash;
		 case 8:return LightMode_BreathFlash;
		}
 return LightMode_None;
 }

//当用户输入的挡位模式选项无效时指示用户应该写什么
//输入：无  输出:无
void DisplayCorrectMode(void)
 {
 int i;
 UARTPuts("\r\n有效的挡位模式参数如下:");
 for(i=0;i<9;i++)UartPrintf("\r\n%s : 手电筒的主LED将%s",LightModeString[i],ModeUsageString[i]);
 }	 
//检查用户选择了哪个挡位组
//输入：字符串  输出:枚举方式分别表示用户输入了什么
const char *ModeGrpString[3]={"常规","双击","特殊功能"};
static const char *ModeGrpUsage[3]={"常规功能的8个挡位所在的挡位组","双击特殊功能所在的组","三击的4个特殊功能挡位所在的挡位组"};

ModeGrpSelDef CheckUserInputForModeGroup(char *Param)
 {
 int i;
 int InputLen;
 if(Param==NULL)return ModeGrp_None;
 InputLen=strlen(Param);
 for(i=0;i<3;i++)//找到匹配的字符串
	 if(InputLen==strlen(ModeGrpString[i])&&!strcmp(ModeGrpString[i],Param))switch(i)
	  {
		 case 0:return ModeGrp_Regular;
		 case 1:return ModeGrp_DoubleClick;
		 case 2:return ModeGrp_Special;
		}
 return ModeGrp_None;
 }
//当用户输入的挡位组选项无效时指示用户应该写什么
//输入：无  输出:无
void DisplayCorrectModeGroup(void)
 {
 int i;
 UARTPuts("\r\n有效的挡位组参数如下:");
 for(i=0;i<3;i++)UartPrintf("\r\n%s : %s",ModeGrpString[i],ModeGrpUsage[i]);
 }	

//最后指示用户你更改了哪个挡位的指示输出
//输入:挡位组 挡位编号 输出：无 
void DisplayWhichModeSelected(ModeGrpSelDef UserSelect,int modenum)
 {
 char *ModeName="未知";
 ModeConfStr *SelectedMode=GetSelectedModeConfig(UserSelect,modenum);//获取指定的挡位
 if(SelectedMode!=NULL)ModeName=SelectedMode->ModeName; //选择名称
 if(UserSelect!=ModeGrp_DoubleClick)UartPrintf("\r\n%s挡位组中的第%d挡位",ModeGrpString[(int)UserSelect],modenum);
 else UARTPuts("\r\n双击特殊功能挡位");
 UartPrintf("(挡位名称:%s)",ModeName); //显示挡位名称
 }
 
//检查用户输入的字符串是true,false还是别的东西
//输入：字符串  输出:枚举方式分别表示用户输入了什么
UserInputTrueFalseDef CheckUserInputIsTrue(char *Param)
 {
 if(Param==NULL)return UserInput_Nothing;
 //输入true
 if(!strncasecmp(Param,"True",sizeof("True")))
	 return UserInput_True;
 //输入false
 else if(!strncasecmp(Param,"False",sizeof("False")))
   return UserInput_False;
 //其他内容
 return UserInput_Nothing;
 }
//检查数字的参数输入是否仅存在数字
//输入：字符串  输出:如果字符串仅包含数字则返回0，否则返回1
char CheckIfParamOnlyDigit(char *Param)
 {
 int i=0;
 if(Param==NULL)return 1;
 while(Param[i])
   {
   //找到非数字内容
	 if(Param[i]<'0'||Param[i]>'9')return 1;
	 i++;
	 }
 return 0;
 }
//显示指定命令的无参数传入提示
//输入：命令序号(command.h内定义)  输出:无
void UartPrintCommandNoParam(int cmdindex)
 {
 UARTPuts("\r\n错误:未找到可用的命令选项.请输入'");
 UARTPuts((char *)Commands[cmdindex].CommandName);
 UARTPuts(" -'然后按下'Tab'\r\n按键或使用'help'命令查看该命令的所有可用选项。");
 }
//当用户输入了非法的参数之后显示错误
//输入：命令序号(command.h内定义) 选项序号 用户输入内容的字符串  输出:无 
extern const char *ModeRelCommandParam; 
 
void DisplayIllegalParam(char *UserInput,int cmdindex,int optionIndex)
 {
 const char *paramptr[20];
 int argc;
 	if(Commands[cmdindex].IsModeCommand)
	  {
		argc=ParamToConstPtr(paramptr,ModeRelCommandParam,4);
	  argc+=ParamToConstPtr(&paramptr[4],Commands[cmdindex].Parameter,16); //是模式命令，从前面取参数
		}
 else argc=ParamToConstPtr(paramptr,Commands[cmdindex].Parameter,20);//正常取参数
 if(optionIndex<0||optionIndex>argc-1)return;
 //显示
 UARTPuts("\r\n错误:'");
 if(UserInput!=NULL)UARTPuts(UserInput);				
 UartPrintf("'参数对于'%s'",paramptr[optionIndex]);
 if(Commands[cmdindex].IsDoubleParameterCommand)UartPrintf("或'%s'",paramptr[optionIndex+1]);
 UARTPuts("选项而言是非法的.");
 }
