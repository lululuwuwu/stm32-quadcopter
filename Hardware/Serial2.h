#ifndef __SERIAL2_H
#define __SERIAL2_H
#include "stdio.h"
#include "string.h"
extern uint8_t Serial2_RxByte;

//初始化
void Serial2_Init(void);

//发送字节
void Serial2_SendByte(uint8_t byteValue);
	
void Serial2_SendString(char *str);

void Serial2_SendStringLine(char *str);


uint8_t Serial2_GetComplateFlagStatus(void);

char * Serial2_GetRxString(void);

#endif

