#ifndef __MYHSPI_H
#define __MYHSPI_H

#include "stm32f10x.h"

void MyHSPI_Init(void);
void MyHSPI_Start(void);
void MyHSPI_Stop(void);
void MyHSPI_NRF_CE(uint8_t bitValue);
uint8_t MyHSPI_SwapByte(uint8_t byteValue);

#endif
