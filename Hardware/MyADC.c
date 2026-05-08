#include "MyADC.h"
#include "MyFLASH.h"
#include <string.h>

#define ADC_FULL_SCALE 4095U
#define RC_VALUE_MIN 1000
#define RC_VALUE_MAX 2000
#define RC_VALUE_CENTER ((RC_VALUE_MIN + RC_VALUE_MAX) / 2)

#define STICK_FILTER_WINDOW_SIZE 8U
#define STICK_CAL_FLASH_ADDRESS 0x0800FC00U
#define STICK_CAL_MAGIC 0x5243414CU
#define STICK_CAL_VERSION 1U
#define STICK_CAL_MIN_SPAN 200U
#define STICK_CAL_CENTER_MARGIN 20U

typedef struct
{
    uint16_t min;
    uint16_t center;
    uint16_t max;
} StickAxisCalibrationTypeDef;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    StickAxisCalibrationTypeDef axis[STICK_AXIS_COUNT];
    uint32_t checksum;
} StickCalibrationFlashTypeDef;

/* DMA writes raw ADC values in the same order as ADC_RegularChannelConfig. */
static volatile uint16_t StickADCRaw[STICK_AXIS_COUNT];
static uint16_t StickADCWindow[STICK_AXIS_COUNT][STICK_FILTER_WINDOW_SIZE];
static uint32_t StickADCWindowSum[STICK_AXIS_COUNT];
static uint16_t StickADCFiltered[STICK_AXIS_COUNT];
static uint8_t StickADCWindowIndex;
static uint8_t StickADCWindowCount;

static StickAxisCalibrationTypeDef StickADCCalibration[STICK_AXIS_COUNT];
static StickAxisCalibrationTypeDef StickADCCalibrationBackup[STICK_AXIS_COUNT];
static uint8_t StickADCCalibrating;

static void StickADC_SetDefaultCalibration(void)
{
    uint8_t axis;

    for (axis = 0; axis < STICK_AXIS_COUNT; axis++)
    {
        StickADCCalibration[axis].min = 0;
        StickADCCalibration[axis].center = ADC_FULL_SCALE / 2U;
        StickADCCalibration[axis].max = ADC_FULL_SCALE;
    }
}

static uint32_t StickADC_CalcChecksum(const uint8_t *data, uint16_t length)
{
    uint16_t i;
    uint32_t checksum = 0;

    for (i = 0; i < length; i++)
    {
        checksum += data[i];
        checksum = (checksum << 1) | (checksum >> 31);
    }

    return checksum;
}

static uint8_t StickADC_IsAxisCalibrationValid(const StickAxisCalibrationTypeDef *cal)
{
    if (cal->max <= cal->min)
    {
        return 0;
    }

    if ((uint16_t)(cal->max - cal->min) < STICK_CAL_MIN_SPAN)
    {
        return 0;
    }

    if (cal->center <= (uint16_t)(cal->min + STICK_CAL_CENTER_MARGIN))
    {
        return 0;
    }

    if ((uint16_t)(cal->center + STICK_CAL_CENTER_MARGIN) >= cal->max)
    {
        return 0;
    }

    return 1;
}

static uint8_t StickADC_IsCalibrationValid(const StickAxisCalibrationTypeDef *cal)
{
    uint8_t axis;

    for (axis = 0; axis < STICK_AXIS_COUNT; axis++)
    {
        if (!StickADC_IsAxisCalibrationValid(&cal[axis]))
        {
            return 0;
        }
    }

    return 1;
}

static uint8_t StickADC_LoadCalibration(void)
{
    StickCalibrationFlashTypeDef flash_cal;
    uint32_t checksum;

    MyFLASH_ReadByteArray(STICK_CAL_FLASH_ADDRESS,
                          (uint8_t *)&flash_cal,
                          sizeof(flash_cal));

    checksum = StickADC_CalcChecksum((const uint8_t *)&flash_cal,
                                     sizeof(flash_cal) - sizeof(flash_cal.checksum));

    if (flash_cal.magic != STICK_CAL_MAGIC ||
        flash_cal.version != STICK_CAL_VERSION ||
        flash_cal.size != sizeof(flash_cal) ||
        flash_cal.checksum != checksum ||
        !StickADC_IsCalibrationValid(flash_cal.axis))
    {
        StickADC_SetDefaultCalibration();
        return 0;
    }

    memcpy(StickADCCalibration, flash_cal.axis, sizeof(StickADCCalibration));
    return 1;
}

