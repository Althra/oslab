#include "stm32f4xx.h"
#include "delay.h"
#include "usart.h"

void LED_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOF, &GPIO_InitStructure);

    GPIO_SetBits(GPIOF, GPIO_Pin_9 | GPIO_Pin_10);
}

void BEEP_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOF, &GPIO_InitStructure);

    GPIO_ResetBits(GPIOF, GPIO_Pin_8);
}

#define KEY0_PRES 1
#define KEY1_PRES	2
#define KEY2_PRES	3
#define WKUP_PRES	4

void KEY_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

u8 KEY_Scan(u8 mode) {
    static u8 key_up = 1;
    if (mode) key_up = 1;
    
    if (key_up && (GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_4) == 0 || 
                   GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_3) == 0 || 
                   GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_2) == 0 || 
                   GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 1)) {
        delay_ms(10);
        key_up = 0;
        if (GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_4) == 0) return KEY0_PRES;
        if (GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_3) == 0) return KEY1_PRES;
        if (GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_2) == 0) return KEY2_PRES;
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 1) return WKUP_PRES;
    } else if (GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_4) != 0 && 
               GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_3) != 0 && 
               GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_2) != 0 && 
               GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) != 1) {
        key_up = 1;
    }
    return 0;
}

void Usart_SendString(char *str) {
    while (*str != '\0') {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
        USART_SendData(USART1, *str);
        str++;
    }
}


int main(void) {
    u8 key = 0;

    delay_init(168);
    uart_init(115200);
    
    LED_Init();
    BEEP_Init();
    KEY_Init();

    while (1) {
        key = KEY_Scan(0);
        if (key) {
            switch (key) {
                case WKUP_PRES:
                    Usart_SendString("KEY_UP is pressed, beep...\r\n");
                    GPIO_SetBits(GPIOF, GPIO_Pin_8);
                    delay_ms(200);
                    GPIO_ResetBits(GPIOF, GPIO_Pin_8);
                    break;
                case KEY0_PRES:
                    Usart_SendString("KEY0 is pressed\r\n");
                    break;
                case KEY1_PRES:
                    Usart_SendString("KEY1 is pressed\r\n");
                    break;
                case KEY2_PRES:
                    Usart_SendString("KEY2 is pressed\r\n");
                    break;
            }
        } else {
            delay_ms(10);
        }
    }
}
