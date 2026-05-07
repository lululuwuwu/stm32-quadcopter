#include "stm32f10x.h"                  // Device header
#include "DHT11.h"
#include "Delay.h"

//硬件：单总线连接在PB12上


//写入单总线
void DHT11_WriteBit(uint8_t bitValue)
{
	GPIO_WriteBit(GPIOB,GPIO_Pin_12,(BitAction)bitValue);
}

//读取单总线bit
uint8_t DHT11_ReadBit(void)
{
	return GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_12);
}


//STM32给DHT11发送数据模式
void DHT11_Send_Mode(void)
{
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_Out_PP; //模式：推挽
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;	
	
	GPIO_Init(GPIOB,&GPIO_InitTypeStructure);
	
	//默认高电平
	GPIO_SetBits(GPIOB,GPIO_Pin_12);
}

//STM32接收DHT11数据模式
void DHT11_Rec_Mode(void)
{
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IPU; //模式：上拉
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOB,&GPIO_InitTypeStructure);
	
}



//初始化
void DHT11_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	
	DHT11_Send_Mode();//输出模式
	
}

//STM32发起开始信号
void DHT11_Start(void)
{
	//1.确保自己是输出模式
	DHT11_Send_Mode();
	//2.主机至少拉低18ms
	DHT11_WriteBit(0);
	Delay_ms(20);
	//3.主机拉高20-40us
	DHT11_WriteBit(1);
	Delay_us(35);
}

//接收DHT11的响应信号: 1就是有响应，0就是没有响应
uint8_t DHT11_Ack(void)
{
	//1.确保自己输入模式
	DHT11_Rec_Mode();
	//2.DHT响应信号40-50us
	Delay_us(20);
	if(DHT11_ReadBit()==0)
	{
		//消耗低电平
		while(DHT11_ReadBit()==0);
		//消耗掉高电平
		while(DHT11_ReadBit()==1);
		
		return 1;
	}
	return 0;
	
}

//接收1个字节
uint8_t DHT11_Data1Byte(void)
{
	uint8_t byte=0x00; //存储接收的数据
	
	for(uint8_t i=0;i<8;i++)
	{
		//稍微等一下
		Delay_us(3);
		//消耗掉低电平
		while(DHT11_ReadBit()==0);
		//稍微等一下判断传送的是1还是0  --- 范围（29-41）
		Delay_us(35);
		if(DHT11_ReadBit()==1) //如果传送的1，就保存到byte里面
		{
			while(DHT11_ReadBit()==1);//消耗掉高电平
			byte|= (0x80>>i);
		}
		//如果传送的0，byte里面默认的就是0；
	}
	
	return byte;
	
}


//接收40个bit数据 : 1就是接收数据正确，0就是接收数据失败
uint8_t DHT11_Data5Byte(DHT11_DataTypeDef *data)
{
	data->humi_int = DHT11_Data1Byte();
	data->humi_float = DHT11_Data1Byte();
	data->temp_int = DHT11_Data1Byte();
	data->temp_float = DHT11_Data1Byte();
	data->jiaoyan = DHT11_Data1Byte();

	if(data->humi_int+data->humi_float+data->temp_int+data->temp_float == data->jiaoyan)	
	{
		return 1;
	}
	
	return 0;	
}

