#include "stm32f10x.h"
#include <stdio.h>
#include "OLED.h"
#include "remoterData.h"
#include "show.h"

// OLED 一共提供 4 个窗口, 通过 RemoterData.windows 选择显示哪一个。
#define SHOW_WINDOW_COUNT 4

// OLED 中文列坐标是 1~8，每个汉字占 16 像素；ASCII 列坐标是 1~16，每个字符占 8 像素。
static void ShowChinese(uint8_t line, uint8_t column, const char *text)
{
    OLED_Show_Multiple_Chinese(line, column, (char *)text);
}

// 把 NRF_RSSI_count 转成 0 到 5 档信号强度。
// NRF 没连接时直接显示 0 档。
static uint8_t ShowSignalLevel(void)
{
    uint16_t rssi;

    if (RemoterData.NRF_Connect == 0)
    {
        return 0;
    }

    rssi = RemoterData.NRF_RSSI_count;
    if (rssi > 250)
    {
        rssi = 250;
    }

    return (uint8_t)((rssi + 49) / 50);
}

static const char *ShowStateText(uint8_t ok)
{
    return ok ? "正常" : "错误";
}

// 显示一位小数电压，Battery_* 的单位是毫伏。
static void ShowVoltage(uint8_t line, uint8_t column, uint16_t mv)
{
    char voltage[5];

    snprintf(voltage, sizeof(voltage), "%u.%u",
             (unsigned int)(mv / 1000),
             (unsigned int)((mv % 1000) / 100));
    OLED_ShowString(line, column, voltage);
}

// 窗口 0: 遥控主界面。
// 第 1 行: 中文标题、信道号和信号强度。
// 第 2 行: 遥控器电压和飞行器电压。
// 第 3/4 行: 四个遥控通道。
static void ShowMainWindow(void)
{
    char line[17];

    snprintf(line, sizeof(line), "C%03u", (unsigned int)RemoterData.NRF_Channel);
    OLED_ShowString(1, 1, line);
    snprintf(line, sizeof(line), "S%u", (unsigned int)ShowSignalLevel());
    OLED_ShowString(1, 14, line);

    OLED_ShowString(2, 1, "R-V");
    ShowVoltage(2, 5, RemoterData.Battery_RC); 
    OLED_ShowString(2, 9, "F-V");
    ShowVoltage(2, 13, RemoterData.Battery_Fly);

    snprintf(line, sizeof(line), "THR%04d", (int)RemoterData.THR);
    OLED_ShowString(3, 1, line);
    ShowChinese(3, 5, "翻滚");
    snprintf(line, sizeof(line), "%04d", (int)RemoterData.ROL);
    OLED_ShowString(3, 13, line);

    ShowChinese(4, 1, "偏航");
    snprintf(line, sizeof(line), "%04d", (int)RemoterData.YAW);
    OLED_ShowString(4, 5, line);
    ShowChinese(4, 5, "俯仰");
    snprintf(line, sizeof(line), "%04d", (int)RemoterData.PIT);
    OLED_ShowString(4, 13, line);
}

// 窗口 1: 飞行器传感器硬件状态。
// BIT0: MPU6050, BIT1: 气压计, BIT2: 无线模块, BIT3: 光流模块。
static void ShowSensorWindow(void)
{
    char line[17];
    ShowChinese(1, 2, "传感器状态");

    OLED_ShowString(2, 1, "MPU");
    ShowChinese(2, 5, ShowStateText(RemoterData.fly_test_flag & (1 << 0)));
    ShowChinese(3, 1, "气压");
    ShowChinese(3, 5, ShowStateText(RemoterData.fly_test_flag & (1 << 1)));

    
    ShowChinese(4, 1, "光流");
    ShowChinese(4, 5, ShowStateText(RemoterData.fly_test_flag & (1 << 3)));
}

// 窗口 2: 微调设置值。
static void ShowTrimWindow(void)
{
    char line[17];

    ShowChinese(1, 4, "微调");

    ShowChinese(2, 1, "俯仰");
    snprintf(line, sizeof(line), "%+04d", (int)RemoterData.OffSet_Pit);
    OLED_ShowString(2, 5, line);
    ShowChinese(3, 1, "翻滚");
    snprintf(line, sizeof(line), "%+04d", (int)RemoterData.OffSet_Rol);
    OLED_ShowString(3, 5, line);

    ShowChinese(4, 1, "偏航");
    snprintf(line, sizeof(line), "%+04d", (int)RemoterData.OffSet_Yaw);
    OLED_ShowString(4, 5, line);

}

// 窗口 3: 飞行器姿态和高度。
// X/Y/Z 分别对应俯仰、翻滚、偏航角度，H 对应高度厘米值。
static void ShowAngleWindow(void)
{
    char line[17];

    ShowChinese(1, 1, "俯仰");
    snprintf(line, sizeof(line), "%+04d", RemoterData.X);
    OLED_ShowString(1, 5, line);
    ShowChinese(2, 1, "翻滚");
    snprintf(line, sizeof(line), "%+04d", RemoterData.Y);
    OLED_ShowString(2, 5, line);

    ShowChinese(3, 1, "偏航");
    snprintf(line, sizeof(line), "%+04d", RemoterData.Z);
    OLED_ShowString(3, 5, line);
    ShowChinese(4, 1, "高度");
    snprintf(line, sizeof(line), "%04d", RemoterData.H);
    OLED_ShowString(4, 5, line);
}

// OLED 显示模块初始化。
// 任务启动时调用一次即可。
void Show_Init(void)
{
    OLED_Init();
    OLED_Clear();
}

// 根据 RemoterData.windows 刷新当前窗口。
// windows 超过 3 时会自动取余, 保证界面在 0 到 3 之间循环。
void Show_Refresh(void)
{
    OLED_Clear();

    switch (RemoterData.windows % SHOW_WINDOW_COUNT)
    {
        case 0:
            ShowMainWindow();
            break;

        case 1:
            ShowSensorWindow();
            break;

        case 2:
            ShowTrimWindow();
            break;

        case 3:
            ShowAngleWindow();
            break;

        default:
            ShowMainWindow();
            break;
    }
}
