#include "stm32f10x.h"                  // Device header


//硬件： 控制引脚IO链接在PA1上 ， 默认低电平就会响


//初始化
void Buzzer_Init(void)
{
	//1.开启时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE)   ;//代码补全的快捷键 Ctrl + Alt + Space
	
	//2.配置GPIO
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_Out_PP; //模式:推挽 
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_1; //引脚
	GPIO_InitTypeStructure.GPIO_Speed =GPIO_Speed_50MHz;//速度
	GPIO_Init(GPIOA,&GPIO_InitTypeStructure);
	
	//3.默认状态
	GPIO_SetBits(GPIOA,GPIO_Pin_1); //不响
	
}
	
//响
void Buzzer_On(void)
{
	GPIO_ResetBits(GPIOA,GPIO_Pin_1);
}
//灭
void Buzzer_Off(void)
{
	GPIO_SetBits(GPIOA,GPIO_Pin_1);
}
