#ifndef Console
#define Console

//内部包含
#include "ht32.h"
#include <stdbool.h>
#include "modelogic.h"
#include "FirmwareConf.h"

//串口shell的一些定义
#define BISTBaud 115200 //自检时串口的波特率
#define CmdBufLen 134 //命令输入缓冲区的长度(字节)
#define TxqueueLen 384 //发送缓冲区的长度(字节)
#define RXDMAIdleCount 5 //RX DMA Idle trigger count
#define DefaultTimeOutSec 90  //默认的命令行超时时间(秒)

#ifndef FlashLightOS_Debug_Mode
   #define TotalCommandCount 30  //非debug模式，总命令数30
#else
   #define TotalCommandCount 31  //debug模式，总命令数31
#endif

/* 负责处理外部按键锁存器的使能脚的自动define 不允许修改！*/
#define ExtKey_EN_IOB STRCAT2(GPIO_P,ExtKeyLatch_IOBank)
#define ExtKey_EN_IOG STRCAT2(HT_GPIO,ExtKeyLatch_IOBank)
#define ExtKey_EN_IOP STRCAT2(GPIO_PIN_,ExtKeyLatch_IOPN) 

/*******************************************
以下是串口接收和发送缓冲区的一些枚举变量。
负责全局标记串口命令缓冲区和发送队列的状态。
********************************************/
typedef enum 
{
BUF_Idle,//缓冲区等待接收中
BUF_LoopBackReq,//缓冲区回显需求
BUF_DMA_Full,//缓冲区DMA接收已满
BUF_Force_Stop,//强制打断命令执行
BUF_Tab_Req,//缓冲区接收到Tab命令
BUF_Exe_Req,//缓冲区收到回车需要处理命令
BUF_Del_Req,//缓冲区收到删除请求
BUF_Up_Key,//缓冲区向上按键按下
BUF_Down_Key,//缓冲区向下按键按下
BUF_Left_Key,//缓冲区向左按键按下
BUF_Right_Key//缓冲区向右按键按下
}UARTBUFState; //缓冲区状态

typedef enum
{
Transfer_Idle, //队列空闲
Transfer_Busy  //DMA正在发送队列内容
}UARTTXQueueState;//发送队列状态

/*******************************************
以下是负责向命令执行函数提交任务的枚举变量。
这个枚举变量里面的内容会作为shell前端和命令
执行后端的交流渠道，负责向后端传递需要执行的
命令句柄。
********************************************/
typedef enum
{
Command_None,
Command_help,
Command_login,
Command_logout,
Command_UserMod,
Command_cfgmgmt,
Command_clear,
Command_ver,
Command_modepofcfg,
Command_termcfg,
Command_reboot,
Command_imonadj,
Command_logview,
Command_logclr,
Command_runlogview,
Command_runlogcfg,
Command_logbkup,
Command_modeenacfg,
Command_modeadvcfg,
Command_modecurcfg,
Command_strobecfg,
Command_mostranscfg,
Command_breathecfg,
Command_thermalcfg,
Command_modeporcfg,
Command_battcfg,
Command_customflashcfg,
Command_modeview,
Command_fruedit,
Command_thermaltripcfg,
Command_rampcfg,
#ifdef FlashLightOS_Debug_Mode
Command_eepedit
#endif
}CommandHandle;

/*******************************************
以下是一些负责实现shell中的用户交互操作相关
状态机所需要的枚举变量。例如用户输入的回显，
登录权限节点，用户登录/更改密码，以及对于一些
危险操作需要按Y确认等等。
********************************************/
typedef enum
{
PasswordField, //密码
TextField //文字
}ConsoleOperateFelid;//对于用户当前输入字段属性的标记

typedef enum
{
Log_Perm_Guest,
Log_Perm_Admin,
Log_Perm_Root,
Log_Perm_End,
Log_Perm_PlaceHolder //登陆权限置位符，使得任何人无权限执行该指令（用于被移除的命令）
}Conso1eLoginState;//对于当前用户权限节点的枚举

typedef enum
{
ACC_No_Login,
ACC_RequestVerify,
ACC_Enter_UserName,
ACC_Enter_Pswd,
ACC_Verify_OK,
ACC_Verify_Error,
ACC_ChangePassword,
ACC_ChgPswdOK,
ACC_ChgPswdErr_NoPerm,
ACC_ChgPswdErr_LenErr	
}permVerifyStatus;//密码操作函数状态机的枚举

typedef enum
{
VerifyAccount_Admin,
VerifyAccount_Root,
VerifyAccount_None
}VerifyAccountLevel;//密码操作函数的目标操作账户枚举

typedef enum
{
YConfirm_WaitInput,
YConfirm_Idle,
YConfirm_OK,
YConfirm_Error,
}PressYconfirmStatu;//要求用户输入指定字符以确认某操作的状态机的枚举

typedef enum
{
UserInput_True,
UserInput_False,
UserInput_Nothing
}UserInputTrueFalseDef; //有些函数需要判断用户输入的是true false还是其他东西

typedef enum
{
UserInput_MainCfg,
UserInput_BackupCfg,
UserInput_NoCfg
}userSelectConfigDef;  //检测用户输入了什么配置文件

/*******************************************
以下是在控制器进行自检输出时，自检系统向自检
输出模块提交消息时标记消息类型的枚举变量。
消息类型一共有四种，分别是信息，警告，错误和
致命错误。
********************************************/
typedef enum
{
Msg_info,
Msg_warning,
msg_error,
Msg_critical,
Msg_EndOfPost
}Postmessagelevel;

