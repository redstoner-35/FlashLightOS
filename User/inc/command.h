#ifndef _Command_
#define _Command_

#include "console.h"
#include "cmdentry.h"
#include "CmdArgHelp.h"
#include "FirmwareConf.h"

const char *ModeRelCommandStr=" <模式组名称.>\0 <模式组名称.>\0 <挡位序号.>\0 <挡位序号.>\0\n";
const char *ModeRelCommandParam="-mg\0--mode_group\0-mn\0--mode_number\0\n";

const ComamandStringStr Commands[TotalCommandCount]=
//命令
  {
    {
     {Log_Perm_Guest,Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//0
     "help",
		 "提供各个命令参数的帮助信息.",
     "-kw\0--keyword\0-n\0--name\0\n",
		 " <描述关键词>\0 <描述关键词>\0 <命令名称(部分或全部)>\0 <命令名称(部分或全部)>\0\n",
		 &HelpArgument,
		 Command_help,
		 NULL,
		 false,
		 false
    },
    {
     {Log_Perm_Guest,Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//1
     "login",
		 "输入凭据以获得管理员或超级用户权限.",
     "\n",
		 "\n",
		 NULL,
		 Command_login,
		 &login_ctrlC_handler,
		 true,
		 false
    },
    {
     {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//2
     "logout",
		 "退出登录并回到游客模式.",
     "\n",
		 "\n",
		 NULL,
		 Command_logout,
		 NULL,
		 false,
		 false
    },
    {
     {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//3
     "usrmod",
		 "更改用户凭据(用户名，密码)和系统的主机名",
     "-p\0--password\0-u\0--usrname\0-hn\0--host_name\0\n",
		 " \0 \0 <用户名>\0 <用户名>\0 <主机名>\0 <主机名>\0\n",
		 &usrmodArgument,
		 Command_UserMod,
		 &usrmod_ctrlC_handler,
		 true,
		 false
    },
    {
	   {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//4
		"cfgmgmt",
		 "管理系统中的配置文件(例如保存/加载/校验和恢复出厂设置)并且允许用户从电脑备份和恢复配置文件.",
		"-rf\0--restore_factory\0-s\0--save\0-v\0--verify\0-l\0--load\0-b\0--backup\0-rx\0--restore_xmodem\0\n",
		" \0 \0 <配置名>\0 <配置名>\0 <配置名>\0 <配置名>\0 <配置名>\0 <配置名>\0 <配置名>\0 <配置名>\0 \0 \0\n",
		&cfgmgmtArgument,
		Command_cfgmgmt,
		&cfgmgmt_ctrlc_handler,
		true,
	  false
		},		
		{
		 {Log_Perm_Guest,Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//5
		"clear",
		"清空终端的屏幕输出.",
		"\0\n",
		"\0\n",
		NULL,
		Command_clear,
		NULL,
		false,
    false			
		},
		{
		 {Log_Perm_Guest,Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//6
		"ver",
		 "显示系统固件和硬件的版本信息.",
		"\n",
		"\n",
		NULL,
		Command_ver,
		NULL,
		false,
    false			
		},
		{
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//7
		"modepofcfg",
		"配置指定挡位的自动关机定时器的延时时间或者最大的鸡血次数.",
		"-t\0--time\0-mt\0--max_momt_turbo\0\n",
		" <时间(分钟)>\0 <时间(分钟)>\0 <次数(0-5)>\0 <次数(0-5)>\0\n",
		&modepofcfgArgument,
		Command_modepofcfg,
		NULL,
		false,
		true
		},
		{
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//8
		"misccfg",
		"允许用户配置驱动的通用参数(CLI登录超时时间和波特率,省电睡眠超时时间和侧按开关的配置)",
		"-it\0--idle_timeout\0-b\0--baud_rate\0-st\0--sleep_timeout\0-lled\0--locate_led\0-km\0--key_mode\0\n",
		" <终端超时(秒)>\0 <终端超时(秒)>\0 <波特率(bps)>\0 <波特率(bps)>\0 <睡眠超时(秒)>\0 <睡眠超时(秒)>\0 <true或false>\0 <true或false>\0 <true或false>\0 <true或false>\0\n",
		&termcfgArgument,
		Command_termcfg,
		NULL,
		false,
    false			
		},
		{
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//9
		"reboot",
		"允许系统管理员在需要时手动重启驱动的固件",
		"\0\n",
		"\0\n",
		NULL,
		Command_reboot,
		&reboot_CtrlC_Handler,
		false,
    false			
		},
		{
		#ifndef FlashLightOS_Debug_Mode  
    {Log_Perm_Root,Log_Perm_End},//10
		"nan",
		"无描述",  
		"\0\n",
    "\0\n",
    #else		
		{Log_Perm_Guest,Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//1
		"imonadj",
		"允许厂家配置驱动输出电流测量模块的矫正参数.",  		 
		"-v\0--view\0-g\0--gain\0-thr\0--threshold\0-n\0--node\0\n",
		" \0 \0 <增益(0.5-1.5)>\0 <增益(0.5-1.5)>\0 <阈值电流(A)>\0 <阈值电流(A)>\0 <节点编号>\0 <节点编号>\0\n",
		#endif
		&imonadjArgument,
		Command_imonadj,
		NULL,
		false,
    false			
		},
		{
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//11
     "logview",
		 "允许用户查看驱动的错误日志记录。",
     "-d\0--detail\0\n",
		 " <日志编号(0-19)>\0 <日志编号(0-19)>\0\n",
		 &logviewArgument,
		 Command_logview,
		 NULL,
		 false,
		 false
    },
		{
		 #ifndef Firmware_DIY_Mode
		 {Log_Perm_Root,Log_Perm_End},//12
		 #else
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//12
		 #endif
     "logclr",
		 "允许系统管理员清除驱动的错误日志记录。",
     "\0\n",
		 "\0\n",
		 NULL,
		 Command_logclr,
		 NULL,
		 false,
		 false
    },
    {
		 {Log_Perm_Guest,Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//13
     "runlogview",
		 "允许用户查看驱动的运行日志记录和收集到的各项参数",
     "\0\n",
		 "\0\n",
		 NULL,
		 Command_runlogview,
		 NULL,
		 false,
		 false
    },
    {
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//14
     "runlogcfg",
		 "允许系统管理员清空,保存运行日志到电脑或恢复运行日志并根据需要控制日志记录器的运行",
     "-clr\0--clear\0-en\0--enable\0-s\0--save\0-r\0--restore\0\n",
		 " \0 \0 <true或false>\0 <true或false>\0 \0 \0 \0 \0\n",
		 NULL,
		 Command_runlogcfg,
		 NULL,
		 false,
		 false
    },
		{
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//15
     "logbkup",
		 "允许系统管理员保存错误以及系统自检日志到电脑,或从电脑端下载并恢复错误日志.",
     "-serr\0--save_error_logs\0-r\0--restore\0-spos\0--save_post_logs\0\n",
		 " \0 \0 \0 \0 \0 \0\n",
		 &logbkupArgument,
		 Command_logbkup,
		 NULL,
		 false,
     false			 
		},
		{
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//16
     "modeenacfg",
		 "允许用户根据需要临时禁用和启用手电筒的某个挡位",
     "-men\0--mode_enabled\0\n",
		 " <true或false>\0 <true或false>\0\n",
		 &modeenacfgArgument,
		 Command_modeenacfg,
		 NULL,
		 false,
     true			 
		},
		{
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//17
     "modeadvcfg",
		 "允许用户编辑指定挡位的名称,挡位的运行模式(常亮/爆闪/SOS等)并且设定该挡位是否带记忆以及受温控逻辑影响,还可以设置低电流下的PWM调光频率.",
     "-rm\0--run_mode\0-n\0--name\0-mem\0--is_memory\0-ts\0--is_stepdown\0-df\0-dimming_freq\0\n",
		 " <模式>\0 <模式>\0 <挡位名称>\0 <挡位名称>\0 <true或false>\0 <true或false>\0 <true或false>\0 <true或false>\0 <调光频率(Khz)>\0 <调光频率(Khz)>\0\n",
		 &modeadvcfgArgument,
		 Command_modeadvcfg,
		 NULL,
		 false,
     true			 
		},
		{
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//18
     "modecurcfg",
		 "允许用户编辑指定挡位的额定LED电流/亮度(同时作为无极调光和信标模式的天花板电流)和最小LED电流/亮度(作为无极调光和信标模式的地板电流)",
     "-imin\0--minimum_current\0-imax\0--maximum_current\0\n",
		 " <最小电流(A)>\0 <最小电流(A)>\0 <额定电流(A)>\0 <额定电流(A)>\0\n",
		 &modecurcfgArgument,
		 Command_modecurcfg,
		 NULL,
		 false,
     true			 
		},
		{
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//19
     "strobecfg",
		 "在挡位配置为爆闪、随机爆闪和线性变频闪模式时,允许用户编辑指定挡位的爆闪参数.",
     "-sf\0--strobe_freq\0-minf\0--min_freq\0\0-maxf\0--max_freq\0\n",
		 " <爆闪频率(Hz)>\0 <爆闪频率(Hz)>\0 <下限频率(Hz)>\0 <下限频率(Hz)>\0 <上限频率(Hz)>\0 <上限频率(Hz)>\0\n",
		 &strobecfgArgument,
		 Command_strobecfg,
		 NULL,
		 false,
     true			 
		},
		{
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//20
     "mostranscfg",
		 "在挡位配置为SOS/自定义摩尔斯码发送模式时,允许用户编辑指定挡位的发送速度('.'的长度)以及发送的字符串(自定义模式)",
     "-ts\0--transfer_speed\0-tstr\0--transfer_string\0\n",
		 " <'.'的长度(秒)>\0 <'.'的长度(秒)>\0 <字符串>\0 <字符串>\0\n",
		 &mostranscfgArgument,
		 Command_mostranscfg,
		 NULL,
		 false,
     true			 
		},
	  {
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//21
     "breathecfg",
		 "在挡位配置为呼吸(或称为信标)模式时，用户可对信标模式的时间参数(详细描述请阅读文档)进行编辑.",
     "-trup\0--time_rampup\0\0-trdn\0--time_rampdown\0-thup\0--time_hold_up\0-thdn\0--time_hold_down\0\n",
		 " <爬升时间(秒)>\0 <爬升时间(秒)>\0 <下滑时间(秒)>\0 <下滑时间(秒)>\0 <最大亮度保持时间(秒)>\0 <最大亮度保持时间(秒)>\0 <最小亮度保持时间(秒)>\0 <最小亮度保持时间(秒)>\0\n",
		 &breathecfgArgument,
		 Command_breathecfg,
		 NULL,
		 false,
     true			 
		},
		{
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//22
     "thermalcfg",
		 "允许系统管理员配置驱动的PID温控系统的参数.",
     "\0-w\0--weight\0-et\0--pid_trigger_temp\0-mt\0--pid_maintain_temp\0-st\0--pid_stop_temp\0-kp\0--pid_Kp\0-ki\0--pid_Ki\0-kd\0--pid_Kd\0-v\0--view\0\n",
		 " <权重(5-95%)>\0 <权重(5-95%)>\0 <温度('C)>\0 <温度('C)>\0 <温度('C)>\0 <温度('C)>\0 <温度('C)>\0 <温度('C)>\0 <PID参数(0-100)>\0 <PID参数(0-100)>\0 <PID参数(0-100)>\0 <PID参数(0-100)>\0 <PID参数(0-100)>\0 <PID参数(0-100)>\0 \0 \0\n",
		 &thremalcfgArgument,
		 Command_thermalcfg,
		 NULL,
		 false,
     false		 
		},
		{
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//23
     "modeporcfg",
		 "指定驱动上电自检完毕以及不带记忆的挡位运行结束后回到的默认的挡位,以及设置驱动上电完毕后是否强制锁定.",
     "-ipl\0--is_por_lock\0\n",
		 " <true或false>\0 <true或false>\0\n",
		 &modeporcfgArgument,
		 Command_modeporcfg,
		 NULL,
		 false,
		 true	 
		},
		{
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//24
     "battcfg",
		 "允许系统管理员根据需要查看系统的电池状况信息,并配置库仑计和驱动相关的电池保护参数.",
     "-uvalm\0--undervolt_alarm\0-uvlo\0--undervolt_lock\0-fv\0--full_volt\0-ovp\0--overvolt_protect\0-crln\0--coulomb_relearn\0-sbc\0--set_batt_capacity\0-crst\0--coulomb_reset\0-v\0--view\0-ocp\0--over_current_trip\0\n",
		 " <低压警告电压(V)>\0 <低压警告电压(V)>\0 <低压保护电压(V)>\0 <低压保护电压(V)>\0 <满电电压(V)>\0 <满电电压(V)>\0 <过压保护电压(V)>\0 <过压保护电压(V)>\0 \0 \0 <电池容量(mAH)>\0 <电池容量(mAH)>\0 \0 \0 \0 \0 <过流保护值(A)>\0 <过流保护值(A)>\0\n",
		 &battcfgArgument,
		 Command_battcfg,
		 NULL,
		 false,
     false			 
		},
		{
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//25
     "customflashcfg",
		 "允许用户配置某个挡位的自定义闪模块的工作参数.",
     "-ps\0--pattern_string\0-bf\0--blinking_freq\0\n",
		 " <闪烁模式字符串>\0 <闪烁模式字符串>\0 <序列频率(Hz)>\0 <序列频率(Hz)>\0\n",
		 &customflashcfgArgument,
		 Command_customflashcfg,
		 NULL,
		 false,
     true			 
		},
	  {
		 {Log_Perm_Guest,Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//26
     "modeview",
		 "允许用户查看目前驱动的挡位组设置和某一个特定挡位的详细信息.",
     "\0\n",
		 "\0\n",
		 &modeviewArgument,
		 Command_modeview,
		 NULL,
		 false,
     true			 
		},
		{
		 #ifdef FlashLightOS_Debug_Mode	
		 {Log_Perm_Guest,Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//27
     #else
     {Log_Perm_Root,Log_Perm_End},//27
     #endif
		 "fruedit",
		 #ifdef FlashLightOS_Debug_Mode
		 "允许厂家工程师查看和编辑驱动中的FRU信息并给FRU永久上锁.",
     "-v\0--view\0-sn\0-imax\0-p\0-l\0-ntcb\0-bptr\0-mstr\0-avref\0-ledid\0-majv\0-minv\0-ledn\0\n",
		 " \0 \0 <序列号字符串>\0 <最大电流(A)>\0 <目标LED平台>\0 \0 <NTC B值>\0 <LED基板温度修正值'℃'>\0 <驱动MOS温度修正值'℃'>\0 <驱动ADC参考电压(V)>\0 <LED识别码(0x0-0xFFFE)>\0 <大版本号>\0 <小版本号>\0 <LED名称字符串>\0\n",
		 #else
		 "允许厂家工程师查看FRU信息.",
     "-v\0--view\0\n",
		 " \0 \0\n",		 	 
		 #endif
		 &frueditArgument,
		 Command_fruedit,
		 NULL,
		 false,
     false			 
		},
		{
		 {Log_Perm_Root,Log_Perm_End},//28
     "thermaltripcfg",
		 #ifdef FlashLightOS_Debug_Mode
		 "允许厂家工程师编辑驱动的过热关机温度阈值.",
     "-lmax\0--led_max_temp\0-smax\0--sps_max_temp\0\n",
		 " <温度('C)>\0 <温度('C)>\0 <温度('C)>\0 <温度('C)>\0\n",
		 #else
		 "仅debug可用",
     "\0\n",
		 "\0\n",
		 #endif
		 &thremaltripcfgArgument,
		 Command_thermaltripcfg,
		 NULL,
		 false,
     false			 
		},
		{
		 {Log_Perm_Admin,Log_Perm_Root,Log_Perm_End},//29
     "rampcfg",
		 "配置无极调光的亮度爬升速度,是否记忆以及默认和当前亮度等级.同时可以设置无极调光的方向指示是否启用",
     "-rspd\0--ramp_speed\0-mem\0--is_memory\0-bset\0--bright_set\0-db\0-default_brightness\0-iled\0--info_led\0\n",
		 " <爬升总耗时(s)>\0 <爬升总耗时(s)>\0 <true/false>\0  <true/false>\0 <0-100%>\0 <0-100%>\0 <0-100%>\0 <0-100%>\0 <true/false>\0  <true/false>\0\n",
		 &rampcfgArgument,
		 Command_rampcfg,
		 NULL,
		 false,
     true			 
		}
  };
#endif
