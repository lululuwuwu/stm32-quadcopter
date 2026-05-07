#ifndef __DHT11_H
#define __DHT11_H



typedef struct
{
	uint8_t humi_int; //湿度的整数部分  
	uint8_t humi_float; //湿度的小数部分
	uint8_t temp_int; //温度的整数部分  
	uint8_t temp_float; //温度的小数部分
	uint8_t jiaoyan;//校验   
}DHT11_DataTypeDef;


//初始化

void DHT11_Init(void);

//STM32发起开始信号
void DHT11_Start(void);

//接收DHT11的响应信号: 1就是有响应，0就是没有响应
uint8_t DHT11_Ack(void);

//接收40个bit数据 : 1就是接收数据正确，0就是接收数据失败
uint8_t DHT11_Data5Byte(DHT11_DataTypeDef *data);



#endif
