#include "console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modelogic.h"

//检查用户选择了哪个数据来源的温控曲线
//输入：字符串  输出:枚举方式分别表示用户输入了什么
const char *ThermalsensorString[2]={"LED基板","驱动MOS"};

UserInputThermalSensorDef CheckUserInputForThermalSens(char *Param)
  {
	int i;
 int InputLen;
 if(Param==NULL)return ThermalSens_None;
 InputLen=strlen(Param);
 for(i=0;i<2;i++)//找到匹配的字符串
 if(InputLen==strlen(ThermalsensorString[i])&&!strcmp(ThermalsensorString[i],Param))switch(i)
   {
	 case 0:return ThermalSens_LEDNTC;
	 case 1:return ThermalSens_DriverSPS;
	 }
	return ThermalSens_None;
	}

//当用户输入的传感器选项无效时指示用户该输入什么
//输入：字符串  输出:无
void DisplayCorrectSensor(void)
 {
 int i;
 UARTPuts("\r\n有效的温控表数据来源参数如下:");
 for(i=0;i<2;i++)UartPrintf("\r\n%s : 指定使用%s作为数据来源的温控表进行编辑操作.",ThermalsensorString[i],ThermalsensorString[i]);
 }		

//检查用户选择了哪个挡位模式
//输入：字符串  输出:枚举方式分别表示用户输入了什么
const char *LightModeString[7]={"常亮","爆闪","SOS","摩尔斯发送","呼吸","无极调光","自定义闪"};
static const char *ModeUsageString[7]=
 {
 "一直保持点亮",
 "按照用户设置的爆闪频率闪烁",
 "按照用户指定的速度发送SOS求救光学信号(... --- ...)",
 "通过摩尔斯电码以光学方式发送用户设置的自定义字符串",
 "用户指定的参数平滑的增高亮度,等待一会后然后亮度平滑的下跌,再度等待并以此反复循环,模拟呼吸效果。",
 "保持点亮,用户可自由设定亮度."
 "依据用户指定的字符串所指定的闪烁模式闪烁"	 
 };

LightModeDef CheckUserInputForLightmode(char *Param)
 {
 int i;
 int InputLen;
 if(Param==NULL)return LightMode_None;
 InputLen=strlen(Param);
 for(i=0;i<7;i++)//找到匹配的字符串
	 if(InputLen==strlen(LightModeString[i])&&!strcmp(LightModeString[i],Param))switch(i)
	  {
		 case 0:return LightMode_On;
		 case 1:return LightMode_Flash;
		 case 2:return LightMode_SOS;
		 case 3:return LightMode_MosTrans;
		 case 4:return LightMode_Breath;
		 case 5:return LightMode_Ramp;
		 case 6:return LightMode_CustomFlash;
		}
 return LightMode_None;
 }

//当用户输入的挡位模式选项无效时指示用户应该写什么
//输入：无  输出:无
void DisplayCorrectMode(void)
 {
 int i;
 UARTPuts("\r\n有效的挡位模式参数如下:");
 for(i=0;i<7;i++)UartPrintf("\r\n%s : 手电筒的主LED将%s",LightModeString[i],ModeUsageString[i]);
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
 if(UserSelect!=ModeGrp_DoubleClick)UartPrintf("\r\n%s挡位组中的第%d挡位",ModeGrpString[(int)UserSelect],modenum);
 else UARTPuts("\r\n双击特殊功能挡位");
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
void DisplayIllegalParam(char *UserInput,int cmdindex,int optionIndex)
 {
 const char *paramptr[20];
 int argc;
 argc=ParamToConstPtr(paramptr,Commands[cmdindex].Parameter,20);//取参数
 if(optionIndex<0||optionIndex>argc-1)return;
 //显示
 UARTPuts("\r\n错误:'");
 if(UserInput!=NULL)UARTPuts(UserInput);		 
 UartPrintf("'参数对于'%s'或'%s'选项而言是非法的。",paramptr[optionIndex],paramptr[optionIndex+1]);
 }
