#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h> 

dev_t dev;
static struct class *dev_class;
static struct device *dev_device;
static struct cdev   cdev_tmp;

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open  = NULL,
	.write = NULL,
	.read  = NULL,
	.open  = NULL,
	.release = NULL
};

/* function prototype */
static int __init gpio_init(void);
static void __exit gpio_exit(void);

static int __init gpio_init(void)
{
	int ret;
	ret = alloc_chrdev_region(&dev, 1, 0, "gpio");
	
	if (ret < 0) {
		pr_err("alloca_chrdev_region() error\n");
		goto r_unreg;
	}

	cdev_init(&cdev_tmp ,&fops);
	
	ret = cdev_add(&cdev_tmp, dev, 1);
	if (ret < 0) {
		pr_err("cdev_add() error\n");
		goto r_cdev;
	}
	
	if (IS_ERR(dev_class = class_create("gpio"))) {
		pr_info("create_class() error\n");
		goto r_class;
	}

	if (IS_ERR(dev_device = device_create(dev_class, NULL, dev, NULL,"gpio"))) {
		pr_err("create_device() error\n");
		goto r_device;
	}

 	return 0;
r_device:
	device_destroy(dev_class, dev);
r_class:
	class_destroy(dev_class);
r_cdev:
	cdev_del(&cdev_tmp);
r_unreg:
	unregister_chrdev_region(dev,1);
}

static void __exit gpio_exit(void)
{
	device_destroy(dev_class, dev);
	class_destroy(dev_class);
	cdev_del(&cdev_tmp);
	unregister_chrdev_region(dev, 1);
	printk(KERN_INFO "driver unloaded successfully\n");
} 
