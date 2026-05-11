#ifndef __Serial3_H
#define __Serial3_H

#include "stdio.h"
#include "string.h"

void Serial3_SendByte(uint8_t byteValue);

void Serial3_Init(void);

void Serial3_SendArray(uint8_t *arrayAddr, uint8_t length);

void Serial3_SendString(char *str);

uint8_t Serial3_GetRxStatus(void);
char *Serial3_GetRxString(void);

void Serial3_SendLog(char *format, ...);

#endif
