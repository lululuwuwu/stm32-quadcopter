#include "stm32f10x.h"                  // Device header


//硬件：使用PWM输出模式，在通道 TIM3_CH1上

void MyTIM3_Init(void)
{

    //1.开启时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	//2.设置时钟来源为内部时钟
	TIM_InternalClockConfig(TIM3); //TIM3默认使用的就是内部时钟， 这句话可以不写
	
	//3.初始化
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitTypeStructure;
	TIM_TimeBaseInitTypeStructure.TIM_ClockDivision = TIM_CKD_DIV1; //时钟分频 --->没有用
	TIM_TimeBaseInitTypeStructure.TIM_CounterMode =TIM_CounterMode_Up; //向上计数
	TIM_TimeBaseInitTypeStructure.TIM_Period = 1000-1;//(ARR)Auto-Reload Register,   between 0x0000 and 0xFFFF. 
	TIM_TimeBaseInitTypeStructure.TIM_Prescaler = 72-1; //(PSC) prescaler value,between 0x0000 and 0xFFFF 
	TIM_TimeBaseInitTypeStructure.TIM_RepetitionCounter = 0;//重复计数器  only for TIM1 and TIM8. between 0x00 and 0xFF.
	
	TIM_TimeBaseInit(TIM3,&TIM_TimeBaseInitTypeStructure);
	
	//计数器溢出公式： 内部时钟72MHz / （PSC寄存器值+1）/(ARR寄存器值+1)  == 溢出的频率 
	// 溢出的频率是1000Hz == 72Mhz /72-1/1000-1 
	
	//TIM_TimeBaseInit 函数的最后会主动生成一个update event ，这里清除一个，计数器默认值就是0开始。
	TIM_ClearFlag(TIM3,TIM_FLAG_Update);


	//4.配置PWM输出
	TIM_OCInitTypeDef TIM_OCInitTypeStructure;
	//有些参数是用不上的，那么一般是给默认值，而不是空着！！！
	TIM_OCStructInit(&TIM_OCInitTypeStructure);
	
	
//	TIM_OCInitTypeStructure.TIM_OCIdleState = ; // only for TIM1 and TIM8.
//	TIM_OCInitTypeStructure.TIM_OutputNState = ;//only for TIM1 and TIM8.
//	TIM_OCInitTypeStructure.TIM_OCNIdleState = ;//only for TIM1 and TIM8.
//	TIM_OCInitTypeStructure.TIM_OCNPolarity = ;// only for TIM1 and TIM8.

/*
110：PWM模式1－ 在向上计数时，一旦TIMx_CNT<TIMx_CCR1时通道1为有效电平，否则为无效电平；在向下计数时，一旦TIMx_CNT>TIMx_CCR1时通道1为无效电平(OC1REF=0)，否则为有效电平(OC1REF=1)。 
111：PWM模式2－ 在向上计数时，一旦TIMx_CNT<TIMx_CCR1时通道1为无效电平，否则为有效电平；在向下计数时，一旦TIMx_CNT>TIMx_CCR1时通道1为有效电平，否则为无效电平。

*/
	
	TIM_OCInitTypeStructure.TIM_OCMode = TIM_OCMode_PWM1; //使用的是PWM1模式
	/*
	CC1P：输入/捕获1输出极性 (Capture/Compare 1 output polarity) CC1通道配置为输出： 
		0：OC1高电平有效 
		1：OC1低电平有效 
	
	*/
	TIM_OCInitTypeStructure.TIM_OCPolarity = TIM_OCPolarity_High;//极性 ,不翻转
	TIM_OCInitTypeStructure.TIM_OutputState = TIM_OutputState_Enable;  //开启输出通道
	TIM_OCInitTypeStructure.TIM_Pulse = 0;//(CCR捕获比较寄存器) Capture Compare Register. between 0x0000 and 0xFFFF 
	//占空比计算公式：  CCR /(ARR+1) 
	
	TIM_OC1Init(TIM3,&TIM_OCInitTypeStructure);
	
	//如果是使用的高级定时器还需要单独开启PWM输出！！！  
//	TIM_CtrlPWMOutputs(); 通用定时器不用设置
	
	//5.启动
	
	TIM_Cmd(TIM3,ENABLE);
	
	
	//6.开启GPIO输出
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_AF_PP; //模式：推挽复用输出
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOA,&GPIO_InitTypeStructure);
	
}

//修改CCR寄存器
void MyTIM3_SetCompare1( uint16_t Compare1)
{
	TIM_SetCompare1(TIM3,Compare1);
}


