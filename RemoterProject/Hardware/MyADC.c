#include "board.h"
#include "MyADC.h"
#include "MyFLASH.h"
#include <string.h>

/* STM32F103 ADC ??12 位，理论采样范围??0~4095??*/
#define ADC_FULL_SCALE 4095U
#define ADC_REFERENCE_MV 3300U
#define STICK_ADC_DMA_COUNT (STICK_AXIS_COUNT + 1U)
#define RC_BATTERY_ADC_INDEX STICK_AXIS_COUNT

/* 飞控/遥控常用 RC 通道范围??000 为最低，1500 为中点，2000 为最高??*/
#define RC_VALUE_MIN 1000
#define RC_VALUE_MAX 2000
#define RC_VALUE_CENTER ((RC_VALUE_MIN + RC_VALUE_MAX) / 2)

/*
 * 滑动窗口大小?? *
 * 当前摇杆任务 20ms 调用一??StickADC_FilterUpdate()?? * 8 点窗口约等于对最??160ms 的采样做平均?? * 窗口越大越平稳，但响应越慢；窗口越小响应快，但抖动更明显?? */
#define STICK_FILTER_WINDOW_SIZE 8U

/*
 * Flash 校准数据存放地址?? *
 * STM32F103C8 常见 Flash 起始地址??0x08000000?? * 这里使用 0x0800FC00，通常位于 64KB Flash 的最??1KB 页?? * 如果工程代码体积接近 Flash 尾部，需要确认链接脚??Keil 配置没有把程序放到此页?? */
#define STICK_CAL_FLASH_ADDRESS 0x0800FC00U

/* Flash 数据合法性标记，ASCII 含义近似??"RCAL"??*/
#define STICK_CAL_MAGIC 0x5243414CU

/* 校准数据结构版本号，以后结构体字段变化时可递增版本，避免旧数据误用??*/
#define STICK_CAL_VERSION 1U

/*
 * 校准最小跨度?? *
 * 如果某个轴采集到??max - min 太小，说明用户可能没有把摇杆推到极限?? * 或??ADC 通道/接线异常，此时拒绝保存这组校准值?? */
#define STICK_CAL_MIN_SPAN 200U

/*
 * 中点保护边距?? *
 * YAW/PIT/ROL 是自动回中轴，center 必须明显落在 min ??max 之间?? * 这个边距可以避免 center 太靠近端点导致映射比例异常?? */
#define STICK_CAL_CENTER_MARGIN 20U

typedef struct
{
    uint16_t min;    /* 当前轴校准得到的最??ADC ??*/
    uint16_t center; /* 当前轴校准得到的中点 ADC 值；THR 油门不依赖此??*/
    uint16_t max;    /* 当前轴校准得到的最??ADC ??*/
} StickAxisCalibrationTypeDef;

typedef struct
{
    uint32_t magic; /* 固定魔数，用于判??Flash 中是否存过本模块的数??*/
    uint16_t version; /* 数据结构版本，防止结构变化后误读旧格??*/
    uint16_t size; /* 保存结构体大小，进一步校验格式是否匹??*/
    StickAxisCalibrationTypeDef axis[STICK_AXIS_COUNT]; /* 四个摇杆轴的校准??*/
    uint32_t checksum; /* 前面所有字段的简单校验和，用于发??Flash 数据损坏 */
} StickCalibrationFlashTypeDef;

/*
 * ADC + DMA 原始采样缓存?? *
 * DMA 会循环写入这个数组，CPU 不需要手动等??ADC 转换完成?? * 数组下标??StickAxisTypeDef 一致：
 *   [0] THR, [1] YAW, [2] PIT, [3] ROL
 *
 * volatile 的原因：
 *   这个数组会被 DMA 外设在后台修改，编译器不能假设它的值不变?? */
static volatile uint16_t StickADCRaw[STICK_ADC_DMA_COUNT];

/*
 * 每个轴各有一个滑动窗口?? *
 * StickADCWindow[axis][index] 保存某个轴最近若干次采样值；
 * StickADCWindowSum[axis] 保存窗口内样本之和，避免每次求平均都遍历整个窗口?? * StickADCFiltered[axis] 保存当前窗口平均值，也就是滤波后??ADC 值?? */
