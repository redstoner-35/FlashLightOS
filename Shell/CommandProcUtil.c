#include <string.h>
#include "console.h"
#include "CfgFile.h"
#include <stdarg.h>

//常量
const char *ConstHexID="0X0x";

//支持UTF字符正确计算长度的strlen函数
//输入:字符串 输出:该字符串在终端显示的实际等效ASCII字符长度
int ustrlen(char *StringIN)
  {
	int sptr=0,len=0,IgnoreLen=0;
	//
	while(StringIN[sptr])
	  {		
		if(IgnoreLen>0)//在忽略长度范围内
		 {
		 IgnoreLen--;
		 sptr++;
		 }
		else //根据UTF-8编码处理内容
		 {
		 if((StringIN[sptr]&0x80)==0)//单字节ASCII字符
		   {
		   len++;
		   IgnoreLen=1;
			 }
     else if((StringIN[sptr]&0xE0)==0xC0)//双字节编码			 
		   {
		   len+=2;
		   IgnoreLen=2;			 
			 }
		 else if((StringIN[sptr]&0xF0)==0xE0)//三字节编码
		   {
		   len+=2;//两个ASCII字符长度
		   IgnoreLen=3;			 
			 }
		 else if((StringIN[sptr]&0xF8)==0xF0)//四字节编码
		   {
			 len+=2;//两个ASCII字符长度
		   IgnoreLen=4;			
			 }
		 else len++;//其余情况按照长度累加
		 }
		}
  return len;
	}

//验证命令权限是否满足（Tab用）
bool IsCmdExecutable(int cmdindex)
  {
	int i=0;
	while(i<4&&Commands[cmdindex].PermNode[i]!=Log_Perm_End)//遍历权限节点内的权限检测是否有该命令的权限
	  {
		if(Commands[cmdindex].PermNode[i]==AccountState)return true;
		i++;
		}	
	return false;
	}
//在解码后的序列内寻找指定参数，如果找到，则返回参数指针
char *IsParameterExist(const char *TargetArgcList,int CmdIndex,char *Result)
 {
  int i,argc,targc,j,cmdlen,tparamlen,k;
	const char *paramptr[20];
	const char *Targetparam;
	char *outputptr=NULL;
	//对参数序列进行反序列化然后检查参数是否存在非法内容
	if(Result!=NULL)*Result=0;
	if(CmdIndex>=TotalCommandCount)return NULL;//命令index大于总命令数
	targc=strlen(TargetArgcList);
	argc=ParamToConstPtr(paramptr,Commands[CmdIndex].Parameter,20);//取参数
	for(i=0;i<targc;i++)//遍历需要寻找的指针
	 if(AcoI(TargetArgcList[i])>=argc)return NULL;//需要寻找的指针非法
  //开始寻找
	for(i=1;i<ArugmentCount;i++)
	 {
	 cmdlen=strlen(CmdParamBuf[i]);//计算输入参数的长度
	 for(j=0;j<targc;j++)//遍历一次整个buffer里面的参数	
		 {
		 Targetparam=paramptr[AcoI(TargetArgcList[j])];//计算出目标的位置
		 tparamlen=strlen(Targetparam);//计算出目标参数的长度
		 if(cmdlen==tparamlen&&!strcmp(CmdParamBuf[i],Targetparam))
		    { 
				outputptr=(i+1)<ArugmentCount?CmdParamBuf[i+1]:NULL;//找到结果时返回参数指针
		    if(outputptr!=NULL)for(k=0;k<argc;k++)//如果返回的参数有内容，则开始遍历内容检查是否有参数
					{
					if(strlen(outputptr)==strlen(paramptr[k])&&!strcmp(paramptr[k],outputptr))//返回的参数包含命令内容
					  {
					  outputptr=NULL;
						break; //找到的指针里面是其他的参数，返回NULL
						}
					}
				if(Result!=NULL)*Result=1;
		 	  return outputptr;
				}
		 }
	 }
	if(Result!=NULL)*Result=0;
  return NULL;
 } 
