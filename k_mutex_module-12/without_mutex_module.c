#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/kthread.h>

#define  BUFF_SIZE 1024 
static  char device_buffer[BUFF_SIZE] ; 


/* global variables */
dev_t dev ;
uint32_t read_count = 0 ; 
int wait_queue_flag = 0 ; 

struct cdev mycdev ;
struct class *myclass ;
struct device *mydevice ; 

//struct mutex  examp_mutex ; 

static struct task_struct *mutex_thread; 
static struct task_struct *mutex_thread1; 


/* function initialization */
int kthread_function (  void *data) ;
int kthread_function1 ( void *data ) ; 

int my_open(struct inode *inode , struct file *file ) ; 
int my_release(struct inode *inode , struct file *file) ;
ssize_t my_read ( struct file *filp , char __user *buf , size_t len , loff_t *offset) ;
ssize_t my_write(struct file *filp , const char __user *buf , size_t len , loff_t *offset ) ;


/* file operations struct  */
struct file_operations fops ={
.owner = THIS_MODULE,
.read = my_read ,
.write = my_write,
.open = my_open ,
.release = my_release 
};



/**************************** KTHREAD  FUNCTION  *************************/ 

int  kthread_function (  void *data) 
{ 

	while(!kthread_should_stop()) 

	{
	//	mutex_lock(&examp_mutex) ;
		read_count ++ ; 
		pr_info("  FIST_THREAD --> read_count:%u \n", read_count) ;
	//	mutex_unlock(&examp_mutex); 

		msleep(1); 
	
	}
	return 0 ; 
}


int kthread_function1 (void *data ) 
{
	while (!kthread_should_stop()) 
	{
	 //	mutex_lock(&examp_mutex); 
		read_count ++ ; 
		pr_info(" SECOND_THRAD --> read_count:%u \n",read_count) ; 
	 //	mutex_unlock(&examp_mutex); 
	  	msleep(1); 
	} 
	return 0 ; 
} 


/***************************DRIVER FUNCTIONS******************************/


/* INIT  functions */
static int __init hello_init(void) 
{


	// allocating mojor no ; 
	int ret ; 
	ret = alloc_chrdev_region(&dev , 0 , 1 , "mychardev");
	if(ret < 0) 
	{
		return ret ;
	}

	/* creating cdev struct */
	cdev_init(&mycdev , &fops);
	if(cdev_add(&mycdev , dev , 1 ) < 0 ) 
	{
		goto r_class ; 
	} 



	if(IS_ERR(myclass = class_create("mychardev")))
	{
		goto r_class ; 
	} 


	if(IS_ERR(device_create(myclass, NULL , dev , NULL , "mychardev")))
	{
		goto  r_device ; 
	} 

	/* init  mutex */ 
//	mutex_init(&examp_mutex) ; 



	/* creating thread   */

	mutex_thread1 = kthread_run(kthread_function1, NULL , "mutex_thread1"); 
	if(!mutex_thread1) 
	{
		pr_err(" THREAD1_CREAT_ERR\n") ; 
		return -1 ; 
	} 


	mutex_thread = kthread_run(kthread_function, NULL , "mutex_thread") ; 
	if(!mutex_thread) 
	{
		pr_err(" THREAD_CREAT_ERR\n");
		return -1 ; 
	} 


	
	printk(KERN_INFO " DRIVER - LOADED \n");
	return 0 ; 


r_class  : 
	unregister_chrdev_region(dev, 1 );
	cdev_del(&mycdev) ; 
	return -1 ; 


r_device : 
	class_destroy(myclass) ;
	return -1 ;
}




/*` module exit funnction */  
static void __exit hello_exit(void)
{
	kthread_stop(mutex_thread);
	kthread_stop(mutex_thread1);
	device_destroy(myclass , dev ) ;
	class_destroy(myclass);
	cdev_del(&mycdev);
	unregister_chrdev_region(dev, 1 ) ;

	
	printk(KERN_INFO " --MODULE_UNLOADED -- with value of read-count :%u \n", read_count);
	return ;
}



/* Writing function */ 
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

       return to_copy;
              
}




/* Reading function */
ssize_t my_read(struct file *filp , char __user *buf , size_t len , loff_t *offset)  
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



	printk(KERN_INFO " BYE FROM READ FUNCTION \n");
	return  length  ; 

}




// Myopen function
 int my_open ( struct inode *inode , struct file *file) 
{
	printk(KERN_INFO " OPEN FUNCTION \n") ; 
	return 0 ; 
} 




//  my_release function 
int my_release(struct inode *inode , struct file *file) 
{
	printk(KERN_INFO " RELEASE FUNCTION  \n ");
	return 0 ; 
} 






MODULE_LICENSE("GPL");
module_init(hello_init);
module_exit(hello_exit);

