#include <linux/module.h> 
#include <linux/kernel.h> 
#include <linux/init.h> 

static int  __init  hello_init(void)
{
	printk(KERN_INFO "Hello from kernel\n"); 
	return 0; 
} 

static void  __exit hello_exit(void)
{
	pr_info(" Bye from kernel :( \n"); 
	return ; 
} 

MODULE_LICENSE("GPL"); 
module_init(hello_init); 
module_exit(hello_exit); 

