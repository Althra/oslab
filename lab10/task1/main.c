#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "includes.h"

// 任务优先级定义
#define START_TASK_PRIO      4
#define LED0_TASK_PRIO       5
#define LED1_TASK_PRIO       6
#define USART_TASK_PRIO      7

// 任务堆栈大小定义
#define START_STK_SIZE       128
#define LED0_STK_SIZE        128
#define LED1_STK_SIZE        128
#define USART_STK_SIZE       128

// 任务堆栈声明
OS_STK START_TASK_STK[START_STK_SIZE];
OS_STK LED0_TASK_STK[LED0_STK_SIZE];
OS_STK LED1_TASK_STK[LED1_STK_SIZE];
OS_STK USART_TASK_STK[USART_STK_SIZE];

// 任务函数声明
void start_task(void *pdata);
void led0_task(void *pdata);
void led1_task(void *pdata);
void usart_task(void *pdata);

// main 函数
int main(void)
{
    delay_init(168);
    OSInit(); // 初始化uC/OS-II内核
    
    // 创建起始任务
    OSTaskCreate(start_task, (void *)0, &START_TASK_STK[START_STK_SIZE - 1], START_TASK_PRIO);
    
    OSStart(); // 开始多任务调度，此函数永不返回
    
    return 0; 
}

// 起始任务
void start_task(void *pdata)
{
    // 硬件初始化
    LED_Init();
    uart_init(115200);

    printf("--- Start Task: Creating other tasks... ---\r\n");

    // 创建三个工作任务
    OSTaskCreate(led0_task, (void *)0, &LED0_TASK_STK[LED0_STK_SIZE - 1], LED0_TASK_PRIO);
    OSTaskCreate(led1_task, (void *)0, &LED1_TASK_STK[LED1_STK_SIZE - 1], LED1_TASK_PRIO);
    OSTaskCreate(usart_task,(void*)0, &USART_TASK_STK[USART_STK_SIZE - 1], USART_TASK_PRIO);

    // 为了启动唤醒链，先将任务2和任务3挂起
    OSTaskSuspend(LED1_TASK_PRIO);
    OSTaskSuspend(USART_TASK_PRIO);

    // 起始任务完成使命，删除自身
    OSTaskDel(OS_PRIO_SELF);
}

// 任务1：闪烁LED0
void led0_task(void *pdata)
{
    while(1)
    {
        printf("Task 1 (LED0) is running...\r\n");
        
        GPIO_ResetBits(GPIOF, GPIO_Pin_9); // LED0 亮
        OSTimeDlyHMSM(0, 0, 1, 0);        // 延时1s
        GPIO_SetBits(GPIOF, GPIO_Pin_9);   // LED0 灭
        
        OSTaskResume(LED1_TASK_PRIO);      // 唤醒任务2
        OSTaskSuspend(OS_PRIO_SELF);       // 挂起自己
    }
}

// 任务2：闪烁LED1
void led1_task(void *pdata)
{
    while(1)
    {
        printf("Task 2 (LED1) is running...\r\n");
        
        GPIO_ResetBits(GPIOF, GPIO_Pin_10); // LED1 亮
        OSTimeDlyHMSM(0, 0, 1, 0);         // 延时1s
        GPIO_SetBits(GPIOF, GPIO_Pin_10);    // LED1 灭
        
        OSTaskResume(USART_TASK_PRIO);     // 唤醒任务3
        OSTaskSuspend(OS_PRIO_SELF);       // 挂起自己
    }
}

// 任务3：串口输出
void usart_task(void *pdata)
{
    while(1)
    {
        printf("Task 3 (USART) is running and printing this message.\r\n\r\n");
        
        OSTimeDlyHMSM(0, 0, 1, 0);         // 延时1s
        OSTaskResume(LED0_TASK_PRIO);      // 唤醒任务1
        OSTaskSuspend(OS_PRIO_SELF);       // 挂起自己
    }
}
