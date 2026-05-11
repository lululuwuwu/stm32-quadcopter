#ifndef __FLYDATA_H
#define __FLYDATA_H

#include "board.h"

/*
 * 文件目录:
 * 1. _st_Angle  飞机欧拉角姿态数据结构。
 * 2. FlyAngle   当前飞机姿态角全局数据。
 */

typedef struct
{
    float Pitch; /* 欧拉角中的俯仰角 */
    float Roll;  /* 欧拉角中的横滚角 */
    float Yaw;   /* 欧拉角中的偏航角 */
} _st_Angle;

extern _st_Angle FlyAngle;

#endif
