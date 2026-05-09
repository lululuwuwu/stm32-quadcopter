#include "board.h"
#include "buzzer.h"

void Buzzer_Init(void)
{
    GPIO_InitTypeDef GPIO_InitTypeStructure;

    /* 蜂鸣器接在 PB10，低电平响，高电平关闭。 */
    RCC_APB2PeriphClockCmd(BEEP_RCC, ENABLE);

    GPIO_InitTypeStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitTypeStructure.GPIO_Pin = BEEP_PIN;
    GPIO_InitTypeStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BEEP_GPIO, &GPIO_InitTypeStructure);

    Buzzer_Off();
}

void Buzzer_On(void)
{
    GPIO_ResetBits(BEEP_GPIO, BEEP_PIN);
}

void Buzzer_Off(void)
{
    GPIO_SetBits(BEEP_GPIO, BEEP_PIN);
}
