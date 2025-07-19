#include <linux/workqueue.h>
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
#define BUFF_SIZE 1024 
#define IRQ_NO 1 


/* Variables */ 
dev_t dev ;
static  int irq_dev_id ; 
static struct cdev mycdev ;
static struct class *myclass ;
static struct device *mydevice ; 
static struct task_struct *kthread;
static struct kobject *kobject_ref;
static int kobj_value =  10 ; 
static int global_variable = 0 ; 
static char device_buffer[BUFF_SIZE] ; 

/******************************** DRIVER FUNCTION PROTOTYPE *********************** */ 

static int thread_function(void *data) ;
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


/*******************************  SYS_FS FUNCTION PROTOTYPE ************************/ 

static ssize_t sysfs_show(struct kobject *kobj , struct kobj_attribute *attr , char *buff);
static ssize_t sysfs_store(struct kobject *kobj , struct kobj_attribute *attr ,const  char *buff, size_t count);

/* File attribute for sysfs */ 
struct kobj_attribute kobj_attr = __ATTR("test_file", 0660, sysfs_show , sysfs_store) ;


/******************************** INTERRUPT - HANDLER  FUNCTIONS ******************************/ 

void  workqueue_fn(struct work_struct *work) ; 
static  irqreturn_t irq_handler( int irq , void *dev_id ); 

DECLARE_WORK(workqueue,  workqueue_fn) ; 


/* Workqueue fn */ 
void workqueue_fn (struct work_struct *work) 
{
	pr_info(" - WORKQUEU FN -- \n"); 

	msleep(2000);
	return ; 

} 

/* Interrupt Handler */ 
static  irqreturn_t irq_handler(int irq , void *dev_id ) 
{
	pr_info(" Interrupt Occured !\n");
	schedule_work(&workqueue) ;
	return  IRQ_HANDLED ; 
} 




/***************************** THREAD FUNCTIONS ***************************/ 

/* thread function one */
static int thread_function ( void *data ) 
{
	while (!kthread_should_stop())
	{
	
		global_variable ++ ;

		msleep(2000); 

		pr_info(" Value of GV :%d\n", global_variable); 

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


	if(request_irq(IRQ_NO ,irq_handler , IRQF_SHARED , "mychardev",&irq_dev_id))
	{
		pr_info(" IRQ REG ERR \n"); 
		goto r_irq ; 
	} 



	
	printk(KERN_INFO " DRIVER - LOADED \n");
	return 0 ;

r_irq: 
	free_irq(IRQ_NO , &irq_dev_id) ; 


r_thread : 
	kthread_stop(kthread); 
r_sysfs :
	sysfs_remove_file(kobject_ref , &kobj_attr.attr); 
      	kobject_put(kobject_ref);	   


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

	free_irq(IRQ_NO ,&irq_dev_id) ;

	kthread_stop(kthread);

	sysfs_remove_file(kobject_ref , &kobj_attr.attr); 
	kobject_put(kobject_ref);
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

