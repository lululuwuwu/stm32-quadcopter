#include "stm32f10x.h"                  // Device header

//硬件：继电器连接在PA3. 继电器高电平触发 ， 风扇连接在COM 和 NO 上

//开
void Fan_On(void)
{
	GPIO_SetBits(GPIOA,GPIO_Pin_3);
}

//关
void Fan_Off(void)
{
	GPIO_ResetBits(GPIOA,GPIO_Pin_3);
}
//切换
void Fan_Toggle(void)
{
//	GPIO_Write(GPIOA,GPIOA->ODR^GPIO_Pin_3); 
	GPIO_WriteBit(GPIOA,GPIO_Pin_3,(BitAction) (GPIO_ReadOutputDataBit(GPIOA,GPIO_Pin_3) ^ 0x01)); 
	
}



//初始化
void Fan_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOA,&GPIO_InitTypeStructure);
	
	//默认状态 ---关闭灯
	
	Fan_Off();
}




