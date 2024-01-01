#ifndef _CmdEntry_
#define _CmdEntry_

//internal handler entry
void LoginHandler(void);
void LogoutHandler(void);
void usrmodhandler(void);
void cfgmgmthandler(void);
void clearHandler(void);
void verHandler(void);
void modepofcfghandler(void);
void HelpHandler(void);
void termcfgHandler(void);
void Reboothandler(void);
void Imonadjhandler(void);
void logviewhandler(void);
void logclrHandler(void);
void runlogviewHandler(void);
void runlogcfgHandler(void);
void logbkupHandler(void);
void ModeEnacfghandler(void);
void modeadvcfgHandler(void);
void modecurcfghandler(void);
void strobecfghandler(void);
void mostranscfghandler(void);
void breathecfghandler(void);
void thermalcfghandler(void);
void modeporcfghandler(void);
void battcfghandler(void);
void customflashcfgHandler(void);
void modeviewhandler(void);
void fruedithandler(void);
void thermaltripcfgHandler(void);
void rampcfghandler(void);
void eepedithandler(void);

//Ctrl+C force stop entry
void cfgmgmt_ctrlc_handler(void);
void usrmod_ctrlC_handler(void);
void login_ctrlC_handler(void);
void reboot_CtrlC_Handler(void);
void eepedit_CtrlC_Handler(void);

#endif