static uint16_t StickADCWindow[STICK_AXIS_COUNT][STICK_FILTER_WINDOW_SIZE];
static uint32_t StickADCWindowSum[STICK_AXIS_COUNT];
static uint16_t StickADCFiltered[STICK_AXIS_COUNT];

/* 当前要覆盖的窗口位置，以及窗口已经积累的有效样本数量??*/
static uint8_t StickADCWindowIndex;
static uint8_t StickADCWindowCount;

/* 当前正在使用的校准参数??*/
static StickAxisCalibrationTypeDef StickADCCalibration[STICK_AXIS_COUNT];

/*
 * 完整校准开始前的备份?? *
 * 如果用户取消校准，或者结束校准时发现数据非法/Flash 写入失败?? * 就用这份备份恢复，避免把坏参数带入遥控输出?? */
static StickAxisCalibrationTypeDef StickADCCalibrationBackup[STICK_AXIS_COUNT];

/* 完整校准状态标志：0 表示正常运行?? 表示正在采集 min/max??*/
static uint8_t StickADCCalibrating;

/*
 * 设置默认校准参数?? *
 * 默认认为 ADC 可以覆盖完整 0~4095 范围，中点在 2047 左右?? * ??Flash 中没有有效校准数据时，就使用这组默认值保证系统可运行?? */
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

/*
 * 计算简单校验和?? *
 * 这里不是加密或强 CRC，只是为了发现常见的 Flash 数据损坏?? * 未擦除、写入中断、结构体字段错位等问题?? *
 * 算法：逐字节累加后循环左移 1 位，让不同顺序的数据更不容易得到同样结果?? */
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

/*
 * 检查单个轴的校准值是否合理?? *
 * 公共检查：
 *   1. max 必须大于 min?? *   2. max - min 必须大于最小跨度，防止摇杆没有推到极限?? *
 * THR 油门轴：
 *   油门不是自动回中，只需??min/max 两个端点即可?? *   center 对油门映射没有意义，因此不检??center?? *
 * YAW/PIT/ROL 回中轴：
 *   ??min/max 外，还要??center 落在 min ??max 中间，并留有保护边距?? */
