/*
该文件负责实现通过光学方式发送摩尔斯电码的函数。
该函数可以处理大小写字母'A-Z,a-z'、数字'0-9'
小数点'.'、括号'('或')'、以及减号和下划线
('_'或'-')。对于不在这些列表内的非法字符，将会
被函数直接忽略。对于发送速度，可以通过调整挡位
设置中的MosTransferStep来控制。该数值越小发送
则越快。
*/

#include "modelogic.h"
#include "cfgfile.h"
#include "delay.h"
#include "PWMDIM.h"
#include "console.h"

//常量，SOS字符串用于发送
const char SOSString[]={"SOS"};

//枚举
typedef enum
{
MorseCode_LoadString,//加载字符串
MorseCode_SendingLetter,//正在发送
MorseCode_LoadNextLetter,//加载下一个字符的等待时间
MorseCode_StringDoneWait//整个字符串发送完毕，等待
}MorseSendStateDef;

//静态变量
static unsigned char MosSeqPtr=0;//摩尔斯序列的指针
static unsigned char MosStrPtr=0;//要被发送的字符串的指针
static char *Text=NULL;//需要送出的字符串指针
static char MorseWaitTimer=0;//静态变量，等待
static MorseSendStateDef MorseSendState=MorseCode_LoadString;

//重置摩尔斯发送器的状态机
void MorseSenderReset(void)
 {
 MosStrPtr=0;//要被发送的字符串的指针
 MosSeqPtr=0;//摩尔斯序列的指针
 Text=NULL;//需要送出的字符串指针
 MorseWaitTimer=0;//静态变量，等待
 MorseSendState=MorseCode_LoadString;
 SysPstatebuf.ToggledFlash=true;//恢复点亮
 }
//负责将ASCII码转换为摩尔斯序列
static const char *GetMorseSequenceFromASCII(char ASCIIIn)
 {
  switch(ASCIIIn)
   {
	 case 'A':return "101110";  //.-
	 case 'B':return "1110101010";  //-...
	 case 'C':return "111010111010";  //-.-.
	 case 'D':return "11101010";  //-.. 
	 case 'E':return "10";  //.
	 case 'F':return "1010111010";  //..-.
	 case 'G':return "1110111010";  //--.
	 case 'H':return "10101010";  //.... 
	 case 'I':return "1010";  //..
	 case 'J':return "10111011101110";  //.---
	 case 'K':return "1110101110";  //-.-
	 case 'L':return "1011101010";  //.-..
	 case 'M':return "11101110";  //--
	 case 'N':return "111010";  //-.
	 case 'O':return "111011101110";  //---
	 case 'P':return "101110111010";  //.--.
   case 'Q':return "11101110101110";  //--.-		 
	 case 'R':return "10111010";  //.-.
	 case 'S':return "101010";  //...
	 case 'T':return "1110";  //-
	 case 'U':return "10101110";  //..-
	 case 'V':return "1010101110";  //...-
	 case 'W':return "1011101110";  //.--
	 case 'X':return "111010101110";  //-..-
	 case 'Y':return "11101011101110";  //-.--
	 case 'Z':return "111011101010";  //--..
	 case '1':return "101110111011101110";  //.----
	 case '2':return "1010111011101110";  //..---
	 case '3':return "10101011101110";  //...--
	 case '4':return "101010101110";  //....-
	 case '5':return "1010101010";  //.....
	 case '6':return "111010101010";  //-....
	 case '7':return "11101110101010";  //--...
	 case '8':return "1110111011101010";  //---..
	 case '9':return "111011101110111010";  //----.
	 case '0':return "11101110111011101110";  //-----
	 case '?':return "1010111011101010";  //..--..
	 case '/':return "111011101010111010";  //--..-.
	 case '(':
	 case ')':return "11101011101110101110";  //-.--.-
	 case '-':
	 case '_':return "1110101010101110";  //-....-
	 case '.':return "101110101110101110";  //.-.-.-
	 }
 return NULL;
 }
//负责遍历一个序列里面是否有不能被摩尔斯发送的非法内容
int CheckForStringCanBeSentViaMorse(char *StringIN)
 {
 int i;
 if(StringIN==NULL)return 0;//字符串啥也没有不执行下面的东西直接返回0
 for(i=0;i<31;i++)//摩尔斯码能送出的最大字符串就是31个字符,全部检查过没问题就OK
	 if(StringIN[i]!='\0'&&GetMorseSequenceFromASCII(StringIN[i])==NULL)return i;
 return -1;//检查通过返回-1
 }
 
