#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/init_task.h>

#define DRIVER_NAME "process_lister"
#define CLASS_NAME  "process_lister_class"

static dev_t dev_num;         // 用于存储设备号
static struct cdev my_cdev;   // 字符设备结构
static struct class *my_class; // 设备类结构

// 遍历并打印进程信息
static void print_all_processes(const char *trigger_function) {
    struct task_struct *p;
    p = &init_task;
    printk(KERN_INFO "======= Process List (Triggered by %s) =======\n", trigger_function);
    printk(KERN_INFO "PID\tState\tPriority\tParent PID\tName\n");
    
    for_each_process(p) {
        printk(KERN_INFO "%d\t%u\t%d\t\t%d\t\t%s\n",
               p->pid,
               p->__state,      // 进程状态
               p->prio,       // 进程优先级
               p->parent->pid, // 父进程ID
               p->comm);      // 进程名
    }
    printk(KERN_INFO "====================================================\n");
}

// file_operations 的实现

static int my_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "process_lister: Device opened.\n");
    print_all_processes("open"); // 当设备被打开时，打印进程列表
    return 0;
}

static int my_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "process_lister: Device closed.\n");
    return 0;
}

static ssize_t my_read(struct file *file, char __user *buf, size_t len, loff_t *off) {
    printk(KERN_INFO "process_lister: Device read operation triggered.\n");
    print_all_processes("read"); // 当设备被读取时，打印进程列表
    return 0;
}

static ssize_t my_write(struct file *file, const char __user *buf, size_t len, loff_t *off) {
    printk(KERN_INFO "process_lister: Device write operation triggered.\n");
    print_all_processes("write"); // 当设备被写入时，打印进程列表
    // 返回写入的字节数，表示写入成功
    return len;
}

// 定义 file_operations 结构体，并将函数指针与之关联
static const struct file_operations my_fops = {
    .owner   = THIS_MODULE,
    .open    = my_open,
    .release = my_release,
    .read    = my_read,
    .write   = my_write,
};

// 模块加载时执行
static int __init my_driver_init(void) {
    printk(KERN_INFO "process_lister: Initializing the driver...\n");

    // 1. 动态分配设备号
    if (alloc_chrdev_region(&dev_num, 0, 1, DRIVER_NAME) < 0) {
        printk(KERN_ALERT "process_lister: Failed to allocate device number\n");
        return -1;
    }

    // 2. 初始化 cdev 结构，并与 file_operations 关联
    cdev_init(&my_cdev, &my_fops);
    my_cdev.owner = THIS_MODULE;

    // 3. 将 cdev 添加到内核
    if (cdev_add(&my_cdev, dev_num, 1) < 0) {
        printk(KERN_ALERT "process_lister: Failed to add cdev to kernel\n");
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    // 4. 创建设备类 (class)
    my_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(my_class)) {
        printk(KERN_ALERT "process_lister: Failed to create device class\n");
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(my_class);
    }

    // 5. 在 /dev/ 目录下创建设备文件
    if (device_create(my_class, NULL, dev_num, NULL, DRIVER_NAME) == NULL) {
        printk(KERN_ALERT "process_lister: Failed to create device file\n");
        class_destroy(my_class);
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    printk(KERN_INFO "process_lister: Driver initialized successfully.\n");
    printk(KERN_INFO "process_lister: Device file created at /dev/%s\n", DRIVER_NAME);
    return 0;
}

// 模块卸载时执行
static void __exit my_driver_exit(void) {
    printk(KERN_INFO "process_lister: Exiting the driver...\n");

    // 按照初始化的逆序进行清理
    device_destroy(my_class, dev_num);      // 删除设备文件
    class_destroy(my_class);                // 删除设备类
    cdev_del(&my_cdev);                     // 从内核删除 cdev
    unregister_chrdev_region(dev_num, 1);   // 释放设备号

    printk(KERN_INFO "process_lister: Driver cleanup complete.\n");
}

module_init(my_driver_init);
module_exit(my_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SH ZHAO");