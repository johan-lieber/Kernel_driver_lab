/*
 *  /file  :	  sysfs_module.c 
 *
 *
 *
 *  /details :    simple   linux device driver (sysfs) 
 *
 *
 *
 *
 *
 *
 * */

#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/err.h>


// globals  variables and structures 
dev_t dev ;
volatile int  x_value = 0 ;
struct cdev x_cdev ; 
struct class *x_class ;
struct device *x_device ;
struct kobject *kobj_ref ; 





// functions  PROTOTYPE

/******************DRIVER FUNCTIONS****************/

/******************SYSFS FUNCTIONS*****************/
 static ssize_t sysfs_show(struct kobject *kobj , struct kobj_attribute *attr , char *buf);
static ssize_t sysfs_store(struct kobject *kobj , struct kobj_attribute *attr , const  char *buf, size_t count);
struct kobj_attribute   obj_attr = __ATTR(x_value , 0660 , sysfs_show , sysfs_store); 


// file operations 
struct  file_operations fops ={
	.owner  = THIS_MODULE,
	.write  = NULL , 
	.read = NULL , 
	.open = NULL ,
	.release = NULL
};





//   INIT  function 
static int __init driver_init_sysfs(void) 
{

	int result = alloc_chrdev_region(&dev, 0 , 1 , "syschardev") ;
	if( result < 0) 
	{
		return -EFAULT ;
	}


	/*  Creating cdev structure */
	cdev_init(&x_cdev , &fops) ;

	/* Registeing  device to system */
  	if((cdev_add(&x_cdev , dev  ,1 ))<0)
	{
		goto r_class ;
	}


	/* creating struct class  */
	if(IS_ERR(x_class = class_create("syschardev"))){
		goto r_class ; 
	}


	/* creating device */
	if(IS_ERR(x_device = device_create(x_class , NULL , dev , NULL , "syschardev"))){
		goto r_device; 
	}


	/* creating kobject */
	kobj_ref = kobject_create_and_add("x_sysfs" ,kernel_kobj);
	
	if(!kobj_ref)
	{
		return -EFAULT ;
	}


	if(sysfs_create_file(kobj_ref ,&obj_attr.attr))
	{
		pr_info("failed to create file");
		goto  r_sysfs; 
	}





	pr_info(" --MODULE_LOADED--\n");
	return 0 ; 

r_sysfs:
	kobject_put(kobj_ref);
	sysfs_remove_file(kobj_ref, &obj_attr.attr); 


r_device :
	class_destroy(x_class);
	


r_class : 
	unregister_chrdev_region(dev,1) ;
	cdev_del(&x_cdev); 
	return -1; 

}





// EXIT  function 
static void __exit driver_exit_sysfs(void)
{
	device_destroy(x_class , dev ) ;
	class_destroy(x_class);
	cdev_del(&x_cdev);
	unregister_chrdev_region(dev, 1 );
	pr_info("--MODULE_UNLOADED--\n");
	return ; 
}




// sysfs_show function ... 

static ssize_t sysfs_show(struct kobject *kobj , struct kobj_attribute *attr , char *buf)
{
	pr_info(" ---  READ FUNCTION  FROM SYSFS\n");
	return 0; 

}

static ssize_t sysfs_store(struct kobject *kobj , struct kobj_attribute *attr , const  char *buf, size_t count)
{
	pr_info("--WRITE FUNCTION FROM SYSFS\n");
	return count ;
}




MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("  A SIMPLE SYSFS CHARACTER DEVICE DRIVER");
module_init(driver_init_sysfs);
module_exit(driver_exit_sysfs);
