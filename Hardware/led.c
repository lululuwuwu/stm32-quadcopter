#include "stm32f10x.h"
#include "led.h"

void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitTypeStructure;

    /* 红灯 PB9、蓝灯 PB1，均配置为推挽输出。 */
    RCC_APB2PeriphClockCmd(LED_RED_RCC | LED_BLUE_RCC, ENABLE);

    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_InitTypeStructure.GPIO_Pin = LED_RED_PIN;
    GPIO_Init(LED_RED_GPIO, &GPIO_InitTypeStructure);

    GPIO_InitTypeStructure.GPIO_Pin = LED_BLUE_PIN;
    GPIO_Init(LED_BLUE_GPIO, &GPIO_InitTypeStructure);

    LED_RedOff();
    LED_BlueOff();
}

void LED_RedOn(void)
{
    GPIO_SetBits(LED_RED_GPIO, LED_RED_PIN);
}

void LED_RedOff(void)
{
    GPIO_ResetBits(LED_RED_GPIO, LED_RED_PIN);
}

void LED_BlueOn(void)
{
    GPIO_SetBits(LED_BLUE_GPIO, LED_BLUE_PIN);
}

void LED_BlueOff(void)
{
    GPIO_ResetBits(LED_BLUE_GPIO, LED_BLUE_PIN);
}

void LED_RedToggle(void)
{
    GPIO_WriteBit(LED_RED_GPIO, LED_RED_PIN,
                  (BitAction)(1 - GPIO_ReadOutputDataBit(LED_RED_GPIO, LED_RED_PIN)));
}

void LED_BlueToggle(void)
{
    GPIO_WriteBit(LED_BLUE_GPIO, LED_BLUE_PIN,
                  (BitAction)(1 - GPIO_ReadOutputDataBit(LED_BLUE_GPIO, LED_BLUE_PIN)));
}

void LED1_On(void)
{
    /* 兼容旧接口：LED1 对应红灯。 */
    LED_RedOn();
}

void LED1_Off(void)
{
    LED_RedOff();
}

void LED2_On(void)
{
    /* 兼容旧接口：LED2 对应蓝灯。 */
    LED_BlueOn();
}

void LED2_Off(void)
{
    LED_BlueOff();
}

void LED2_Toggle(void)
{
    LED_BlueToggle();
}
