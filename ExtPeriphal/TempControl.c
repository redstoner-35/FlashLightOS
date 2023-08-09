#include "cfgfile.h"
#include "runtimelogger.h"
#include "SideKey.h"
#include "ADC.h"
#include <math.h>
#include <string.h>
#include "LEDMgmt.h"

//��������
float fmaxf(float x,float y);
float fminf(float x,float y);
float LEDFilter(float DIN,float *BufIN,int bufsize);

//ȫ�ֱ���
static float err_last_temp = 0.0;	//��һ�¶����
static float integral_temp = 0.0; //���ֺ���¶����ֵ
static bool TempControlEnabled = false; //�Ƿ񼤻��¿�
static bool DoubleClickPressed = false;//���棬��¼˫��+�����Ƿ���
float ThermalFilterBuf[12]={0}; //�¶��˲���
static char RemainingMomtBurstCount=0; //��ʱ�伦Ѫ���ܵ�ʣ�����

//��ʾ�Ѿ�û�м�Ѫ������
void DisplayNoMomentTurbo(void)
  {
  LED_Reset();//��λLED������
  memset(LEDModeStr,0,sizeof(LEDModeStr));//����ڴ�
  strncat(LEDModeStr,"0003020000E",sizeof(LEDModeStr)-1);//���ָʾ����	
	ExtLEDIndex=&LEDModeStr[0];//��ָ���ȥ	
	}

//�Լ�Э�����֮������¶Ȼ���
void FillThermalFilterBuf(ADCOutTypeDef *ADCResult)
  {
	int i;
	float ActualTemp;
	//���ʵ���¶�
  if(ADCResult->NTCState==LED_NTC_OK)//LED�¶Ⱦ�����ȡSPS�¶�
	  { 
		ActualTemp=ADCResult->LEDTemp*CfgFile.LEDThermalWeight/100;
		if(ADCResult->SPSTMONState==SPS_TMON_OK) //�ֳ�����ֵ��Ч��ȡ�ֳ�����ֵ
		  ActualTemp+=ADCResult->SPSTemp*(100-CfgFile.LEDThermalWeight)/100;
		else //����ȡ������־�����ƽ��SPS�¶�
			ActualTemp+=RunLogEntry.Data.DataSec.AverageSPSTemp*(100-CfgFile.LEDThermalWeight)/100;
		}
	else //�¶���ȫʹ������MOS�¶�
	  {
		if(ADCResult->SPSTMONState==SPS_TMON_OK) //�ֳ�����ֵ��Ч��ȡ�ֳ�����ֵ
		  ActualTemp=ADCResult->SPSTemp;
		else //����ȡ������־�����ƽ��SPS�¶�
			ActualTemp=RunLogEntry.Data.DataSec.AverageSPSTemp;
		}
	//д������
	for(i=0;i<12;i++)ThermalFilterBuf[i]=ActualTemp;
	}

//����PID�㷨���¶ȿ���ģ��
float PIDThermalControl(ADCOutTypeDef *ADCResult)
  {
	float ActualTemp,err_temp,AdjustValue;
	bool result;
	ModeConfStr *TargetMode=GetCurrentModeConfig();
	//�����ǰ��λ֧�ּ�Ѫ�����Զ��ػ���ʱ��û������������߼����
	if(TargetMode!=NULL&&TargetMode->MaxMomtTurboCount>0&&TargetMode->PowerOffTimer==0)
	  {
		result=getSideKeyDoubleClickAndHoldEvent();//��ȡ�û��Ƿ�ʹ�ܲ���
		if(DoubleClickPressed!=result)
		  {
			DoubleClickPressed=result;//ͬ������ж��û��Ƿ���
			if(DoubleClickPressed&&TempControlEnabled) //�û�����
			  {
				if(RemainingMomtBurstCount>0)
				  {
					if(RunLogEntry.Data.DataSec.TotalMomtTurboCount<65534)RunLogEntry.Data.DataSec.TotalMomtTurboCount++; //���ܴ���+1
					RemainingMomtBurstCount--;
					TempControlEnabled=false; //���ܻ���ʣ��������������ܽ����¿�ǿ���������
					}
				else //�����Ѿ��ù����޷�����,��ʾ�û�
					DisplayNoMomentTurbo();
				}			
			}
		}
	//���ʵ���¶�
  if(ADCResult->NTCState==LED_NTC_OK)//LED�¶Ⱦ�����ȡSPS�¶�
	  { 
		ActualTemp=ADCResult->LEDTemp*CfgFile.LEDThermalWeight/100;
		if(ADCResult->SPSTMONState==SPS_TMON_OK) //�ֳ�����ֵ��Ч��ȡ�ֳ�����ֵ
		  ActualTemp+=ADCResult->SPSTemp*(100-CfgFile.LEDThermalWeight)/100;
		else //����ȡ������־�����ƽ��SPS�¶�
			ActualTemp+=RunLogEntry.Data.DataSec.AverageSPSTemp*(100-CfgFile.LEDThermalWeight)/100;
		}
	else //�¶���ȫʹ������MOS�¶�
	  {
		if(ADCResult->SPSTMONState==SPS_TMON_OK) //�ֳ�����ֵ��Ч��ȡ�ֳ�����ֵ
		  ActualTemp=ADCResult->SPSTemp;
		else //����ȡ������־�����ƽ��SPS�¶�
			ActualTemp=RunLogEntry.Data.DataSec.AverageSPSTemp;
		}
	ActualTemp=LEDFilter(ActualTemp,ThermalFilterBuf,12); //ʵ���¶ȵ����¶Ƚ����˲����ľ������
	if(TargetMode!=NULL&&ActualTemp<=CfgFile.PIDRelease)   //�¿ص���release�㣬װ�Ѫ����
		RemainingMomtBurstCount=TargetMode->MaxMomtTurboCount;	
	//�ж��¿��Ƿ�ﵽrelease����trigger��
	if(!TempControlEnabled&&ActualTemp>=CfgFile.PIDTriggerTemp)TempControlEnabled=true;
	else if(TempControlEnabled&&ActualTemp<=CfgFile.PIDRelease) TempControlEnabled=false;  //�¿ؽ�����ָ�turbo����
	//�¿ز���Ҫ������ߵ�ǰLED��Ϩ��״̬��ֱ�ӷ���100%
	if(!TempControlEnabled||!SysPstatebuf.ToggledFlash||SysPstatebuf.TargetCurrent==0)return 100;
	//�¿ص�PID����
	err_temp=CfgFile.PIDTargetTemp-ActualTemp; //�������ֵ
	integral_temp+=err_temp;
	if(integral_temp>10)integral_temp=10;
	if(integral_temp<-85)integral_temp=-85; //���ֺͻ����޷�
	AdjustValue=CfgFile.ThermalPIDKp*err_temp+CfgFile.ThermalPIDKi*integral_temp+CfgFile.ThermalPIDKd*(err_temp-err_last_temp); //PID�㷨�������ֵ
	err_last_temp=err_temp;//��¼��һ�����ֵ
	return 90+AdjustValue; //����baseֵ+����value
	}
