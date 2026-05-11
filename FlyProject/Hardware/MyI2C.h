/*
 * 文件名: MyI2C.h
 * 功能说明:
 * 1. MyI2C_Init      初始化软件 I2C GPIO，并尝试恢复总线。
 * 2. MyI2C_Start     发送 I2C 起始信号。
 * 3. MyI2C_Stop      发送 I2C 停止信号。
 * 4. MyI2C_SendByte  按高位先发的顺序发送 1 字节。
 * 5. MyI2C_RecByte   按高位先收的顺序接收 1 字节。
 * 6. MyI2C_SendACK   主机发送 ACK 或 NACK。
 * 7. MyI2C_RecACK    主机读取从机 ACK 状态。
 */
#ifndef __MYI2C_H
#define __MYI2C_H

#include "stm32f10x.h"

uint8_t MyI2C_RecACK(void);
void MyI2C_SendACK(uint8_t ackValue);
uint8_t MyI2C_RecByte(void);
void MyI2C_SendByte(uint8_t byteValue);
void MyI2C_Stop(void);
void MyI2C_Start(void);
void MyI2C_Init(void);

#endif
