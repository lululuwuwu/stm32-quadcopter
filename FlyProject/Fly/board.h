#ifndef __BOARD_H
#define __BOARD_H
#include "stm32f10x.h" // Device header
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "Serial3.h"
#include "FlyTask.h"
#include "LED.h"

#include "MyPWM.h"
#include "FlyContrl.h"
#include "MyI2C.h"
#include "MPU6050.h"



// *******************************串口定义
#define uxQueueSerial3Length 10
#define uxQueueSerial3ItemSize 150

#define Serial3_RCC_UART RCC_APB1Periph_USART3
#define Serial3_RCC_GPIO RCC_APB2Periph_GPIOB
#define Serial3_Type USART3 // 硬件USART3
#define Serial3_GPIO GPIOB
#define Serial3_Tx_PIN GPIO_Pin_10
#define Serial3_Rx_PIN GPIO_Pin_11
#define Serial3_BaudRate 115200
extern QueueHandle_t xQueueSerial3;

/*******************************MPU6050 软件 IIC 通讯引脚定义 */
#define MPU6050_RCC RCC_APB2Periph_GPIOB
#define MPU6050_GPIO GPIOB
#define MPU6050_SCL_Pin GPIO_Pin_6
#define MPU6050_SDA_Pin GPIO_Pin_7

/*******************************LED灯定义 */
#define LED_RCC RCC_APB2Periph_GPIOB
#define LED_GPIO GPIOB
#define LED_RedLeft_Pin GPIO_Pin_8   // 机身后灯 --ok
#define LED_RedRight_Pin GPIO_Pin_9  // 机身后灯  --ok
#define LED_BlueLeft_Pin GPIO_Pin_3  // 机身前灯  --ok
#define LED_BlueRight_Pin GPIO_Pin_1 // 机身前灯 --ok

/*******************************定时器 PWM 引脚定义 */
#define TIM2_RCC RCC_APB1Periph_TIM2
#define TIM2_GPIO GPIOA
#define TIM2_CH1_Pin GPIO_Pin_0
#define TIM2_CH2_Pin GPIO_Pin_1

#define TIM3_RCC RCC_APB1Periph_TIM3
#define TIM3_GPIO GPIOB
#define TIM3_CH1_Pin GPIO_Pin_4
#define TIM3_CH2_Pin GPIO_Pin_5



#endif
