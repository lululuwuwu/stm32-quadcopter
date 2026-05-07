#include "board.h"

#define REMOTER_KEY_SCAN_PERIOD_MS 20
#define REMOTER_FEEDBACK_ON_MS 80
#define REMOTER_FEEDBACK_OFF_MS 120
#define REMOTER_WINDOW_COUNT 4
#define REMOTER_TRIM_STEP 10

typedef enum
{
    REMOTER_LED_RED = 0,
    REMOTER_LED_BLUE
} RemoterLedTypeDef;

/* 根据业务传入的颜色，打开对应的 LED。 */
static void RemoterLedOn(RemoterLedTypeDef led)
{
    if(led == REMOTER_LED_RED)
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
    if(led == REMOTER_LED_RED)
    {
        LED_RedOff();
    }
    else
    {
        LED_BlueOff();
    }
}

/* 按业务要求闪烁指定颜色 LED，同时让蜂鸣器响相同次数。 */
static void RemoterFeedback(RemoterLedTypeDef led, uint8_t count)
{
    uint8_t i;

    for(i = 0; i < count; i++)
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
    /* 当前工程还没有摇杆 ADC 中心值保存模块，这里先把微调值归零作为校准入口。 */
    RemoterData.OffSet_Pit = 0;
    RemoterData.OffSet_Rol = 0;
    RemoterData.OffSet_Yaw = 0;
}

static void RemoterKeyProcess(KeyEventTypeDef event)
{
    switch(event)
    {
        case KEY_EVENT_RIGHT_SHORT:
            /* 右上短按：切换 OLED 页面，蓝灯和蜂鸣器反馈 1 次。 */
            RemoterData.windows = (RemoterData.windows + 1) % REMOTER_WINDOW_COUNT;
            RemoterFeedback(REMOTER_LED_BLUE, 1);
            break;

        case KEY_EVENT_RIGHT_LONG:
            /* 右上长按：原需求是对频；当前项目固定通信频率，因此不执行对频。 */
            break;

        case KEY_EVENT_LEFT_LONG:
            /* 左上长按：遥感值校准，红灯和蜂鸣器反馈 3 次。 */
            RemoterJoystickCalibrate();
            RemoterFeedback(REMOTER_LED_RED, 3);
            break;

        case KEY_EVENT_OFFSET_UP_SHORT:
            /* 微调上键：俯仰微调 +10。 */
            RemoterData.OffSet_Pit += REMOTER_TRIM_STEP;
            RemoterFeedback(REMOTER_LED_RED, 1);
            break;

        case KEY_EVENT_OFFSET_DOWN_SHORT:
            /* 微调下键：俯仰微调 -10。 */
            RemoterData.OffSet_Pit -= REMOTER_TRIM_STEP;
            RemoterFeedback(REMOTER_LED_RED, 1);
            break;

        case KEY_EVENT_OFFSET_LEFT_SHORT:
            /* 微调左键：翻滚微调 +10。 */
            RemoterData.OffSet_Rol += REMOTER_TRIM_STEP;
            RemoterFeedback(REMOTER_LED_RED, 1);
            break;

        case KEY_EVENT_OFFSET_RIGHT_SHORT:
            /* 微调右键：翻滚微调 -10。 */
            RemoterData.OffSet_Rol -= REMOTER_TRIM_STEP;
            RemoterFeedback(REMOTER_LED_RED, 1);
            break;

        default:
            break;
    }
}

void vTaskKeyProcess(void *paramters)
{
    (void)paramters;

    Key_Init();
    LED_Init();
    Buzzer_Init();

    while(1)
    {
        /* 20ms 扫描一次按键，扫描函数本身不阻塞任务。 */
        RemoterKeyProcess(Key_ScanEvent());
        vTaskDelay(pdMS_TO_TICKS(REMOTER_KEY_SCAN_PERIOD_MS));
    }
}

void vTaskSerial1SendLogCode(void * pvParameters)
{
    char pvBuffer[uxQueueSerial1ItemSize];

    Serial1_Init();

    while(1)
    {
        xQueueReceive(xQueueSerial1, pvBuffer, portMAX_DELAY);
        Serial1_SendString(pvBuffer);
    }
}

void vTaskOLEDShow(void *paramters)
{
    (void)paramters;

    Show_Init();

    RemoterData.windows = 3;
    RemoterData.NRF_Channel = 28;
    RemoterData.NRF_Connect = 1;
    RemoterData.RC_low_power = 0;
    RemoterData.Fly_low_power = 1;
    RemoterData.NRF_RSSI_count = 51;
    RemoterData.Battery_RC = 3789;
    RemoterData.Battery_Fly = 3678;
    RemoterData.THR = 1111;
    RemoterData.ROL = 1112;
    RemoterData.YAW = 1113;
    RemoterData.PIT = 1114;
    RemoterData.OffSet_Pit = 12;
    RemoterData.OffSet_Rol = 13;
    RemoterData.OffSet_Yaw = 14;
    RemoterData.X = 101;
    RemoterData.Y = 102;
    RemoterData.Z = 103;
    RemoterData.H = 104;
    RemoterData.fly_test_flag = 0x03;

    while(1)
    {
        Show_Refresh();
        vTaskDelay(500);
    }
}

void RemoterCreateTask(void *paramters)
{
    (void)paramters;

    taskENTER_CRITICAL();

    xTaskCreate(vTaskSerial1SendLogCode, "TaskSerial1Log", 256, NULL,
                (configMAX_PRIORITIES - 1), NULL);
    xTaskCreate(vTaskOLEDShow, "TaskOLEDShow", 256, NULL, 1, NULL);
    xTaskCreate(vTaskKeyProcess, "TaskKeyProcess", 128, NULL, 2, NULL);

    taskEXIT_CRITICAL();

    vTaskDelete(NULL);
}
