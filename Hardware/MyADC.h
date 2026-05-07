#ifndef __MYADC_H
#define __MYADC_H

//初始化
void MyADC_Init(void);

//获取数字值
uint16_t MyADC_GetDataValue(void);
	

//获取模拟电压值, 返回mv(毫伏)
uint16_t MyADC_GetAnalogValue(void);	



#endif
