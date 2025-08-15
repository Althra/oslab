#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "key.h"
#include "beep.h" 
#include "includes.h"
#include "malloc.h"
#include "os_cpu.h"

// 任务优先级、堆栈大小、IPC对象声明
#define START_TASK_PRIO         4
#define TASK1_QUEUE_PRIO        5 // 任务1：队列消费者
#define TASK2_KEY_PRODUCER_PRIO 6 // 任务2：按键生产者
#define TASK3_FLAG_PRIO         7 // 任务3：事件标志消费者
#define TASK4_COORDINATOR_PRIO  8 // 任务4：协调器

#define START_STK_SIZE          128
#define TASK1_STK_SIZE          128
#define TASK2_STK_SIZE          128
#define TASK3_STK_SIZE          128
#define TASK4_STK_SIZE          256

OS_STK START_TASK_STK[START_STK_SIZE];
OS_STK TASK1_STK[TASK1_STK_SIZE];
OS_STK TASK2_STK[TASK2_STK_SIZE];
OS_STK TASK3_STK[TASK3_STK_SIZE];
OS_STK TASK4_STK[TASK4_STK_SIZE];

OS_EVENT    *KeyMbox;
OS_FLAG_GRP *KeyFlagGroup;
OS_EVENT    *MsgQueue;
#define     MSG_QUEUE_SIZE 20
void        *MsgQueueTbl[MSG_QUEUE_SIZE];
OS_TMR      *myTimer;

#define FLAG_KEY0    0x01
#define FLAG_KEY1    0x02
#define FLAG_KEY2    0x04
#define FLAG_WKUP    0x08

// 任务和回调函数声明
void start_task(void *pdata);
void task1_queue_consumer(void *pdata);
void task2_key_producer(void *pdata);
void task3_flag_consumer(void *pdata);
void task4_coordinator(void *pdata);
void timer_callback(OS_TMR *ptmr, void *p_arg);

int main(void)
{
    OSInit();
    OSTaskCreate(start_task, (void *)0, &START_TASK_STK[START_STK_SIZE - 1], START_TASK_PRIO);
    OSStart();
    return 0; 
}

void start_task(void *pdata)
{
    INT8U err;
    
    // 硬件和内存初始化
    delay_init(168);
    uart_init(115200);
    LED_Init();
    KEY_Init();
    BEEP_Init();
    my_mem_init(SRAMIN); // 初始化内部SRAM内存池

    KeyMbox = OSMboxCreate((void *)0);
    KeyFlagGroup = OSFlagCreate(0, &err);
    MsgQueue = OSQCreate(&MsgQueueTbl[0], MSG_QUEUE_SIZE);
    
    myTimer = OSTmrCreate(0, 100, OS_TMR_OPT_PERIODIC, 
                          (OS_TMR_CALLBACK)timer_callback,
                          (void *)0, "MyTimer", &err);

    printf("--- Start Task: Creating all worker tasks... ---\r\n");

    OSTaskCreate(task1_queue_consumer, (void *)0, &TASK1_STK[TASK1_STK_SIZE - 1], TASK1_QUEUE_PRIO);
    OSTaskCreate(task2_key_producer, (void *)0, &TASK2_STK[TASK2_STK_SIZE - 1], TASK2_KEY_PRODUCER_PRIO);
    OSTaskCreate(task3_flag_consumer, (void *)0, &TASK3_STK[TASK3_STK_SIZE - 1], TASK3_FLAG_PRIO);
    OSTaskCreate(task4_coordinator, (void *)0, &TASK4_STK[TASK4_STK_SIZE - 1], TASK4_COORDINATOR_PRIO);

    OSTaskDel(OS_PRIO_SELF);
}

// 任务2：按键生产者，扫描按键并将键值发送到邮箱
void task2_key_producer(void *pdata)
{
    u8 key;
    while(1)
    {
        key = KEY_Scan(0);
        if (key) {
            printf("TASK2 (Key Producer): Key %d pressed, send to Mailbox...\r\n", key);
            OSMboxPost(KeyMbox, (void *)(INT32U)key);
        }
        OSTimeDlyHMSM(0, 0, 0, 20); // 每20ms扫描一次
    }
}

