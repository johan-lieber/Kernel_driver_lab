#include <linux/slab.h>
#include <linux/random.h>
#include <linux/proc_fs.h> 
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/kthread.h> 
#include <linux/delay.h>
#include <linux/mutex.h> 
#include <linux/string.h>
#define BUFF_SIZE 1024 

DEFINE_SPINLOCK(spinlock_test);

/* global and static variables */ 

dev_t dev ;
static struct cdev mycdev ;
static struct class *myclass ;
static struct device *mydevice ; 
static struct task_struct *kthread;
static struct task_struct *kthread1; 
static struct kobject *kobject_ref;
static int kobj_value =  10 ; 
static int global_variable = 0 ; 
static char device_buffer[BUFF_SIZE] ; 

/******************************** DRIVER FUNCTION PROTOTYPE *********************** */ 

static int thread_function(void *data) ;
static int thread_function1(void *data) ; 
static int my_open(struct inode *inode , struct file *file ) ; 
static int my_release(struct inode *inode , struct file *file) ;
static ssize_t my_read ( struct file *filp , char __user *buf , size_t len , loff_t *offset) ;
static ssize_t my_write(struct file *filp , const char __user *buf , size_t len , loff_t *offset ) ;

/*******************************  SYS_FS FUNCTION PROTOTYPE ************************/ 

static ssize_t sysfs_show(struct kobject *kobj , struct kobj_attribute *attr , char *buff);
static ssize_t sysfs_store(struct kobject *kobj , struct kobj_attribute *attr ,const  char *buff, size_t count);



/* file attribute for sysfs */ 

struct kobj_attribute kobj_attr = __ATTR("test_file", 0660, sysfs_show , sysfs_store) ;


/* file operations for driver    */

static struct file_operations fops ={
.owner = THIS_MODULE,
.read = my_read ,
.write = my_write,
.open = my_open ,
.release = my_release 
};



/***************************** THREAD FUNCTIONS ***************************/ 

/* thread function one */
static int thread_function ( void *data ) 
{
	while (!kthread_should_stop())
	{
		if(!spin_is_locked(&spinlock_test)) 
		{
			pr_info(" Not Locked\n"); 
		} 
		spin_lock(&spinlock_test) ; 
		
		global_variable ++ ; 

		pr_info(" Value of GV :%d\n", global_variable); 

		if(spin_is_locked(&spinlock_test))
		{
			pr_info("Spin Locked \n"); 
		} 

		spin_unlock(&spinlock_test) ; 	

		msleep(2000); 
	} 
	return 0 ;
}

/* Thread function two */ 
static int thread_function1(void *data) 
{ 
	while(!kthread_should_stop())
	{
		spin_lock(&spinlock_test) ; 
	        	
		global_variable ++ ;
		pr_info(" Value of GV :%d \n", global_variable) ;
		spin_unlock(&spinlock_test);

		msleep(2000);
	} 
	return 0 ;
} 



/***************************** PROC_FS  FUNCTIONS ************************/

static ssize_t sysfs_show(struct kobject *kobj , struct kobj_attribute *attr , char *buff)
{
	return sprintf(buff , "%d",kobj_value) ; 
}

static ssize_t sysfs_store(struct kobject *kobj , struct kobj_attribute *attr ,const  char *buff, size_t count)
{
	sscanf(buff , "%d", &kobj_value);
	return count  ; 
}




/***************************** DRIVE FUNCTION ****************************/ 

/*  init function  */
static int __init hello_init(void)
{


	/* allocating major no */
	int ret ; 
	ret = alloc_chrdev_region(&dev , 0 , 1 , "mychardev");
	if(ret < 0) 
	{
		return ret ;
	}

	
	cdev_init(&mycdev , &fops);

	if(cdev_add(&mycdev , dev , 1 )<0)
	{
		goto r_class; 
	} 



	if(IS_ERR(myclass = class_create("mychardev")))
	{
		goto r_class ; 
	} 


	if(IS_ERR(mydevice  = device_create(myclass, NULL , dev , NULL , "mychardev")))
	{
		goto r_device ; 
	} 
	


	/* creating a dir in /sys/ */

	kobject_ref = kobject_create_and_add("test_file",kernel_kobj);

	/* creating a file in  /sys/interrupt_test/ */ 

	if(sysfs_create_file(kobject_ref,&kobj_attr.attr)) 
	{
		goto r_sysfs ; 
	}


	kthread = kthread_run(thread_function, NULL, "thread_function_one"); 
	if(!kthread)
	{
		goto r_thread ;
	}

	kthread1 = kthread_run(thread_function1 , NULL , "thread_function_two"); 
	if(!kthread1)
	{
		goto r_thread ; 
	} 




	
	printk(KERN_INFO " DRIVER - LOADED \n");
	return 0 ;
r_thread : 
	kthread_stop(kthread); 
	kthread_stop(kthread1) ;
r_sysfs :
      	kobject_put(kobject_ref);	   
	sysfs_remove_file(kobject_ref , &kobj_attr.attr); 


r_class :
	unregister_chrdev_region(dev,1);
	cdev_del(&mycdev); 
	return -1 ; 

r_device : 
	class_destroy(myclass);
	return -1 ; 

}





/* module exit funnction */ 
static void __exit hello_exit(void)
{

	kthread_stop(kthread);
	kthread_stop(kthread1);

	kobject_put(kobject_ref);
	sysfs_remove_file(kobject_ref , &kobj_attr.attr); 
	device_destroy(myclass , dev ) ;
	class_destroy(myclass);
	cdev_del(&mycdev);
	unregister_chrdev_region(dev, 1 ) ;

	
	printk(KERN_INFO "-- DRIVER UNLOADED -- \n");
	return ;
}



/* Writing function */ 
static ssize_t my_write(struct file *filp , const char __user *buf , size_t len , loff_t *offset ) 
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

       return to_copy;
              
}




/* Reading function  */
static ssize_t my_read(struct file *filp , char __user *buf , size_t len , loff_t *offset)  
{
	size_t   length =  strlen(device_buffer) ;
  
   	if(*offset >= length ) 
 	{
 	return 0 ;
 	}	

        device_buffer[length] = '\0' ; 

	if(copy_to_user( buf , device_buffer , length) !=0) 
	{
		return -EFAULT ;
	}
	*offset += length ; 



	return  length  ; 

}




/* Myopen function */
static  int my_open ( struct inode *inode , struct file *file) 
{
	printk(KERN_INFO " - OPEN FUNC - \n") ; 
	return 0 ; 
} 




/*  my_release function */ 
static int my_release(struct inode *inode , struct file *file) 
{
	printk(KERN_INFO " - RELEASE FUNC -  \n ");
	return 0 ;
} 




MODULE_LICENSE("GPL");
module_init(hello_init);
module_exit(hello_exit);