static uint8_t StickADC_IsAxisCalibrationValid(StickAxisTypeDef axis,
                                               const StickAxisCalibrationTypeDef *cal)
{
    if (cal->max <= cal->min)
    {
        return 0;
    }

    if ((uint16_t)(cal->max - cal->min) < STICK_CAL_MIN_SPAN)
    {
        return 0;
    }

    if (axis == STICK_AXIS_THR)
    {
        return 1;
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

/* 检查四个轴的校准值是否全部合理??*/
static uint8_t StickADC_IsCalibrationValid(const StickAxisCalibrationTypeDef *cal)
{
    uint8_t axis;

    for (axis = 0; axis < STICK_AXIS_COUNT; axis++)
    {
        if (!StickADC_IsAxisCalibrationValid((StickAxisTypeDef)axis, &cal[axis]))
        {
            return 0;
        }
    }

    return 1;
}

/*
 * ??Flash 读取校准参数?? *
 * 读取后依次检查：
 *   magic    ：确认这??Flash 存的是摇杆校准数据；
 *   version  ：确认数据版本和当前代码一致；
 *   size     ：确认结构体大小一致；
 *   checksum ：确认数据内容没有明显损坏；
 *   axis     ：确认每个轴??min/center/max 合理?? *
 * 任意检查失败都会回退到默认校准参数?? */
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

/*
 * 保存当前校准参数??Flash?? *
 * STM32 内部 Flash 写入前必须先擦除整页，所以这里的流程是：
 *   1. 检查当前校准参数是否合法；
 *   2. 组装 Flash 数据结构?? *   3. 计算 checksum?? *   4. 擦除 STICK_CAL_FLASH_ADDRESS 所在页?? *   5. 按字节数组写入?? *
 * 返回 1 表示写入成功，返??0 表示校准数据非法??Flash 操作失败?? */
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

/*
 * 通用线性映射函数?? *
 * ??adc_min~adc_max 范围内的 ADC 值映射到 rc_min~rc_max?? * 映射前会做端点钳位：
 *   adc_value <= adc_min -> rc_min
 *   adc_value >= adc_max -> rc_max
 *
 * numerator 中加 adc_span / 2 是为了四舍五入，减少整数除法的截断误差?? */
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

/*
 * 把滤波后??ADC 值转换成遥控 RC 值?? *
 * THR 油门?? *   油门不是自动回中，因此只使用 min/max 做单段线性映射：
 *     min -> 1000
 *     max -> 2000
 *
 * YAW/PIT/ROL?? *   这些轴自动回中，因此使用 min/center/max 做分段线性映射：
 *     min    -> 1000
 *     center -> 1500
 *     max    -> 2000
 *
 * 分段映射的好处是即使摇杆中点不在 ADC 量程正中，也能让回中输出稳定??1500?? */
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

    if (axis == STICK_AXIS_THR)
    {
        return StickADC_MapLinearToRC(adc_value, cal->min, cal->max,
                                      RC_VALUE_MIN, RC_VALUE_MAX);
    }

    if (adc_value < cal->center)
    {
        return StickADC_MapLinearToRC(adc_value, cal->min, cal->center,
                                      RC_VALUE_MIN, RC_VALUE_CENTER);
    }

    return StickADC_MapLinearToRC(adc_value, cal->center, cal->max,
                                  RC_VALUE_CENTER, RC_VALUE_MAX);
}

/*
 * 初始??ADC 采样模块?? *
 * 这个函数完成从硬件到软件状态的全部初始化：
 *   1. 打开 GPIOA、ADC1、DMA1 时钟?? *   2. ??PA0~PA3 配置成模拟输入；
 *   3. 配置 DMA1_Channel1，把 ADC1->DR 自动搬运??StickADCRaw[]?? *   4. 配置 ADC1 扫描模式、连续转换模式；
 *   5. 设置四个规则通道的转换顺序；
 *   6. 执行 ADC 内部自校准；
 *   7. 读取 Flash 中保存的摇杆校准值；
 *   8. 清空滑动窗口滤波状态；
 *   9. 启动 DMA ??ADC 软件转换?? */
void StickADC_Init(void)
{
    GPIO_InitTypeDef gpio_init;
    ADC_InitTypeDef adc_init;
    DMA_InitTypeDef dma_init;

    /* ADC1 挂在 APB2，总线；DMA1 挂在 AHB 总线??*/
    RCC_APB2PeriphClockCmd(STICK_RCC | RC_BAT_RCC | STICK_ADC_RCC, ENABLE);
    RCC_AHBPeriphClockCmd(STICK_DMA_RCC, ENABLE);

    /*
     * STM32F103 ??ADC 时钟不能超过 14MHz??     * 如果系统 PCLK2 ??72MHz?? 分频??ADC 时钟??12MHz，处于安全范围??     */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* 电位器输出为模拟电压，所??GPIO 配置为模拟输??AIN??*/
    gpio_init.GPIO_Mode = GPIO_Mode_AIN;
    gpio_init.GPIO_Pin = STICK_THR_PIN | STICK_YAW_PIN | STICK_PITH_PIN | STICK_ROLL_PIN;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(STICK_GPIO, &gpio_init);

    /* PB0 ???????????????????????????????? */
    gpio_init.GPIO_Pin = RC_BAT_PIN;
    GPIO_Init(RC_BAT_GPIO, &gpio_init);

    /*
     * ADC1 ??DMA 通道固定??DMA1_Channel1??     * PeripheralSRC 表示数据方向为外设到内存：ADC1->DR -> StickADCRaw[]??     * Circular 模式表示搬完 4 个半字后自动回到数组开头继续搬??     */
    DMA_DeInit(DMA1_Channel1);
    dma_init.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    dma_init.DMA_MemoryBaseAddr = (uint32_t)StickADCRaw;
    dma_init.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma_init.DMA_BufferSize = STICK_ADC_DMA_COUNT;
    dma_init.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma_init.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma_init.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma_init.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    dma_init.DMA_Mode = DMA_Mode_Circular;
    dma_init.DMA_Priority = DMA_Priority_High;
    dma_init.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &dma_init);

    /*
     * ADC 扫描 + 连续转换??     *   ScanConvMode       ：按规则通道序列依次采样多个通道??     *   ContinuousConvMode ：完成一轮后自动开始下一轮；
     *   NbrOfChannel       ：本轮规则通道数量??5，包??4 个摇杆轴??PB0 电量检测??     */
    adc_init.ADC_Mode = ADC_Mode_Independent;
    adc_init.ADC_ScanConvMode = ENABLE;
    adc_init.ADC_ContinuousConvMode = ENABLE;
    adc_init.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    adc_init.ADC_DataAlign = ADC_DataAlign_Right;
    adc_init.ADC_NbrOfChannel = STICK_ADC_DMA_COUNT;
    ADC_Init(ADC1, &adc_init);

    /*
     * 规则通道顺序决定 DMA 数组下标含义??     *   StickADCRaw[0] = ADC_Channel_1 = THR 油门
     *   StickADCRaw[1] = ADC_Channel_0 = YAW 偏航
     *   StickADCRaw[2] = ADC_Channel_3 = PIT 俯仰
     *   StickADCRaw[3] = ADC_Channel_2 = ROL 翻滚
     *   StickADCRaw[4] = ADC_Channel_8 = BAT 遥控器电??     */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 2, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 3, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 4, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_8, 5, ADC_SampleTime_55Cycles5);

    ADC_DMACmd(ADC1, ENABLE);

    /*
     * ADC 自校准流程：
     *   1. 先短暂关??ADC??     *   2. 再开??ADC??     *   3. 复位校准寄存器；
     *   4. 启动校准并等待完成??     *
     * 这样可以减小 ADC 内部偏差，让电位器采样更稳定??     */
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

    /*
     * 校准参数初始化策略：
     *   先设置一份默认值，保证没有 Flash 数据时也能工作；
     *   再尝试读??Flash，如果读取成功会覆盖默认值??     */
    StickADC_SetDefaultCalibration();
    StickADC_LoadCalibration();

    /* 清空滤波窗口和校准状态，避免使用上电??RAM 中的不确定值??*/
    memset(StickADCWindow, 0, sizeof(StickADCWindow));
    memset(StickADCWindowSum, 0, sizeof(StickADCWindowSum));
    memset(StickADCFiltered, 0, sizeof(StickADCFiltered));
    StickADCWindowIndex = 0;
    StickADCWindowCount = 0;
    StickADCCalibrating = 0;

    /*
     * 先启??DMA，再启动 ADC 软件转换??     * 从此以后 ADC 会连续转换，DMA 会持续刷??StickADCRaw[]??     */
    DMA_Cmd(DMA1_Channel1, ENABLE);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