//负责将传入的字符转换为摩尔斯电码，当发送完毕后，返回true
static bool MorseLetterTrasnsmit(char Letter)
 {
 const char *Morseq;
 //获得对应的字符串
 if(Letter>0x60&&Letter<0x7B)Letter-=0x20;//小写转大写
 Morseq=GetMorseSequenceFromASCII(Letter);
 //发送出对应的摩尔斯代码
 if(Morseq==NULL) //序列为空,非法内容,跳过
	 {	 
	 SysPstatebuf.ToggledFlash=false;//关闭LED
	 return true;//发送完毕
	 }
 else if(Morseq[MosSeqPtr]=='0'||Morseq[MosSeqPtr]=='1')
   {
	 SysPstatebuf.ToggledFlash=Morseq[MosSeqPtr]=='1'?true:false;//1等于灯亮，0等于灯灭
	 MosSeqPtr++;//指向下一个内容
	 }
 else //其余字符，表明已经发送完毕
   {	 
	 SysPstatebuf.ToggledFlash=false;//关闭LED
	 return true;//发送完毕
	 }
 return false;//发送未完毕，继续
 }
//实现摩尔斯代码发送的状态机
void MorseSenderStateMachine(void)
 {
 ModeConfStr *CurrentMode;
 //摩尔斯码发送的状态机
 switch(MorseSendState)
   {
	 /******************************************************
	 发送前重置指针并令字符串指针指向待发送的字符串完成加载
	 字符串的过程
	 ******************************************************/
	 case MorseCode_LoadString:
	   {
		 CurrentMode=GetCurrentModeConfig();//获取目前挡位信息
		 /*如果是SOS模式，则默认使用固定的'SOS'字符串否则使用
		 对应挡位的摩尔斯发送模式下指定的的字符串。*/
		 if(CurrentMode->Mode==LightMode_SOS)Text=(char *)SOSString;
		 else Text=CurrentMode->MosTransStr;//
		 MosSeqPtr=0;
		 MosStrPtr=0;//重置字符串和单个字母发送序列的指针
		 MorseSendState=MorseCode_SendingLetter;//跳转到发送字母
		 break;
		 }
	 /******************************************************
	 正在发送一个字母，此时系统执行发送字母的程序直到该字母
	 对应的摩尔斯码序列发送完毕。
	 ******************************************************/
	 case MorseCode_SendingLetter:
	   {
		 if(MorseLetterTrasnsmit(Text[MosStrPtr]))//字母发送完成
		   {
			 MosSeqPtr=0;//一个字母发送完毕，重置指针
			 MorseWaitTimer=2;//等待2个周期的时间
			 MosStrPtr++;//指向下一个待发送字符
			 MorseSendState=MorseCode_LoadNextLetter;//跳转到加载下一个字母的等待时间
			 }
		 break;
		 }		
	 /******************************************************
	 一个字母发送完毕的等待时间。与此同时程序会判断加载的字符
	 串是否发送完毕，如果完毕则跳到一串字符发送完毕的等待中。
	 ******************************************************/
	 case MorseCode_LoadNextLetter:
	   {
		 if(Text[MosStrPtr]=='\0')//到末尾的空字符，字符串发送完毕
		   {
			 CurrentMode=GetCurrentModeConfig();//获取目前挡位信息
			 MorseWaitTimer=(int)((float)2/CurrentMode->MosTransferStep);//等待2秒
			 MorseSendState=MorseCode_StringDoneWait;//跳转到发送完毕的等待时间
			 }
		 else if(MorseWaitTimer==0)//还有内容且时间到了
       MorseSendState=MorseCode_SendingLetter;//跳转到发送字母
		 else MorseWaitTimer--;//时间还没到，继续等待
		 break;
		 }
	 /******************************************************
	 加载的字符串已经发送完毕，在这里进行2秒的中场休息时间。
	 休息结束后程序将会返回到加载字符串的阶段再度使能一次
	 完整的发送过程。
	 ******************************************************/
	 case MorseCode_StringDoneWait:
	   {
		 if(MorseWaitTimer==0)MorseSendState=MorseCode_LoadString;//时间到了，跳转到加载字符串
		 else MorseWaitTimer--;//时间还没到，继续等待
		 break;
		 }
	 }
 }