static uint8_t StickADC_SaveCalibration(void)
{
    StickCalibrationFlashTypeDef flash_cal;
    FLASH_Status status;

    if (!StickADC_IsCalibrationValid(StickADCCalibration))
    {
        return 0;
    }

    memset(&flash_cal, 0xFF, sizeof(flash_cal));
    flash_cal.magic = STICK_CAL_MAGIC;
    flash_cal.version = STICK_CAL_VERSION;
    flash_cal.size = sizeof(flash_cal);
    memcpy(flash_cal.axis, StickADCCalibration, sizeof(StickADCCalibration));
    flash_cal.checksum = StickADC_CalcChecksum((const uint8_t *)&flash_cal,
                                               sizeof(flash_cal) - sizeof(flash_cal.checksum));

    status = MyFLASH_ErasePage(STICK_CAL_FLASH_ADDRESS);
    if (status != FLASH_COMPLETE)
    {
        return 0;
    }

    status = MyFLASH_ByteArrayProgram(STICK_CAL_FLASH_ADDRESS,
                                      (uint8_t *)&flash_cal,
                                      sizeof(flash_cal));
    return status == FLASH_COMPLETE;
}

static int16_t StickADC_MapLinearToRC(uint16_t adc_value,
                                      uint16_t adc_min,
                                      uint16_t adc_max,
                                      int16_t rc_min,
                                      int16_t rc_max)
{
    uint32_t numerator;
    uint16_t adc_span;
    int16_t rc_span;

    if (adc_max <= adc_min)
    {
        return rc_min;
    }

    if (adc_value <= adc_min)
    {
        return rc_min;
    }

    if (adc_value >= adc_max)
    {
        return rc_max;
    }

    adc_span = adc_max - adc_min;
    rc_span = rc_max - rc_min;
    numerator = (uint32_t)(adc_value - adc_min) * (uint16_t)rc_span + adc_span / 2U;

    return (int16_t)(rc_min + numerator / adc_span);
}

static int16_t StickADC_MapToRC(StickAxisTypeDef axis, uint16_t adc_value)
{
    StickAxisCalibrationTypeDef *cal;

    if (axis >= STICK_AXIS_COUNT)
    {
        return RC_VALUE_MIN;
    }

    if (adc_value > ADC_FULL_SCALE)
    {
        adc_value = ADC_FULL_SCALE;
    }

    cal = &StickADCCalibration[axis];

    if (adc_value < cal->center)
    {
        return StickADC_MapLinearToRC(adc_value, cal->min, cal->center,
                                      RC_VALUE_MIN, RC_VALUE_CENTER);
    }

    return StickADC_MapLinearToRC(adc_value, cal->center, cal->max,
                                  RC_VALUE_CENTER, RC_VALUE_MAX);
}

