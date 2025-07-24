#include <linux/signal.h>
#include <linux/interrupt.h>
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
#include <linux/ioctl.h>
#include <linux/jiffies.h> 
#include <linux/timer.h> 


#define BUFF_SIZE 1024 
#define  TIMEOUT  5000 




/* Variables */ 
dev_t dev ;
static struct cdev mycdev ;
static struct class *myclass ;
static struct device *mydevice ; 
static char device_buffer[BUFF_SIZE] ; 
static struct  timer_list   my_timer ; 
static   int timer_count = 0 ;
static  long target_jiffies  = jiffies + msecs_to_jiffies(10) ; 


/*********************************  TIMER  FUNCTION *********************************/ 

void timer_handler (  struct timer_list *data ) ;


void  timer_handler (  struct timer_list *data  ) 
{ 

	pr_info(" INSIDE TIMER %d \n",timer_count++);


        unsigned long  fired = jiffies ; 
	long delta_jiffies  = fired - target_jiffies ; 
	pr_info("timer_list  delta :%ld jiffies ( ~%ld ms) \n" , delta_jiffies , jiffies_to_msecs(delta_jiffies));  

	mod_timer(&my_timer ,   jiffies + msecs_to_jiffies(TIMEOUT)); 


	return ; 
} 


/******************************** DRIVER FUNCTION PROTOTYPE *********************** */ 

static int my_open(struct inode *inode , struct file *file ) ; 
static int my_release(struct inode *inode , struct file *file) ;
static ssize_t my_read ( struct file *filp , char __user *buf , size_t len , loff_t *offset) ;
static ssize_t my_write(struct file *filp , const char __user *buf , size_t len , loff_t *offset ) ;



/* File operations for driver    */
static struct file_operations fops ={
.owner = THIS_MODULE,
.read = my_read ,
.write = my_write,
.open = my_open ,
.release = my_release 
};





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
	


	timer_setup(&my_timer ,timer_handler ,0 ) ;

	mod_timer(&my_timer , jiffies + msecs_to_jiffies(TIMEOUT)); 

	printk(KERN_INFO " DRIVER - LOADED \n");
	return 0 ;

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


	del_timer(&my_timer) ;
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

	pr_info(" -READ FN --\n") ; 
	
	if(length < 0 ) 
	{ 
	 	
		char *backupdata  = " NO DATA \n" ; 
		
		length = strlen(backupdata) ; 
		if(copy_to_user(buf, backupdata , length)!=0) 
		{ 
			return -EFAULT ; 
		} 
		*offset += length ;
	       return length ; 	
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

