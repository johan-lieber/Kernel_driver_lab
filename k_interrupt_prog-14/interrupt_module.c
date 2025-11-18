#include <linux/random.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/kthread.h> 
#include <linux/delay.h>
#include <linux/interrupt.h> 

#define IRQ_NO  1 



dev_t dev ;
static int irq_dev_id ; 
static struct cdev mycdev ;
static struct class *myclass ;
static struct device *mydevice ; 

static irqreturn_t irq_handler ( int  irq   , void *dev_id) 
{
	if(irq == 0x80) 
	pr_info("it is 80h\n");
	return  IRQ_HANDLED ;
} 

static struct file_operations fops ={
.owner = THIS_MODULE,
.read = NULL ,
.write =NULL,
.open = NULL ,
.release =NULL  
};

static int __init hello_init(void)
{
	int ret ; 
	
	ret = alloc_chrdev_region(&dev , 0 , 1 , "mychardev");

	if(ret < 0) 
		return ret ;

	cdev_init(&mycdev , &fops);

	if(cdev_add(&mycdev , dev , 1 )<0)
		goto r_class; 

	if(IS_ERR(myclass = class_create("mychardev")))
		goto r_class ; 

	if(IS_ERR(mydevice  = device_create(myclass, NULL , dev , NULL , "mychardev")))
		goto r_device ; 

	if(request_irq(IRQ_NO , irq_handler , IRQF_SHARED , "interrupt_test" , &irq_dev_id)) 
	{
		pr_info("request_irq() failed\n"); 
		goto r_irq ; 
	} 

	pr_info("Driver loaded \n");	
	return 0 ;
r_irq:
	free_irq(IRQ_NO  , &irq_dev_id); 
r_class :
	unregister_chrdev_region(dev,1);
	cdev_del(&mycdev); 
	return -1 ; 
r_device : 
	class_destroy(myclass);
	return -1 ; 
}

static void __exit hello_exit(void)
{
	free_irq(IRQ_NO  , &irq_dev_id); 
	device_destroy(myclass , dev ) ;
	class_destroy(myclass);
	cdev_del(&mycdev);
	unregister_chrdev_region(dev, 1 ) ;

	pr_info(" Module unloaded\n");
	return ;
}

MODULE_LICENSE("GPL");
module_init(hello_init);
module_exit(hello_exit);

