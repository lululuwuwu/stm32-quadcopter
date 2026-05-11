#ifndef __BOARD_H
#define __BOARD_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "queue.h"

/*
 * 遥控板引脚统一定义。
 *
 * 以后调整硬件连线时，优先修改本区域；各外设模块不再单独维护引脚表。
 */

/* USART1 调试串口：TX PA9，RX PA10。 */
#define Serial1_RCC RCC_APB2Periph_GPIOA
#define Serial1_GPIO GPIOA
#define Serial1_Tx_Pin GPIO_Pin_9
#define Serial1_Rx_Pin GPIO_Pin_10

/* OLED 软件 SPI：DC PB12，SCLK PB13，RES PB14，MOSI PB15。 */
#define OLED_RCC RCC_APB2Periph_GPIOB
#define OLED_GPIO_PORT GPIOB
#define OLED_DC GPIO_Pin_12
#define OLED_SCLK GPIO_Pin_13
#define OLED_RES GPIO_Pin_14
#define OLED_MOSI GPIO_Pin_15
#define OLED_CMD 0
#define OLED_DATA 1

/* NRF24L01 硬件 SPI1：CSN PA4，SCK PA5，MISO PA6，MOSI PA7，IRQ PA8，CE PA15。 */
#define NRF24L01_RCC RCC_APB2Periph_GPIOA
#define NRF24L01_SPI_RCC RCC_APB2Periph_SPI1
#define NRF24L01_CSN_GPIO GPIOA
#define NRF24L01_CSN_PIN GPIO_Pin_4
#define NRF24L01_SCK_GPIO GPIOA
#define NRF24L01_SCK_PIN GPIO_Pin_5
#define NRF24L01_MISO_GPIO GPIOA
#define NRF24L01_MISO_PIN GPIO_Pin_6
#define NRF24L01_MOSI_GPIO GPIOA
#define NRF24L01_MOSI_PIN GPIO_Pin_7
#define NRF24L01_IRQ_GPIO GPIOA
#define NRF24L01_IRQ_PIN GPIO_Pin_8
#define NRF24L01_CE_GPIO GPIOA
#define NRF24L01_CE_PIN GPIO_Pin_15

/* 摇杆 ADC：YAW PA0，THR PA1，ROL PA2，PIT PA3；遥控器电池检测 PB0。 */
#define STICK_ADC_RCC RCC_APB2Periph_ADC1
#define STICK_DMA_RCC RCC_AHBPeriph_DMA1
#define STICK_RCC RCC_APB2Periph_GPIOA
#define STICK_GPIO GPIOA
#define STICK_YAW_PIN GPIO_Pin_0
#define STICK_THR_PIN GPIO_Pin_1
#define STICK_ROLL_PIN GPIO_Pin_2
#define STICK_PITH_PIN GPIO_Pin_3
#define STICK_PITCH_PIN STICK_PITH_PIN
#define RC_BAT_RCC RCC_APB2Periph_GPIOB
#define RC_BAT_GPIO GPIOB
#define RC_BAT_PIN GPIO_Pin_0

/* 按键：左键 PB8，右键 PB7，微调上/左/下/右分别为 PB3/PB4/PB5/PB6。 */
#define Key_RCC RCC_APB2Periph_GPIOB
#define Key_GPIO GPIOB
#define Key_Left GPIO_Pin_8
#define Key_Right GPIO_Pin_7
#define Key_OffSet_Font GPIO_Pin_3
#define Key_OffSet_Left GPIO_Pin_4
#define Key_OffSet_Back GPIO_Pin_5
#define Key_OffSet_Right GPIO_Pin_6

/* 声光反馈：红灯 PB9，蓝灯 PB1，蜂鸣器 PB10。 */
#define LED_RED_RCC RCC_APB2Periph_GPIOB
#define LED_RED_GPIO GPIOB
#define LED_RED_PIN GPIO_Pin_9
#define LED_BLUE_RCC RCC_APB2Periph_GPIOB
#define LED_BLUE_GPIO GPIOB
#define LED_BLUE_PIN GPIO_Pin_1
#define BEEP_RCC RCC_APB2Periph_GPIOB
#define BEEP_GPIO GPIOB
#define BEEP_PIN GPIO_Pin_10

#define uxQueueSerial1Length 10
#define uxQueueSerial1ItemSize 150
extern QueueHandle_t xQueueSerial1;

#endif