//16进制转10进制转换
//输出：十六进制字符串对应的无符号10进制数值  
//输入：十六进制字符串指针,bool指针用于指向一个提示错误是否发生的变量
unsigned int atoh(char *Text,bool *IsError)
 {
 unsigned int buf,i;
 char temp;
 for(i=0;i<2;i++) 
  if(ConstHexID[i+2]!=Text[i]&&ConstHexID[i]!=Text[i])//开头不是0x
   {
   *IsError=true;
   return 0;
	 }
 i=2;//从第二个字符开始查找
 buf=0;
 while(i<10&&Text[i]!=0)//最多处理8个字符且碰到NULL就结束
  {
	//判断字符类型并且转换内容
  if(Text[i]>='0'&&Text[i]<='9')
	  temp=Text[i]-'0';//数字0-9
	else if(Text[i]>='A'&&Text[i]<='F')
	  temp=Text[i]-'A'+10; //大写字母A-F
	else if(Text[i]>='a'&&Text[i]<='f')
	  temp=Text[i]-'a'+10; //小写字母a-f
	else
	  {
    *IsError=true;
    return 0;	//出现非16进制内容		
		}
	//将结果加入到缓存内
	buf<<=4;
  buf+=temp;//移位之后加入内容里面取
	i++;//指向下一位数据
	} 
 //转换成功
 *IsError=false;
 return buf;
 }
//在终端打印shell的命令提示符
//输出：无  输入：无
void PrintShellIcon(void)
 {
 UARTPuts("\r\n\r\n");
 if(AccountState==Log_Perm_Guest)UARTPuts("Guest@");
 else if(AccountState==Log_Perm_Admin)UARTPuts("Admin@");
 else if(AccountState==Log_Perm_Root)UARTPuts("Root@");	 
 UARTPuts(CfgFile.HostName);
 UARTPuts(">>");
 }
//处理参数字符串并解码为每个参数
//输入：存储参数的字符串指针数组，待解析常量字符串，输出的最大参数个数
//输出：找到的参数个数
int ParamToConstPtr(const char **argv,const char *paramin,int maxargc)
 {
 int argc=0,i=0;
 while(argc<maxargc&&paramin[i]!='\n')	 
   {
	 //跳过所有的\0
	 while(paramin[i]=='\0')i++;
	 if(paramin[i]=='\n')break;//参数结束符
	 //写找到的参数指针
	 argv[argc]=(const char *)&paramin[i];
	 argc++;
	 //跳过所有的非\0字符
	 while(paramin[i]!='\0'&&paramin[i]!='\n')i++;
	 }
 return argc;
 }
//将0-9 ABCDEFGHIJ的ASCII码转换为对应的16进制数值
//输入:ASCII字符 输出:数值
int AcoI(char IN)
 {
 //纯数字
 if(IN>='0'&&IN<='9')return IN&0x0F;
 //大写字母
 else if(IN>='A'&&IN<='J')return 10+(IN-'A');
 //小写字母
 else if(IN>='a'&&IN<='j')return 10+(IN-'a');
 return 0;	 
 }
//清空串口的命令接收缓存准备接收新的数据
//输出：无  输入：无
void ClearRecvBuffer(void)
 {
  int i;
	for(i=0;i<RXLinBufPtr;i++)RXBuffer[i]=0;//清除所有数据
	RXLastLinBufPtr=0;
  RXLinBufPtr=0;//指针设置为0
	ConsoleStat=BUF_Idle;//标记串口可以继续接收	
 } 
//将传入的命令字符串进行反序列化得到字符串指针数组和参数数目
//输入：存储参数的字符串指针数组，待解析字符缓存，输出的最大参数个数
//输出：找到的参数个数
int Str2Argv(char* argv[],char* cmdbuf,int maxargc)
  {
  int argc=0,i=0,LongPara;
  while(cmdbuf[i]!=0&&argc<maxargc)
   {
   //found is argument has "
   LongPara=0;
   while(cmdbuf[i]==0x22)//loop until bypass the "
     {
     i++;
     LongPara=1;
     }
   //write pointer for argument to pointer array
   argv[argc]=(char *)&cmdbuf[i];
   argc++;
   //loop until this argument finished than check does this argument end
   while(cmdbuf[i]!=0x00&&cmdbuf[i]!=((LongPara!=0)?0x22:0x20))i++;
   if(cmdbuf[i]==0x22||cmdbuf[i]==0x20)
     //space or " for split arguments,replace them to NULL
     while(cmdbuf[i]==((LongPara!=0)?0x22:0x20))
      {
      cmdbuf[i]=0;//Replace space with null,for decode func can end properly
      i++;//skip 1 char of space to the next data
      }
   }
  return argc;
  }
