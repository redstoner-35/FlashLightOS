#include "console.h"
#include "CfgFile.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//参数帮助entry
const char *thremalcfgArgument(int ArgCount)
  {
	switch(ArgCount)
	  {
    case 0:
		case 1:return "指定要编辑的温控表所属的传感器名称(LED基板或者驱动MOS管)";
		case 2:
		case 3:return "指定要编辑的温控表阈值点的编号(0-4)";
		case 4:
		case 5:return "指定要编辑的温控表阈值点中的阈值温度(单位为摄氏度)";
		case 6:
		case 7:return "指定要编辑的温控表阈值点中的挡位输出百分比(5-100%)";
	  case 8:
		case 9:return "设置LED基板和驱动MOS管对降档幅度影响的权重比值";
		case 10:
		case 11:return "查看指定的温控表内的阈值温度和输出百分比数值";
		}
	return NULL;
	}
//显示温度表参数
static void DisplayTempTable(float *Thr,float *Value)
  {
	int i;
	UARTPuts("\r\n");
	UARTPutc('-',41);
	UARTPuts("\r\n|  编号  | 阈值点(摄氏度) | 挡位输出(%) |\r\n");
	for(i=0;i<5;i++)
		{
		UartPrintf("\r\n|  NO.%d  |    ",i);
		if(Thr[i]>99)UartPrintf("%.2f'C    |",Thr[i]);
		else UartPrintf("%.2f'C     |",Thr[i]);
		if(Value[i]>99)UartPrintf("  %.2f%%    |",Value[i]);
		else if(Value[i]>9)UartPrintf("   %.2f%%    |",Value[i]);
		else UartPrintf("   %.2f%%     |",Value[i]);
		}
	UARTPuts("\r\n");
	UARTPutc('-',41);
	UARTPuts("\r\n");
	}
//显示阈值点更新	
static void DisplayTableUpdate(float *Thr,float *Value,bool IsThr,UserInputThermalSensorDef Thermal)
  {
	UartPrintf("\r\n%s传感器所对应温度表的%s参数已经成功更新.",ThermalsensorString[(int)Thermal-1],IsThr?"阈值":"挡位输出百分比");
	UARTPuts("新的温度表参数如下所示:");
	DisplayTempTable(Thr,Value);
	}
//主处理函数
bool CheckLinearTable(int TableSize,float *TableIn);//线性表查表
	
