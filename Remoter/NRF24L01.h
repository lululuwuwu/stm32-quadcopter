#ifndef __NRF24L01_H
#define __NRF24L01_H

#include "stm32f10x.h"

// NRF24L01 ----引脚定义
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

#define NRF_MODEL_RX 1 // 接收模式，用于双向传输
#define NRF_MODEL_TX 0 // 发送模式，用于双向传输

//***************************************NRF24L01寄存器指令*******************************************************
#define NRF_READ_REG 0x00           // 读寄存器指令
#define NRF_WRITE_REG 0x20          // 写寄存器指令
#define NRF_R_RX_PLOAD 0x61         // 读取接收数据指令
#define NRF_W_TX_PLOAD 0xA0         // 写待发数据指令
#define NRF_FLUSH_TX 0xE1           // 冲洗发送 FIFO 指令
#define NRF_FLUSH_RX 0xE2           // 冲洗接收 FIFO 指令
#define NRF_REUSE_TX_PL 0xE3        // 重复装载上一帧发送数据指令
#define NRF_R_RX_PL_WID 0x60        // 读取收到的数据字节数
#define NRF_W_ACK_PAYLOAD 0xA8      // 写 ACK payload，最多允许三帧数据存于 FIFO 中
#define NRF_W_TX_PAYLOAD_NOACK 0xB0 // 无 ACK 发送指令，需要配合 FEATURE 使用
#define NRF_NOP 0xFF                // 空操作指令

//*************************************SPI(nRF24L01)寄存器地址***************************************************
#define NRF_CONFIG 0x00      // 配置收发状态、CRC 校验模式以及中断响应方式
#define NRF_EN_AA 0x01       // 自动应答功能设置
#define NRF_EN_RXADDR 0x02   // 接收管道使能设置
#define NRF_SETUP_AW 0x03    // 收发地址宽度设置
#define NRF_SETUP_RETR 0x04  // 自动重发功能设置
#define NRF_RF_CH 0x05       // 工作频率设置
#define NRF_RF_SETUP 0x06    // 发射速率和功率设置
#define NRF_STATUS 0x07      // 状态寄存器
#define NRF_OBSERVE_TX 0x08  // 发送监测功能
#define NRF_RSSI 0x09        // 接收信号强度检测
#define NRF_RX_ADDR_P0 0x0A  // 频道0接收数据地址
#define NRF_RX_ADDR_P1 0x0B  // 频道1接收数据地址
#define NRF_RX_ADDR_P2 0x0C  // 频道2接收数据地址
#define NRF_RX_ADDR_P3 0x0D  // 频道3接收数据地址
#define NRF_RX_ADDR_P4 0x0E  // 频道4接收数据地址
#define NRF_RX_ADDR_P5 0x0F  // 频道5接收数据地址
#define NRF_TX_ADDR 0x10     // 发送地址寄存器
#define NRF_RX_PW_P0 0x11    // 接收频道0接收数据长度
#define NRF_RX_PW_P1 0x12    // 接收频道1接收数据长度
#define NRF_RX_PW_P2 0x13    // 接收频道2接收数据长度
#define NRF_RX_PW_P3 0x14    // 接收频道3接收数据长度
#define NRF_RX_PW_P4 0x15    // 接收频道4接收数据长度
#define NRF_RX_PW_P5 0x16    // 接收频道5接收数据长度
#define NRF_FIFO_STATUS 0x17 // FIFO 状态寄存器
#define NRF_DYNPD 0x1C       // 动态 payload 长度使能
#define NRF_FEATURE 0x1D     // 特征寄存器

//*************************************STATUS 状态标志位**********************************************************
#define RX_DR_BIT 6  // RX FIFO 有新数据
#define TX_DS_BIT 5  // 发送完成并收到 ACK
#define MAX_RT_BIT 4 // 达到最大重发次数

#define NRF_TX_RESULT_FAIL 0
#define NRF_TX_RESULT_OK 1
#define NRF_TX_RESULT_MAX_RT 2
#define NRF_TX_RESULT_INVALID_LENGTH 3

#define NRF_REMOTER_TX_PAYLOAD_LENGTH 20
#define NRF_FLY_ACK_PAYLOAD_LENGTH 17

extern uint8_t NRF_RX_DATA[];
extern uint8_t NRF_TX_DATA[];

void NRF24L01_Init(uint8_t mode, uint8_t chanle);
uint8_t NRF24L01_TX(uint8_t *txBuffer, uint8_t length);
uint8_t NRF24L01_RX(void);
uint8_t NRF24L01_GetLastAckLength(void);
void NRF24L01_RemoterUpdate(void);

#endif
