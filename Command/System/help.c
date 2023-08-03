#include "console.h"
#include <string.h>

//const vars
const char AllResultListed[]={"\r\n\r\n提示:您可使用'\033[40;33m-kw\033[0m'参数指定一个命令描述的查找关键词或者'\033[40;33m-n\033[0m'参数指定待查找的部分或全部命令名称."};
extern const char *ModeRelCommandStr;
extern const char *ModeRelCommandParam;

const char *HelpArgument(int ArgCount)
  {
	if(ArgCount<2)return "提供一个用于描述匹配的关键词";
	return "提供部分或完整的命令名称用于匹配";
	}

//帮助菜单处理函数
void HelpHandler(void)
 {
 int i,j,resultcount=0;
 const char *KeyWord;
 const char *CmdName;
 bool IsNeedtoprint;
 const char *paramptr[20];
 const char *ParamUsage[20];
 int ParamCount,ParamUsageCount;
 int TotalLength,ParamLength;
 //从命令中取出描述关键词或者命令名称
 CmdName=IsParameterExist("23",0,NULL);	 
 KeyWord=IsParameterExist("01",0,NULL);
 //从0开始找
 for(i=0;i<TotalCommandCount;i++)
   {
	 //验证当前命令是否可以执行
	 if(!IsCmdExecutable(i))continue;
	 //判断是否符合打印的要求
	 if(CmdName==NULL&&KeyWord==NULL)IsNeedtoprint=true;//没有输入任何参数	 
	 else if(CmdName!=NULL&&strstr(Commands[i].CommandName,CmdName)!=NULL)IsNeedtoprint=true;//命令名称打印出了指定内容
	 else if(KeyWord!=NULL&&strstr(Commands[i].CommandDesc,KeyWord)!=NULL)IsNeedtoprint=true;//关键词检测到指定内容
	 else IsNeedtoprint=false;
	 //开始打印处理
	 if(IsNeedtoprint)  
		 {
		 //开始转换命令参数
     if(Commands[i].IsModeCommand) //模式相关命令,解码
			 {
			 ParamCount=ParamToConstPtr(paramptr,ModeRelCommandParam,4);
			 ParamCount+=ParamToConstPtr(&paramptr[4],Commands[i].Parameter,16);//对指定的参数字符串进行解码
			 ParamUsageCount=ParamToConstPtr(ParamUsage,ModeRelCommandStr,4); //取出前面的模式参数的提示字符串
			 ParamUsageCount+=ParamToConstPtr(&ParamUsage[4],Commands[i].ParamUsage,16);//取出后面的其余参数
			 }
			else //模式无关命令,正常解码
			 {
			 ParamCount=ParamToConstPtr(paramptr,Commands[i].Parameter,20);
			 ParamUsageCount=ParamToConstPtr(ParamUsage,Commands[i].ParamUsage,20);
			 }
		 //打印各个参数的用法
		 UARTPuts("\r\n\r\n");
		 TotalLength=UartPrintf("---------------- '\033[40;32m%s\033[0m' 命令的使用方法  ----------------",Commands[i].CommandName);
		 TotalLength-=strlen("\033[40;32m\033[0m");
		 UARTPuts("\r\n 命令描述 : ");
     UARTPutsAuto((char *)Commands[i].CommandDesc,(TotalLength-strlen(" 命令描述 : "))-1,TotalLength);		 
		 if(ParamCount==0||ParamUsageCount==0)
			 UartPrintf("\r\n 使用方法 : '\033[40;32m%s\033[0m'",Commands[i].CommandName);
		 else //有多个参数，打印出来
		   {
			 UartPrintf("\r\n 使用方法 : '\033[40;32m%s \033[40;33m[选项1] \033[40;35m<参数1>",Commands[i].CommandName);
			 UARTPuts(" \033[40;33m[选项2(无参数)] [选项3]\r\n \033[40;35m<参数3> \033[0m...'\r\n 参数和选项('*'表示该选项没有参数) :\r\n");
			 for(j=0;j<ParamCount;j++)
				 {
				 if(j>=ParamUsageCount)continue;//参数数量超过允许值，不显示这一行的内容
				 ParamLength=UartPrintf(strlen(ParamUsage[j])<2?"\r\n*":"\r\n");	//根据选项有没有附加参数在前面显示是否有参数
				 ParamLength+=UartPrintf(" \033[40;33m%s\033[40;35m%s\033[0m",paramptr[j],ParamUsage[j]);//显示命令参数和选项
         ParamLength+=UartPrintf(strlen(ParamUsage[j])<2?": ":" : ");	//根据选项有没有附加参数调整显示
				 ParamLength-=strlen("\033[40;33m\033[40;35m\033[0m");//减去着色部分VT100代码带来的额外长度
				 if(Commands[i].QueueHelpProc!=NULL)
				   {
					 if(Commands[i].QueueHelpProc(j)!=NULL)
					   UARTPutsAuto((char *)Commands[i].QueueHelpProc(j),(TotalLength-ParamLength)-1,TotalLength);//执行打印函数打印这一行的内容
					 else UARTPuts("该选项没有描述。");
					 }
				 else UARTPuts("该命令没有描述。");
				 }
			 }
     //找到匹配结果，标记一下
		 resultcount++;
		 }
	 }
 //找到匹配的结果
 if(CmdName==NULL&&KeyWord==NULL)
	 UARTPutsAuto((char *)AllResultListed,TotalLength,TotalLength);
 else if(resultcount)
	 UartPrintf("\r\n\r\n查找完毕,共找到%d条结果.",resultcount);
 else
	 UARTPuts("\r\n\r\n未找到匹配结果,您可尝试缩短或去除关键词限制.");
 ClearRecvBuffer();//清除接收缓冲
 CmdHandle=Command_None;//命令执行完毕		
 }
