#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("Dual BSD/GPL");
//实现模块初始化函数
static int __init hello_init(void)
{
	printk(KERN_ALERT"Hello,Kernel!\n");
	return 0;     
}

//实现模块退出函数 
static void __exit hello_exit(void)
{
	printk(KERN_ALERT"Goodbye,Kernel!\n");
}
//申明模块初始化函数
module_init(hello_init);
//申明模块退出函数
module_exit(hello_exit);
