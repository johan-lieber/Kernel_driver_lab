#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include "ioctl_header.h"

#define BUFF_SIZE  1024 



MODULE_LICENSE("GPL");

// Function .. 
ssize_t  my_write(struct file *file , const char __user *buf , size_t len , loff_t *offset);
ssize_t  my_read (struct file *file , char __user *buf , size_t len , loff_t *offset );
static long my_ioctl(struct file *file , unsigned int  cmd , unsigned long  arg ) ;



// Structures ..

struct file_operations fops  = 

{
	.owner = THIS_MODULE,
	.write =my_write , 
	.read = my_read,
	.open = NULL , 
	.release = NULL , 
	.unlocked_ioctl = my_ioctl
};


// Variables ..
dev_t dev ; 
struct cdev   my_cdev; 
struct class *my_class ;
struct device *my_device ;
static  char device_buffer[BUFF_SIZE] ;
struct my_data data ; 



// Init  module ...
static int __init ioctl_init(void)
{

	int ret  ; 
	ret = alloc_chrdev_region(&dev , 0 , 1 , "mychadev") ;
	if(ret < 0) 
	{
		return ret ;
	}

	cdev_init(&my_cdev , &fops) ;
	cdev_add(&my_cdev , dev , 1 ) ;

	my_class = class_create("mychardev");
	my_device  = device_create(my_class , NULL , dev,  NULL , "mychardev");


	printk(KERN_INFO "MODULE-LOADED\n");
	return 0  ; 
}


// Exit module ...
static void __exit ioctl_exit(void)
{
	device_destroy(my_class, dev) ;
	class_destroy(my_class);
	cdev_del(&my_cdev);
	unregister_chrdev_region(dev, 1 ) ;
	printk(KERN_INFO " MODULE-UNLOADED\n");
	return ;
}




//  My_write function .. 

ssize_t my_write(struct file *file , const char __user *buf , size_t len , loff_t *offset)
{

	size_t to_copy ; 
	if(len >=BUFF_SIZE) 
	{
		to_copy = BUFF_SIZE ;
	}else{
		to_copy = len ;
	}
		
	if(copy_from_user(device_buffer ,  buf  , to_copy)!=0)
	{
		return -EFAULT;
	}

	size_t safe_len = ( to_copy > BUFF_SIZE -1) ? to_copy : len ;
	device_buffer[safe_len] = '\0';
	printk(KERN_INFO " WROTE : %s  size : %ld \n",device_buffer , strlen(device_buffer));


	return to_copy; 
}



// My_read function .. 

ssize_t my_read(struct file *file , char __user *buf ,  size_t len , loff_t *offset)
{
	size_t to_copy = strlen(device_buffer); 
	if(*offset >= to_copy)
		{
			printk(KERN_INFO " RETURN \n");
			return 0 ; 
		}

	if(copy_to_user(buf , device_buffer , to_copy)!=0)
	{
		return -EFAULT;
	}

	*offset += to_copy ; 
	return to_copy;
}




// My_ioctl function ... 
static long my_ioctl (struct file *file , unsigned int cmd ,   unsigned long arg ) 
{
	pr_info(" IOCTL_BEGINS \n");
static 	int my_value ;
	switch(cmd) 
	{
		case IOCTL_CMD_RESET :
			pr_info(" CMD : RESET \n") ;
			break;
		case IOCTL_CMD_SET_VAL :
			pr_info("CMD : SET VALUE \n");
			if(copy_from_user(&my_value , (int __user *)arg, sizeof(int))!=0)
			{
				return -EFAULT ;
			}
			pr_info(" VALUE SET : %d \n", my_value); 
			break;
		case IOCTL_CMD_GET_VAL : 
			pr_info("CMD : GET VALUE \n");
			if(copy_to_user((int __user *)arg , &my_value , sizeof(int))!=0)
			{
				return -EFAULT ;
			}
			pr_info(" VALUE gET : %d \n",my_value);
			break ;
		case IOCTL_CMD_CALC : 

			pr_info("my_data \n");

			if(copy_from_user(&data , ( struct my_data __user *)arg , sizeof(data))!=0)
			{
				return -EFAULT ;
			}

			pr_info(" int a : %d , int b :%d \n", data.a , data.b); 

			data.result = data.a + data.b ; 

			if(copy_to_user((struct my_data __user *)arg , &data , sizeof(data))!=0)
			{
				return -EFAULT ;
			}
			pr_info("result : %d \n", data.result);
			break; 


		default :
		
			pr_info(" NO CASE \n");
			return -EINVAL ; 


	}
	pr_info(" IOCTL ENDS \n");
	 return my_value ; 
}

module_init(ioctl_init);
module_exit(ioctl_exit);


