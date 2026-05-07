#include "stm32f10x.h"                  // Device header

//SS片选： PB12  -- 推挽
//SCK时钟：PB13   -- 推挽
//MOSI主机输出从机输入：PB14   -- 推挽
//MISO主机输入从机输出：PB15   -- 上拉


void MySPI_SS_W(uint8_t bitValue)
{
	GPIO_WriteBit(GPIOB,GPIO_Pin_12,(BitAction)bitValue);
}

void MySPI_SCK_W(uint8_t bitValue)
{
	GPIO_WriteBit(GPIOB,GPIO_Pin_13,(BitAction)bitValue);
}


void MySPI_MOSI_W(uint8_t bitValue)
{
	GPIO_WriteBit(GPIOB,GPIO_Pin_14,(BitAction)bitValue);
}

uint8_t  MySPI_MISO_R(void)
{
	return GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_15);
}




void MySPI_Init(void)
{
	//开启时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	
	//初始化GPIO
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOB,&GPIO_InitTypeStructure);
	
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_15;	

	GPIO_Init(GPIOB,&GPIO_InitTypeStructure);	
	
	//控制默认状态 --- 模式0 （时钟极性0；时钟相位0）
	MySPI_SS_W(1);//不选中
	MySPI_SCK_W(0);	//时钟极性0
	
}


void MySPI_Start(void)
{
	MySPI_SS_W(0);
}

void MySPI_Stop(void)
{
	MySPI_SS_W(1);
}

//交换字节

uint8_t MySPI_SwapByte(uint8_t byteValue)
{
	for(uint8_t i=0;i<8;i++)
	{
		//主机放置bit7 ， 从机也会对应的放置bit7
		MySPI_MOSI_W(byteValue&0x80); //如果是0x80 就发送高电平， 如果是0x00就发送低电平
		byteValue<<=1; //左移一位，空出最低位。
		//拉高时钟
		MySPI_SCK_W(1);
		//主机读取bit7 ， 从机自动读取bit7
		byteValue|=MySPI_MISO_R(); //读取后的值放置在最低位
		//拉低时钟
		MySPI_SCK_W(0);
	}
	return byteValue;
}



