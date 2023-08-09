#include "modelogic.h"
#include "cfgfile.h"
#include "delay.h"

/*
���ļ�����ʵ�����Ա�Ƶ�����Ĺ��ܣ���������£�
ÿ�ζ������ö�ʱ������Ƶ�ʱ仯����˸
*/

#define MaxFreqHoldTime 3
#define MinFreqHoldTime 2

static int TimerReloadValue=999; //��ʱ����װ��ֵ
static int IncreseValue=20;
static char MaxFreqHoldTimer;
static bool IncreseDirection=true; //������Ʊ���

//��������λ����
void LinearFlashReset(void)
 {
 ModeConfStr *CurrentMode=GetCurrentModeConfig();//��ȡĿǰ��λ��Ϣ
 if(CurrentMode!=NULL)//��λ���ò�ΪNULL��ִ������
	 TimerReloadValue=(int)(10000/(CurrentMode->RandStrobeMinFreq*2))-1;//���㶨ʱ����װֵ
 else
	 TimerReloadValue=999; //������ֵ
 IncreseDirection=true;
 IncreseValue=20;
 MaxFreqHoldTimer=0;
 }

//������������
void LinearFlashHandler(void)
 {
 ModeConfStr *CurrentMode; 
 int maxreload,minreload;
 //��ȡ��λ����
 CurrentMode=GetCurrentModeConfig();//��ȡĿǰ��λ��Ϣ
 if(CurrentMode==NULL)return; //�ַ���ΪNULL
 maxreload=(int)(10000/(CurrentMode->RandStrobeMaxFreq*2)); 
 minreload=(int)(10000/(CurrentMode->RandStrobeMinFreq*2)); //������Ͷ�ʱ����װֵ
 //����˸�������׶��𲽼���Ƶ��
 if(!SysPstatebuf.ToggledFlash)
   {
	 if(MaxFreqHoldTimer>0)MaxFreqHoldTimer--; //���Ƶ�ʶ�ʱ������
	 else if(!IncreseDirection)//Ƶ�����¼���(��ʱ����װֵ����)
	   {
		 if(TimerReloadValue==minreload) //�ѵ�Ƶ������
		   {
			 IncreseValue=30;
			 MaxFreqHoldTimer=MinFreqHoldTime*CurrentMode->RandStrobeMinFreq;//��ʱ
		   IncreseDirection=true;
			 }
		 else if(TimerReloadValue>=minreload-IncreseValue) //���붥��ֵ��ʣ��30���ڣ�����ʣ�����ֵ
		   TimerReloadValue+=minreload-TimerReloadValue;
		 else //���кܴ���ֵ��������30
		   TimerReloadValue+=IncreseValue;
		 //�𲽵ݼ�increaseֵ
	   if(IncreseValue>20)IncreseValue-=4;
		 else IncreseValue=20;
		 }
	 else //Ƶ����������(��ʱ����װֵ����)
	   {
		 if(TimerReloadValue==maxreload) //�ѵ�Ƶ������
			 {
			 IncreseValue=30;
			 MaxFreqHoldTimer=MaxFreqHoldTime*CurrentMode->RandStrobeMaxFreq;//��ʱ
		   IncreseDirection=false;
			 }
		 else if(TimerReloadValue<=maxreload+IncreseValue)//���붥��ֵ��ʣ��30���ڣ�����ʣ�����ֵ
		   TimerReloadValue-=TimerReloadValue-maxreload;
		 else //���кܴ���ֵ������-30
		   TimerReloadValue-=IncreseValue;	 
		 //�𲽵���increaseֵ
	   if(IncreseValue<100)IncreseValue+=4;
		 else IncreseValue=100;
		 }
	 //дGPTM��ʱ���ļĴ���
	 HT_GPTM1->CRR=TimerReloadValue-1;
	 }
 //�������
 SysPstatebuf.ToggledFlash=SysPstatebuf.ToggledFlash?false:true;
 }
