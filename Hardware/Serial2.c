#include "stm32f10x.h"                  // Device header
#include <stdio.h>
#include <stdarg.h>

//硬件：UART2： Tx-- PA2 ,RX -PA3

//初始化
void Serial2_Init(void)
{
	//1.开启时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_AF_PP; //模式：推挽复用输出
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOA,&GPIO_InitTypeStructure);
	
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IPU; //模式：上拉输入 或 浮空输入
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_3; //接收线
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOA,&GPIO_InitTypeStructure);
	
	
	//2.串口初始化
	USART_InitTypeDef USART_InitTypeStructure;
	USART_InitTypeStructure.USART_BaudRate = 115200; // 波特率：直接填写即可
	USART_InitTypeStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//不使用硬件流控	
	USART_InitTypeStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;//发送模式 ， 接收模式要一起开
	USART_InitTypeStructure.USART_Parity = USART_Parity_No; //一般不用奇偶校验
	USART_InitTypeStructure.USART_StopBits = USART_StopBits_1;// 一个停止位
	USART_InitTypeStructure.USART_WordLength = USART_WordLength_8b;// 八位字长
	
	USART_Init(USART2,&USART_InitTypeStructure);	
	
	
	//3.开启中断
	USART_ITConfig(USART2,USART_IT_RXNE,ENABLE);
	
	//NVIC配置
	NVIC_InitTypeDef NVIC_InitTypeStructure;
	NVIC_InitTypeStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitTypeStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitTypeStructure.NVIC_IRQChannelPreemptionPriority = 3;
	NVIC_InitTypeStructure.NVIC_IRQChannelSubPriority = 0;
	
	NVIC_Init(&NVIC_InitTypeStructure);
	
	
	//4.启动
	USART_Cmd(USART2,ENABLE);
	
	
	
	

}

//发送字节
void Serial2_SendByte(uint8_t byteValue)
{
	//先等待TXE 发送数据寄存器为空，再写入USART_DR寄存器
	/*
	TXE:发送数据寄存器空 (Transmit data register empty) 
	当TDR寄存器中的数据被硬件转移到移位寄存器的时候，该位被硬件置位。如果USART_CR1寄存器中的TXEIE为1，则产生中断。对USART_DR的写操作，将该位清零。 
	0：数据还没有被转移到移位寄存器； 
	1：数据已经被转移到移位寄存器
	
	*/
	
	while(USART_GetFlagStatus(USART2,USART_FLAG_TXE)!=SET);
	
	USART_SendData(USART2,byteValue);
}
	

//发送字符串
void Serial2_SendString(char *str)
{
	for(uint8_t i=0;str[i]!='\0';i++)
	{
		Serial2_SendByte(str[i]);
	}
}

//发送字符串
void Serial2_SendStringLine(char *str)
{
	for(uint8_t i=0;str[i]!='\0';i++)
	{
		Serial2_SendByte(str[i]);
	}
	Serial2_SendByte('\r');
	Serial2_SendByte('\n');
}

// 重写printf底层的fputc，重定向输出到串口   ---- 编译进项目就行
//int fputc(int ch, FILE *f) {
//	Serial2_SendByte(ch);	// 将printf的底层重定向到自己的发送字节函数
//	return ch;
//}


//封装成接收字符串的模式 ----  字符串格式的（字符，字符串，整数，负数，浮点数..... ....）

//一句话的结束符号是\r\n  或 \n

uint8_t Serial2_RxByte=0x00;
char Serial2_RxString[256];//存储
uint16_t Serial2_RxIndex=0;//下标
uint8_t Serial2_RxComplateFlag=0;//1就是接收字符串完成，0就是接收字符串未完成


//获取字符串接收完成的标志位
uint8_t Serial2_GetComplateFlagStatus(void)
{
	return Serial2_RxComplateFlag;
}

//获取字符串
char * Serial2_GetRxString(void)
{
	//清除标志位
	Serial2_RxComplateFlag = 0;
	//下标回到0
	Serial2_RxIndex=0;
	
	return Serial2_RxString;
}



//接收数据的中断业务函数
void USART2_IRQHandler(void)
{	
	/*
	
	RXNE：读数据寄存器非空 (Read data register not empty) 
	当RDR移位寄存器中的数据被转移到USART_DR寄存器中，该位被硬件置位。如果
	USART_CR1寄存器中的RXNEIE为1，则产生中断。对USART_DR的读操作可以将该位清零。RXNE位也可以通过写入0来清除，只有在多缓存通讯中才推荐这种清除程序。 
	0：数据没有收到； 
	1：收到数据，可以读出
	*/
	
	if(USART_GetITStatus(USART2,USART_IT_RXNE)==SET)
	{
		Serial2_RxByte = USART_ReceiveData(USART2); //接收一个字节
		
		if(Serial2_RxIndex<256 && Serial2_RxByte!='\r' && Serial2_RxByte!='\n' && Serial2_RxComplateFlag!=1)
		{
			Serial2_RxString[Serial2_RxIndex++] = Serial2_RxByte;
		}
		
		if(Serial2_RxByte=='\n')
		{
			Serial2_RxString[Serial2_RxIndex++]='\0';//手动加一个结束符
			Serial2_RxComplateFlag=1;
		}		
		
		
		USART_ClearITPendingBit(USART2,USART_IT_RXNE);
	}
}

