#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "beep.h"
#include "key.h" 
#include "includes.h"

// 任务优先级定义
#define START_TASK_PRIO      4
#define PRODUCER1_TASK_PRIO  5 // 生产者1：按键检测
#define CONSUMER1_TASK_PRIO  6 // 消费者1：执行动作
#define PRODUCER2_TASK_PRIO  7 // 生产者2：串口接收
#define CONSUMER2_TASK_PRIO  8 // 消费者2：反向显示

// 任务堆栈大小定义
#define START_STK_SIZE       128
#define PRODUCER1_STK_SIZE   128
#define CONSUMER1_STK_SIZE   128
#define PRODUCER2_STK_SIZE   128
#define CONSUMER2_STK_SIZE   128

// 任务堆栈声明
OS_STK START_TASK_STK[START_STK_SIZE];
OS_STK PRODUCER1_TASK_STK[PRODUCER1_STK_SIZE];
OS_STK CONSUMER1_TASK_STK[CONSUMER1_STK_SIZE];
OS_STK PRODUCER2_TASK_STK[PRODUCER2_STK_SIZE];
OS_STK CONSUMER2_TASK_STK[CONSUMER2_STK_SIZE];

// 信号量
OS_EVENT *Key0Sem;      // 对应 KEY0
OS_EVENT *Key1Sem;      // 对应 KEY1
OS_EVENT *Key2Sem;      // 对应 KEY2
OS_EVENT *KeyWkupSem;   // 对应 WK_UP
OS_EVENT *SerialSem;    // 对应串口接收

// 共享缓冲区
char shared_buffer[USART_REC_LEN];

// 任务函数声明
void start_task(void *pdata);
void producer1_task(void *pdata); // 按键
void consumer1_task(void *pdata); // 动作
void producer2_task(void *pdata); // 串口接收
void consumer2_task(void *pdata); // 串口反向显示

int main(void)
{
    delay_init(168);
    OSInit();
    OSTaskCreate(start_task, (void *)0, &START_TASK_STK[START_STK_SIZE - 1], START_TASK_PRIO);
    OSStart();
    return 0; 
}

// 起始任务
void start_task(void *pdata)
{
    // 硬件初始化
    delay_init(168);
    uart_init(115200);
    LED_Init();
    KEY_Init();
    BEEP_Init();

    // 创建信号量，初始值为0
    Key0Sem    = OSSemCreate(0);
    Key1Sem    = OSSemCreate(0);
    Key2Sem    = OSSemCreate(0);
    KeyWkupSem = OSSemCreate(0);
    SerialSem  = OSSemCreate(0);

    printf("--- Start Task: Creating worker tasks... ---\r\n");

    // 创建4个工作任务
    OSTaskCreate(producer1_task, (void *)0, &PRODUCER1_TASK_STK[PRODUCER1_STK_SIZE - 1], PRODUCER1_TASK_PRIO);
    OSTaskCreate(consumer1_task, (void *)0, &CONSUMER1_TASK_STK[CONSUMER1_STK_SIZE - 1], CONSUMER1_TASK_PRIO);
    OSTaskCreate(producer2_task, (void *)0, &PRODUCER2_TASK_STK[PRODUCER2_STK_SIZE - 1], PRODUCER2_TASK_PRIO);
    OSTaskCreate(consumer2_task, (void *)0, &CONSUMER2_TASK_STK[CONSUMER2_STK_SIZE - 1], CONSUMER2_TASK_PRIO);

    // 初始化任务链：只让生产者1运行，其他都挂起
    OSTaskSuspend(CONSUMER1_TASK_PRIO);
    OSTaskSuspend(PRODUCER2_TASK_PRIO);
    OSTaskSuspend(CONSUMER2_TASK_PRIO);

    // 删除起始任务
    OSTaskDel(OS_PRIO_SELF);
}

// 生产者1：检测按键
void producer1_task(void *pdata)
{
    u8 key;
    printf("Producer 1 (KEY): Waiting for key press...\r\n");
    while(1)
    {
        key = KEY_Scan(0); // 使用非连续扫描模式
        if (key) {
            switch(key) {
                case KEY0_PRES: OSSemPost(Key0Sem);    break;
                case KEY1_PRES: OSSemPost(Key1Sem);    break;
                case KEY2_PRES: OSSemPost(Key2Sem);    break;
                case WKUP_PRES: OSSemPost(KeyWkupSem); break;
            }
            printf("Producer 1 (KEY): Key pressed. Resuming Consumer 1.\r\n");
            OSTaskResume(CONSUMER1_TASK_PRIO); // 唤醒消费者1
            OSTaskSuspend(OS_PRIO_SELF);       // 挂起自己
            
            printf("Producer 1 (KEY): Waiting for key press...\r\n");
        }
        OSTimeDly(10); // 短暂延时
    }
}

