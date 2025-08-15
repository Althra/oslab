#include "sys.h"
#include "delay.h"
#include "usart.h"

extern void Mode_Change(void);

int main(void)
{
	delay_init(168);
	uart_init(115200);

	Mode_Change();
	
	while(1)
	{
	}
}
