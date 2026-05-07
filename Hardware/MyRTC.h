#ifndef __MYRTC_H
#define __MYRTC_H


uint32_t RTC_GetTimeStamp(void);

void MyRTC_Init(void);

void RTC_GetCurrentDate(uint16_t *date);

void RTC_SetCurrentDate(uint16_t *date);

uint16_t RTC_Get_MS(void);

void MyRTC_SetAlarm(uint16_t *AlarmDate);

void MyRTC_GetAlarm(uint16_t *alarm);

#endif

