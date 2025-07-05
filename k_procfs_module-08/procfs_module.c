#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>

#define  BUFF_SIZE 1024 
static  char device_buffer[BUFF_SIZE] ; 



// Variable intitializations 
dev_t dev ;
char *proc_buffer = NULL ;
char *driver_buffer = NULL ;
static struct cdev mycdev ;
static struct class *myclass ;
static struct device *mydevice ; 
static struct proc_dir_entry *parent ; 

/***************** DRIVER  FUNCTIONS *****************/
static int my_open(struct inode *inode , struct file *file ) ; 
static int my_release(struct inode *inode , struct file *file) ;
static ssize_t my_read ( struct file *filp , char __user *buf , size_t len , loff_t *offset) ;
static ssize_t my_write(struct file *filp , const char __user *buf , size_t len , loff_t *offset ) ;

/***************** PROCFS  FUNCTIONS *****************/
static ssize_t proc_write(struct file *filp ,  const char __user *buf , size_t len , loff_t *offset) ;
static ssize_t proc_read(struct file *filp , char __user *buf , size_t len , loff_t *offset);
static int proc_open(struct inode *inode,  struct file *file) ;
static int proc_release(struct inode *node , struct file *file) ;



//  driver file operations struct 
static  struct file_operations fops ={
.owner = THIS_MODULE,
.read = my_read ,
.write = my_write,
.open = my_open ,
.release = my_release 
};


// proc file operations struct 
static struct proc_ops  proc_fops ={
	.proc_open = proc_open ,
	.proc_write = proc_write ,
	.proc_read = proc_read, 
	.proc_release = proc_release
};




/* INIT  function */
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

	if(cdev_add(&mycdev , dev , 1 ) < 0) 
	{
	
		goto  r_class; 
	}


	if(IS_ERR(myclass = class_create("mychardev")))
	{
		goto r_class ; 
	}


	if(IS_ERR(mydevice  = device_create(myclass, NULL , dev , NULL , "mychardev")))
	{
		goto r_device ;
	}


	parent = proc_mkdir("x_procfs", NULL) ;
	if(!parent)
	{
		pr_info("PROC_DIR_ERR");
		return -EFAULT;
	}


	proc_create("x_status",0666 , parent , &proc_fops) ;


	
	printk(KERN_INFO " DRIVER - LOADED \n");
	return 0 ; 


r_class :

	cdev_del(&mycdev) ; 
	unregister_chrdev_region(dev, 1 ) ;
	return -1 ; 
	
r_device :
	class_destroy(myclass);
	return -1 ; 


}


/* EXIT  funnction */ 
static void __exit hello_exit(void)
{
	kfree(proc_buffer);
	proc_remove(parent);
	device_destroy(myclass , dev ) ;
	class_destroy(myclass);
	cdev_del(&mycdev);
	unregister_chrdev_region(dev, 1 ) ;

	
	printk(KERN_INFO "bye - from - kernel \n");
	return ;
}



/*******************************DRIVER FUNCTIONS****************************/


// Writing function ... 
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




// Reading function ... `
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


	printk(KERN_INFO " Reading  functins begins \n");
       	printk(KERN_INFO "  Here is : %s \n",device_buffer ) ;	

	printk(KERN_INFO " BYE FROM READ FUNCTION \n");
	return  length  ; 

}





// Myopen function
static  int my_open ( struct inode *inode , struct file *file) 
{
	printk(KERN_INFO "  hello from open function\n") ; 
	return 0 ; 
} 




//  my_release function 
static int my_release(struct inode *inode , struct file *file) 
{
	printk(KERN_INFO " Hullu from release function \n ");
	return 0 ; 
} 


/**************************PROCFS-FUNCTIONS**************************/


// proc_write function
static ssize_t proc_write(struct file *filp ,  const char __user *buf , size_t len , loff_t *offset)
{
	if(proc_buffer = !=NULL) 
	{
		kfree(proc_buffer) ; 
	}


	proc_buffer = kmalloc((len+1)  ,GFP_KERNEL); 
	if(proc_buffer == NULL) 
	{
		pr_err("ALLOC_MEM_ERR");
		return -EFAULT;
	}


	if(copy_from_user(proc_buffer, buf , len)!=0)
	{
		return -EFAULT ;
	}

	proc_buffer[len+1] = '\0'; 

	pr_info("PRO_WRITE :%s  \n", proc_buffer);
	return len ; 
}



// proc_read function
static ssize_t proc_read(struct file *filp , char __user *buf , size_t len , loff_t *offset)
{

	size_t length = strlen(proc_buffer) ; 

	if(*offset >= length)
	{
		return 0 ; 
	}


	if(copy_to_user(buf , proc_buffer , length)!=0)
	{
		return -EFAULT ;
	}

	*offset += length ; 


	pr_info("PROC_READ \n");
	return length ;
}



//proc_open function 
static int proc_open(struct inode *inode,  struct file *file)
{
	pr_info("PROC_OPEN\n");
	return 0 ;
}



// proc_release function 
static int proc_release(struct inode *node , struct file *file)
{
	pr_info("PROC_RELEASE\n");
	return 0 ;
}






MODULE_LICENSE("GPL");
module_init(hello_init);
module_exit(hello_exit);

