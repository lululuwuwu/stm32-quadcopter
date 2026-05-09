#ifndef __BOARD_H
#define __BOARD_H

#include "stm32f10x.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "Serial1.h"
#include "MyADC.h"
#include "key.h"
#include "led.h"
#include "buzzer.h"
#include "remotetask.h"
#include "remoterData.h"
#include "remoterstick.h"
#include "remoterkey.h"
#include "show.h"

#include "OLED.h"

#define uxQueueSerial1Length 10
#define uxQueueSerial1ItemSize 150
extern QueueHandle_t xQueueSerial1;

#define Serial1_Tx_Pin GPIO_Pin_9
#define Serial1_Rx_Pin GPIO_Pin_10
#define Serial1_GPIO GPIOA

#define OLED_RCC RCC_APB2Periph_GPIOB
#define OLED_GPIO_PORT GPIOB

#define OLED_DC GPIO_Pin_12
#define OLED_SCLK GPIO_Pin_13
#define OLED_MOSI GPIO_Pin_15
#define OLED_RES GPIO_Pin_14

#define OLED_CMD 0
#define OLED_DATA 1

#endif
