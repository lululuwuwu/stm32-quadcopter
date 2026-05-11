#include "board.h"

SetLEDTypeDef setLED;

void LED_RedLeft_ON(void)
{
    GPIO_ResetBits(LED_GPIO, LED_RedLeft_Pin);
}
void LED_RedLeft_OFF(void)
{
    GPIO_SetBits(LED_GPIO, LED_RedLeft_Pin);
}
void LED_RedLeft_Toggle(void)
{
    if (GPIO_ReadOutputDataBit(LED_GPIO, LED_RedLeft_Pin))
    {
        GPIO_ResetBits(LED_GPIO, LED_RedLeft_Pin);
    }
    else
    {
        GPIO_SetBits(LED_GPIO, LED_RedLeft_Pin);
    }
}
void LED_RedRight_ON(void)
{
    GPIO_ResetBits(LED_GPIO, LED_RedRight_Pin);
}
void LED_RedRight_OFF(void)
{
    GPIO_SetBits(LED_GPIO, LED_RedRight_Pin);
}
void LED_RedRight_Toggle(void)
{
    if (GPIO_ReadOutputDataBit(LED_GPIO, LED_RedRight_Pin))
    {
        GPIO_ResetBits(LED_GPIO, LED_RedRight_Pin);
    }
    else
    {
        GPIO_SetBits(LED_GPIO, LED_RedRight_Pin);
    }
}
void LED_BlueLeft_ON(void)
{
    GPIO_ResetBits(LED_GPIO, LED_BlueLeft_Pin);
}
void LED_BlueLeft_OFF(void)
{
    GPIO_SetBits(LED_GPIO, LED_BlueLeft_Pin);
}
void LED_BlueLeft_Toggle(void)
{
    if (GPIO_ReadOutputDataBit(LED_GPIO, LED_BlueLeft_Pin))
    {
        GPIO_ResetBits(LED_GPIO, LED_BlueLeft_Pin);
    }
    else
    {
        GPIO_SetBits(LED_GPIO, LED_BlueLeft_Pin);
    }
}
void LED_BlueRight_ON(void)
{
    GPIO_ResetBits(LED_GPIO, LED_BlueRight_Pin);
}
void LED_BlueRight_OFF(void)
{
    GPIO_SetBits(LED_GPIO, LED_BlueRight_Pin);
}
void LED_BlueRight_Toggle(void)
{
    if (GPIO_ReadOutputDataBit(LED_GPIO, LED_BlueRight_Pin))
    {
        GPIO_ResetBits(LED_GPIO, LED_BlueRight_Pin);
    }
    else
    {
        GPIO_SetBits(LED_GPIO, LED_BlueRight_Pin);
    }
}
void LED_Init(void)
{
    // 1.开启时钟
    RCC_APB2PeriphClockCmd(LED_RCC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    // 使用到了PB3，用AFIO切换到普通输出口
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    // 2.初始化

    GPIO_InitTypeDef GPIO_InitTypeStructure;
    GPIO_InitTypeStructure.GPIO_Mode  = GPIO_Mode_Out_PP;                                                          // 推挽
    GPIO_InitTypeStructure.GPIO_Pin   = LED_RedLeft_Pin | LED_RedRight_Pin | LED_BlueLeft_Pin | LED_BlueRight_Pin; // 引脚号
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;                                                          // 速度

    GPIO_Init(LED_GPIO, &GPIO_InitTypeStructure);

    // 3.给灯初始化的状态

    setLED.FlashTime = 300;      // 毫秒
    setLED.status    = OneByOne; // 一个接一个的闪烁
}
/**
 * @brief LED灯闪烁的函数
 *
 */
void LED_Blink(void)
{
    switch (setLED.status)
    {
    case AlwaysOn: // 全亮
        LED_RedLeft_ON();
        LED_RedRight_ON();
        LED_BlueLeft_ON();
        LED_BlueRight_ON();

        break;
    case AlwaysOff: // 全灭
        LED_RedLeft_OFF();
        LED_RedRight_OFF();
        LED_BlueLeft_OFF();
        LED_BlueRight_OFF();

        break;
    case OneByOne: // 一个接一个的逆时针闪烁
        LED_BlueRight_Toggle();
        vTaskDelay(setLED.FlashTime);
        LED_BlueLeft_Toggle();
        vTaskDelay(setLED.FlashTime);
        LED_RedLeft_Toggle();
        vTaskDelay(setLED.FlashTime);
        LED_RedRight_Toggle();
        vTaskDelay(setLED.FlashTime);

        break;
    case AllFlashLight: // 全部同时闪烁
        LED_RedLeft_Toggle();
        LED_RedRight_Toggle();
        LED_BlueLeft_Toggle();
        LED_BlueRight_Toggle();
        vTaskDelay(setLED.FlashTime);
        break;
    case AlternateFlash: // 交替闪烁
        LED_RedLeft_Toggle();
        LED_RedRight_Toggle();
        vTaskDelay(setLED.FlashTime);
        LED_BlueLeft_Toggle();
        LED_BlueRight_Toggle();
        vTaskDelay(setLED.FlashTime);

        break;
    }
}
