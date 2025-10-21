#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/module.h> 
#include <linux/init.h> 
#include <linux/ioctl.h>
#include "ioctl_headers.h"


int32_t value = 0;

MODULE_LICENSE("GPL");
dev_t dev = 0;
static struct class *dev_class;
static struct cdev tmp_cdev; 

static long tmp_ioctl(struct  file *file ,  unsigned int cmd ,unsigned long arg);
struct file_operations fops = { 
	.write = NULL,
	.read = NULL ,
	.open = NULL,
	.release = NULL,
	.owner = THIS_MODULE,
	.unlocked_ioctl = tmp_ioctl
};



/* Init function */
static int __init ioctl_driver(void)
{
	/* Allocating major number */
	if((alloc_chrdev_region(&dev ,0,1,"ioctl-drv")) <0) {

		pr_info("Cannot allocate major number\n");
		return -1;
	}
	/* creating cdev struture */
	cdev_init(&tmp_cdev , &fops) ;

	if ((cdev_add(&tmp_cdev, dev ,1)) < 0) {

		pr_err("Cannot create cdev structure \n"); 
		goto r_class;
	}

	if (IS_ERR(dev_class = class_create("ioctl_drv"))) {

		pr_err("class_create() error\n");
		goto r_class;
	}

	if (IS_ERR(device_create(dev_class, NULL, dev, NULL,"ioctl_drv"))) {

		pr_err("device_create() error \n");
		goto r_device;
	}

	pr_info("Device Driver Ready .\n"); 
	return 0; 

r_device:
	class_destroy(dev_class);
r_class:
	unregister_chrdev_region(dev,1);
	return -1;
}

/* Module exit function */ 
static void __exit ioctl_exit(void)
{
	device_destroy(dev_class,dev);
	class_destroy(dev_class);
	unregister_chrdev_region(dev,1);
	pr_info("Device driver removed .\n");
}

static long tmp_ioctl(struct  file *file ,  unsigned int cmd ,unsigned long arg)
{
	switch(cmd) {
		case IO_WRITE:
			if(copy_from_user(&value,(int32_t*) arg, sizeof(value)))
				pr_err("copy_from_user() error\n");
			pr_info(" Value :%d\n",value);
			break;
		case IO_READ:
			if(copy_to_user((int32_t*)arg, &value , sizeof(value)))
				pr_err("copy_from_user() error\n");
			printk( KERN_INFO "copy_to_user sucess \n"); 

			break;
		default:
			pr_info("Uknown command\n");
			break;
	}
	return 0;
}
module_init(ioctl_driver);
module_exit(ioctl_exit); 

