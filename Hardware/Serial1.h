#ifndef __SERIAL1_H
#define __SERIAL1_H
#include "stdio.h"
#include "string.h"
extern uint8_t Serial1_RxByte;

//初始化
void Serial1_Init(void);

//发送字节
void Serial1_SendByte(uint8_t byteValue);
	
void Serial1_SendString(char *str);

void Serial1_SendStringLine(char *str);


uint8_t Serial1_GetComplateFlagStatus(void);

char * Serial1_GetRxString(void);

void Serial1_SendLog(char *format, ...);

#endif

