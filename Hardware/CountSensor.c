#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"

//硬件连接：连接在PB12.

static uint32_t Counter_Sensor_Value_Data=0x00;

//初始化硬件
void CountSensor_Init(void)
{
	//1.开启GPIOB的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	//开启AFIO时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);
	
	//2.配置GPIO
	
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode =GPIO_Mode_IPU ; //模式：上拉（√），下拉（x），浮空（√）
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOB,&GPIO_InitTypeStructure);
	
	//3.AFIO选择中断引脚
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB,GPIO_PinSource12);
	
	//4.EXTI配置 --- 不需要开启时钟
	EXTI_InitTypeDef EXTI_InitTypeStructure;
	EXTI_InitTypeStructure.EXTI_Line = EXTI_Line12; //12号线
	EXTI_InitTypeStructure.EXTI_LineCmd = ENABLE; //开启
	EXTI_InitTypeStructure.EXTI_Mode = EXTI_Mode_Interrupt; //模式：中断CPU处理模式
	EXTI_InitTypeStructure.EXTI_Trigger  =EXTI_Trigger_Rising;  //触发：可以是上升沿，下降沿，或双边沿配置
	
	EXTI_Init(&EXTI_InitTypeStructure);
	

	
	//6.设置中断优先级
	NVIC_InitTypeDef NVIC_InitTypeStructure;
	NVIC_InitTypeStructure.NVIC_IRQChannel = EXTI15_10_IRQn; //中断请求通道
	NVIC_InitTypeStructure.NVIC_IRQChannelCmd = ENABLE;//开启
	NVIC_InitTypeStructure.NVIC_IRQChannelPreemptionPriority = 1; //抢占优先级 --- 可以中断嵌套
	NVIC_InitTypeStructure.NVIC_IRQChannelSubPriority = 0; //响应优先级--- 可以优先排队
	
	NVIC_Init(&NVIC_InitTypeStructure);
	
	//7.当出现中断的时候计数器加1？
	
	
	
}

//获取计数值
uint32_t CountSensor_GetValue(void)
{
	return Counter_Sensor_Value_Data;
}


//中断执行的函数，这个来自于 startup_stm32f10x_md.s
//不能传入参数，也没有返回值
//这个函数在出现中断的时候会自动调用，不要主动调!!！
void EXTI15_10_IRQHandler(void)
{
	
	//因为EXTI15_10 包含了 10,11,12,13,14,15 通道，因此要判断一下
	if(EXTI_GetITStatus(EXTI_Line12)==SET)
	{
		Delay_ms(10);
		if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_12)==SET)
		{
			while(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_12)==SET);
		}
		Delay_ms(10);
		Counter_Sensor_Value_Data++;
		
		/*
		
		PRx: 挂起位 (Pending bit) 
			0：没有发生触发请求 
			1：发生了选择的触发请求 
			当在外部中断线上发生了选择的边沿事件，该位被置’1’。在该位中写入’1’可以清除它，也可以通过改变边沿检测的极性清除。
		*/
		EXTI_ClearITPendingBit(EXTI_Line12);//一定要清楚中断标志位
		
		
		
	}
	
	
	
}

