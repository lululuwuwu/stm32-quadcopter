#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
 * FreeRTOS API DOCUMENTATION AVAILABLE ON THE FreeRTOS.org WEB SITE.
 *
 * See http://www.freertos.org/a00110.html
 *----------------------------------------------------------*/

#define configUSE_PREEMPTION		1  // 1: 抢占式调度器, 0: 协程式调度器
#define configUSE_IDLE_HOOK			0  //如果希望使用空闲钩子，请将其设置为 1 (vApplicationIdleHook()函数将在每次空闲任务被执行时调用)
#define configUSE_TICK_HOOK			0  //如果希望使用滴答钩子，请将其设置为 1；(vApplicationTickHook()函数会在每次 FreeRTOS 的时钟中断（也称为“滴答”中断）触发时被调用)
#define configCPU_CLOCK_HZ			( ( unsigned long ) 72000000 )  //输入内部时钟的执行频率（单位为 Hz） 
#define configTICK_RATE_HZ			( ( TickType_t ) 1000 ) //RTOS 滴答中断的频率 , 一个时间片是1ms
#define configMAX_PRIORITIES		( 10 ) //最大优先级是10，每个任务均被分配了从 0 到 ( configMAX_PRIORITIES - 1 ) 的优先级，configMAX_PRIORITIES 无法高于 32，优先级数字小表示任务优先级低
#define configMINIMAL_STACK_SIZE	( ( unsigned short ) 128 ) //任务使用的堆栈大小, STM32F103 实际的字节是STACK_SIZE * 4
#define configTOTAL_HEAP_SIZE		( ( size_t ) ( 15 * 1024 ) ) //堆中可用的 RAM 总量
#define configMAX_TASK_NAME_LEN		( 16 ) //创建任务时，赋予该任务的描述性名称的最大允许长度
#define configUSE_TRACE_FACILITY	0  //协助执行可视化和跟踪,调试用的
#define configUSE_16_BIT_TICKS		0 //假设滴答频率 为 1000Hz,使用 32 位计数器时为 4,294,967.296 秒  约等于49.7天。
#define configIDLE_SHOULD_YIELD		1  //设置为 1， 则在其他具有空闲优先级的任务准备运行时，空闲任务将立即让出 CPU


/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function.  启用/关闭 功能 */

#define INCLUDE_vTaskPrioritySet		1
#define INCLUDE_uxTaskPriorityGet		1
#define INCLUDE_vTaskDelete				1
#define INCLUDE_vTaskCleanUpResources	0
#define INCLUDE_vTaskSuspend			1
#define INCLUDE_vTaskDelayUntil			1
#define INCLUDE_vTaskDelay				1   //开启任务延时


//开启计数信号量
#define configUSE_COUNTING_SEMAPHORES    0
#define configUSE_MUTEXES	0
#define configUSE_RECURSIVE_MUTEXES	0


/* This is the raw value as per the Cortex-M3 NVIC.  Values can be 255
(lowest) to 0 (1?) (highest). */
#define configKERNEL_INTERRUPT_PRIORITY 		255  //设置 RTOS 内核自身使用的中断优先级。 一般设置为最低优先级, 不至于屏蔽其他优先级程序
/* !!!! configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to zero !!!!
See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html. */
// #define configMAX_SYSCALL_INTERRUPT_PRIORITY 	191 /* equivalent to 0xb0, or priority 11. */ // FreeRTOS 的管理的最高优先级
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 	5<<4  //建议配置，大于等于5的中断中才能调用FromISR等FreeRTOS API函数


/* This is the value being used as per the ST library which permits 16
priority values, 0 to 15.  This must correspond to the
configKERNEL_INTERRUPT_PRIORITY setting.  Here 15 corresponds to the lowest
NVIC value of 255. */
#define configLIBRARY_KERNEL_INTERRUPT_PRIORITY	15  //FreeRTOS 的库函数会在优先级 15 上运行（最低优先级），并且不会干扰优先级较高的中断（如优先级 0-14 的中断）

//使用FreeRTOS的内置方法处理，把原STM32中的方法注释掉
#define vPortSVCHandler SVC_Handler  //用于初始化第一次任务切换。
#define xPortSysTickHandler SysTick_Handler  //用于定时产生系统滴答，触发任务调度。
#define xPortPendSVHandler PendSV_Handler  //用于延迟触发上下文切换，确保上下文切换只在需要时执行。


#endif /* FREERTOS_CONFIG_H */
