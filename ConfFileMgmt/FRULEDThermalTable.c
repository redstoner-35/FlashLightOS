#include "FRU.h"

//加载默认值
static void LoadDefaultValue(LEDThermalConfStrDef *ParamOut)
  {
	ParamOut->MaxLEDTemp=75;
	ParamOut->PIDTriggerTemp=65;
	ParamOut->PIDMaintainTemp=55;
	}

//对于通用LED则根据LED ID进行匹配
int CheckForOEMLEDTable(LEDThermalConfStrDef *ParamOut,FRUBlockUnion *FRU)	
  {
	switch(FRU->FRUBlock.Data.Data.CustomLEDIDCode)
	  {
	  //8颗光宏的紫外LED
		case 0x3125 :
		   ParamOut->MaxLEDTemp=65;
		   ParamOut->PIDTriggerTemp=56;
		   ParamOut->PIDMaintainTemp=50;
		   break;
		//8颗Luminus SFT-40-W 6500K
		case 0x4E65:
		//8颗Luminus SFT-40-W 5000K
		case 0x4E50:
		   ParamOut->MaxLEDTemp=75;
		   ParamOut->PIDTriggerTemp=65;
		   ParamOut->PIDMaintainTemp=55;			
		   break;
		//3颗CREE XHP70.3HI 4500K 90CRI
		case 0x7002:   
		//3颗CREE XHP70.3HI 5700K 90CRI
		case 0x7001:
		   ParamOut->MaxLEDTemp=78;
		   ParamOut->PIDTriggerTemp=70;
		   ParamOut->PIDMaintainTemp=60;		
		   break;
		//昌达SFH43 3000K
	  case 0x1001:
		//昌达SFH45 5500K
		case 0x1002:
		   ParamOut->MaxLEDTemp=80;
		   ParamOut->PIDTriggerTemp=70;
		   ParamOut->PIDMaintainTemp=60;
		   break;			 
		//未定义ID
		default : LoadDefaultValue(ParamOut);return 1;
		}
	//查表成功，返回0
  return 0;
	}
	
//查表得出温度数据
void QueueLEDThermalSettings(LEDThermalConfStrDef *ParamOut)
  {
	FRUBlockUnion FRU;
	//读取FRU数据，如果失败则直接跳过解析
  if(ReadFRU(&FRU)||!CheckFRUInfoCRC(&FRU))
	  {
    LoadDefaultValue(ParamOut);
	  return;
		}
	//开始解析
	switch(FRU.FRUBlock.Data.Data.FRUVersion[0])
	  {
		/* SBT90.2 LED */
		case 0x08:
			ParamOut->MaxLEDTemp=80;
		  ParamOut->PIDTriggerTemp=70;
		  ParamOut->PIDMaintainTemp=58;			
		  break;
		/* SBT70-B LED（使用和SBT70G一样的参数）*/
		case 0x07: 
		/* SBT90-R LED（使用和SBT70G一样的参数）*/
	  case 0x04:
		/* SBT70-G LED */
		case 0x05:
			ParamOut->MaxLEDTemp=65;
		  ParamOut->PIDTriggerTemp=55;
		  ParamOut->PIDMaintainTemp=50; //这些LED非常贵而且不耐热。			
      break;		
		/* 其他任意的未指定类型的3V/6V LED */
    case 0x03:		
		case 0x06:
			CheckForOEMLEDTable(ParamOut,&FRU);
		  break;
	  //其余参数，加载默认值
	  default : LoadDefaultValue(ParamOut);
		}
	}
