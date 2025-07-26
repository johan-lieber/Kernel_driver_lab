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
struct cdev mycdev ;
struct class *myclass ;
struct device *mydevice ; 
int shared_variable = 0 ; 

/* function initialization */ 
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



/*********************  EXTERN  FUNCTION************************/ 
void extern_shared_function(void); 

void   extern_shared_function ( void ) 
{ 
	pr_info(" -- FROM  SHARED FUNCTION \n"); 
	shared_variable ++ ; 
} 


EXPORT_SYMBOL(extern_shared_function); 
EXPORT_SYMBOL(shared_variable) ;

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
	device_destroy(myclass , dev ) ;
	class_destroy(myclass);
	cdev_del(&mycdev);
	unregister_chrdev_region(dev, 1 ) ;

	
	printk(KERN_INFO "bye - from - kernel \n");
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

