#include "stm32f10x.h"                  // Device header


//硬件：通用定时器2 --->TIM2


uint32_t MyTIM2_Counter=0x00000000;

//初始化

void MyTIM2_Init(void)
{
	//1.开启时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);
	
	//2.设置时钟来源为内部时钟
	TIM_InternalClockConfig(TIM2); //TIM2默认使用的就是内部时钟， 这句话可以不写
	
	//3.初始化
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitTypeStructure;
	TIM_TimeBaseInitTypeStructure.TIM_ClockDivision = TIM_CKD_DIV1; //时钟分频 --->没有用
	TIM_TimeBaseInitTypeStructure.TIM_CounterMode =TIM_CounterMode_Up; //向上计数
	TIM_TimeBaseInitTypeStructure.TIM_Period = 10000-1;//(ARR)Auto-Reload Register,   between 0x0000 and 0xFFFF. 
	TIM_TimeBaseInitTypeStructure.TIM_Prescaler = 7200-1; //(PSC) prescaler value,between 0x0000 and 0xFFFF 
	TIM_TimeBaseInitTypeStructure.TIM_RepetitionCounter = 0;//重复计数器  only for TIM1 and TIM8. between 0x00 and 0xFF.
	
	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseInitTypeStructure);
	
	//计数器溢出公式： 内部时钟72MHz / （PSC寄存器值+1）/(ARR寄存器值+1)  == 溢出的频率 
	// 溢出的频率是1Hz == 72Mhz /7200-1/10000-1 
	
	//TIM_TimeBaseInit 函数的最后会主动生成一个update event ，这里清除一个，计数器默认值就是0开始。
	TIM_ClearFlag(TIM2,TIM_FLAG_Update);
	
	//5.配置中断
	
	TIM_ITConfig(TIM2,TIM_IT_Update,ENABLE);
	
	//6.NVIC
	NVIC_InitTypeDef NVIC_InitTypeStructure;
	NVIC_InitTypeStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitTypeStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitTypeStructure.NVIC_IRQChannelPreemptionPriority = 3;
	NVIC_InitTypeStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitTypeStructure);
	
	
	//4.启动
	
	TIM_Cmd(TIM2,ENABLE);
}


//获取值
uint32_t MyTIM2_GetValue(void)
{
	return MyTIM2_Counter;
}


//中断业务函数
void TIM2_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM2,TIM_IT_Update)==SET)
	{
		MyTIM2_Counter++;
		
		TIM_ClearITPendingBit(TIM2,TIM_IT_Update);
	}
	
}
