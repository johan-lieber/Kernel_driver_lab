#include <linux/gpio/consumer.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h> 
#include <linux/uaccess.h>
#include <linux/gpio.h>
#define GPIO_NO (17)

dev_t dev;
static struct class *dev_class;
static struct device *dev_device;
static struct cdev   cdev_tmp;
static struct gpio_desc *led_desc; 

static ssize_t g_write(struct file *filp, const char __user *buf, size_t len, loff_t *offset)
{
 	int value;
	
	if( len  >= 2)
		len = 2 ;
		
	if (copy_from_user(&value, buf, sizeof(int))) {
		pr_info("copy_from_user() failed\n");
	}
	
	if (value == 1) {
		gpiod_set_value(led_desc, 1);
		pr_info("GPIO led on \n");
		value = 0;
	} else {
		gpiod_set_value(led_desc, 0);
		pr_info("GPIO led off \n");
	}
	len = value;
	return sizeof(value);
}

static ssize_t g_read(struct file *filp, char __user *buf, size_t len, loff_t *offset)
{
	uint8_t value = gpiod_get_value(led_desc);

	if (!led_desc) {
		pr_err("led_desc NULL \n");
		return -ENODEV;
	}	
	if (copy_to_user(buf, &value , 1)) {
		pr_info("copy_to_user() failed \n");
		return -1 ;
	}
	pr_info("GPIO_%d\n",value);
	return 1;
}


static struct file_operations fops = {
	.owner = THIS_MODULE,
	.write = g_write,
	.read  = g_read,
	.open  = NULL,
	.release = NULL,
};

static int __init gpio_init(void)
{
	int ret;
	ret = alloc_chrdev_region(&dev, 0, 1, "gpio");
	
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

	/* gpio configurations */

	if (!gpio_is_valid(GPIO_NO)) {
		pr_err("Invalid GPIO_%d\n",GPIO_NO);
		goto r_device;
	}
	

	ret = gpio_request(GPIO_NO, "led_gpio");
	if (ret) {
		pr_err("gpio_request() error\n");
		goto r_device;
	}

	led_desc = gpio_to_desc(GPIO_NO);

	if (!led_desc) {
		pr_err("gpio_to_desc() error%d\n",GPIO_NO);
		goto r_gpio;
	}


	ret = gpiod_direction_output(led_desc, 0);

	if (ret) {
		pr_err("gpiod_direction_output() error\n");
		goto r_gpio;
	}

	gpiod_export(led_desc, false);
	gpiod_export_link(NULL, "led", led_desc);

 	return 0;
r_gpio:
	gpio_free(GPIO_NO);
r_device:
	device_destroy(dev_class, dev);
r_class:
	class_destroy(dev_class);
r_cdev:
	cdev_del(&cdev_tmp);
r_unreg:
	unregister_chrdev_region(dev,1);
	return -1;
}

static void __exit gpio_exit(void)
{
	gpiod_unexport(led_desc);
	gpiod_put(led_desc);
	gpio_free(GPIO_NO);
	device_destroy(dev_class, dev);
	class_destroy(dev_class);
	cdev_del(&cdev_tmp);
	unregister_chrdev_region(dev, 1);
	printk(KERN_INFO "driver unloaded successfully\n");
}


module_init(gpio_init);
module_exit(gpio_exit);
MODULE_DESCRIPTION("simple gpio module for led\n");
MODULE_LICENSE("GPL");
