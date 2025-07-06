#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>

#define  BUFF_SIZE 1024 
static  char device_buffer[BUFF_SIZE] ; 


/* global variables */
dev_t dev ; 
struct cdev mycdev ;
struct class *myclass ;
struct device *mydevice ; 
struct kmem_cache *mycache ;

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

/* custom struct */ 
struct  custom_struct {
	int age ;
	int  marks ; 
	int id ; 
}; 


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

	// cdev ..
	cdev_init(&mycdev , &fops);
	ret = cdev_add(&mycdev , dev , 1 ) ;
	if(ret < 0 ) 
	{
	 	return ret ;
	}

	myclass = class_create("mychardev");
	mydevice  = device_create(myclass, NULL , dev , NULL , "mychardev");

	/* using custom slab cache  allocations  */
	mycache = kmem_cache_create("mycache" , sizeof(struct custom_struct) , __alignof__(struct custom_struct) , SLAB_HWCACHE_ALIGN, NULL );


	
	printk(KERN_INFO " DRIVER - LOADED \n");
	return 0 ; 
}




/*` module exit funnction */  
static void __exit hello_exit(void)
{
	kmem_cache_destroy(mycache) ;
	device_destroy(myclass , dev ) ;
	class_destroy(myclass);
	cdev_del(&mycdev);
	unregister_chrdev_region(dev, 1 ) ;

	
	printk(KERN_INFO "bye - from - kernel \n");
	return ;
}

/***************************DRIVER FUNCTIONS******************************/


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

	/*  allocating memory from custom slab cache  */
	struct  custom_struct   *ptr = 	kmem_cache_alloc(mycache , GFP_KERNEL);
	
	if(ptr == NULL ) 
	{
		return -EFAULT ;
	}

	ptr->age = 23 ; 
	ptr->marks = 66 ;
	ptr->id =  63 ; 



	printk(KERN_INFO " Age : %d\n", ptr->age); 
	printk(KERN_INFO" marks : %d \n", ptr->marks) ;
	pr_info(" id : %d \n ",  ptr->id ) ; 

	kmem_cache_free(mycache , ptr) ; 

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


	printk(KERN_INFO " Reading  functins begins \n");
       	printk(KERN_INFO "  Here is : %s \n",device_buffer ) ;	

	printk(KERN_INFO " BYE FROM READ FUNCTION \n");
	return  length  ; 

}




// Myopen function
 int my_open ( struct inode *inode , struct file *file) 
{
	printk(KERN_INFO "  hello from open function\n") ; 
	return 0 ; 
} 




//  my_release function 
int my_release(struct inode *inode , struct file *file) 
{
	printk(KERN_INFO " HELLO from release function \n ");
	return 0 ; 
} 






MODULE_LICENSE("GPL");
module_init(hello_init);
module_exit(hello_exit);

