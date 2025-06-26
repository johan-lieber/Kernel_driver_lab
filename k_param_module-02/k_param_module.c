#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>


static int myvar = 0 ; 
module_param(myvar, int , 0644);
MODULE_PARM_DESC(myvar, "An integer passed to module\n");


static int  __init param_init(void)
{
	printk(KERN_INFO "Loaded module with myvar:%d\n",myvar);
	return 0; 
}

static  void __exit  param_exit(void)
{
	printk(KERN_INFO "Unloaded module\n");
}


MODULE_LICENSE("GPL");

module_init(param_init);
module_exit(param_exit);


