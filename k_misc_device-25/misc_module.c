#include <linux/miscdevice.h> 
#include <linux/fs.h>
#include <linux/kernel.h> 
#include <linux/module.h> 
#include <linux/init.h> 




static struct   file_operations fops = { 
	.owner = THIS_MODULE, 
	.write = NULL , 
	.read = NULL, 
	.open = NULL, 
	.release = NULL , 
	.llseek = NULL 
}; 


struct miscdevice my_miscdevice = 
{
	.minor = MISC_DYNAMIC_MINOR, 
	.name = "simple_misc_device", 
	.fops =&fops , 
}; 


static int __init misc_init(void) 
{ 

	if(misc_register(&my_miscdevice) < 0 ) 
	{ 
		pr_info("MISC_REG_ERR\n"); 
		goto r_miscdevice ; 
	} 


	pr_info("--MISC DEVICE LOADED -\n"); 

	return 0 ; 

r_miscdevice: 
	misc_deregister(&my_miscdevice) ; 
	return 0 ; 
} 


static void  __exit  misc_exit( void ) 

{ 

	misc_deregister(&my_miscdevice); 
	pr_info("- MISC DEVICE UNLOADED - \n"); 
	 return ; 
} 




module_init(misc_init); 
module_exit(misc_exit); 
MODULE_LICENSE("GPL") ; 

