#include "board.h"

#define REMOTER_KEY_SCAN_PERIOD_MS 20
#define REMOTER_STICK_SCAN_PERIOD_MS 20
#define REMOTER_RC_MIN 1000
#define REMOTER_RC_CENTER 1500

/*
 * 初始化全局 RemoterData。
 * 创建任务前先给遥控数据安全默认值：
 *   THR 油门最低 1000, 避免上电瞬间误给油；
 *   YAW/PIT/ROL 回中 1500；
 *   其他字段清零。
 */
static void RemoterDataInit(void)
{
    memset(&RemoterData, 0, sizeof(RemoterData));
    RemoterData.windows = 0;
    RemoterData.NRF_Channel = 28;
    RemoterData.THR = REMOTER_RC_MIN;
    RemoterData.YAW = REMOTER_RC_CENTER;
    RemoterData.PIT = REMOTER_RC_CENTER;
    RemoterData.ROL = REMOTER_RC_CENTER;
    RemoterStick_ResetCalibrationState();
}

/*
 * 摇杆扫描任务。
 * 启动时初始化 ADC + DMA + Flash 校准读取；
 * 之后每 20ms 更新一次 THR/YAW/PIT/ROL 和遥控器电量。
 */
void vTaskStickScan(void *paramters)
{
    (void)paramters;

    StickADC_Init();

    while (1)
    {
        RemoterStick_Update();
        vTaskDelay(pdMS_TO_TICKS(REMOTER_STICK_SCAN_PERIOD_MS));
    }
}

/*
 * 按键处理任务。
 * 每 20ms 扫描一次按键事件, 并触发窗口切换、校准、微调等逻辑。
 */
void vTaskKeyProcess(void *paramters)
{
    (void)paramters;

    RemoterKey_Init();

    while (1)
    {
        RemoterKey_Update();
        vTaskDelay(pdMS_TO_TICKS(REMOTER_KEY_SCAN_PERIOD_MS));
    }
}

/* 串口日志发送任务: 从队列中取字符串并通过 Serial1 发出。 */
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

/* OLED 显示任务: 周期刷新当前窗口。 */
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

/*
 * 遥控器任务创建入口。
 * main() 只创建 RemoterCreateTask；本函数再统一创建串口、OLED、按键、摇杆扫描等子任务。
 * 创建完成后删除自身, 节省任务控制块和栈空间。
 */
void RemoterCreateTask(void *paramters)
{
    (void)paramters;

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
