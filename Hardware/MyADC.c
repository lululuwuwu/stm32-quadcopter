#include "MyADC.h"

#define ADC_FULL_SCALE 4095U
#define RC_VALUE_MIN 1000
#define RC_VALUE_MAX 2000
#define RC_VALUE_RANGE (RC_VALUE_MAX - RC_VALUE_MIN)

/* DMA 循环写入的摇杆原始 ADC 值。
 * 数组顺序必须和 ADC_RegularChannelConfig 的规则通道顺序保持一致。
 */
static volatile uint16_t StickADCRaw[STICK_AXIS_COUNT];

/* 把 12 位 ADC 采样值线性换算为遥控器常用的 1000~2000 控制量。 */
static int16_t StickADC_MapToRC(uint16_t adc_value)
{
    if (adc_value > ADC_FULL_SCALE)
    {
        adc_value = ADC_FULL_SCALE;
    }

    return (int16_t)(RC_VALUE_MIN +
                     ((uint32_t)adc_value * RC_VALUE_RANGE + ADC_FULL_SCALE / 2U) / ADC_FULL_SCALE);
}

void StickADC_Init(void)
{
    GPIO_InitTypeDef gpio_init;
    ADC_InitTypeDef adc_init;
    DMA_InitTypeDef dma_init;

    /* 打开 GPIOA、ADC1、DMA1 时钟。
     * ADC 时钟由 PCLK2 分频得到，STM32F103 的 ADC 时钟不能超过 14MHz。
     */
    RCC_APB2PeriphClockCmd(STICK_RCC | STICK_ADC_RCC, ENABLE);
    RCC_AHBPeriphClockCmd(STICK_DMA_RCC, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* PA0~PA3 接摇杆电位器输出，配置为模拟输入。 */
    gpio_init.GPIO_Mode = GPIO_Mode_AIN;
    gpio_init.GPIO_Pin = STICK_THR_PIN | STICK_YAW_PIN | STICK_PITH_PIN | STICK_ROLL_PIN;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(STICK_GPIO, &gpio_init);

    /* ADC1 的 DMA 固定使用 DMA1_Channel1。
     * 循环模式会不断刷新 StickADCRaw，不需要任务里手动等待每次转换结束。
     */
    DMA_DeInit(DMA1_Channel1);
    dma_init.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    dma_init.DMA_MemoryBaseAddr = (uint32_t)StickADCRaw;
    dma_init.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma_init.DMA_BufferSize = STICK_AXIS_COUNT;
    dma_init.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma_init.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma_init.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma_init.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    dma_init.DMA_Mode = DMA_Mode_Circular;
    dma_init.DMA_Priority = DMA_Priority_High;
    dma_init.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &dma_init);

    /* 扫描 + 连续转换：ADC 会按规则通道 1~4 反复采样四个摇杆轴。 */
    adc_init.ADC_Mode = ADC_Mode_Independent;
    adc_init.ADC_ScanConvMode = ENABLE;
    adc_init.ADC_ContinuousConvMode = ENABLE;
    adc_init.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    adc_init.ADC_DataAlign = ADC_DataAlign_Right;
    adc_init.ADC_NbrOfChannel = STICK_AXIS_COUNT;
    ADC_Init(ADC1, &adc_init);

    /* 规则通道顺序决定 StickADCRaw[] 的下标含义：
     * [0] 油门 PA1, [1] 偏航 PA0, [2] 俯仰 PA3, [3] 翻滚 PA2。
     */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 2, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 3, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 4, ADC_SampleTime_55Cycles5);

    ADC_DMACmd(ADC1, ENABLE);

    /* 上电后执行一次 ADC 自校准，减小内部误差带来的采样偏差。 */
    ADC_Cmd(ADC1, DISABLE);
    for (volatile uint32_t i = 0; i < 10000; i++)
    {
    }
    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1) != RESET)
    {
    }

    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1) != RESET)
    {
    }

    /* 先启动 DMA，再启动 ADC 软件转换，之后采样会自动循环进行。 */
    DMA_Cmd(DMA1_Channel1, ENABLE);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

uint16_t StickADC_GetRaw(StickAxisTypeDef axis)
{
    if (axis >= STICK_AXIS_COUNT)
    {
        return 0;
    }

    return StickADCRaw[axis];
}

int16_t StickADC_GetRCValue(StickAxisTypeDef axis)
{
    return StickADC_MapToRC(StickADC_GetRaw(axis));
}
