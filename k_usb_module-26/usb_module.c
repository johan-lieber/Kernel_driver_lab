#include <linux/kernel.h> 
#include <linux/module.h>
#include <linux/usb.h> 

#define  VENDOR_ID (0x04e8)
#define  PRODUCT_ID (0x6860)




static int   usb_probe(struct usb_interface *interface , const struct  usb_device_id *id); 
static  void usb_disconnect(struct  usb_interface *interface) ; 

static int   usb_probe(struct usb_interface *interface , const struct  usb_device_id *id)
{
	unsigned  int  endpoints_count ; 
	struct  usb_host_interface *iface_desc  = interface->cur_altsetting; 
	dev_info(&interface->dev, "USB-D PROBE: VID :0X%02X |  PID : 0x%02x\n", id->idVendor , id->idProduct); 

	endpoints_count = iface_desc->desc.bNumEndpoints ; 


	return 0 ; 
} 



static  void usb_disconnect(struct  usb_interface *interface) 
{ 
	pr_info(" USB DISCONNECTED \n"); 
	return ; 
} 



const struct usb_device_id   usb_table[] = 
{ 
	{ USB_DEVICE (VENDOR_ID , PRODUCT_ID ) } , 
	{} 
}; 


MODULE_DEVICE_TABLE(usb , usb_table) ; 


static struct  usb_driver  usb_driver = 
{ 
	.name =  " SAMSUNG USB DRIVER " ,
	.probe = usb_probe , 
	.disconnect = usb_disconnect, 
	.id_table = usb_table 
};



static int  __init  usb_device_init(void) 
{ 
	pr_info(" USB - INIT FN -\n") ;
       usb_register(&usb_driver) ; 

	return 0 ; 
} 


static  void  __exit usb_device_exit(void) 
{ 
	pr_info(" USB - EXIT fn \n"); 
	usb_deregister(&usb_driver); 
}


module_init(usb_device_init); 
module_exit(usb_device_exit); 
MODULE_LICENSE("GPL"); 

