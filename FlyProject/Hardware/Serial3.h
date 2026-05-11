/*
 * 文件名: Serial3.h
 * 功能说明:
 * 1. Serial3_Init          初始化 USART3、GPIO 和接收中断。
 * 2. Serial3_SendByte      阻塞发送 1 字节。
 * 3. Serial3_SendArray     发送指定长度数组。
 * 4. Serial3_SendString    发送 C 字符串。
 * 5. Serial3_GetRxStatus   获取一帧字符串是否接收完成。
 * 6. Serial3_GetRxString   获取接收到的字符串，并清除接收状态。
 * 7. Serial3_SendLog       格式化日志并写入串口日志队列。
 */
#ifndef __Serial3_H
#define __Serial3_H

#include "stdio.h"
#include "string.h"

void Serial3_Init(void);
void Serial3_SendByte(uint8_t byteValue);
void Serial3_SendArray(uint8_t *arrayAddr, uint8_t length);
void Serial3_SendString(char *str);
uint8_t Serial3_GetRxStatus(void);
char *Serial3_GetRxString(void);
void Serial3_SendLog(char *format, ...);

#endif
