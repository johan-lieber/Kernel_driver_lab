#include <linux/kernel.h>
#include <linux/init.h> 
#include <linux/device.h>
#include <linux/module.h> 

dev_t dev = 0;
static struct class *dev_class;
static struct device *dev_device;
static int __init dev_init(void)
{
	int ret;
	ret = alloc_chrdev_region(&dev, 0, 1, "dev_class");
	
	if (ret < 0) {
		pr_info("alloc_chrdev_region() error\n");
		goto r_chrdev;
	}
	dev_class = class_create("dev_class");
	if (IS_ERR(dev_class)) {
		pr_err("class_create() error\n");
		goto r_class;
	}

	if (IS_ERR(dev_device = device_create(dev_class, NULL, dev, NULL, "dev_class"))) {
		pr_err("device_create() error\n");
		goto r_device;
	}



	pr_info("Driver loaded.\n");
	return 0;

r_device:
	device_destroy(dev_class,dev);
r_class:
	class_destroy(dev_class);
r_chrdev:
	unregister_chrdev_region(dev, 1);
	return 1;


}

static void __exit dev_exit(void)
{
	device_destroy(dev_class, dev);
	class_destroy(dev_class);
	unregister_chrdev_region(dev, 1);

	pr_info("Driver unloaded..\n");
}

module_init(dev_init);
module_exit(dev_exit);
MODULE_LICENSE("GPL");