void thermalcfghandler(void)
  {
	char *Param;
	UserInputThermalSensorDef Sensor;
	char ParamOK;
	int buf;
	float newvalue,oldvalue,min,max;
	bool IsCmdParamOK=false;
	float *Threshold;
	float *Value;
  //识别最后的权重控制的参数
	Param=IsParameterExist("89",22,NULL);
  if(Param!=NULL)
	  {
		IsCmdParamOK=true;
		if(!CheckIfParamOnlyDigit(Param))buf=atoi(Param);
		else buf=-1;
		if(buf<5||buf>95)
		  {
			DisplayIllegalParam(Param,22,8);//显示用户输入了非法参数
			UARTPuts("\r\n您应当将指定一个5-95(单位%)的数值作为LED降档曲线的权重值.");
			}
		else
		  {
			CfgFile.LEDThermalStepWeight=buf;
			UartPrintf("\r\n降档权重值已更新成功.降档数值权重为:LED温度=%d%%,驱动MOS温度=%d%%",buf,100-buf);
			}
		}		
	//检查里面是否有需要用到阈值编号和挡位模式参数
	IsParameterExist("4567AB",22,&ParamOK);
	if(ParamOK)
	  {
		//准备参数
		IsCmdParamOK=true;
		Param=IsParameterExist("01",22,NULL);
		Sensor=CheckUserInputForThermalSens(Param);//取用户选择的传感器值		
		Param=IsParameterExist("23",22,NULL);
		IsParameterExist("AB",22,&ParamOK);  //检查是否包含查看函数
		if(!CheckIfParamOnlyDigit(Param))buf=atoi(Param);
		else buf=-1;
		//判断
		if(Sensor==ThermalSens_None)//用户没输入传感器参数
		  {
			DisplayIllegalParam(Param,22,0);//显示用户输入了非法参数
			DisplayCorrectSensor();
			}
		else if((buf<0||buf>4)&&!ParamOK) //阈值数值错误
		  {
			DisplayIllegalParam(Param,22,0);//显示用户输入了非法参数
			UARTPuts("\r\n您应当指定一个0-4之间的数值作为欲编辑的温控曲线阈值点的编号.");
			}
		else //数值正常执行其他部分
		  {
			Threshold=((Sensor==ThermalSens_LEDNTC)?CfgFile.LEDThermalStepThr:CfgFile.SPSThermalStepThr);
			Value=((Sensor==ThermalSens_LEDNTC)?CfgFile.LEDThermalStepRatio:CfgFile.SPSThermalStepRatio);//将要处理的buffer赋值过来
			//处理显示部分
			IsParameterExist("AB",22,&ParamOK);
			if(ParamOK)
			  {
				UartPrintf("\r\n------ 温控表数值查看器(基于%s) -------\r\n",(Sensor==ThermalSens_LEDNTC)?"LED温度":"驱动MOS");				
				DisplayTempTable(Threshold,Value);
				//因为执行-v时用户可能没有指定节点编号这会导致下面的逻辑出问题,因此显示完就直接跳出
				ClearRecvBuffer();//清除接收缓冲
        CmdHandle=Command_None;//命令执行完毕
				return;
				}
			//处理阈值部分
			Param=IsParameterExist("45",22,NULL);
			if(Param!=NULL)
			  {			
				newvalue=atof(Param); //取用户输入的数值
				oldvalue=((Sensor==ThermalSens_LEDNTC)?CfgFile.LEDThermalTripTemp:CfgFile.MOSFETThermalTripTemp);//取热跳闸温度
			  if(newvalue==NAN||newvalue>oldvalue) //温度过低或低于
			    {
			    DisplayIllegalParam(Param,22,4);//显示用户输入了非法参数
					UartPrintf("\r\n您应当指定一个不超过%.1f摄氏度(热跳闸温度)的数值作为阈值温度.",oldvalue);
			    }				
				else //正常计算
				  {
					oldvalue=Threshold[buf];
					Threshold[buf]=newvalue;//存下原来的阈值然后查表
						if(newvalue<30||!CheckLinearTable(5,Threshold))//查表失败，恢复初始值
					  {
						Threshold[buf]=oldvalue;//恢复数值	
						min=Threshold[(buf>0)?buf-1:0]+0.05;
						if(buf==4)max=((Sensor==ThermalSens_LEDNTC)?CfgFile.LEDThermalTripTemp:CfgFile.MOSFETThermalTripTemp);//最后一个也是嘴啊高温度的，取热跳闸温度
						else max=Threshold[(buf<4)?buf+1:4]-0.05; //算出合法的温度范围
						DisplayIllegalParam(Param,22,4);//显示用户输入了非法参数
						UartPrintf("\r\n为了使温度曲线可以正常工作,您应为设定点#%d指定一个在",buf);
						UartPrintf("%.2f到%.2f摄氏度之间的数值作为阈值温度.",(buf==0)?30:min,max);
						}
					else DisplayTableUpdate(Threshold,Value,true,Sensor); //更新成功
					}
				}
			//处理温度数值部分
			Param=IsParameterExist("67",22,NULL);
      if(Param!=NULL)
			  {			
				newvalue=atof(Param); //取用户输入的数值
			  if(newvalue==NAN||newvalue>100) //温度过低或低于
			    {
			    DisplayIllegalParam(Param,22,4);//显示用户输入了非法参数
					UartPrintf("\r\n您应当指定一个不超过100(%)的数值指定驱动输出的百分比.");
			    }				
				else //正常计算
				  {
					oldvalue=Value[buf];
					Value[buf]=newvalue;//存下原来的阈值然后查表
						if(newvalue<5||!CheckLinearTable(5,Value))//查表失败，恢复初始值
					  {
						Value[buf]=oldvalue;//恢复数值	
						max=Value[(buf<4)?buf+1:4]; //算出合法的温度范围
						min=Value[(buf>0)?buf-1:0];
						DisplayIllegalParam(Param,22,4);//显示用户输入了非法参数
						UartPrintf("\r\n为了使温度曲线可以正常工作,您应为设定点#%d指定一个",buf);
						if(buf==0) //在最低点
						  {
							if(Value[buf]<max)UartPrintf("大于5%%且小于%.2f%%的额定输出百分比.",max);
							else UartPrintf("在100%%范围且大于%.2f%%的额定输出百分比.",max);
							}
			      else if(buf==4) //在最高点
						  {
							if(min<Value[buf])UartPrintf("在100%%范围且大于%.2f%%的额定输出百分比.",min);				
							else UartPrintf("大于5%%且小于%.2f%%的额定输出百分比.",min);
							}			
						else UartPrintf("在%.2f%%和%.2f%%之间的额定输出百分比.",min<max?min:max,min<max?max:min);							
						}
					else DisplayTableUpdate(Threshold,Value,true,Sensor); //更新成功
					}
				 }
		   }
		 }
  if(!IsCmdParamOK)UartPrintCommandNoParam(21);//显示啥也没找到的信息 
	//命令处理完毕
	ClearRecvBuffer();//清除接收缓冲
  CmdHandle=Command_None;//命令执行完毕
	}
