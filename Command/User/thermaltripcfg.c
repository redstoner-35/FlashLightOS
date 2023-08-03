#include "console.h"
#include "CfgFile.h"
#include "ADC.h"
#include <stdlib.h>
#include <string.h>

//�ַ���
static const char *IllegalTripTemperature="�����õ�%s��ֵ���Ϸ�.�Ϸ�����ֵ��Χ��%d-%d(���϶�).";
static const char *TripTemperatureHasSet="%s��ֵ�ѱ��ɹ�����Ϊ%d���϶�.";

//��������entry
const char *thremaltripcfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
    case 0:
		case 1:return "����LED���ȹػ��ļ����¶�.";
    case 2:
		case 3:return "��������MOS���ȹػ��ļ����¶�";
		}
	return NULL;
	}

void thermaltripcfgHandler(void)
  {
	int buf;
	char *Param;
  bool IsCmdParamOK=false;
	//����LED���ȹػ��¶�
	Param=IsParameterExist("01",22,NULL);
	if(Param!=NULL)
	  {
		IsCmdParamOK=true;
		if(!CheckIfParamOnlyDigit(Param))buf=atoi(Param);
		else buf=-1;
		if(buf>90||buf<60)
			UartPrintf((char *)IllegalTripTemperature,"LED���ȹػ�",60,90);
		else
		  {
			UartPrintf((char *)TripTemperatureHasSet,"LED���ȹػ�",buf);
			CfgFile.LEDThermalTripTemp=(char)buf;
			}
		}	
	//����MOSFET���ȹػ��¶�
	Param=IsParameterExist("23",22,NULL);
	if(Param!=NULL)
	  {
		IsCmdParamOK=true;
		if(!CheckIfParamOnlyDigit(Param))buf=atoi(Param);
		else buf=-1;
		if(buf>110||buf<80)
			UartPrintf((char *)IllegalTripTemperature,"�������ȹػ�",80,110);
		else
		  {
			UartPrintf((char *)TripTemperatureHasSet,"�������ȹػ�",buf);
			CfgFile.MOSFETThermalTripTemp=(char)buf;
			}
		}	
	if(!IsCmdParamOK)UartPrintCommandNoParam(21);//��ʾɶҲû�ҵ�����Ϣ
	//��������
	ClearRecvBuffer();//������ջ���
  CmdHandle=Command_None;//����ִ�����
	}
