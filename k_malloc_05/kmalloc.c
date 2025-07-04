#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/string.h>



MODULE_LICENSE("GPL");




//functions ...
ssize_t my_write (struct file *file , const char __user *buf , size_t len , loff_t  *offset) ;





struct file_operations fops = {
	.owner = THIS_MODULE, 
	.write = my_write,
	.read =  NULL,
	.open  = NULL, 
	.release = NULL
};



//  global and  static variables .. 

dev_t dev ;
struct cdev my_cdev;
struct  class *my_class ;
struct device *my_device ; 





// Init module  function ... 
static int __init kmalloc_init(void)
{
	int ret = alloc_chrdev_region(&dev, 0 , 1 , "kchardev");
	if(ret < 0 ) 
	{
		return -EFAULT ;
	}

	cdev_init(&my_cdev , &fops); 
	cdev_add(&my_cdev , dev , 1 ) ;


	my_class = class_create("kchardev");
	my_device = device_create(my_class , NULL ,  dev , NULL , "kchardev");


	pr_info(" MODULE_LOADED\n");
	return 0;
}





// Exit module function 
static void __exit kmalloc_exit(void)
{
	device_destroy(my_class , dev) ;
	class_destroy(my_class) ;
	cdev_del(&my_cdev); 
	unregister_chrdev_region(dev , 1 ) ; 
	pr_info(" --MODULE_UNLOADED--\n");
	return ;
}




// My_write function .. 
ssize_t my_write(struct file *file , const char __user *buf , size_t len , loff_t *offset) 
{
	pr_info(" --READING BEINGS ---\n");
	
	char *mem = kmalloc( (len+1) , GFP_KERNEL) ;
	if(mem == NULL) 
	{
		return -EFAULT; 
	}


	if(copy_from_user(mem , buf , len+1)!=0)
	{
		return -EFAULT ;
	}

	mem[len+1] = '\0';

	pr_info(" Msg :%s  & len %ld \n",mem , strlen(mem)) ;
	
	kfree(mem);
	return len ; 


	
}




module_init(kmalloc_init);
module_exit(kmalloc_exit); 

