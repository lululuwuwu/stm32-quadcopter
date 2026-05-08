#ifndef __LIGHT_SENSOR_H
#define __LIGHT_SENSOR_H

//初始化
void LightSensor_Init(void);

//获取光线强度: 1- 光线弱  ； 0 - 光线强
uint8_t LightSensor_GetValue(void);

#endif