//串口命令结构体的定义
typedef struct 
  {
	Conso1eLoginState PermNode[4];
	const char *CommandName; 
	const char *CommandDesc; //命令名称和描述符
	const char *Parameter;
	const char *ParamUsage;
	const char *(*QueueHelpProc)(int);
	CommandHandle TargetHandle;
	void (*CtrlCProc)(void);//CtrlC处理的槽函数指针
	const bool AllowInputDuringExecution;
	const bool IsModeCommand;//是否是模式相关命令
	const bool IsDoubleParameterCommand; //是否双参数命令
	}ComamandStringStr;

//负责发送内容的函数(内部使用的函数，切勿在除了命令执行后端的handler以外的任何其他地方引用)
void UARTPuts(char *string);
void UARTPutsAuto(char *string,int FirstlineLen,int OtherLineLen);//具有自动换行功能的函数
void UARTPutd(char *Buf,int Len);
void UARTPutc(char Data,int Len);
void PrintShellIcon(void);
int UartPrintf(char *Format,...);	

//负责检查用户参数输入和权限验证的函数(内部使用的函数，切勿在除了命令执行后端的handler以外的任何其他地方引用)	
bool IsCmdExecutable(int cmdindex);
ReverseTacModeDef getReverseTacModeFromUserInput(char *Param);
char *IsParameterExist(const char *TargetArgcList,int CmdIndex,char *Result);
char CheckIfParamOnlyDigit(char *Param);	
UserInputTrueFalseDef CheckUserInputIsTrue(char *Param);
ModeGrpSelDef CheckUserInputForModeGroup(char *Param);	
LightModeDef CheckUserInputForLightmode(char *Param);
bool GetUserModeNum(int cmdindex,ModeGrpSelDef *UserSelect,int *modenum);//负责获取用户选择了哪个模式组的函数
char GetLEDTypeFromUserInput(char *Param);//根据用户输入解码出FRU LED Code的函数
userSelectConfigDef getCfgTypeFromUserInput(char *Param);//根据用户输入判断使用什么配置文件的判断函数
	
//负责输出特定错误提示信息和提示符的函数(内部使用的函数，切勿在除了命令执行后端的handler以外的任何其他地方引用)		
void UartPrintCommandNoParam(int cmdindex);	
void DisplayReverseTacModeName(char *ParamO,int size,ReverseTacModeDef ModeIn);//显示用户选择了哪个模式组
void DisplayCorrectModeGroup(void);//输出正确的模式组的功能
void DisplayIllegalParam(char *UserInput,int cmdindex,int optionIndex); //显示用户输入的非法参数
void DisplayCorrectMode(void);//显示正确的挡位信息
void DisplayWhichModeSelected(ModeGrpSelDef UserSelect,int modenum);//显示用户的操作
void DisplayCorrectLEDType(void);//当用户的FRU LED类型输入错误时显示应该输入什么

//在命令结束后负责复位接收缓冲的函数(内部使用的函数，切勿在除了命令执行后端的handler以外的任何其他地方引用)
void ClearRecvBuffer(void);

//负责实现命令反序列化和特殊转换的函数(内部使用的函数，切勿在任何其他地方引用)	
int AcoI(char IN);
int Str2Argv(char* argv[],char* cmdbuf,int maxargc);
int ustrlen(char *StringIN);
int ParamToConstPtr(const char **argv,const char *paramin,int maxargc);


//函数(各个功能的处理函数,内部连接使用，切勿在任何其他地方引用)
void InitHistoryBuf(void);
void CommandHistoryWrite(void);
void CommandHistoryUpward(void);
void CommandHistoryDownward(void);
void IdleTimerHandler(void);
void RXDMACallback(void);
void CtrlCHandler(void);
void LoopbackAndDelHandler(void);
void ArrowKeyHandler(void);
void TabHandler(void);
void CmdExecuteHandler(void);
void PasswordVerifyHandler(void);
void CommandHandler(void);
void DMAFullHandler(void);
void YconfirmHandler(void);
void PDMAConfig(u32 TargetBufAddr,u16 Size);
void TXDMAIntCallBack(void);
void TXDMA_StartFirstTransfer(void);
void DMATxInit(void);

//函数(封装好映射到外部使用的函数)
void SelfTestErrorHandler(void);//在出现自检错误的时候进入的tinyshell环境
void ConsoleInit(void); //按照自检配置初步配置console并使能自检debug信息输出
void ConsoleReconfigure(void);  //自检完毕后按照shell的配置重新配置shell和串口
void ShellProcUtilHandler(void); //处理shell中各种实物的handler
void UartPost(Postmessagelevel msglvl,const char *Modules,char *Format,...); //自检debug信息输出，用于系统自检时输出文字消息。

//外部参考变量(供其他函数使用)
extern const char *ThermalsensorString[2];//温度传感字符串
extern const char *LightModeString[9];//模式字符串
extern const char *ModeGrpString[3];//模式组字符串
extern char RXRingBuffer[CmdBufLen];//RX 环形FIFO
extern short QueueRearPTR;
extern permVerifyStatus Verifystat;
extern VerifyAccountLevel TargetAccount;
extern PressYconfirmStatu YConfirmState;
extern const char *YConfirmstr;
extern CommandHandle CmdHandle;//命令句柄
extern char *CmdParamBuf[20];//命令各参数的指针
extern int ArugmentCount;//总参数指针
extern char RXBuffer[CmdBufLen+1];//解码操作缓存
extern short RXLinBufPtr;//当前RX操作的指针
extern short RXLastLinBufPtr;//上一轮RX操作的指针
extern Conso1eLoginState AccountState;//用户登录状态
extern UARTBUFState ConsoleStat;//串口缓冲区状态
extern ConsoleOperateFelid CurCmdField;//当前输入的文字属性(普通文字，密码，Xmodem)
extern const char EncryptedCopyRight[48];//被加密的版权标识
extern const ComamandStringStr Commands[TotalCommandCount];//命令声明结构体

#endif
