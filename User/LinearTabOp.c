#include <math.h>
#include <stdlib.h>
#include "console.h"

//自己实现的四舍五入函数
//输入：浮点数
//输出：该浮点数四舍五入后的整数
int iroundf(float IN)
  {
	int buf;
	float fbuf=IN;
	buf=(int)IN;
	fbuf-=(float)buf;
	fbuf*=10;
	return (fbuf>=5)?buf+1:buf;
	}

/*
检查补偿用线性表的数值域是否合法的函数
输入：待查找的表大小(int)、待查表数值区的浮点数组(浮点指针)
输出:如果阈值表内容异常，返回false，否则返回true
*/	
bool CheckLinearTableValue(int TableSize,float *TableIn)	
  {
	int i;
	//检查传入的线性阈值表的内容
	if(TableIn==NULL||TableSize<=0)return false;//传入的数组为空或大小为0
	for(i=0;i<TableSize;i++)
		{
	  //数值不合法
		if(TableIn[i]>2.00||TableIn<=0)return false;	
		//数值是无效值
		if(TableIn[i]==NAN)return false;
		}
	//检查通过
  return true;
	}	
/*
检查线性阈值表函数
输入：待查找的表大小(int)、待查表阈值的浮点数组(浮点指针)
输出:如果阈值表内容异常，返回false，否则返回true
*/
bool CheckLinearTable(int TableSize,float *TableIn)
  {	
	int i,upper,lower;
  //检查传入的线性阈值表的内容
	if(TableIn==NULL||TableSize<=0)return false;//传入的数组为空或大小为0
  for(i=0;i<TableSize;i++)
     {
		 //检查数值是否含有0和非法值,不允许表中含有数值0
		 if(TableIn[i]==0||TableIn[i]==NAN)return false;
		 //检查其他数值有没有相关性问题
		 upper=i<(TableSize-1)?i+1:(TableSize-1);
		 lower=i>0?i-1:0;//计算要检查的上一个元素和下一个元素的下标
		 /*风扇表包含非法内容
		 60 35=40 20
		 0  ^  2  3(待检查的元素和上一个阈值相等且当前的阈值指针小于最大值-1)*/	 
		 if(TableIn[i]==TableIn[upper]&&i<(TableSize-1))break;
			
		 /*风扇表包含非法内容
		 60=35 40 20
		 0  ^  2  3(待检查的元素和上一个阈值相等且当前的阈值指针大于0)*/	 
		 if(TableIn[lower]==TableIn[i]&&i>0)break;
		 
		 /*风扇表包含非法内容
		 60>35<40 20
		  0  ^  2  3(待检查的元素在线性阈值表中无序排列)*/
		 if(TableIn[lower]>TableIn[i]&&TableIn[i]<TableIn[upper])break;
		 
		  /*
		  60<80>40 20
		  0  ^  2  3(待检查的元素在线性阈值表中无序排列)*/
		 if(TableIn[lower]<TableIn[i]&&TableIn[i]>TableIn[upper])break;
		 }
	if(i<TableSize)return false;//风扇表检查错误,返回false
  //检查通过返回true
	return true;
	}
/*查线性阈值表函数
输入：待查找的表大小(int)、待比较的阈值(浮点)、待查表阈值的浮点数组和待查表的值域数组(浮点指针)输出结果是否有效(指针)
输出：如果表数值成立则输出合法值，否则输出NaN
*/
float QueueLinearTable(int TableSize,float Input,float *Thr,float *Value,bool *IsResultOK)
  {
	int i,upper,lower;
  float max,min,buf;
  //检查传入的线性阈值表的内容
	if(!CheckLinearTable(TableSize,Thr))
	 {
	 #ifdef Internal_Driver_Debug
   UartPost(msg_error,"LinTabLook","Linear Table Lookup module failed due to input table invalid.");
	 #endif
	 *IsResultOK=false; //出现错误
	 return 0;//检查错误,返回0
	 }
  //求表内最大或最小，如果传入的数值超过或等于阈值表的最大或最小，则输出对应值
	*IsResultOK=true;
	max=-335544322;
	min=335544322;
	for(i=0;i<TableSize;i++)
		 {
		 if(max<Thr[i])//求最大值（除了计算出结果以外顺便求下标）
		   {
			 upper=i;
		   max=Thr[i];
			 }
		 if(min>Thr[i])//求最小值（除了计算出结果以外顺便求下标）
		   {
			 lower=i;
			 min=Thr[i];
			 }
		 }
	if(Input>=max)return Value[upper];
	if(Input<=min)return Value[lower];
	//其余情况，正常求数值
	#ifdef Internal_Driver_Debug
  UartPost(Msg_info,"LinTabLook","Linear Table Lookup module called.");
	#endif
	for(i=(TableSize-1);i>=0;i--)if(Input>=Thr[i])break;
  buf=Thr[i+1]-Thr[i];//计算出阈值高点和低点的差值
  max=Value[i+1]-Value[i];//计算出数值点低点和高点的差值
	if(buf==0||max==0)return Value[i]; //如果2个点阈值或者数值差距为0，为了避免除以0则直接返回数值
  min=max/buf;//计算出ΔValue/ΔThreshold
  #ifdef Internal_Driver_Debug
	UartPost(Msg_info,"LinTabLook","BasePoint Threshold/Value(number %d):%.2f/%.2f",i,Thr[i],Value[i]);
	UartPost(Msg_info,"LinTabLook","ΔThreshold:%.5f",buf);
	UartPost(Msg_info,"LinTabLook","ΔValue:%.5f",max);
  UartPost(Msg_info,"LinTabLook","ΔValue/ΔThreshold:%.5f",min);		 
	#endif
  buf=Input-Thr[i];//计算出input相比第一个阈值基准点的Δ
  #ifdef Internal_Driver_Debug
	UartPost(Msg_info,"LinTabLook","Input ΔThreshold Compared to Base Point:%.5f",buf);
	#endif
	buf*=min;//乘以ΔValue/ΔThreshold得到相比第一个数值点的Δ
  #ifdef Internal_Driver_Debug
	UartPost(Msg_info,"LinTabLook","Input ΔValue Compared to Base Point:%.5f",buf);
	#endif
	buf+=Value[i];//加上对应数值点的基准值，得到最终的数值
	return buf;
	}
