#include "stm32f10x.h"                  // Device header
#include "time.h"
#include "MyRTC.h"
#include "led.h"
#include "Serial1.h"

uint16_t CurrentDate_Write[6]={2025,4,25,14,23,10};

void MyRTC_Init(void)
{

	
	/*
	系统复位后，对后备寄存器和RTC的访问被禁止，这是为了防止对后备区域(BKP)的意外写操作。执行以下操作将使能对后备寄存器和RTC的访问： 
	● 设置寄存器RCC_APB1ENR的PWREN和BKPEN位，使能电源和后备接口时钟 
	● 设置寄存器PWR_CR的DBP位，使能对后备寄存器和RTC的访问
	*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR,ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_BKP,ENABLE);
	
	PWR_BackupAccessCmd(ENABLE);
	
	/*
	可以选择以下三种RTC的时钟源： 
		─ HSE时钟除以128； 
		─ LSE振荡器时钟； 
		─ LSI振荡器时钟   --- 一般是不选中，太不准了。
	*/
	//一般选择LSE。原因是LSE是在备份域里面，可以通过电池供电， 经过2^15分频就能得到1Hz
	//LSE默认没有开启，手动开启
	/*
	LSE晶体通过在备份域控制寄存器(RCC_BDCR)里的【LSEON位启动】和关闭。 
   在备份域控制寄存器(RCC_BDCR)里的LSERDY指示LSE晶体振荡是否稳定。在启动阶段，直到这个位被硬件置’1’后，LSE时钟信号才被释放出来。
	*/

	if(BKP_ReadBackupRegister(BKP_DR3)!=0xAA55)		
	{
		
	
		//先复位
	/*
	BDRST：备份域软件复位 (Backup domain software reset) 由软件置’1’或清’0’ 
	0：复位未激活； 
	1：复位整个备份域。
	*/
	RCC_BackupResetCmd(ENABLE);
	for(uint32_t i=0;i<999999;i++);
	RCC_BackupResetCmd(DISABLE);
		
	BKP_WriteBackupRegister(BKP_DR3,0xAA55);

	
	RCC_LSEConfig(RCC_LSE_ON);//打开
	/*
	LSERDY：外部低速LSE就绪 (External low-speed oscillator ready) 
	由硬件置’1’或清’0’来指示是否外部32kHz振荡器就绪。在LSEON被清零后，该位需要6个外部低速振荡器的周期才被清零。 
	0：外部32kHz振荡器未就绪； 
	1：外部32kHz振荡器就绪。
	*/
	while(RCC_GetFlagStatus(RCC_FLAG_LSERDY)!=SET);
	
	//配置LSE为RTC的时钟来源
	
	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
	
	
	//选择HSE作为时钟来源
	
	/*由软件设置来选择RTC时钟源。一旦RTC时钟源被选定，直到下次后备域被复位，它不能在被改变。可通过设置BDRST位来清除。*/
//	RCC_RTCCLKConfig(RCC_RTCCLKSource_HSE_Div128);
	
	
	//开启RTC功能
	RCC_RTCCLKCmd(ENABLE);
	
	/*
	
	如果APB1接口曾经被关闭，而读操作又是在刚刚重新开启APB1之后，则在第一次的内部寄存器更新之前，从APB1上读出的RTC寄存器数值可能被破坏了(通常读到0)。下述几种情况下能够发生这种情形： 
		● 发生系统复位或电源复位 
		● 系统刚从待机模式唤醒
		● 系统刚从停机模式唤醒(参见第4.3节：低功耗模式)。 
	所有以上情况中，APB1接口被禁止时(复位、无时钟或断电)RTC核仍保持运行状态。 
	因此，若在读取RTC寄存器时，RTC的APB1接口曾经处于禁止状态，则软件首先必须等待RTC_CRL寄存器中的【RSF位(寄存器同步标志)被硬件置’1’】。
	*/
	RTC_WaitForSynchro();//等待同步
	
	/*配置分频值*/
	/*
	配置过程： 
	1. 查询RTOFF位，直到RTOFF的值变为’1’ 
	2. 置CNF值为1，进入配置模式 
	3. 对一个或多个RTC寄存器进行写操作 
	4. 清除CNF标志位，退出配置模式 
	5. 查询RTOFF，直至RTOFF位变为’1’以确认写操作已经完成。 
	
	*/
	
	RTC_WaitForLastTask();
	uint32_t psc = 32768;//分频值
	RTC_SetPrescaler(psc-1); //1. 进入配置模式 2.写入RTC_PRL 3.退出配置
	RTC_WaitForLastTask();
	
	
	//预分频值存储在BKP_DR1 ,BKP_DR2 
	BKP_WriteBackupRegister( BKP_DR1,   psc&0x0000FFFF    );
	BKP_WriteBackupRegister( BKP_DR2,  (psc&0xFFFF0000)>>16);
	
	
	//假设设置一个默认时间(只设置一次)
	RTC_SetCurrentDate(CurrentDate_Write);//设置时间
	

	
	
	}
	else
	{
		RTC_WaitForSynchro();//等待同步,在操作
	}
	
	
	//开启闹钟中断
	RTC_ITConfig(RTC_IT_ALR,ENABLE);
	
	//设置NVIC
	NVIC_InitTypeDef NVIC_InitTypeStructure;
	NVIC_InitTypeStructure.NVIC_IRQChannel = RTC_IRQn;
	NVIC_InitTypeStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitTypeStructure.NVIC_IRQChannelPreemptionPriority = 3;
	NVIC_InitTypeStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitTypeStructure);
	
}


uint32_t RTC_GetTimeStamp(void)
{
	return RTC_GetCounter(); 
}


//获取当前的日历
//数组存储： uint16_t CurrentDate[]={2026,4,25,14,15,13};

