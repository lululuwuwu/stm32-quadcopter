#ifndef __KEY_H
#define __KEY_H


//初始化硬件
void Key_Init(void);

//检测案件是否被按下（1就是按了一次，0就是没有按）
uint8_t Key1_Pressed(void);

uint8_t Key2_Pressed(void);


#endif
