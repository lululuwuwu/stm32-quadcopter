#ifndef __MYTIM1_H
#define __MYTIM1_H


//初始化TIM1为输入捕获PWMI模式
void MyTIM1_Init(void);


//获取频率
uint32_t MyTIM1_GetFreq(void);


//获取占空比: 百分比
uint8_t MyTIM1_GetDuty(void);

#endif


