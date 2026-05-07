#include "stm32f10x.h"                  // Device header


/*
测量波形的频率和占空比步骤：
1.需要固定的计数频率  --- TIM1时基单元
2.需要通过GPIO输出被测频率 --- 只能接入通道1 （或 通道2）
3.触发输入源为 -- TI1FP1
4.从模式设置为复位功能
5.通过CCR1和CCR2的值计算频率和占空比



*/



//初始化TIM1为输入捕获PWMI模式
void MyTIM1_Init(void)
{
	//1.开启时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	//通过定时器的通道1测量频率和占空比
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; //模式： 只能是浮空输入
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOA,&GPIO_InitTypeStructure);
	
	//2.配置时基单元	

	//2.设置时钟来源为内部时钟
	TIM_InternalClockConfig(TIM1); //TIM3默认使用的就是内部时钟， 这句话可以不写
	
	//3.初始化
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitTypeStructure;
	TIM_TimeBaseInitTypeStructure.TIM_ClockDivision = TIM_CKD_DIV1; //时钟分频 --->没有用
	TIM_TimeBaseInitTypeStructure.TIM_CounterMode =TIM_CounterMode_Up; //向上计数
	TIM_TimeBaseInitTypeStructure.TIM_Period = 65536-1;//(ARR)Auto-Reload Register,   between 0x0000 and 0xFFFF. 
	TIM_TimeBaseInitTypeStructure.TIM_Prescaler = 720-1; //(PSC) prescaler value,between 0x0000 and 0xFFFF 
	TIM_TimeBaseInitTypeStructure.TIM_RepetitionCounter = 0;//重复计数器  only for TIM1 and TIM8. between 0x00 and 0xFF.
	
	TIM_TimeBaseInit(TIM1,&TIM_TimeBaseInitTypeStructure);
	
	/*
	ARR:配置为最大即可。  ---希望记录的数越多越好
	PSC:配置依赖要测量的频率  --- 最好CNT至少能记录到100个数
		
	被测量的频率范围计算公式： 
		PSC不分频：	72000000Hz  (计数为0) ~   1,098.6Hz (计数为65536)  ==> (至少记录100个数) ==>720000Hz ~1,098.6Hz (计数为65536)
		PSC分最大： 1,098.6Hz  (计数为0) ~   0.01676Hz (计数为65536)  ==> (至少记录100个数) ==>11Hz ~0.01676Hz
		
		假设需要测量的频率是1KHz ,那么分频值要设置多少比较合适？ PSC小于等于720(72MHz/(100*1KHz)) , 大于等于2 (72MHz/(1000*65536))。
	
	*/
	
	//PWMI输入捕获模式配置
	TIM_ICInitTypeDef TIM_ICInitTypeStructure;
	TIM_ICInitTypeStructure.TIM_Channel = TIM_Channel_1; //捕获的输入通道
	TIM_ICInitTypeStructure.TIM_ICFilter = 0xA; // 输入滤波 between 0x0 and 0xF 
	TIM_ICInitTypeStructure.TIM_ICPolarity = TIM_ICPolarity_Rising; //上升沿有效
	TIM_ICInitTypeStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1; //如果输入的频率太快，可以进行分频
	TIM_ICInitTypeStructure.TIM_ICSelection = TIM_ICSelection_DirectTI; // 直连
	
	TIM_PWMIConfig(TIM1,&TIM_ICInitTypeStructure);
	
	//TIM 配置从模式控制器为复位模式
	
	TIM_SelectSlaveMode(TIM1,TIM_SlaveMode_Reset);
	
	//配置从模式的触发型号来源
	TIM_SelectInputTrigger(TIM1,TIM_TS_TI1FP1);
	
	//启动
	TIM_Cmd(TIM1,ENABLE);
	

}


//获取频率
uint32_t MyTIM1_GetFreq(void)
{
		return 100000/(TIM_GetCapture1(TIM1)+1); //公式： 计数频率/计数值
}


//获取占空比: 百分比
uint8_t MyTIM1_GetDuty(void)
{
	return (TIM_GetCapture2(TIM1)+1)*100/(TIM_GetCapture1(TIM1)+1)   ; //公式： (CRR2+1) / (CRR1+1)
}




