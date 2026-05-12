/*
 * 文件名: board.h
 * 功能说明:
 * 1. 系统头文件包含区      集中包含 STM32 标准外设库、FreeRTOS 和本项目业务模块头文件。
 * 2. 串口 3 宏定义区       定义 USART3、PB10/PB11、波特率和日志队列参数。
 * 3. MPU6050 软件 IIC 区   定义 MPU6050 使用的软件 IIC GPIO：PB6=SCL、PB7=SDA。
 * 4. NRF24L01 硬件 SPI 区  定义无线模块使用的 SPI2 和控制引脚。
 * 5. LED 宏定义区          定义四个机身状态灯使用的 GPIO 和引脚。
 * 6. PWM 宏定义区          定义四路电机 PWM 使用的 TIM2/TIM3 通道和引脚。
 *
 * 注意:
 * - 本文件是板级硬件资源的集中入口，新增外设引脚时优先放在这里，避免散落到多个 .c 文件。
 * - 串口 3 使用 PB10/PB11，MPU6050 改用软件 IIC 的 PB6/PB7，避免和 USART3 引脚冲突。
 */
#ifndef __BOARD_H
#define __BOARD_H

#include "stm32f10x.h" // STM32F103 标准外设库设备头文件
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "Serial3.h"
#include "FlyTask.h"
#include "FlyAttitude.h"
#include "LED.h"
#include "MyPWM.h"
#include "FlyContrl.h"
#include "MyI2C.h"
#include "MPU6050.h"
#include "NRF24L01.h"

/*******************************串口 3 定义 */
#define uxQueueSerial3Length 10       // 串口日志队列最多缓存 10 条字符串
#define uxQueueSerial3ItemSize 150    // 每条日志字符串最大缓存长度

#define Serial3_RCC_UART RCC_APB1Periph_USART3
#define Serial3_RCC_GPIO RCC_APB2Periph_GPIOB
#define Serial3_Type USART3           // 硬件 USART3
#define Serial3_GPIO GPIOB
#define Serial3_Tx_PIN GPIO_Pin_10    // USART3_TX
#define Serial3_Rx_PIN GPIO_Pin_11    // USART3_RX
#define Serial3_BaudRate 115200
extern QueueHandle_t xQueueSerial3;

/*******************************MPU6050 软件 IIC 通讯引脚定义 */
#define MPU6050_RCC RCC_APB2Periph_GPIOB
#define MPU6050_GPIO GPIOB
#define MPU6050_SCL_Pin GPIO_Pin_6    // 软件 IIC SCL
#define MPU6050_SDA_Pin GPIO_Pin_7    // 软件 IIC SDA

/*******************************NRF24L01 硬件 SPI 通讯引脚定义 */
/*
 * 实际连线: CSN PB12，SCK PB13，MISO PB14，MOSI PB15，IRQ PB2，CE PA8。
 * PB13/PB14/PB15 是 STM32F103 的 SPI2 引脚，因此这里按硬件连线使用 SPI2。
 */
#define NRF24L01_SPI SPI2
#define NRF24L01_SPI_RCC RCC_APB1Periph_SPI2
#define NRF24L01_GPIOB_RCC RCC_APB2Periph_GPIOB
#define NRF24L01_GPIOA_RCC RCC_APB2Periph_GPIOA
#define NRF24L01_CSN_GPIO GPIOB
#define NRF24L01_CSN_PIN GPIO_Pin_12
#define NRF24L01_SCK_GPIO GPIOB
#define NRF24L01_SCK_PIN GPIO_Pin_13
#define NRF24L01_MISO_GPIO GPIOB
#define NRF24L01_MISO_PIN GPIO_Pin_14
#define NRF24L01_MOSI_GPIO GPIOB
#define NRF24L01_MOSI_PIN GPIO_Pin_15
#define NRF24L01_IRQ_GPIO GPIOB
#define NRF24L01_IRQ_PIN GPIO_Pin_2
#define NRF24L01_CE_GPIO GPIOA
#define NRF24L01_CE_PIN GPIO_Pin_8
#define NRF24L01_DEFAULT_CHANNEL 72

/*******************************LED 灯定义 */
#define LED_RCC RCC_APB2Periph_GPIOB
#define LED_GPIO GPIOB
#define LED_RedLeft_Pin GPIO_Pin_8    // 机身后左红灯
#define LED_RedRight_Pin GPIO_Pin_9   // 机身后右红灯
#define LED_BlueLeft_Pin GPIO_Pin_3   // 机身前左蓝灯，使用该引脚前需要关闭 JTAG
#define LED_BlueRight_Pin GPIO_Pin_1  // 机身前右蓝灯

/*******************************定时器 PWM 引脚定义 */
#define TIM2_RCC RCC_APB1Periph_TIM2
#define TIM2_GPIO GPIOA
#define TIM2_CH1_Pin GPIO_Pin_0       // 电机 1 PWM
#define TIM2_CH2_Pin GPIO_Pin_1       // 电机 2 PWM

#define TIM3_RCC RCC_APB1Periph_TIM3
#define TIM3_GPIO GPIOB
#define TIM3_CH1_Pin GPIO_Pin_4       // 电机 3 PWM，TIM3 部分重映射后使用
#define TIM3_CH2_Pin GPIO_Pin_5       // 电机 4 PWM，TIM3 部分重映射后使用

#endif
