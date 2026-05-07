#include "stm32f10x.h"                  // Device header

//使用硬件SPI1

//SS片选： PA4  -- 软件控制NSS(推挽)
//SCK时钟：PA5   -- 复用推挽
//MOSI主机输出从机输入：PA7   -- 复用推挽
//MISO主机输入从机输出：PA6   -- 浮空输入或带上拉输入


//片选由软件控制
void MyHSPI_SS_W(uint8_t bitValue)
{
	GPIO_WriteBit(GPIOA,GPIO_Pin_4,(BitAction)bitValue);
}



void MyHSPI_Init(void)
{
	//开启时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1,ENABLE);
	
	//初始化GPIO
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_4;//SS片选
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOA,&GPIO_InitTypeStructure);
	
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_5|GPIO_Pin_7; //SCK时钟,MOSI
	
	GPIO_Init(GPIOA,&GPIO_InitTypeStructure);
	
	
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_6;//MISO	

	GPIO_Init(GPIOA,&GPIO_InitTypeStructure);	
	
	//配置SPI1
	SPI_InitTypeDef SPI_InitTypeStructure;
	SPI_InitTypeStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2; //SPI1在PCLK2上（72MHz）--> 分频后的值要比W25Q64慢就行
	SPI_InitTypeStructure.SPI_CPHA = SPI_CPHA_1Edge; //时钟相位：0   第一个边沿采样 clock active edge for the bit capture
	SPI_InitTypeStructure.SPI_CPOL = SPI_CPOL_Low; //时钟极性：0 默认低电平
	SPI_InitTypeStructure.SPI_CRCPolynomial = 7; //用不上，填写默认值 CRC calculation
	SPI_InitTypeStructure.SPI_DataSize = SPI_DataSize_8b; //每次8bit
	SPI_InitTypeStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;//全双工双线SPI
	SPI_InitTypeStructure.SPI_FirstBit = SPI_FirstBit_MSB;//高位先行
	SPI_InitTypeStructure.SPI_Mode = SPI_Mode_Master;//模式：主机
	SPI_InitTypeStructure.SPI_NSS = SPI_NSS_Soft; //我自己用软件管理，不用硬件的
	
	SPI_Init(SPI1,&SPI_InitTypeStructure);
	
	//启用SPI
	SPI_Cmd(SPI1,ENABLE);
	
	//控制默认状态 
	MyHSPI_SS_W(1);//不选中
	
}


void MyHSPI_Start(void)
{
	MyHSPI_SS_W(0);
}

void MyHSPI_Stop(void)
{
	MyHSPI_SS_W(1);
}

//交换字节

uint8_t MyHSPI_SwapByte(uint8_t byteValue)
{
	
	
	//发送数据：等待TXE为空
	/*
	TXE：发送缓冲为空 (Transmit buffer empty) 
		0：发送缓冲非空； 
		1：发送缓冲为空。
	*/
	while(SPI_I2S_GetFlagStatus(SPI1,SPI_I2S_FLAG_TXE)!=SET);
	SPI_I2S_SendData(SPI1,byteValue);
	
	//接收数据：等待RXNE置位
	/*
	RXNE：接收缓冲非空 (Receive buffer not empty) 
	   0：接收缓冲为空； 
	   1：接收缓冲非空。
	*/
	while(SPI_I2S_GetFlagStatus(SPI1,SPI_I2S_FLAG_RXNE)!=SET);
	 

	
	return SPI_I2S_ReceiveData(SPI1);	
}
