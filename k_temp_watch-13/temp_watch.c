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
DECLARE_WAIT_QUEUE_HEAD(wq);



// Varivale intitializations 

dev_t dev ;
static int threshold_value = 75  ; 
static int temperature = 0; 
uint32_t wait_flag = 0 ; 
static struct cdev mycdev ;
static struct class *myclass ;
static struct device *mydevice ; 
static  char device_buffer[BUFF_SIZE] ; 
static struct task_struct *kthread;
//static struct task_struct *kthread1; 
//static struct task_struct *kthread2; 
static char device_buffer[BUFF_SIZE];
static struct proc_dir_entry *parent ; 
static struct mutex   mutex_ref;


/******************************** DRIVER FUNCTION PROTOTYPE *********************** */ 

static int thread_function(void *data) ; 

static int my_open(struct inode *inode , struct file *file ) ; 
static int my_release(struct inode *inode , struct file *file) ;
static ssize_t my_read ( struct file *filp , char __user *buf , size_t len , loff_t *offset) ;
static ssize_t my_write(struct file *filp , const char __user *buf , size_t len , loff_t *offset ) ;

/*******************************  PROC_FS FUNCTION PROTOTYPE ************************/ 


static int proc_open(struct inode *inode , struct file *filp ) ; 
static int proc_release(struct inode *inode , struct file *filp) ;

static ssize_t wait_read(struct file *filp , char __user *buf , size_t len , loff_t *offset) ;
static ssize_t wait_write(struct file *filp , const char __user *buf , size_t len , loff_t *offset ) ;

static ssize_t threshold_read ( struct file *filp , char __user *buf , size_t len , loff_t *offset) ;
static ssize_t threshold_write(struct file *filp , const char __user *buf , size_t len , loff_t *offset ) ;


static ssize_t value_read ( struct file *filp , char __user *buf , size_t len , loff_t *offset) ;
static ssize_t value_write(struct file *filp , const char __user *buf , size_t len , loff_t *offset ) ;




/* file operations for driver    */

static struct file_operations fops ={
.owner = THIS_MODULE,
.read = my_read ,
.write = my_write,
.open = my_open ,
.release = my_release 
};


/*   file operation for wait procfs   */ 
 
static const  struct  proc_ops wait_fops  = {
	.proc_write = wait_write ,
	.proc_read = wait_read ,
       .proc_release = proc_release , 
	.proc_open = proc_open
}; 


/* file operation for threshold procfs    */ 
static const struct  proc_ops  threshold_fops = {
	.proc_write =  threshold_write ,
	.proc_read  =  threshold_read ,  
}; 


/* file operation for value  procfs   */ 
static  const struct   proc_ops  value_fops = {
	.proc_write =  value_write ,
	.proc_read  =  value_read ,  
}; 





/***************************** THREAD FUNCTIONS ***************************/ 

/* thread function one */
static int thread_function ( void *data ) 
{
	while (!kthread_should_stop())
	{
		pr_info(" --WAITING FOR EVENT -- \n");
		temperature = get_random_u32() % 100;
	       if(temperature >  threshold_value   ) 
	       {
		        pr_info(" calling \n");
			wait_flag = 2 ;
			wake_up_interruptible(&wq); 
	       }

		pr_info(" TEMP :%d && THRESHOLD :%d \n", temperature, threshold_value);
		msleep(2000); 
	} 
	return 0 ;
} 
/***************************** PROC_FS  FUNCTIONS ************************/

static int proc_open( struct  inode *inode , struct file *flip)
{
	pr_info(" PROC_FS OPEN  LOADED ") ;
	return 0 ; 
} 




static int proc_release(struct inode *inode , struct file *filp)
{
	pr_info(" PROC_FS  RELEASE LOADED \n") ;
	return 0 ;
}



/***************************************** THRESHOLD PROCFS FUNCTIONS **********************************/ 

