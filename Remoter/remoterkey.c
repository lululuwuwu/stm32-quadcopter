#include "board.h"
#include "remoterkey.h"
#include "remoterstick.h"

#define REMOTER_FEEDBACK_ON_MS 80
#define REMOTER_FEEDBACK_OFF_MS 120
#define REMOTER_WINDOW_COUNT 4
#define REMOTER_TRIM_STEP 10

/* 校准和按键反馈用到的 LED 类型。 */
typedef enum
{
    REMOTER_LED_RED = 0,
    REMOTER_LED_BLUE
} RemoterLedTypeDef;

/* 打开指定反馈灯。 */
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

/* 关闭指定反馈灯。 */
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

/*
 * 声光反馈。
 * count 表示蜂鸣和 LED 闪烁次数。
 * 当前约定：
 *   蓝灯 1 次: 切换 OLED 页面；
 *   蓝灯 2 次: 进入完整校准；
 *   蓝灯 3 次: 完整校准保存成功；
 *   红灯 1 次: 取消校准或普通微调；
 *   红灯 3 次: 快速中点校准成功；
 *   红灯 5 次: 校准失败。
 */
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

/*
 * 完整校准状态切换。
 * 第一次右键长按: 进入完整校准；
 * 第二次右键长按: 结束完整校准并保存。
 */
static void RemoterKeyToggleFullCalibration(void)
{
    if (!RemoterStick_IsCalibrating())
    {
        RemoterStick_StartFullCalibration();
        RemoterFeedback(REMOTER_LED_BLUE, 2);
        return;
    }

    if (RemoterStick_FinishFullCalibration())
    {
        RemoterFeedback(REMOTER_LED_BLUE, 3);
    }
    else
    {
        RemoterFeedback(REMOTER_LED_RED, 5);
    }
}

/*
 * 处理按键事件。
 * 右键短按: 切换 OLED 页面；
 * 右键长按: 进入或结束完整校准；
 * 左键长按: 校准中则取消, 非校准中则执行快速中点校准；
 * OFFSET 上/下: 俯仰微调；
 * OFFSET 左/右: 翻滚微调。
 */
static void RemoterKeyProcess(KeyEventTypeDef event)
{
    switch (event)
    {
        case KEY_EVENT_RIGHT_SHORT:
            RemoterData.windows = (RemoterData.windows + 1) % REMOTER_WINDOW_COUNT;
            RemoterFeedback(REMOTER_LED_BLUE, 1);
            break;

        case KEY_EVENT_RIGHT_LONG:
            RemoterKeyToggleFullCalibration();
            break;

        case KEY_EVENT_LEFT_LONG:
            if (RemoterStick_IsCalibrating())
            {
                RemoterStick_CancelCalibration();
                RemoterFeedback(REMOTER_LED_RED, 1);
            }
            else
            {
                if (RemoterStick_CalibrateCenter())
                {
                    RemoterFeedback(REMOTER_LED_RED, 3);
                }
                else
                {
                    RemoterFeedback(REMOTER_LED_RED, 5);
                }
            }
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

void RemoterKey_Init(void)
{
    Key_Init();
    LED_Init();
    Buzzer_Init();
}

void RemoterKey_Update(void)
{
    RemoterKeyProcess(Key_ScanEvent());
}
