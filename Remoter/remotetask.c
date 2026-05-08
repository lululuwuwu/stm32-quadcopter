#include "board.h"

#define REMOTER_KEY_SCAN_PERIOD_MS 20
#define REMOTER_STICK_SCAN_PERIOD_MS 20
#define REMOTER_FEEDBACK_ON_MS 80
#define REMOTER_FEEDBACK_OFF_MS 120
#define REMOTER_WINDOW_COUNT 4
#define REMOTER_TRIM_STEP 10
#define REMOTER_RC_MIN 1000
#define REMOTER_RC_CENTER 1500
#define REMOTER_RC_MAX 2000

typedef enum
{
    REMOTER_LED_RED = 0,
    REMOTER_LED_BLUE
} RemoterLedTypeDef;

/* 遥控量最终限制在 1000~2000，避免微调或校准后超过飞控期望范围。 */
static int16_t RemoterLimitRC(int16_t value)
{
    if (value < REMOTER_RC_MIN)
    {
        return REMOTER_RC_MIN;
    }

    if (value > REMOTER_RC_MAX)
    {
        return REMOTER_RC_MAX;
    }

    return value;
}

static void RemoterDataInit(void)
{
    /* 上电先给遥控数据一个安全默认值，ADC 任务启动后会持续刷新摇杆数据。 */
    memset(&RemoterData, 0, sizeof(RemoterData));
    RemoterData.windows = 0;
    RemoterData.NRF_Channel = 28;
    RemoterData.THR = REMOTER_RC_MIN;
    RemoterData.YAW = REMOTER_RC_CENTER;
    RemoterData.PIT = REMOTER_RC_CENTER;
    RemoterData.ROL = REMOTER_RC_CENTER;
}

static void RemoterStickUpdate(void)
{
    /* 从 ADC DMA 缓冲区读取四个摇杆轴，并写入 RemoterData 供 OLED 和通信任务使用。 */
    RemoterData.THR = StickADC_GetRCValue(STICK_AXIS_THR);

    /* 偏航、俯仰、翻滚允许叠加按键微调；油门不叠加微调，避免上电油门被抬高。 */
    RemoterData.YAW = RemoterLimitRC(StickADC_GetRCValue(STICK_AXIS_YAW) + RemoterData.OffSet_Yaw);
    RemoterData.PIT = RemoterLimitRC(StickADC_GetRCValue(STICK_AXIS_PIT) + RemoterData.OffSet_Pit);
    RemoterData.ROL = RemoterLimitRC(StickADC_GetRCValue(STICK_AXIS_ROL) + RemoterData.OffSet_Rol);
}

static void RemoterLedOn(RemoterLedTypeDef led)
{
    if (led == REMOTER_LED_RED)
    {
        LED_RedOn();
    }
    else
    {
        LED_BlueOn();
    }
}

static void RemoterLedOff(RemoterLedTypeDef led)
{
    if (led == REMOTER_LED_RED)
    {
        LED_RedOff();
    }
    else
    {
        LED_BlueOff();
    }
}

static void RemoterFeedback(RemoterLedTypeDef led, uint8_t count)
{
    uint8_t i;

    for (i = 0; i < count; i++)
    {
        RemoterLedOn(led);
        Buzzer_On();
        vTaskDelay(pdMS_TO_TICKS(REMOTER_FEEDBACK_ON_MS));
        Buzzer_Off();
        RemoterLedOff(led);
        vTaskDelay(pdMS_TO_TICKS(REMOTER_FEEDBACK_OFF_MS));
    }
}

static void RemoterJoystickCalibrate(void)
{
    /* 校准时把当前三轴摇杆位置作为中点，计算出让输出回到 1500 的补偿量。 */
    RemoterData.OffSet_Pit = REMOTER_RC_CENTER - StickADC_GetRCValue(STICK_AXIS_PIT);
    RemoterData.OffSet_Rol = REMOTER_RC_CENTER - StickADC_GetRCValue(STICK_AXIS_ROL);
    RemoterData.OffSet_Yaw = REMOTER_RC_CENTER - StickADC_GetRCValue(STICK_AXIS_YAW);
    RemoterStickUpdate();
}

