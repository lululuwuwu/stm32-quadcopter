/*
 * 文件名: LED.c
 * 功能说明:
 * 1. setLED                  LED 闪烁模式和间隔配置。
 * 2. LED_RedLeft_ON/OFF      控制后左红灯亮灭。
 * 3. LED_RedLeft_Toggle      翻转后左红灯。
 * 4. LED_RedRight_ON/OFF     控制后右红灯亮灭。
 * 5. LED_RedRight_Toggle     翻转后右红灯。
 * 6. LED_BlueLeft_ON/OFF     控制前左蓝灯亮灭。
 * 7. LED_BlueLeft_Toggle     翻转前左蓝灯。
 * 8. LED_BlueRight_ON/OFF    控制前右蓝灯亮灭。
 * 9. LED_BlueRight_Toggle    翻转前右蓝灯。
 * 10. LED_Init               初始化 LED GPIO 和默认灯效。
 * 11. LED_Blink              按当前模式执行阻塞式闪烁流程。
 *
 * 硬件说明:
 * - 当前 LED 低电平点亮，因此 ON 使用 GPIO_ResetBits，OFF 使用 GPIO_SetBits。
 * - PB3/PB4 默认与 JTAG 相关，LED 初始化会关闭 JTAG 以释放 PB3。
 */
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
    GPIO_InitTypeDef GPIO_InitTypeStructure;

    // 1. 开启 GPIOB 和 AFIO 时钟。AFIO 用于关闭 JTAG，释放 PB3 给 LED 使用。
    RCC_APB2PeriphClockCmd(LED_RCC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    // 2. 四个 LED 引脚配置为普通推挽输出。
    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitTypeStructure.GPIO_Pin = LED_RedLeft_Pin | LED_RedRight_Pin | LED_BlueLeft_Pin | LED_BlueRight_Pin;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_GPIO, &GPIO_InitTypeStructure);

    // 3. 设置默认灯效。LED_Blink 会根据这些字段决定闪烁方式和延时。
    setLED.FlashTime = 300;
    setLED.status = OneByOne;
}

void LED_Blink(void)
{
    switch (setLED.status)
    {
    case AlwaysOn:
        LED_RedLeft_ON();
        LED_RedRight_ON();
        LED_BlueLeft_ON();
        LED_BlueRight_ON();
        break;

    case AlwaysOff:
        LED_RedLeft_OFF();
        LED_RedRight_OFF();
        LED_BlueLeft_OFF();
        LED_BlueRight_OFF();
        break;

    case OneByOne:
        // 依次翻转四个灯，每一步都延时，因此该函数适合放在独立 LED 任务中。
        LED_BlueRight_Toggle();
        vTaskDelay(setLED.FlashTime);
        LED_BlueLeft_Toggle();
        vTaskDelay(setLED.FlashTime);
        LED_RedLeft_Toggle();
        vTaskDelay(setLED.FlashTime);
        LED_RedRight_Toggle();
        vTaskDelay(setLED.FlashTime);
        break;

    case AllFlashLight:
        LED_RedLeft_Toggle();
        LED_RedRight_Toggle();
        LED_BlueLeft_Toggle();
        LED_BlueRight_Toggle();
        vTaskDelay(setLED.FlashTime);
        break;

    case AlternateFlash:
        LED_RedLeft_Toggle();
        LED_RedRight_Toggle();
        vTaskDelay(setLED.FlashTime);
        LED_BlueLeft_Toggle();
        LED_BlueRight_Toggle();
        vTaskDelay(setLED.FlashTime);
        break;
    }
}