void RTC_GetCurrentDate(uint16_t *date)
{
	//1.获取时间戳
	time_t time_counter = RTC_GetCounter() + 8*60*60; //切换为北京时间
	//2.获取time struct
	//struct tm *localtime(const time_t * /*timer*/)
	struct tm struct_time = *localtime(&time_counter);
	
	
	/*
	struct tm {
    int tm_sec;    seconds after the minute, 0 to 60
                     (0 - 60 allows for the occasional leap second) 
    int tm_min;   minutes after the hour, 0 to 59
    int tm_hour;   hours since midnight, 0 to 23 
    int tm_mday;   day of the month, 1 to 31 
    int tm_mon;    months since January, 0 to 11 
    int tm_year;   years since 1900 
    int tm_wday;   days since Sunday, 0 to 6 
    int tm_yday;  days since January 1, 0 to 365 
    int tm_isdst; Daylight Savings Time flag  //夏令时
	*/
	//3.放置在数组里面
	date[0] = struct_time.tm_year + 1900;//年
	date[1] = struct_time.tm_mon + 1;//月
	date[2] = struct_time.tm_mday;//日  
	date[3] = struct_time.tm_hour;//时  
	date[4] = struct_time.tm_min;//分
	date[5] = struct_time.tm_sec;//秒
	
}

//设置时间
void RTC_SetCurrentDate(uint16_t *date)
{
	//1.获取struct_tm
	struct tm struct_time ={0};
	
	struct_time.tm_year = date[0] - 1900;
	struct_time.tm_mon = date[1] - 1;
	struct_time.tm_mday = date[2];
	struct_time.tm_hour = date[3];
	struct_time.tm_min = date[4];
	struct_time.tm_sec = date[5];	
	
	//2.获取time_t
	//time_t mktime(struct tm * /*timeptr*/)
	time_t time_counter = mktime(&struct_time) -  8*60*60;
	
	//3.写入RTC计数器
	RTC_WaitForLastTask();
	RTC_SetCounter(time_counter);
	RTC_WaitForLastTask();	
	
}

//获取毫秒
uint16_t RTC_Get_MS(void)

{
	uint32_t read_psc = BKP_ReadBackupRegister(BKP_DR1)+(BKP_ReadBackupRegister(BKP_DR2)<<16);
	
	return (read_psc -1 - RTC_GetDivider()) * 1000 /  read_psc  ;
}


//设置闹钟 : uint16_t CurrentDate[]={2026,4,25,14,15,13};
void MyRTC_SetAlarm(uint16_t *AlarmDate)
{
	
	//1.获取struct_tm
	struct tm struct_time = {0};
	
	struct_time.tm_year = AlarmDate[0] - 1900;
	struct_time.tm_mon = AlarmDate[1] - 1;
	struct_time.tm_mday = AlarmDate[2];
	struct_time.tm_hour = AlarmDate[3];
	struct_time.tm_min = AlarmDate[4];
	struct_time.tm_sec = AlarmDate[5];	
	//2.获取time_t
	//time_t mktime(struct tm * /*timeptr*/)
	time_t time_counter = mktime(&struct_time) -  8*60*60 -1; //到点就闹钟响
	
	/*
	3. 配置过程： 
	1. 查询RTOFF位，直到RTOFF的值变为’1’ 
	2. 置CNF值为1，进入配置模式 
	3. 对一个或多个RTC寄存器进行写操作 
	4. 清除CNF标志位，退出配置模式 
	5. 查询RTOFF，直至RTOFF位变为’1’以确认写操作已经完成。 
	
	*/
	
	RTC_WaitForLastTask();
	RTC_SetAlarm(time_counter); //1. 进入配置模式 2.写入RTC_ALR 3.退出配置
	RTC_WaitForLastTask();	
	
	
	//保存闹钟值
	BKP_WriteBackupRegister( BKP_DR5,   time_counter&0x0000FFFF    );
	BKP_WriteBackupRegister( BKP_DR6,  (time_counter&0xFFFF0000)>>16);
		
}


//读出闹钟值
void MyRTC_GetAlarm(uint16_t *alarm)
{
	uint32_t alarm_time =  BKP_ReadBackupRegister(BKP_DR5)+(BKP_ReadBackupRegister(BKP_DR6)<<16);
	
	//1.获取时间戳
	time_t time_counter =alarm_time + 8*60*60 + 1; //切换为北京时间
	//2.获取time struct
	//struct tm *localtime(const time_t * /*timer*/)
	struct tm struct_time = *localtime(&time_counter);
	
	
	/*
	struct tm {
    int tm_sec;    seconds after the minute, 0 to 60
                     (0 - 60 allows for the occasional leap second) 
    int tm_min;   minutes after the hour, 0 to 59
    int tm_hour;   hours since midnight, 0 to 23 
    int tm_mday;   day of the month, 1 to 31 
    int tm_mon;    months since January, 0 to 11 
    int tm_year;   years since 1900 
    int tm_wday;   days since Sunday, 0 to 6 
    int tm_yday;  days since January 1, 0 to 365 
    int tm_isdst; Daylight Savings Time flag  //夏令时
	*/
	//3.放置在数组里面
	alarm[0] = struct_time.tm_year + 1900;//年
	alarm[1] = struct_time.tm_mon + 1;//月
	alarm[2] = struct_time.tm_mday;//日  
	alarm[3] = struct_time.tm_hour;//时  
	alarm[4] = struct_time.tm_min;//分
	alarm[5] = struct_time.tm_sec;//秒
}



//中断函数
void RTC_IRQHandler(void)
{

	if(RTC_GetITStatus(RTC_IT_ALR)==SET)
	{

		RTC_ClearITPendingBit(RTC_IT_ALR);
	}
}


