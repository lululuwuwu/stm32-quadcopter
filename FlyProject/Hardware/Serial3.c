#include "board.h"

// 硬件： UART3： Tx - PB10 ,Rx - PB11

void Serial3_Init(void)
{
    // 开启时钟
    RCC_APB2PeriphClockCmd(Serial3_RCC_GPIO, ENABLE);
    RCC_APB1PeriphClockCmd(Serial3_RCC_UART, ENABLE);

    // 配置GPIO
    GPIO_InitTypeDef GPIO_InitTypeStructure;
    GPIO_InitTypeStructure.GPIO_Mode  = GPIO_Mode_AF_PP; // PB10 是串口的输出 Tx ---> 推挽复用输出
    GPIO_InitTypeStructure.GPIO_Pin   = Serial3_Tx_PIN;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(Serial3_GPIO, &GPIO_InitTypeStructure);

    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IPU; // PB11 是串口的输入 Rx --->  浮空输入或带上拉输入
    GPIO_InitTypeStructure.GPIO_Pin  = Serial3_Rx_PIN;
    GPIO_Init(Serial3_GPIO, &GPIO_InitTypeStructure);

    // 串口配置

    USART_InitTypeDef USART_InitTypeStructure;
    USART_InitTypeStructure.USART_BaudRate            = Serial3_BaudRate;               // 直接填写波特率即可
    USART_InitTypeStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 不用硬件流控
    USART_InitTypeStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;  // 发送模式 和 接收模式
    USART_InitTypeStructure.USART_Parity              = USART_Parity_No;                // 不用校验位（奇 ， 偶 校验）
    USART_InitTypeStructure.USART_StopBits            = USART_StopBits_1;               // 1个停止位
    USART_InitTypeStructure.USART_WordLength          = USART_WordLength_8b;            // 8位数据长度

    USART_Init(Serial3_Type, &USART_InitTypeStructure);

    // 用中断来控制接收
    USART_ITConfig(Serial3_Type, USART_IT_RXNE, ENABLE);

    // NVIC
    NVIC_InitTypeDef NVIC_InitTypeStructure;
    NVIC_InitTypeStructure.NVIC_IRQChannel                   = USART3_IRQn;
    NVIC_InitTypeStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_InitTypeStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitTypeStructure.NVIC_IRQChannelSubPriority        = 0;

    NVIC_Init(&NVIC_InitTypeStructure);

    // 串口使能
    USART_Cmd(Serial3_Type, ENABLE);
}

// 发送字节数据
void Serial3_SendByte(uint8_t byteValue)
{
    /*
    位7
    TXE:发送数据寄存器空 (Transmit data register empty)
    当TDR寄存器中的数据被硬件转移到移位寄存器的时候，该位被硬件置位。如果USART_CR1
    寄存器中的TXEIE为1，则产生中断。对USART_DR的写操作，将该位清零。
    0：数据还没有被转移到移位寄存器；
    1：数据已经被转移到移位寄存器。
    注意：单缓冲器传输中使用该位
    */
    while (USART_GetFlagStatus(Serial3_Type, USART_FLAG_TXE) != SET)
        ;

    USART_SendData(Serial3_Type, byteValue);
}

// 发送数组
void Serial3_SendArray(uint8_t* arrayAddr, uint8_t length)
{
    for (uint8_t i = 0; i < length; i++)
    {
        Serial3_SendByte(arrayAddr[i]);
    }
}

// 发送字符串
void Serial3_SendString(char* str)
{
    for (uint8_t i = 0; str[i] != '\0'; i++)
    {
        Serial3_SendByte(str[i]);
    }
}

// 希望用串口打印日志 ---> 修改printf的底层，把字节传送到串口UATR上

// 重写printf底层的fputc，重定向输出到串口----> 编译到项目中即可
int fputc(int ch, FILE* f)
{
    Serial3_SendByte(ch); // 将printf的底层重定向到自己的发送字节函数
    return ch;
}

// 用队列的方式打印日志   ---> Serial3_SendLog("hello %d %d",5,6)

#include "stdio.h"
#include <stdarg.h>
void Serial3_SendLog(char* format, ...) //...表示可变参数
{
    static char str[uxQueueSerial3ItemSize];
    va_list     arg;                          // 定义可变参数列表数据类型的变量arg
    va_start(arg, format);                    // 从format开始，接收参数列表到arg变量
    vsnprintf(str, sizeof(str), format, arg); // 使用vsprintf打印格式化字符串和参数列表到字符数组中
    va_end(arg);                              // 结束变量arg

    xQueueSendToBack(xQueueSerial3, str, 0); // portMAX_DELAY 一直等
}

uint8_t Serial3_RxByte = 0x00;

char     Serial3_RxString[256];
uint16_t Serial3_RxIndex    = 0; // 下标
uint8_t  Serial3_RxComplate = 0; // 0就是没有接收完成，1就是接收完成

// 获取是否接收完成标志

uint8_t Serial3_GetRxStatus(void)
{
    return Serial3_RxComplate;
}

// 读取数据
char* Serial3_GetRxString(void)
{
    Serial3_RxIndex    = 0; // 重置
    Serial3_RxComplate = 0; // 重置
    return Serial3_RxString;
}

// 接收中断处理函数
void USART3_IRQHandler(void)
{
    if (USART_GetITStatus(Serial3_Type, USART_IT_RXNE) == SET)
    {
        // 常见的字符串结束符号是 \r\n  或 \n

        Serial3_RxByte = USART_ReceiveData(Serial3_Type);

        if (Serial3_RxIndex < 255 && Serial3_RxByte != '\r' && Serial3_RxByte != '\n' && Serial3_RxComplate != 1)
        {
            Serial3_RxString[Serial3_RxIndex++] = Serial3_RxByte;
        }

        if (Serial3_RxByte == '\n')
        {
            Serial3_RxString[Serial3_RxIndex++] = '\0'; // 手动加一个结束符
            Serial3_RxComplate                  = 1;
        }

        USART_ClearITPendingBit(Serial3_Type, USART_IT_RXNE);
    }
}
