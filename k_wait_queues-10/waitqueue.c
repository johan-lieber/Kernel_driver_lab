#include <linux/wait.h>
#include <linux/init.h> 
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/device.h>
/*
 *  Waitqueue practice 
 *  program 
 */

dev_t dev;
struct cdev tmp_cdev;
struct class *tmp_class;
struct device *tmp_device;

struct file_operations fops = {
	.owner = THIS_MODULE,
	.open  = NULL,
	.release = NULL, 
	.write = NULL,
	.read = NULL,
};

static int __init waitqueue_init(void)
{
	int ret;
	ret = alloc_chrdev_region(&dev ,0,1,"waitqueue_drv");
	if (ret < 0)
		return ret;
	
	cdev_init(&tmp_cdev , &fops);
	if (cdev_add(&tmp_cdev , dev , 1) < 0) 
		goto r_class;

	if (IS_ERR(tmp_class = class_create("waitqueue_drv")))
		goto r_class;

	if (IS_ERR(device_create(tmp_class,NULL,dev,NULL,"waitqueue_drv")))
		goto  r_device;

	pr_info("Driver ready ...\n");
	return 0;

r_class  : 
	unregister_chrdev_region(dev,1);
	cdev_del(&tmp_cdev);
	return -1;


r_device :
	class_destroy(tmp_class);
	return 0;
}

static void __exit waitqueue_exit(void)
{ 
	device_destroy(tmp_class ,dev);
	class_destroy(tmp_class);
	cdev_del(&tmp_cdev);
	unregister_chrdev_region(dev,1);
	pr_info("Unregistering driver..\n");
}

module_init(waitqueue_init);
module_exit(waitqueue_exit);
MODULE_LICENSE("GPL");
