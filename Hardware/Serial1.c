#include "board.h"


//硬件：UART1： Tx-- PA9 ,RX-PA10

QueueHandle_t xQueueSerial1; //串口队列句柄

//初始化
void Serial1_Init(void)
{
	//1.开启时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_AF_PP; //模式：推挽复用输出
	GPIO_InitTypeStructure.GPIO_Pin = Serial1_Tx_Pin;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(Serial1_GPIO,&GPIO_InitTypeStructure);
	
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IPU; //模式：上拉输入 或 浮空输入
	GPIO_InitTypeStructure.GPIO_Pin = Serial1_Rx_Pin; //接收线
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(Serial1_GPIO,&GPIO_InitTypeStructure);
	
	
	//2.串口初始化
	USART_InitTypeDef USART_InitTypeStructure;
	USART_InitTypeStructure.USART_BaudRate = 115200; // 波特率：直接填写即可
	USART_InitTypeStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//不使用硬件流控	
	USART_InitTypeStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;//发送模式 ， 接收模式要一起开
	USART_InitTypeStructure.USART_Parity = USART_Parity_No; //一般不用奇偶校验
	USART_InitTypeStructure.USART_StopBits = USART_StopBits_1;// 一个停止位
	USART_InitTypeStructure.USART_WordLength = USART_WordLength_8b;// 八位字长
	
	USART_Init(USART1,&USART_InitTypeStructure);	
	
	
	//3.开启中断
	USART_ITConfig(USART1,USART_IT_RXNE,ENABLE);
	
	//NVIC配置
	NVIC_InitTypeDef NVIC_InitTypeStructure;
	NVIC_InitTypeStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitTypeStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitTypeStructure.NVIC_IRQChannelPreemptionPriority = 3;
	NVIC_InitTypeStructure.NVIC_IRQChannelSubPriority = 0;
	
	NVIC_Init(&NVIC_InitTypeStructure);
	
	
	//4.启动
	USART_Cmd(USART1,ENABLE);	

}

//发送字节
void Serial1_SendByte(uint8_t byteValue)
{
	//先等待TXE 发送数据寄存器为空，再写入USART_DR寄存器
	/*
	TXE:发送数据寄存器空 (Transmit data register empty) 
	当TDR寄存器中的数据被硬件转移到移位寄存器的时候，该位被硬件置位。如果USART_CR1寄存器中的TXEIE为1，则产生中断。对USART_DR的写操作，将该位清零。 
	0：数据还没有被转移到移位寄存器； 
	1：数据已经被转移到移位寄存器
	
	*/
	
	while(USART_GetFlagStatus(USART1,USART_FLAG_TXE)!=SET);
	
	USART_SendData(USART1,byteValue);
}
	

//发送字符串
void Serial1_SendString(char *str)
{
	for(uint8_t i=0;str[i]!='\0';i++)
	{
		Serial1_SendByte(str[i]);
	}
}

//发送字符串
void Serial1_SendStringLine(char *str)
{
	for(uint8_t i=0;str[i]!='\0';i++)
	{
		Serial1_SendByte(str[i]);
	}
	Serial1_SendByte('\r');
	Serial1_SendByte('\n');
}

//日志函数
void Serial1_SendLog(char *format, ...)
{
	char str[uxQueueSerial1ItemSize];
	va_list arg;
	va_start(arg, format);
	vsprintf(str, format, arg);
	va_end(arg);

	xQueueSendToBack(xQueueSerial1,str,0); //发送到队列
}


// 重写printf底层的fputc，重定向输出到串口   ---- 编译进项目就行
int fputc(int ch, FILE *f) {
	Serial1_SendByte(ch);	// 将printf的底层重定向到自己的发送字节函数
	return ch;
}


//封装成接收字符串的模式 ----  字符串格式的（字符，字符串，整数，负数，浮点数..... ....）

//一句话的结束符号是\r\n  或 \n

uint8_t Serial1_RxByte=0x00;
char Serial1_RxString[256];//存储
uint16_t Serial1_RxIndex=0;//下标
uint8_t Serial1_RxComplateFlag=0;//1就是接收字符串完成，0就是接收字符串未完成


//获取字符串接收完成的标志位
uint8_t Serial1_GetComplateFlagStatus(void)
{
	return Serial1_RxComplateFlag;
}

//获取字符串
char * Serial1_GetRxString(void)
{
	//清除标志位
	Serial1_RxComplateFlag = 0;
	//下标回到0
	Serial1_RxIndex=0;
	
	return Serial1_RxString;
}



//接收数据的中断业务函数
void USART1_IRQHandler(void)
{	
	/*
	
	RXNE：读数据寄存器非空 (Read data register not empty) 
	当RDR移位寄存器中的数据被转移到USART_DR寄存器中，该位被硬件置位。如果
	USART_CR1寄存器中的RXNEIE为1，则产生中断。对USART_DR的读操作可以将该位清零。RXNE位也可以通过写入0来清除，只有在多缓存通讯中才推荐这种清除程序。 
	0：数据没有收到； 
	1：收到数据，可以读出
	*/
	
	if(USART_GetITStatus(USART1,USART_IT_RXNE)==SET)
	{
		Serial1_RxByte = USART_ReceiveData(USART1); //接收一个字节
		
		if(Serial1_RxIndex<256 && Serial1_RxByte!='\r' && Serial1_RxByte!='\n' && Serial1_RxComplateFlag!=1)
		{
			Serial1_RxString[Serial1_RxIndex++] = Serial1_RxByte;
		}
		
		if(Serial1_RxByte=='\n')
		{
			Serial1_RxString[Serial1_RxIndex++]='\0';//手动加一个结束符
			Serial1_RxComplateFlag=1;
		}		
		
		
		USART_ClearITPendingBit(USART1,USART_IT_RXNE);
	}
	

	
}

