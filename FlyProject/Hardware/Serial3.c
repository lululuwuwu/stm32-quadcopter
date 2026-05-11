/*
 * 文件名: Serial3.c
 * 功能说明:
 * 1. Serial3_Init          初始化 USART3，硬件连接为 PB10=TX、PB11=RX。
 * 2. Serial3_SendByte      等待 TXE 置位后发送 1 字节。
 * 3. Serial3_SendArray     逐字节发送数组。
 * 4. Serial3_SendString    逐字节发送以 \0 结尾的字符串。
 * 5. fputc                 重定向 printf 到 USART3。
 * 6. Serial3_SendLog       使用 vsnprintf 格式化日志，并投递到 FreeRTOS 队列。
 * 7. Serial3_GetRxStatus   查询串口接收是否完成一行。
 * 8. Serial3_GetRxString   返回接收缓冲区并复位接收状态。
 * 9. USART3_IRQHandler     USART3 接收中断，按换行符结束一帧字符串。
 *
 * 注意:
 * - 日志发送采用队列，真正的串口发送在 TaskSerial3 中执行，避免业务任务长时间阻塞。
 * - Serial3_SendLog 当前投递队列等待时间为 0，高频打印时可能丢日志，但不会拖慢控制任务。
 */
#include "board.h"
#include "stdio.h"
#include <stdarg.h>

uint8_t Serial3_RxByte = 0x00;
char Serial3_RxString[256];
uint16_t Serial3_RxIndex = 0;
uint8_t Serial3_RxComplate = 0;

void Serial3_Init(void)
{
    GPIO_InitTypeDef GPIO_InitTypeStructure;
    USART_InitTypeDef USART_InitTypeStructure;
    NVIC_InitTypeDef NVIC_InitTypeStructure;

    // 1. 打开 USART3 所在 APB1 时钟和 GPIOB 所在 APB2 时钟。
    RCC_APB2PeriphClockCmd(Serial3_RCC_GPIO, ENABLE);
    RCC_APB1PeriphClockCmd(Serial3_RCC_UART, ENABLE);

    // 2. PB10 是 USART3_TX，配置为复用推挽输出。
    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitTypeStructure.GPIO_Pin = Serial3_Tx_PIN;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(Serial3_GPIO, &GPIO_InitTypeStructure);

    // 3. PB11 是 USART3_RX，配置为上拉输入，空闲状态保持高电平。
    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitTypeStructure.GPIO_Pin = Serial3_Rx_PIN;
    GPIO_Init(Serial3_GPIO, &GPIO_InitTypeStructure);

    // 4. USART3 配置为 115200 8N1，无硬件流控，同时开启收发。
    USART_InitTypeStructure.USART_BaudRate = Serial3_BaudRate;
    USART_InitTypeStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitTypeStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitTypeStructure.USART_Parity = USART_Parity_No;
    USART_InitTypeStructure.USART_StopBits = USART_StopBits_1;
    USART_InitTypeStructure.USART_WordLength = USART_WordLength_8b;
    USART_Init(Serial3_Type, &USART_InitTypeStructure);

    // 5. 开启接收非空中断，用于接收上位机发来的字符串命令。
    USART_ITConfig(Serial3_Type, USART_IT_RXNE, ENABLE);

    // 6. 配置 USART3 中断优先级。当前未在 ISR 中调用 FreeRTOS API，因此优先级只需避开系统关键中断。
    NVIC_InitTypeStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitTypeStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitTypeStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitTypeStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_Init(&NVIC_InitTypeStructure);

    USART_Cmd(Serial3_Type, ENABLE);
}

void Serial3_SendByte(uint8_t byteValue)
{
    // TXE=1 表示发送数据寄存器空，可以写入下一个字节。
    while (USART_GetFlagStatus(Serial3_Type, USART_FLAG_TXE) != SET)
        ;

    USART_SendData(Serial3_Type, byteValue);
}

void Serial3_SendArray(uint8_t *arrayAddr, uint8_t length)
{
    uint8_t i;

    for (i = 0; i < length; i++)
    {
        Serial3_SendByte(arrayAddr[i]);
    }
}

void Serial3_SendString(char *str)
{
    uint8_t i;

    for (i = 0; str[i] != '\0'; i++)
    {
        Serial3_SendByte(str[i]);
    }
}

int fputc(int ch, FILE *f)
{
    // 重定向 printf 的底层输出，便于临时调试。
    Serial3_SendByte((uint8_t)ch);
    return ch;
}

void Serial3_SendLog(char *format, ...)
{
    static char str[uxQueueSerial3ItemSize];
    va_list arg;

    // 使用固定大小缓冲区，避免 sprintf 越界破坏内存。
    va_start(arg, format);
    vsnprintf(str, sizeof(str), format, arg);
    va_end(arg);

    // 等待时间为 0：队列满时直接丢弃本条日志，保证高频任务不被串口拖住。
    xQueueSendToBack(xQueueSerial3, str, 0);
}

uint8_t Serial3_GetRxStatus(void)
{
    return Serial3_RxComplate;
}

char *Serial3_GetRxString(void)
{
    // 读取后复位索引和完成标志，下一帧从缓冲区起始位置重新接收。
    Serial3_RxIndex = 0;
    Serial3_RxComplate = 0;
    return Serial3_RxString;
}

void USART3_IRQHandler(void)
{
    if (USART_GetITStatus(Serial3_Type, USART_IT_RXNE) == SET)
    {
        Serial3_RxByte = USART_ReceiveData(Serial3_Type);

        // 未收到换行且缓冲区未满时继续保存字符。
        if (Serial3_RxIndex < 255 && Serial3_RxByte != '\r' && Serial3_RxByte != '\n' && Serial3_RxComplate != 1)
        {
            Serial3_RxString[Serial3_RxIndex++] = Serial3_RxByte;
        }

        // 使用 \n 作为一帧结束符，末尾手动补 \0，方便按字符串处理。
        if (Serial3_RxByte == '\n')
        {
            Serial3_RxString[Serial3_RxIndex++] = '\0';
            Serial3_RxComplate = 1;
        }

        USART_ClearITPendingBit(Serial3_Type, USART_IT_RXNE);
    }
}
