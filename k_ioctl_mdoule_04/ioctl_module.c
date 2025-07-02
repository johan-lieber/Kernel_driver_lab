#include "ioctl_header.h"
#include <linux/ioctl.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

MODULE_LICENSE("GPL");


// Funtions ..
long my_ioctl (struct file *file , unsigned int cmd , unsigned long arg ) ; 



//  fops struct ..
struct  file_operations  fops = 
{
	.owner = THIS_MODULE,
 	.read = NULL , 
	.write = NULL ,
	.open = NULL ,
	.release = NULL,
	.unlocked_ioctl = my_ioctl 
};


//variables ..
dev_t dev ;
struct cdev my_cdev ;
struct class *my_class;
struct device *my_device;
static  int my_value = 0 ; 




// Init_module ... 
static int __init  init_ioctl(void) 
{
	int ret  = alloc_chrdev_region(&dev , 0 , 1 , "mychardevsecond" ) ;
 	if(ret < 0)
	{
		return ret;
	}	

	cdev_init(&my_cdev , &fops) ;
	cdev_add(&my_cdev , dev , 1 ) ;


	my_class = class_create("mychardevsecond");
	my_device = device_create(my_class , NULL , dev , NULL , "mychardevsecond"); 

	printk(KERN_INFO  "MODULE_LOADED\n");
	 return 0;
}




// exit_nodule ...
static void __exit exit_ioctl(void)
{	
	device_destroy(my_class, dev );
	class_destroy(my_class);
	cdev_del(&my_cdev) ;
	unregister_chrdev_region(dev, 1 ) ;

	printk(KERN_INFO "MODULE_UNLOADED\n");
	return ;
}




// ioctl function ...
long  my_ioctl (struct file *file  , unsigned int cmd  , unsigned  long arg ) 
{
	printk(KERN_INFO " IOCTL_STARTS\n");
	switch(cmd) 
	{
	case IOCTL_CMD_RESET :
	my_value = 0 ; 
       	printk(KERN_INFO " RESET CMD RECEIVED \n") ; 
 	break;
	

	case  IOCTL_CMD_SET_VAL :
	if(copy_from_user(&my_value , ( int __user *)arg , sizeof(int)!=0)){
		return -EFAULT ;
	}
	printk(KERN_INFO " SET VALUE : %d\n", my_value ) ;
	break ; 

	case IOCTL_CMD_GET_VAL : 
	if(copy_to_user((int __user *)arg, &my_value , sizeof(int))!=0)
	{
		return -EFAULT ;
	}
	printk(KERN_INFO " GOT VALUE : %d\n",my_value) ;
	 break ; 

	default : 
	 return  -ENOTTY  ;

	}
	
	printk(KERN_INFO "IOCTL_ENDS\n");
	return 0 ; 
}



module_init(init_ioctl) ;
module_exit(exit_ioctl) ; 

