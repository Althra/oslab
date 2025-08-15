#include "sys.h"
#include "delay.h"
#include "usart.h"

// ASM Function
extern int Assembly_ADD(int value, int op);

// C Function
int C_transfer(void)
{
	int value = 8;
	return value;
}

int main(void)
{
	int return_value = 0;
	
	delay_init(168);
	uart_init(115200);
	
	return_value = Assembly_ADD(10, 1); // 调用ASM程序
	
	while(1)
	{
        printf("result: %d\r\n", return_value); // 持续输出
	}
}