static void RemoterKeyProcess(KeyEventTypeDef event)
{
    switch (event)
    {
        case KEY_EVENT_RIGHT_SHORT:
            RemoterData.windows = (RemoterData.windows + 1) % REMOTER_WINDOW_COUNT;
            RemoterFeedback(REMOTER_LED_BLUE, 1);
            break;

        case KEY_EVENT_RIGHT_LONG:
            break;

        case KEY_EVENT_LEFT_LONG:
            RemoterJoystickCalibrate();
            RemoterFeedback(REMOTER_LED_RED, 3);
            break;

        case KEY_EVENT_OFFSET_UP_SHORT:
            RemoterData.OffSet_Pit += REMOTER_TRIM_STEP;
            RemoterFeedback(REMOTER_LED_RED, 1);
            break;

        case KEY_EVENT_OFFSET_DOWN_SHORT:
            RemoterData.OffSet_Pit -= REMOTER_TRIM_STEP;
            RemoterFeedback(REMOTER_LED_RED, 1);
            break;

        case KEY_EVENT_OFFSET_LEFT_SHORT:
            RemoterData.OffSet_Rol += REMOTER_TRIM_STEP;
            RemoterFeedback(REMOTER_LED_RED, 1);
            break;

        case KEY_EVENT_OFFSET_RIGHT_SHORT:
            RemoterData.OffSet_Rol -= REMOTER_TRIM_STEP;
            RemoterFeedback(REMOTER_LED_RED, 1);
            break;

        default:
            break;
    }
}

void vTaskStickScan(void *paramters)
{
    (void)paramters;

    /* ADC 初始化一次即可；DMA 会在后台持续刷新四路摇杆原始值。 */
    StickADC_Init();

    while (1)
    {
        /* 20ms 更新一次遥控数据，和按键扫描周期保持一致，响应足够平滑。 */
        RemoterStickUpdate();
        vTaskDelay(pdMS_TO_TICKS(REMOTER_STICK_SCAN_PERIOD_MS));
    }
}

void vTaskKeyProcess(void *paramters)
{
    (void)paramters;

    Key_Init();
    LED_Init();
    Buzzer_Init();

    while (1)
    {
        RemoterKeyProcess(Key_ScanEvent());
        vTaskDelay(pdMS_TO_TICKS(REMOTER_KEY_SCAN_PERIOD_MS));
    }
}

void vTaskSerial1SendLogCode(void *pvParameters)
{
    char pvBuffer[uxQueueSerial1ItemSize];

    (void)pvParameters;
    Serial1_Init();

    while (1)
    {
        xQueueReceive(xQueueSerial1, pvBuffer, portMAX_DELAY);
        Serial1_SendString(pvBuffer);
    }
}

void vTaskOLEDShow(void *paramters)
{
    (void)paramters;

    Show_Init();

    while (1)
    {
        Show_Refresh();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void RemoterCreateTask(void *paramters)
{
    (void)paramters;

    /* 创建各任务前先初始化共享数据，避免 OLED 任务先运行时显示未定义值。 */
    RemoterDataInit();

    taskENTER_CRITICAL();

    xTaskCreate(vTaskSerial1SendLogCode, "TaskSerial1Log", 256, NULL,
                (configMAX_PRIORITIES - 1), NULL);
    xTaskCreate(vTaskOLEDShow, "TaskOLEDShow", 256, NULL, 1, NULL);
    xTaskCreate(vTaskKeyProcess, "TaskKeyProcess", 128, NULL, 2, NULL);
    xTaskCreate(vTaskStickScan, "TaskStickScan", 128, NULL, 2, NULL);

    taskEXIT_CRITICAL();

    vTaskDelete(NULL);
}
