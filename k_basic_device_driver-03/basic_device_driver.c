#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>



struct file_operations fops ={
.owner = THIS_MODULE,
.read = NULL ,
.write = my_write,
.open = NULL ,
.release = NULL 
};


dev_t dev ; 
struct cdev mycdev ;
struct class *myclass ;
struct device *mydevice ; 


#define char  BUFF_SIZE  =  1024 

static  char device_buffer[BUFF_SIZE] ; 


static int __init hello_init(void)
{


	// allocating mojor no ; 
	int ret ; 
	ret = alloc_chrdev_region(&dev , 0 , 1 , "mychardev");
	if(ret < 0) 
	{
		return ret ;
	}

	// cdev ..
	cdev_init(&mycdev , &fops);
	ret = cdev_add(&mycdev , dev , 1 ) ;
	if(ret < 0 ) 
	{
	 	return ret ;
	}

	myclass = class_create("mychardev");
	mydevice  = device_create(myclass, NULL , dev , NULL , "mychardev");

	
	printk(KERN_INFO " DRIVER - LOADED \n");
	return 0 ; 
}



ssize_t my_write(struct file *filp , const char __user *buf , size_t len , loff_t *offset ) 
{
	size_t to_copy ; 

	if(len > BUFF_SIZE) 
	{
		to_copy = BUFF_SIZE ; 
	}else{
		to_copy = len ; 
	}

	if(copy_from_user(device_buffer , buf , to_copy ) !=0)
	{
		return -EFAULT ; 
	}

	printk(KERN_INFO "  rev %zu from user \n",to_copy) ;

	size_t safe_len = (to_copy < BUFF_SIZE -1 ) ? to_copy : BUFF_SIZE -1 ; 

	device_buffer[safe_len] = '\0' ; 

	printk(KERN_INFO " MESSAGE : %s \n " , device_buffer) ; 
}


static void __exit hello_exit(void)
{
	device_destroy(myclass , dev ) ;
	class_destroy(myclass);
	cdev_del(&mycdev);
	unregister_chrdev_region(dev, 1 ) ;

	
	printk(KERN_INFO "bye - from - kernel \n");
	return ;
}

MODULE_LICENSE("GPL");
module_init(hello_init);
module_exit(hello_exit);

