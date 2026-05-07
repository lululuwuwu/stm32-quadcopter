#ifndef __MYI2C_H
#define __MYI2C_H
uint8_t MyI2C_RecACK(void);
void MyI2C_SendACK(uint8_t ackValue);
uint8_t MyI2C_RecByte(void);
void MyI2C_SendByte(uint8_t byteValue);
void MyI2C_Stop(void);
void MyI2C_Start(void);
void MyI2C_Init(void);

#endif
