#include "stm32f10x.h"                  // Device header

//硬件：电位器连接在PA0上

//初始化
void MyADC_Init(void)
{
	//1.时钟开启
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1,ENABLE);
	
	/*
	ADC的输入时钟不得超过14MHz，它是由PCLK2经分频产生。
	*/
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);
	
	
	
	//2.配置GPIO
	GPIO_InitTypeDef GPIO_InitTypeStructure;
	GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_AIN;//模拟输入
	GPIO_InitTypeStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOA,&GPIO_InitTypeStructure);
	
	//3.配置ADC
	ADC_InitTypeDef ADC_InitTypeStructure;
	ADC_InitTypeStructure.ADC_ContinuousConvMode = DISABLE; //单次转换
	ADC_InitTypeStructure.ADC_DataAlign = ADC_DataAlign_Right; //12位数据存储在16位的寄存器里面，右对齐
	ADC_InitTypeStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;//没有外部触发就是软件触发
	ADC_InitTypeStructure.ADC_Mode = ADC_Mode_Independent;//使用单ADC功能，独立模式
	ADC_InitTypeStructure.ADC_NbrOfChannel = 1;//通道数 This parameter must range from 1 to 16.
	ADC_InitTypeStructure.ADC_ScanConvMode = DISABLE; //不扫描，只有1个通道
	
	ADC_Init(ADC1,&ADC_InitTypeStructure);
	
	//4.指定通道的顺序
	ADC_RegularChannelConfig(ADC1,ADC_Channel_0,1,ADC_SampleTime_55Cycles5);
	
	
	//检查内部的参考电压 --- 典型值是1.2v  不受供电电压的影响
	/*
	内部参照电压VREFINT和ADC1_IN17相连接
	注意： 必须设置TSVREFE位激活内部通道：ADC1_IN16(温度传感器)和ADC1_IN17(VREFINT)的转换。
	*/
	ADC_TempSensorVrefintCmd(ENABLE);
	
	
	//5.校准一下
	/*
	ADC有一个内置自校准模式。校准可大幅减小因内部电容器组的变化而造成的准精度误差
	
	注意： 1  建议在每次上电后执行一次校准。 
           2  启动校准前，ADC必须处于关电状态(ADON=’0’)超过至少两个ADC时钟周期。
	
	*/
	ADC_Cmd(ADC1,DISABLE);
	for(uint32_t i=0;i<999999;i++);
	ADC_Cmd(ADC1,ENABLE);
	
	//复位校准器
	/*
	
	RSTCAL：复位校准 (Reset calibration) 
	该位由软件设置并由硬件清除。在校准寄存器被初始化后该位将被清除。 
	0：校准寄存器已初始化； 
	1：初始化校准寄存器。 
	注：如果正在进行转换时设置RSTCAL，清除校准寄存器需要额外的周期
	*/
	ADC_ResetCalibration(ADC1);
	//等待初始化完成。
	while(ADC_GetResetCalibrationStatus(ADC1)!=RESET);
	
	//开始校准
	/*
	
	CAL：A/D校准 (A/D Calibration) 
	该位由软件设置以开始校准，并在校准结束时由硬件清除。 
	0：校准完成； 
	1：开始校准。 
	*/
	ADC_StartCalibration(ADC1);
	//等待校准完成
	while(ADC_GetCalibrationStatus(ADC1)!=RESET);
	
	
	//开启ADC
	ADC_Cmd(ADC1,ENABLE);
	
	
	
}

//获取数字值
uint16_t MyADC_GetDataValue(void)
{
	//软件触发转换
	ADC_SoftwareStartConvCmd(ADC1,ENABLE);
	//等待转换结束
	/*
	
	EOC：转换结束位 (End of conversion) 
	该位由硬件在(规则或注入)通道组转换结束时设置，由软件清除或由读取ADC_DR时清除 
	0：转换未完成； 
	1：转换完成
	*/
	while(ADC_GetFlagStatus(ADC1,ADC_FLAG_EOC)!=SET);
	
	//读取ADC_DR寄存器  --> 自动清除EOC标志
	return ADC_GetConversionValue(ADC1);	
}
	

//获取模拟电压值, 返回mv(毫伏)
uint16_t MyADC_GetAnalogValue(void)
{
	//1.先获取通道17的值（Vrefint 通道）
	ADC_RegularChannelConfig(ADC1,ADC_Channel_17,1,ADC_SampleTime_239Cycles5); //推荐采样时间是17.1μs----> 17.1/(1/14)
	
		//软件触发转换
	ADC_SoftwareStartConvCmd(ADC1,ENABLE);
	//等待转换结束
	/*
	
	EOC：转换结束位 (End of conversion) 
	该位由硬件在(规则或注入)通道组转换结束时设置，由软件清除或由读取ADC_DR时清除 
	0：转换未完成； 
	1：转换完成
	*/
	while(ADC_GetFlagStatus(ADC1,ADC_FLAG_EOC)!=SET);
	
	//读取ADC_DR寄存器  --> 自动清除EOC标志
	uint16_t Vrefint = ADC_GetConversionValue(ADC1);
	
	
	//2.再读取PA0上的值
	
	ADC_RegularChannelConfig(ADC1,ADC_Channel_0,1,ADC_SampleTime_55Cycles5);
	ADC_SoftwareStartConvCmd(ADC1,ENABLE);
	while(ADC_GetFlagStatus(ADC1,ADC_FLAG_EOC)!=SET);
	return ADC_GetConversionValue(ADC1)*1200/Vrefint;	
}	

