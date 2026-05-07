#include "stm32f10x.h"                  // Device header




void MyIWDG_Init(void)
{
	//1.如果独立看门狗已经由硬件选项或软件启动，LSI振荡器将被强制在打开状态，并且不能被关闭。
	//在LSI振荡器稳定后，时钟供应给IWDG。
	
	RCC_LSICmd(ENABLE);
	
	/*
	LSIRDY：内部低速振荡器就绪 (Internal low-speed oscillator ready) 
	由硬件置’1或清’0’来指示内部40kHz RC振荡器是否就绪。在LSION清零后，3个内部40kHz RC振荡器的周期后LSIRDY被清零。 
	0：内部40kHz RC振荡器时钟未就绪； 
	1：内部40kHz RC振荡器时钟就绪。
	*/
	while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY)!=SET);
	
	//配置寄存器
	
	//IWDG_PR和IWDG_RLR寄存器具有写保护功能。要修改这两个寄存器的值，必须先向IWDG_KR寄存器中写入0x5555。
	
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);//IWDG_KR寄存器中写入0x5555。
	
	//先设置预分频值 256分频
	/*
	PVU: 看门狗预分频值更新 (Watchdog prescaler value update) 
	此位由硬件置’1’用来指示预分频值的更新正在进行中。
	当在VDD域中的预分频值更新结束后，此位由硬件清’0’(最多需5个40kHz的RC周期)。预分频值只有在PVU位被清’0’后才可更新。
	*/
	while(IWDG_GetFlagStatus(IWDG_FLAG_PVU)!=RESET);
	
	IWDG_SetPrescaler(IWDG_Prescaler_256);
	
	//设置从转载寄存器
	/*
	RVU: 看门狗计数器重装载值更新 (Watchdog counter reload value update) 
	此位由硬件置’1’用来指示重装载值的更新正在进行中。当在VDD域中的重装载更新结束后，
	此位由硬件清’0’(最多需5个40kHz的RC周期)。重装载值只有在RVU位被清’0’后才可更新。
	*/
	while(IWDG_GetFlagStatus(IWDG_FLAG_RVU)!=RESET);
	IWDG_SetReload(1563-1);//
	//需求是要求超过10秒就复位
	//公式：10秒/(1/40KHz * 256)  -1 
	
	//开启：在键寄存器(IWDG_KR)中写入0xCCCC，开始启用独立看门狗；此时计数器开始从其复位值0xFFF递减计数
	IWDG_Enable(); //0xCCCC 写入IWDG->KR寄存器 
	
	//喂一次够，就可以从我自己设计的Reload值开始计数
	
	//无论何时，只要在键寄存器IWDG_KR中写入0xAAAA， IWDG_RLR中的值就会被重新加载到计数器，从而避免产生看门狗复位 。
	IWDG_ReloadCounter();// 0xAAAA 写入IWDG->KR寄存器 
	
}


//10秒内必须喂狗，否则复位
void MyIWDG_ReloadCounter(void)
{
	IWDG_ReloadCounter();
}