/*
 * 更新滑动窗口滤波?? *
 * 该函数每调用一次，就把四个轴当前最新原??ADC 值放入窗口?? * 使用“减旧??+ 加新值”的方式维护 sum，计算量固定且很小?? *
 * 刚上电时窗口还没有填满：
 *   sample_count 会从 1 增长??STICK_FILTER_WINDOW_SIZE?? *   因此不会把未填充??0 也算入平均值?? */
void StickADC_FilterUpdate(void)
{
    uint8_t axis;
    uint8_t sample_count;

    sample_count = StickADCWindowCount;
    if (sample_count < STICK_FILTER_WINDOW_SIZE)
    {
        sample_count++;
    }

    /* 四个轴共用同一个窗口下标，表示每一轮同时更新四个轴的一格样本??*/
    for (axis = 0; axis < STICK_AXIS_COUNT; axis++)
    {
        uint16_t old_value = StickADCWindow[axis][StickADCWindowIndex];
        uint16_t new_value = StickADC_GetRaw((StickAxisTypeDef)axis);

        /* 环形窗口：当前位置旧样本被新样本覆盖，sum 同步减旧加新??*/
        StickADCWindow[axis][StickADCWindowIndex] = new_value;
        StickADCWindowSum[axis] += new_value;
        StickADCWindowSum[axis] -= old_value;

        /* ??sample_count / 2 用于整数平均时四舍五入??*/
        StickADCFiltered[axis] = (uint16_t)((StickADCWindowSum[axis] + sample_count / 2U) /
                                            sample_count);
    }

    /* 移动窗口写入位置，到末尾后回??0，形成环形缓冲??*/
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

/* 读取指定轴的原始 ADC 值??*/
uint16_t StickADC_GetRaw(StickAxisTypeDef axis)
{
    if (axis >= STICK_AXIS_COUNT)
    {
        return 0;
    }

    return StickADCRaw[axis];
}

/* 读取指定轴的滤波??ADC 值??*/
uint16_t StickADC_GetFilteredRaw(StickAxisTypeDef axis)
{
    if (axis >= STICK_AXIS_COUNT)
    {
        return 0;
    }

    if (StickADCWindowCount == 0)
    {
        /*
         * 如果滤波器还没有更新过，直接返回 DMA 原始值??         * 这可以避免初始化早期读到??0 的滤波缓存??         */
        return StickADC_GetRaw(axis);
    }

    return StickADCFiltered[axis];
}

/* 对外提供的最终摇杆值接口：滤波 ADC -> 校准映射 -> 1000~2000??*/
int16_t StickADC_GetRCValue(StickAxisTypeDef axis)
{
    return StickADC_MapToRC(axis, StickADC_GetFilteredRaw(axis));
}

/* ?????????????????λ mV?? */
uint16_t StickADC_GetBatteryMv(void)
{
    uint32_t adc_pin_mv;
    uint32_t battery_mv;

    adc_pin_mv = ((uint32_t)StickADCRaw[RC_BATTERY_ADC_INDEX] * ADC_REFERENCE_MV +
                  ADC_FULL_SCALE / 2U) /
                 ADC_FULL_SCALE;

    battery_mv = (adc_pin_mv * RC_BAT_DIVIDER_NUM + RC_BAT_DIVIDER_DEN / 2U) /
                 RC_BAT_DIVIDER_DEN;

    if (battery_mv > 0xFFFFU)
    {
        battery_mv = 0xFFFFU;
    }

    return (uint16_t)battery_mv;
}

/*
 * 开始完整校准?? *
 * 进入校准前先备份当前参数，方便失败或取消时回滚?? * 开始时把当前滤波值同时作??min/center/max 的初值，后续 Sample()
 * 会不断拓??min/max；Finish() 会重新记??center?? */
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

/*
 * 完整校准采样?? *
 * 用户在校准过程中需要把摇杆推到各个极限位置?? * 本函数周期调用时会记录每个轴见过的最小值和最大值?? */
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

/*
 * 结束完整校准?? *
 * 结束瞬间把当前滤波值作??center?? *   THR 油门虽然会记??center，但后续映射不会使用它；
 *   YAW/PIT/ROL 会用 center 作为 1500 的校准点?? *
 * 如果校准参数不合法，或者保??Flash 失败，会恢复到校准前备份?? */
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

/* 取消完整校准，恢复进入校准前的参数??*/
void StickADC_CalibrationCancel(void)
{
    if (StickADCCalibrating)
    {
        memcpy(StickADCCalibration, StickADCCalibrationBackup, sizeof(StickADCCalibration));
        StickADCCalibrating = 0;
    }
}

/* 查询当前是否正在进行完整校准??*/
uint8_t StickADC_IsCalibrationRunning(void)
{
    return StickADCCalibrating;
}

/*
 * 快速中点校准?? *
 * 只更??YAW/PIT/ROL 三个自动回中轴的 center?? * 典型使用场景?? *   用户松开右摇??方向摇杆，让其自然回中；
 *   长按按键触发该函数；
 *   当前值被保存??1500 对应??ADC 中点?? *
 * 不处??THR 的原因：
 *   油门不是自动回中轴，当前位置不能代表中点?? *   THR 只依赖完整校准得到的 min/max?? */
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
