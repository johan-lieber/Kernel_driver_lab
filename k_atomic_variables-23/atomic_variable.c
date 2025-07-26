#include <asm/atomic.h> 
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/kthread.h> 
#include <linux/wait.h>
#include <linux/delay.h>



dev_t dev ; 
struct cdev mycdev ; 
struct class *myclass ; 
struct device *mydevice ; 
static struct task_struct *exmp_thread; 
static struct task_struct *exmp_thread2 ; 
atomic_t  atomic_v  = ATOMIC_INIT(0); 
unsigned  int bit_check = 0 ; 

/* FUNC prototype  */
ssize_t  my_write(struct file *filp , const char __user *buf , size_t len , loff_t *offset) ;
ssize_t  my_read(struct file *filp , char   __user *buf , size_t len , loff_t *offset) ;
int thread_function (void *data); 

int thread_function2(void *data); 

struct file_operations fops = {
	.owner = THIS_MODULE,
	.write = NULL,
	.read = NULL,
	.open = NULL,
	.release = NULL 
};


/*************************** KTHREAD FUNCTON *****************************/
int thread_function(void *data) 
{
	unsigned int per_value = 0 ; 
	while(!kthread_should_stop())
	{
		pr_info(" -THREAD RUNNING\n");
	       atomic_inc(&atomic_v); 
	       per_value = test_and_change_bit(1 ,(void *) &bit_check); 
	       pr_info("function  [ value :%u] [ bit :%u]\n", atomic_read(&atomic_v ) , per_value) ; 

		msleep(1000);
	}
	return 0 ;
} 

/* second thread */ 

int thread_function2(void *data) 
{
	unsigned int   per_value  ; 
	while(!kthread_should_stop())
	{
		pr_info(" -THREAD RUNNING\n");
	       atomic_inc(&atomic_v); 
	       per_value = test_and_change_bit(1 ,(void *) &bit_check); 
	       pr_info("function  [ value :%u] [ bit :%u]\n", atomic_read(&atomic_v ) , per_value) ; 

		msleep(1000);
	}
	return 0 ;
} 


/*************************** DRIVER FUNCTIONS ****************************/

/* INIT function */
static int __init   kthread_module_init(void) 
{
	if(alloc_chrdev_region(&dev , 0 ,1 , "mythreadev") < 0) 
	{
		pr_err(" MAJOR_NO_ALLOC_ERR") ; 
		return -1 ;
	} 

	cdev_init(&mycdev , &fops) ; 
	
	if(cdev_add(&mycdev , dev,  1 ) < 0 ) 
	{
		goto r_class ; 
	} 


	if(IS_ERR(myclass = class_create("mythreadev")))
	{
		goto r_class;
	}

	if(IS_ERR( mydevice = device_create(myclass , NULL , dev ,NULL , "mythreadev")))
	{
		goto r_device  ; 
	} 


	exmp_thread = kthread_create(thread_function , NULL , "examp_thread"); 
	if(!exmp_thread) 
	{
		pr_info(" KTHREAD_CREAT_ERR\n");

		return -1 ;
	}

	wake_up_process(exmp_thread) ;

	exmp_thread2 = kthread_run(thread_function2 ,NULL, "examp_thread2") ; 
	if(!exmp_thread2)
	{ 
		pr_info(" KTHREAD_CREAT_ERR\n"); 
	} 


	pr_info(" --MODULE LOADED --\n");

	return 0 ; 


r_class :
	unregister_chrdev_region(dev, 1 ) ;
	cdev_del(&mycdev);
	return -1 ; 
r_device :
	class_destroy(myclass);
	return -1 ;

} 




/* EXIT functin */

static void  __exit   kthread_module_exit(void) 
{

	kthread_stop(exmp_thread) ;
	kthread_stop(exmp_thread2) ;
	device_destroy(myclass, dev ) ;
	class_destroy(myclass);
	cdev_del(&mycdev); 
	unregister_chrdev_region(dev, 1 ) ;
	 pr_info(" --MODULE UNLOADED --\n") ; 
	 return ;
} 





MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(" --  A SIMPLE  KTHREAD MODULE EXAMPLE --") ;
module_init(kthread_module_init);
module_exit(kthread_module_exit);