// 消费者1：根据信号量执行动作
void consumer1_task(void *pdata)
{
    while(1)
    {
        // 使用 OSSemAccept 非阻塞地检查哪个信号量被释放了
        if(OSSemAccept(Key0Sem) > 0) {
            printf("Consumer 1 (ACTION): KEY0 signal received. Blinking LED0.\r\n");
            GPIO_ResetBits(GPIOF, GPIO_Pin_9); // LED0 亮
            OSTimeDly(100);
            GPIO_SetBits(GPIOF, GPIO_Pin_9);   // LED0 灭
        } else if (OSSemAccept(Key1Sem) > 0) {
            printf("Consumer 1 (ACTION): KEY1 signal received. Blinking LED1.\r\n");
            GPIO_ResetBits(GPIOF, GPIO_Pin_10); // LED1 亮
            OSTimeDly(100);
            GPIO_SetBits(GPIOF, GPIO_Pin_10);   // LED1 灭
        } else if (OSSemAccept(Key2Sem) > 0) {
            printf("Consumer 1 (ACTION): KEY2 signal received. Beeping.\r\n");
            GPIO_SetBits(GPIOF, GPIO_Pin_8);    // BEEP on
            OSTimeDly(200);
            GPIO_ResetBits(GPIOF, GPIO_Pin_8);  // BEEP off
        } else if (OSSemAccept(KeyWkupSem) > 0) {
            printf("Consumer 1 (ACTION): WK_UP signal received. Sending string via USART.\r\n");
            printf("### This is a string from Consumer 1 ###\r\n");
        }
        
        printf("Consumer 1 (ACTION): Task finished. Resuming Producer 2.\r\n");
        OSTaskResume(PRODUCER2_TASK_PRIO); // 唤醒生产者2
        OSTaskSuspend(OS_PRIO_SELF);       // 挂起自己
    }
}

// 生产者2：接收串口数据
void producer2_task(void *pdata)
{
    printf("Producer 2 (USART): Waiting for serial input...\r\n");
    while(1)
    {
        // 检查串口是否接收到一帧数据 (以回车换行为结束标志)
        if(USART_RX_STA & 0x8000) {
            // 将串口硬件缓冲区的数据复制到共享缓冲区
            strncpy(shared_buffer, (const char*)USART_RX_BUF, USART_REC_LEN);
            shared_buffer[USART_RX_STA & 0x3fff] = '\0'; // 添加字符串结束符
            
            printf("Producer 2 (USART): Received. Resuming Consumer 2.\r\n");
            
            USART_RX_STA = 0; // 清空串口接收状态
            OSSemPost(SerialSem); // 发送信号
            
            OSTaskResume(CONSUMER2_TASK_PRIO); // 唤醒消费者2
            OSTaskSuspend(OS_PRIO_SELF);       // 挂起自己

            printf("Producer 2 (USART): Waiting for serial input...\r\n");
        }
        OSTimeDly(10); // 短暂延时
    }
}

// 消费者2：反向显示串口数据
void consumer2_task(void *pdata)
{
    INT8U err;
    char temp;
    int i, j;
    
    while(1)
    {
        OSSemPend(SerialSem, 0, &err); // 等待信号
        if(err == OS_ERR_NONE) {
            printf("Consumer 2 (REVERSE): Signal received. Reversing string.\r\n");
            
            // 字符串反转
            i = 0;
            j = strlen(shared_buffer) - 1;
            while(i < j) {
                temp = shared_buffer[i];
                shared_buffer[i] = shared_buffer[j];
                shared_buffer[j] = temp;
                i++;
                j--;
            }
            
            printf("Consumer 2 (REVERSE): Reversed string is '%s'\r\n", shared_buffer);
        }
        
        printf("Consumer 2 (REVERSE): Task finished. Resuming Producer 1.\r\n\r\n");
        OSTaskResume(PRODUCER1_TASK_PRIO); // 唤醒生产者1
        OSTaskSuspend(OS_PRIO_SELF);       // 挂起自己
    }
}
