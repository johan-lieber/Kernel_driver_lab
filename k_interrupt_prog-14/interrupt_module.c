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
#include <linux/interrupt.h> 
#include <asm/io.h> 
#include <linux/kobject.h> 

#define IRQ_NO  1 


/* global and static variables */ 

dev_t dev ;
static int irq_dev_id ; 
static struct cdev mycdev ;
static struct class *myclass ;
static struct device *mydevice ; 
static struct kobject *kobject_ref;
static int kobj_value =  10 ; 
static char ch ;
static int count =0 ; 
static char keystroke_buffer[130] ; 
/* scancode to ascii  */
static const char  scancode_to_ascii[128] = 
{

       0, 27 , '1' , '2' , '3' , '4' , '5' , '6' , '7' ,'8' , '9' , '0' , '-', '=', '\b',
	'\t' , 'q' , 'w','e', 'r' ,'t' , 'y' , 'u' , 'i' , 'o', 'p' , '[' , ']' , '\n' , 
      'a' , 's' , 'd', 'f', 'g', 'h', 'j', 'k','l',';', '\''  , '`' , '\\'	, 
      'z', 'x','c' ,'v', 'b', 'n' , 'm' ,',' ,'.' ,'/', '*' , ' ' , 0 

};



/******************************** DRIVER FUNCTION PROTOTYPE *********************** */ 

static int my_open(struct inode *inode , struct file *file ) ; 
static int my_release(struct inode *inode , struct file *file) ;
static ssize_t my_read ( struct file *filp , char __user *buf , size_t len , loff_t *offset) ;
static ssize_t my_write(struct file *filp , const char __user *buf , size_t len , loff_t *offset ) ;

/*******************************  SYS_FS FUNCTION PROTOTYPE ************************/ 

static ssize_t sysfs_show(struct kobject *kobj , struct kobj_attribute *attr , char *buff);
static ssize_t sysfs_store(struct kobject *kobj , struct kobj_attribute *attr ,const  char *buff, size_t count);

/*********************************  IRQ HANDLER *************************************/ 

static irqreturn_t irq_handler ( int  irq   , void *dev_id) 
{
	unsigned char scancode = inb(0x60);
       if(scancode  < 128 ) 
       {
	  ch  =    scancode_to_ascii[scancode] ; 
	 if(ch != 0) 
	 {
		 if(count < sizeof(keystroke_buffer) - 2 ) 
		 {

			keystroke_buffer[count] = ch ;
		        keystroke_buffer[count +1] = '\0' ;
		       count ++ ;	
		 }
		pr_info("Key Entered : %c With scancode :  0x%02x\n",ch, scancode) ; 
	 } 

       }

	pr_info("  SHARED IRQ  : Interrupt Occured \n"); 
	return  IRQ_HANDLED ;
} 



/* file attribute for sysfs */ 

struct kobj_attribute kobj_attr = __ATTR(interrupt_file, 0660, sysfs_show , sysfs_store) ;


/* file operations for driver    */

static struct file_operations fops ={
.owner = THIS_MODULE,
.read = my_read ,
.write = my_write,
.open = my_open ,
.release = my_release 
};


 
/***************************** PROC_FS  FUNCTIONS ************************/

static ssize_t sysfs_show(struct kobject *kobj , struct kobj_attribute *attr , char *buff)
{
	return sprintf(buff  , "%s",keystroke_buffer) ; 
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

	kobject_ref = kobject_create_and_add("interrupt_test",kernel_kobj);

	/* creating a file in  /sys/interrupt_test/ */ 

	if(sysfs_create_file(kobject_ref,&kobj_attr.attr)) 
	{
		goto r_sysfs ; 
	}


	if(request_irq(IRQ_NO , irq_handler , IRQF_SHARED , "interrupt_test" , &irq_dev_id)) 
	{
		pr_info(" FAILED TO REG IRQ \n"); 
		goto r_irq ; 
	} 


	
	printk(KERN_INFO " DRIVER - LOADED \n");
	return 0 ;
r_irq:
	free_irq(IRQ_NO  , &irq_dev_id); 

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


	count = 0 ; 
	free_irq(IRQ_NO  , &irq_dev_id); 
	sysfs_remove_file(kobject_ref , &kobj_attr.attr); 
	kobject_put(kobject_ref);
	sysfs_remove_file(kobject_ref , &kobj_attr.attr); 
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


	char    *device_buffer = kmalloc((len+1) , GFP_KERNEL) ; 
	if(device_buffer == NULL) 
	{
		pr_info(" ENOMEM\n"); 
		return -ENOMEM ; 
	} 

	if(copy_from_user(device_buffer, buf , len)!=0) 
	{ 
		pr_info(" ENOMEM\n"); 
		return -EFAULT ; 
	} 

	device_buffer[len+1] = '\0' ; 

	pr_info(" READ FUNCTION  : %s \n", device_buffer) ; 
	kfree(device_buffer) ; 
	device_buffer = NULL ; 

        return  len;
              
}




/* Reading function  */
static ssize_t my_read(struct file *filp , char __user *buf , size_t len , loff_t *offset)  
{

	size_t length = strlen(keystroke_buffer); 

	if(*offset >= length ) 
	{
		return 0 ; 
	} 

	if(copy_to_user(buf , keystroke_buffer, length)!=0)
	{
		return -EFAULT ;
	} 

	*offset += length ; 
	
	return  length ; 
}




/* Myopen function */
static  int my_open ( struct inode *inode , struct file *file) 
{
	printk(KERN_INFO " DEVICE FILE OPENED\n") ; 
	return 0 ; 
} 




/*  my_release function */ 
static int my_release(struct inode *inode , struct file *file) 
{
	printk(KERN_INFO "  DEVICE FILE  CLOSED  \n ");
	return 0 ; 
} 



MODULE_LICENSE("GPL");
module_init(hello_init);
module_exit(hello_exit);