static ssize_t  threshold_write(struct file *filp, const char __user *buf , size_t len , loff_t *offset) 
{

	char kbuf[32] ; 


	if(len  >= sizeof(kbuf)) 
	{
		return -EINVAL ;
	}


	if(copy_from_user(kbuf  , buf, len)!=0)
	{
		return -EFAULT ;
	}


	kbuf[len] = '\0'; 



	int value ; 

	if(kstrtoint(kbuf , 10 , &value) <0) 
	{
		return -EINVAL ;
	} 

	pr_info(" THRESHOLD SET TO:%d \n", value );

	threshold_value = value ; 

	return len  ;
} 



static ssize_t threshold_read(struct file *filp , char __user *buf , size_t len , loff_t *offset ) 
{
	return -EPERM ;

}




/********************************** WAIT PROCFS  FUNCTIONS ****************************************/ 

static ssize_t  wait_read(struct file *filp , char __user *buf , size_t len , loff_t *offset ) 
{
	char kbuf[16] ; 
	int temp_buffer =  44 ;  


	int  length  = snprintf(kbuf , sizeof(kbuf) , " %d\n", temp_buffer); 




	if(*offset >= length) 
	{
		return 0 ;
	}
	 
	
	wait_event_interruptible(wq ,wait_flag  != 0 );

	if(wait_flag == 2  ) 
	{

		if(copy_to_user(buf , kbuf ,  length )!=0)
		{
			return -EFAULT ;
		}
	
	} 

	wait_flag = 0 ; 

	*offset +=  length  ; 

	return  length  ; 
} 


static ssize_t  wait_write(struct file *filp , const  char __user *buf , size_t len , loff_t *offset ) 
{
	return -EPERM ;
}



/*****************************   PROCFS VALUE FUNCTION ********************/ 

static ssize_t  value_read(struct file *filp , char __user *buf , size_t len , loff_t *offset ) 
{
	char kbuf[16] ; 
	

	int temp_buffer =  temperature;
      		


	int  length  = snprintf(kbuf , sizeof(kbuf) , " %d\n", temp_buffer); 




	if(*offset >= length) 
	{
		return 0 ;
	}
	 
	


	pr_info(" wait flag  has been 2 :%d \n", temperature ); 
	
	if(copy_to_user(buf , kbuf ,  length )!=0)
	{
		return -EFAULT ;
	}
	*offset +=  length  ; 

	return  length  ; 
} 


static ssize_t  value_write(struct file *filp , const  char __user *buf , size_t len , loff_t *offset ) 
{
	return -EPERM ;
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
	
	/*mutex initialization */
	mutex_init(&mutex_ref) ; 

	/* proc_fs  dir  */
	parent = proc_mkdir("tempwatch", NULL ) ;
	if(parent == NULL )
	{
		pr_err(" PROC_CREAT_ERR\n");

		return 0 ;
	} 

	/* proc_fs file */ 
	proc_create("wait" , 0444 , parent , &wait_fops);
	proc_create("value" , 0444 , parent , &value_fops ); 
	proc_create("threshold", 0222, parent , &threshold_fops) ;



	kthread = kthread_run(thread_function, NULL, "thread_function_one"); 
	if(!kthread)
	{
		goto r_class ;
	} 



	
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

	kthread_stop(kthread);
	remove_proc_entry("wait" , parent);
	remove_proc_entry("value", parent);
	remove_proc_entry("threshold",parent) ;
	proc_remove(parent) ;

	device_destroy(myclass , dev ) ;
	class_destroy(myclass);
	cdev_del(&mycdev);
	unregister_chrdev_region(dev, 1 ) ;

	
	printk(KERN_INFO "bye - from - kernel \n");
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


	wait_flag = 1 ; 
	wake_up_interruptible(&wq); 

	return  length  ; 

}




/* Myopen function */
static  int my_open ( struct inode *inode , struct file *file) 
{
	printk(KERN_INFO "  hello from open function\n") ; 
	return 0 ; 
} 




/*  my_release function */ 
static int my_release(struct inode *inode , struct file *file) 
{
	printk(KERN_INFO " Hullu from release function \n ");
	return 0 ; 
} 




MODULE_LICENSE("GPL");
module_init(hello_init);
module_exit(hello_exit);

