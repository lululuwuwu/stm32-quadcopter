#ifndef __MYHSPI_H
#define __MYHSPI_H

#include "stm32f10x.h"

// NRF24L01 ----Òý½Å¶¨Òå
#ifndef NRF24L01_CSN_GPIO
#define NRF24L01_CSN_GPIO GPIOA
#define NRF24L01_CSN_PIN GPIO_Pin_4
#endif

#ifndef NRF24L01_CE_GPIO
#define NRF24L01_CE_GPIO GPIOA
#define NRF24L01_CE_PIN GPIO_Pin_15
#endif

#ifndef NRF24L01_MOSI_GPIO
#define NRF24L01_MOSI_GPIO GPIOA
#define NRF24L01_MOSI_PIN GPIO_Pin_7
#endif

#ifndef NRF24L01_SCK_GPIO
#define NRF24L01_SCK_GPIO GPIOA
#define NRF24L01_SCK_PIN GPIO_Pin_5
#endif

#ifndef NRF24L01_IRQ_GPIO
#define NRF24L01_IRQ_GPIO GPIOA
#define NRF24L01_IRQ_PIN GPIO_Pin_8
#endif

#ifndef NRF24L01_MISO_GPIO
#define NRF24L01_MISO_GPIO GPIOA
#define NRF24L01_MISO_PIN GPIO_Pin_6
#endif

void MyHSPI_Init(void);

void MyHSPI_Start(void);

void MyHSPI_Stop(void);

void MyHSPI_NRF_CE(uint8_t bitValue);

uint8_t MyHSPI_SwapByte(uint8_t byteValue);

#endif