// 任务4：协调器，处理核心逻辑
void task4_coordinator(void *pdata)
{
    u8 key;
    INT8U err;
    INT8U tmr_state;
    OS_TCB task1_tcb;

    while(1)
    {
        key = (u8)(INT32U)OSMboxPend(KeyMbox, 0, &err);
        printf("TASK4 (Coordinator): Get Key %d from Mbox...\r\n", key);
        
        switch(key)
        {
            case KEY0_PRES:
                OSFlagPost(KeyFlagGroup, FLAG_KEY0, OS_FLAG_SET, &err);
                LED0 = !LED0;
                break;
            case KEY1_PRES:
                OSFlagPost(KeyFlagGroup, FLAG_KEY1, OS_FLAG_SET, &err);
                tmr_state = OSTmrStateGet(myTimer, &err);
                if (tmr_state == OS_TMR_STATE_RUNNING) {
                    OSTmrStop(myTimer, OS_TMR_OPT_CALLBACK, (void*)0, &err);
                    printf("TASK4 (Coordinator): Timer Stopped.\r\n");
                } else {
                    OSTmrStart(myTimer, &err);
                    printf("TASK4 (Coordinator): Timer Started.\r\n");
                }
                break;
            case KEY2_PRES:
                OSFlagPost(KeyFlagGroup, FLAG_KEY2, OS_FLAG_SET, &err);
                BEEP = !BEEP;
                break;
            case WKUP_PRES:
                OSFlagPost(KeyFlagGroup, FLAG_WKUP, OS_FLAG_SET, &err);
                
                OSTaskQuery(TASK1_QUEUE_PRIO, &task1_tcb);
                if ((task1_tcb.OSTCBStat & OS_STAT_SUSPEND) != 0) { 
                    // 如果已挂起，则恢复
                    OSTaskResume(TASK1_QUEUE_PRIO);
                    printf("TASK4 (Coordinator): Task1 resumed.\r\n");
                } else {
                    // 如果未挂起，则挂起
                    OSTaskSuspend(TASK1_QUEUE_PRIO);
                    printf("TASK4 (Coordinator): Task1 suspended.\r\n");
                }
                break;
        }
        OSTimeDly(1);
    }
}

// 任务3：事件标志消费者，报告哪个按键被按下
void task3_flag_consumer(void *pdata)
{
    INT8U err;
    OS_FLAGS flags;
    while(1)
    {
        flags = OSFlagPend(KeyFlagGroup, 
                           FLAG_KEY0 | FLAG_KEY1 | FLAG_KEY2 | FLAG_WKUP, 
                           OS_FLAG_WAIT_SET_ANY | OS_FLAG_CONSUME, 
                           0, 
                           &err);
        
        printf("TASK3 (Flag Consumer): Get flag, ");
        if(flags & FLAG_KEY0) printf("KEY0 was pressed.\r\n");
        if(flags & FLAG_KEY1) printf("KEY1 was pressed.\r\n");
        if(flags & FLAG_KEY2) printf("KEY2 was pressed.\r\n");
        if(flags & FLAG_WKUP) printf("WK_UP was pressed.\r\n");
        OSTimeDly(1);
    }
}

// 任务1：消息队列消费者，处理定时器发来的消息
void task1_queue_consumer(void *pdata)
{
    char *msg;
    INT8U err;
    while(1)
    {
        // 等待来自定时器的消息队列
        msg = (char*)OSQPend(MsgQueue, 0, &err);
        
        printf("TASK1 (Queue Consumer): Received Msg -> \"%s\"\r\n", msg);
        
        myfree(SRAMIN, msg);
    }
}

// 软件定时器回调函数
void timer_callback(OS_TMR *ptmr, void *p_arg)
{
    static int counter = 0;
    char *msg_payload;

    msg_payload = mymalloc(SRAMIN, 32); 
    if (msg_payload) {
        sprintf(msg_payload, "Timer message #%d", counter++);
        // 向队列发送消息指针
        OSQPost(MsgQueue, (void*)msg_payload);
    }
}
