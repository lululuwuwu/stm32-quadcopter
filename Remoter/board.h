#ifndef __BOARD_H
#define __BOARD_H

#include "stm32f10x.h" // Device header
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "Serial1.h"
#include "remotetask.h"
#include "remoterData.h"
#include "show.h"

#include "OLED.h"




//串口配置
#define uxQueueSerial1Length 10
#define uxQueueSerial1ItemSize 150
extern QueueHandle_t xQueueSerial1; //队列句柄

#define Serial1_Tx_Pin GPIO_Pin_9
#define Serial1_Rx_Pin GPIO_Pin_10
#define Serial1_GPIO GPIOA


//OLED屏幕引脚定义
#define OLED_RCC RCC_APB2Periph_GPIOB
#define OLED_GPIO_PORT GPIOB

#define OLED_DC GPIO_Pin_12	  // Data or Commcand   ---推挽
#define OLED_SCLK GPIO_Pin_13 // 时钟线   --推挽
#define OLED_MOSI GPIO_Pin_15 // OLED数据线   --推挽
#define OLED_RES GPIO_Pin_14  // 低电平就初始化芯片，高电平就是正常模式   --推挽

							  // CS 是默认连接到GND，没有操作引脚
							  
#define OLED_CMD 0			  // 指令
#define OLED_DATA 1			  // 数据



#endif