void StickADC_Init(void)
{
    GPIO_InitTypeDef gpio_init;
    ADC_InitTypeDef adc_init;
    DMA_InitTypeDef dma_init;

    RCC_APB2PeriphClockCmd(STICK_RCC | STICK_ADC_RCC, ENABLE);
    RCC_AHBPeriphClockCmd(STICK_DMA_RCC, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    gpio_init.GPIO_Mode = GPIO_Mode_AIN;
    gpio_init.GPIO_Pin = STICK_THR_PIN | STICK_YAW_PIN | STICK_PITH_PIN | STICK_ROLL_PIN;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(STICK_GPIO, &gpio_init);

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

    adc_init.ADC_Mode = ADC_Mode_Independent;
    adc_init.ADC_ScanConvMode = ENABLE;
    adc_init.ADC_ContinuousConvMode = ENABLE;
    adc_init.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    adc_init.ADC_DataAlign = ADC_DataAlign_Right;
    adc_init.ADC_NbrOfChannel = STICK_AXIS_COUNT;
    ADC_Init(ADC1, &adc_init);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 2, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 3, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 4, ADC_SampleTime_55Cycles5);

    ADC_DMACmd(ADC1, ENABLE);

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

    StickADC_SetDefaultCalibration();
    StickADC_LoadCalibration();

    memset(StickADCWindow, 0, sizeof(StickADCWindow));
    memset(StickADCWindowSum, 0, sizeof(StickADCWindowSum));
    memset(StickADCFiltered, 0, sizeof(StickADCFiltered));
    StickADCWindowIndex = 0;
    StickADCWindowCount = 0;
    StickADCCalibrating = 0;

    DMA_Cmd(DMA1_Channel1, ENABLE);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

void StickADC_FilterUpdate(void)
{
    uint8_t axis;
    uint8_t sample_count;

    sample_count = StickADCWindowCount;
    if (sample_count < STICK_FILTER_WINDOW_SIZE)
    {
        sample_count++;
    }

    for (axis = 0; axis < STICK_AXIS_COUNT; axis++)
    {
        uint16_t old_value = StickADCWindow[axis][StickADCWindowIndex];
        uint16_t new_value = StickADC_GetRaw((StickAxisTypeDef)axis);

        StickADCWindow[axis][StickADCWindowIndex] = new_value;
        StickADCWindowSum[axis] += new_value;
        StickADCWindowSum[axis] -= old_value;
        StickADCFiltered[axis] = (uint16_t)((StickADCWindowSum[axis] + sample_count / 2U) /
                                            sample_count);
    }

    StickADCWindowIndex++;
    if (StickADCWindowIndex >= STICK_FILTER_WINDOW_SIZE)
    {
        StickADCWindowIndex = 0;
    }

    if (StickADCWindowCount < STICK_FILTER_WINDOW_SIZE)
    {
        StickADCWindowCount++;
    }
}

uint16_t StickADC_GetRaw(StickAxisTypeDef axis)
{
    if (axis >= STICK_AXIS_COUNT)
    {
        return 0;
    }

    return StickADCRaw[axis];
}

uint16_t StickADC_GetFilteredRaw(StickAxisTypeDef axis)
{
    if (axis >= STICK_AXIS_COUNT)
    {
        return 0;
    }

    if (StickADCWindowCount == 0)
    {
        return StickADC_GetRaw(axis);
    }

    return StickADCFiltered[axis];
}

int16_t StickADC_GetRCValue(StickAxisTypeDef axis)
{
    return StickADC_MapToRC(axis, StickADC_GetFilteredRaw(axis));
}

void StickADC_CalibrationStart(void)
{
    uint8_t axis;

    memcpy(StickADCCalibrationBackup, StickADCCalibration, sizeof(StickADCCalibrationBackup));

    for (axis = 0; axis < STICK_AXIS_COUNT; axis++)
    {
        uint16_t value = StickADC_GetFilteredRaw((StickAxisTypeDef)axis);

        StickADCCalibration[axis].min = value;
        StickADCCalibration[axis].center = value;
        StickADCCalibration[axis].max = value;
    }

    StickADCCalibrating = 1;
}

void StickADC_CalibrationSample(void)
{
    uint8_t axis;

    if (!StickADCCalibrating)
    {
        return;
    }

    for (axis = 0; axis < STICK_AXIS_COUNT; axis++)
    {
        uint16_t value = StickADC_GetFilteredRaw((StickAxisTypeDef)axis);

        if (value < StickADCCalibration[axis].min)
        {
            StickADCCalibration[axis].min = value;
        }

        if (value > StickADCCalibration[axis].max)
        {
            StickADCCalibration[axis].max = value;
        }
    }
}

uint8_t StickADC_CalibrationFinish(uint8_t save_to_flash)
{
    uint8_t axis;

    if (!StickADCCalibrating)
    {
        return 0;
    }

    for (axis = 0; axis < STICK_AXIS_COUNT; axis++)
    {
        StickADCCalibration[axis].center = StickADC_GetFilteredRaw((StickAxisTypeDef)axis);
    }

    StickADCCalibrating = 0;

    if (!StickADC_IsCalibrationValid(StickADCCalibration))
    {
        memcpy(StickADCCalibration, StickADCCalibrationBackup, sizeof(StickADCCalibration));
        return 0;
    }

    if (save_to_flash && !StickADC_SaveCalibration())
    {
        memcpy(StickADCCalibration, StickADCCalibrationBackup, sizeof(StickADCCalibration));
        return 0;
    }

    return 1;
}

void StickADC_CalibrationCancel(void)
{
    if (StickADCCalibrating)
    {
        memcpy(StickADCCalibration, StickADCCalibrationBackup, sizeof(StickADCCalibration));
        StickADCCalibrating = 0;
    }
}

uint8_t StickADC_IsCalibrationRunning(void)
{
    return StickADCCalibrating;
}

uint8_t StickADC_CalibrationSetCenter(uint8_t save_to_flash)
{
    uint8_t axis;
    StickAxisCalibrationTypeDef backup[STICK_AXIS_COUNT];

    memcpy(backup, StickADCCalibration, sizeof(backup));

    for (axis = STICK_AXIS_YAW; axis < STICK_AXIS_COUNT; axis++)
    {
        StickADCCalibration[axis].center = StickADC_GetFilteredRaw((StickAxisTypeDef)axis);
    }

    if (!StickADC_IsCalibrationValid(StickADCCalibration))
    {
        memcpy(StickADCCalibration, backup, sizeof(StickADCCalibration));
        return 0;
    }

    if (save_to_flash && !StickADC_SaveCalibration())
    {
        memcpy(StickADCCalibration, backup, sizeof(StickADCCalibration));
        return 0;
    }

    return 1;
}
