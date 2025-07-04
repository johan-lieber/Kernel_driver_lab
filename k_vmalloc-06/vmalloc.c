#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>

MODULE_LICENSE("GPL");



//  functions  inits 
ssize_t my_write(struct file *file , const char __user *buf , size_t len , loff_t *offset) ; 
ssize_t my_read (struct file *file , char __user *buf , size_t  len , loff_t *offset) ;




//  Global and static variables and structs
struct file_operations fops = { 
	.owner =  THIS_MODULE,
	.write = my_write , 
	.read = my_read ,
	.open = NULL,
	.release = NULL 
}; 


dev_t dev ; 
struct cdev my_cdev ; 
struct class *my_class ; 
struct device *my_device ;
static char *device_buffer = NULL ; 





// module init function				   
static  int __init  vmalloc_init(void)
{
       	int ret = alloc_chrdev_region(&dev, 0 , 1 , "myvmallocdev");
      
	if(ret < 0)
       	{
	       	return -EFAULT ;
       	}

	cdev_init(&my_cdev , &fops); 
	cdev_add(&my_cdev , dev , 1 ) ; 

	my_class = class_create("myvmallocdev");
	my_device = device_create(my_class, NULL , dev , NULL , "myvmallocdev");

	printk(KERN_INFO " --MODULE LOADED--- \n");
	
	return 0; 

}





// module exit function 
static void __exit vmalloc_exit(void)
{

	vfree(device_buffer) ;	

	device_destroy(my_class , dev ) ;
	class_destroy(my_class);
	cdev_del(&my_cdev);
	unregister_chrdev_region(dev, 1);
	 
	pr_info("--MODULE UNLOADED\n");
	return ;
}




// my write function 
ssize_t my_write(struct file *file , const char __user *buf , size_t len , loff_t *offset) 
{


	pr_info("write fun \n");

	if(in_interrupt()){
		pr_alert("  interrupt  alert");
	}

	dump_stack();

	device_buffer = vmalloc( len + 1);
	if(!device_buffer)
	{
		pr_err("failed alloc");
		return -ENOMEM;
	}


	if(copy_from_user(device_buffer , buf , len)!=0)
	{
		return -EFAULT ;
	}
	device_buffer[len +1] = '\0';

	pr_info(" size of user string %ld \n", len);


	pr_info(" msg : %s  and len :%ld \n", device_buffer , strlen(device_buffer));
	

	return len  ; 
}




// my_read function 
ssize_t my_read(struct file *file , char __user *buf , size_t  len , loff_t *offset)
{
 	size_t 	length  = strlen(device_buffer); 


	if(*offset >= length)
	{
		return 0 ; 
	}


	if(copy_to_user(buf ,  device_buffer ,length)!=0)
	{
		return -EFAULT ;
	}

	

	*offset += length  ; 

	
	
	return len ; 

}




module_init(vmalloc_init);
module_exit(vmalloc_exit);



