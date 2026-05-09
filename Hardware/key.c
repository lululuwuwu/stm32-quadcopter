#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "key.h"
#include "stdio.h"

#define KEY_DEBOUNCE_COUNT 2
#define KEY_LONG_PRESS_TIME pdMS_TO_TICKS(1000)

/* 单个按键的软件状态，用于消抖、短按和长按判断。 */
typedef struct
{
    uint16_t pin;
    KeyEventTypeDef short_event;
    KeyEventTypeDef long_event;
    uint8_t raw_pressed;  //最近一次读到的原始状态
    uint8_t stable_pressed; //消抖后的稳定状态
    uint8_t same_count; //原始状态连续相同的次数
    uint8_t long_sent;  // 长按是否已经触发过
    TickType_t press_tick;  //  按下时刻，用来计算长按时间
} KeyStateTypeDef;

static KeyStateTypeDef KeyStates[] =
{
    /* 右上按键：短按切换页面，长按预留对频。 */
    {Key_Right, KEY_EVENT_RIGHT_SHORT, KEY_EVENT_RIGHT_LONG, 0, 0, 0, 0, 0},
    /* 左上按键：只处理长按校准。 */
    {Key_Left, KEY_EVENT_NONE, KEY_EVENT_LEFT_LONG, 0, 0, 0, 0, 0},
    /* 四个微调按键：只处理短按。 */
    {Key_OffSet_Font, KEY_EVENT_OFFSET_UP_SHORT, KEY_EVENT_NONE, 0, 0, 0, 0, 0},
    {Key_OffSet_Back, KEY_EVENT_OFFSET_DOWN_SHORT, KEY_EVENT_NONE, 0, 0, 0, 0, 0},
    {Key_OffSet_Left, KEY_EVENT_OFFSET_LEFT_SHORT, KEY_EVENT_NONE, 0, 0, 0, 0, 0},
    {Key_OffSet_Right, KEY_EVENT_OFFSET_RIGHT_SHORT, KEY_EVENT_NONE, 0, 0, 0, 0, 0},
};

static uint8_t Key_ReadPressed(uint16_t pin)
{
    return GPIO_ReadInputDataBit(Key_GPIO, pin) == Bit_RESET;
}

void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitTypeStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | Key_RCC, ENABLE);
    /* PB3/PB4 默认复用为 JTAG 引脚，关闭 JTAG 后才能作为普通按键输入。 */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitTypeStructure.GPIO_Pin = Key_Left | Key_Right |
                                      Key_OffSet_Font | Key_OffSet_Back |
                                      Key_OffSet_Left | Key_OffSet_Right;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(Key_GPIO, &GPIO_InitTypeStructure);
}

KeyEventTypeDef Key_ScanEvent(void)
{
    uint8_t i;
    TickType_t now = xTaskGetTickCount();

    for (i = 0; i < sizeof(KeyStates) / sizeof(KeyStates[0]); i++)
    {
        KeyStateTypeDef *key = &KeyStates[i];
        uint8_t raw = Key_ReadPressed(key->pin);

        /* 连续多次读到相同电平才认为稳定，避免机械按键抖动误触发。 */
        if (raw == key->raw_pressed)
        {
            if (key->same_count < 0xFF)
            {
                key->same_count++;
            }
        }
        else
        {
            key->raw_pressed = raw;
            key->same_count = 0;
        }

        if (key->same_count >= KEY_DEBOUNCE_COUNT && raw != key->stable_pressed)
        {
            key->stable_pressed = raw;
            if (raw)
            {
                /* 记录按下时刻，后续用来判断是否达到长按时间。 */
                key->press_tick = now;
                key->long_sent = 0;
            }
            else if (!key->long_sent)
            {
                /* 松手时如果没有触发过长按，就上报一次短按事件。 */
                return key->short_event;
            }
        }

        /* 长按只上报一次，松手时不会再补发短按事件。 */
        if (key->stable_pressed && !key->long_sent &&
            (now - key->press_tick) >= KEY_LONG_PRESS_TIME)
        {
            key->long_sent = 1;
            return key->long_event;
        }
    }

    return KEY_EVENT_NONE;
}

uint8_t Key1_Pressed(void)
{
    if (Key_ReadPressed(Key_Left))
    {
        vTaskDelay(10);
        while (Key_ReadPressed(Key_Left));
        vTaskDelay(10);
        return 1;
    }

    return 0;
}

uint8_t Key2_Pressed(void)
{
    if (Key_ReadPressed(Key_Right))
    {
        vTaskDelay(10);
        while (Key_ReadPressed(Key_Right));
        vTaskDelay(10);
        return 1;
    }

    return 0;
}
