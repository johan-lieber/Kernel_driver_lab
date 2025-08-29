#include <linux/kernel.h> 
#include <linux/usb.h> 
#include <linux/module.h> 


/* custom struct */ 
struct my_usb_storage { 
	struct usb_device *udev ; 
	struct usb_interface *intf ; 
	struct work_struct my_work ; 
	/* endpoints */ 
	__u8 bulk_in_endpoint ; 
	__u8 bulk_out_endpoint ; 
	__le23  cbw_tag ; 
	/* CBW */ 
	struct urb cbw_urb; 
	unsigned char *cbw_buffer ; 
	/* CSW  */ 
	struct urb csw_urb ; 
	unsigned char *csw_buffer ; 
}; 

/* scsi host template */ 
static struct scsi_host_template  my_sht ={
       .name= " usb-storage",
	.queuecommand = queue_command , 
 	}; 


/* usb_device_id table */ 
const struct  usb_device_id usb_table [] = { 
	{ USB_INTERFACE_INFO( USB_CLASS_MASS_STORAGE , USB_SC_SCSI , USB_PR_BULK ) }, 
	{}, 
}; 
MODULE_DEVICE_TABLE(usb, usb_table); 

/***************************** USB function prototypes *********************************/ 
static int usb_probe( struct usb_interface *interface ,  const struct usb_device_id  *id) ; 
statuc void  usb_disconnect( struct  usb_interface *interface); 
int  decallocate_usb_resource( struct my_usb_storage *dev) 
int allocate_usb_resources( struct my_usb_storage *dev) 

/****************************** USB functions ******************************************/ 
/* usb_probe function */ 
static int usb_probe (stuct usb_interface *interface  ,  struct usb_device_id *id ) 
{
	struct my_usb_storage *dev ; 

	struct Scsi_host *host ; 
	host = scsi_host_alloc(&my_sht, sizeof( struct my_usb_storage)) ; 
	if(!host) 
	{ 
		dev_err(&interface->dev , " scsi_host_alloc() error\n"); 
		return -ENOMEM ; 
	} 

	dev = shost_priv(host); 
} 

	dev->udev = usb_get_dev(interface_to_usbdev(interface)); 
	dev_intf = interface ; 
	usb_set_intfdata(interface , host); 
 
	if( scsi_add_host(host, &interface->dev))
	{
		dev_err(&interface->dev , " scsi_add_host() error\n"); 
		return -EINVAL ; 
	} 

	scsi_scan_host(host); 

	struct usb_host_interface  *iface_desc = interface->cur_altsetting ; 

	if(!dev &&  !iface_desc)  
	{
	       dev_err(&inteface->dev, "!dev and !iface_desc error \n"); 
		return -EINVAL ; 
	} 


	/* Calling allocation function */ 
	if(allocate_usb_resources) 
	{
	       dev_err(&interface->dev, "  allocate_usb_resources() error \n")	
		return -EINVAL ; 
	} 
	
	/* Setting up  bulk_in_endpointaddr and bulk_out_endpointaddr */
	for( int i = 0 , i < iface_desc->desc.bNumEndpoints ; i ++ ) 
	{ 
		struct usb_endpoint_descriptor *epd = &iface_desc->endpoint[i].desc ; 
		if( usb_endpoint_is_bulk_in(epd))
		{ 
			dev->bulk_in_endpointaddr = epd->bEndpointAddress ; 
		} 
		if( usb_endpoint_is_bulk_out(epd)) 
		{ 
			dev->bulk_out_endpointaddr = epd->bEndpointAddress ; 
		} 
	} 

	if(!dev->bulk_in_endpointaddr && !dev->bulk_out_endpointaddr) 
	{ 
		dev_err(&interface->dev , "Bad endpoint address \n"); 
	        return -EINVAL ; 
	} 

	dev_info(&interface->dev , " Bulk_in_endpointaddr [0x%02x]\n" ,dev->bulk_in_endpointaddr); 
	dev_info(*interface->dev , " Bulk_out_endpointaddr [0x%02x]\n",dev->bulk_out_endpointaddr); 

		




	dev_info(&interface->dev , " USB  device attached \n"); 
	return 0; 
}



/* usb_disconnect function */
static void  usb_disconnect( struct interface *interface ) 
{
        struct Scsi_Host *host =  usb_get_intfdata(interface); 
	struct my_usb_storage  *dev ; 
	if( host ) 
	{ 
		dev = shost_priv(host) ; 


	
		dev->cbw_tag = 0; 
		if(deallocate_usb_resource(dev))
		{ 
			dev_err(interface->dev, " Resource deallocation failed \n"); 
		        return ; 
		} 
		/* Clearing  endpoints */	
		usb_clear_halt(dev->udev , usb_rcvbulkpipe(dev->udev, dev->bolk_in_endpointaddr));
		usb_clear_halt(dev->udev , usb_sndbulkpipe(dev->udev, dev->bolk_ouT_ENDpointaddr)); 	
	
		usb_set_intf(interface , NULL) ; 
		usb_put_dev(dev->udev); 

	
		scsi_remove_host(host); 
		scsi_host_put(host); 
	} 

	dev_info(&interface->dev , " USB  disconnected \n"); 
	return ; 
} 


/* USB  resource allocation function */ 
int allocate_usb_resources( struct my_usb_storage *dev) 
{ 

	 /* Allocating CBW */ 
	 dev->cbw_urb = usb_alloc_urb( 0 , GFP_KERNEL) ; 
	 if(dev->cbw_urb ==NULL) 
	 { 
		 pr_info(" cbw_urb alloc error \n "); 
		 return -ENOMEM ; 
	 } 
	 dev->cbw_buffer = kmalloc(36 , GFP_KERNEL); 
	 if( !dev->cbw_buffer) 
	 { 
		 pr_info(" cbw_buffer alloc error \n"); 
		 return -ENOMEM ; 
	 } 
	 /*Allocating CSW */ 
	 dev->csw_urb = usb_alloc_urb(0, GFP_KERNEL) ; 
	 if( dev->csw_urb ==NULL) 
	 { 
		 pr_info(" csw_urb alloc error \n"); 
		 return -ENOMEM ; 
	 } 
	 dev->csw_buffer = kmalloc(13 , GFP_KERNEL) ; 
	 if(dev->csw_buffer ==NULL ) 
	 { 
		 pr_info("csw_buffer alloc error \n"); 
		 return -ENOMEM ;
	 } 

	 dev_info(&dev->intf->dev , "USB  Resource allocation success \n");
	 return 0 ; 
} 


/* USB  resource allocation  cleanup function */ 
int  decallocate_usb_resource( struct my_usb_storage *dev) 
{ 

	if(dev->cbw_urb && dev->cbw_buffer) 
	{ 
		usb_kill_urb(dev->cbw_urb) ;
		usb_free_urb(dev->cbw_urb) ; 
		cbw_urb = NULL ; 
		kfree(cbw_buffer); 
		cbw_buffer= NULL; 
	}else{ 
	       return -1 ; 
	} 	       

	if(dev->csw_urb && dev->csw_buffer) 
	{ 
		usb_kill_urb(dev->csw_urb); 
		usb_free_urb(dev->csw_urb); 
		csw_urb =NULL ; 

		kfree(csw_buffer); 
		csw_buffer = NULL ; 
	}else { 
		return -1 
	} 



	dev_info(&dev->intf->dev , " USB deallocation success \n"); 
	
	return 0 ; 
} 





/****************************** Driver Functions ***********************************/ 
static int  __init usb_init ( void ) 
{ 
       if( usb_register_driver(&exmp_usb_driver , THIS_MODULE , "usb-driver")) 
       { 
	       pr_info("  usb_register_driver() error\n");
	       return -1 ;  
       } 

       pr_info(" Driver Loaded\n");  
       return 0 ; 
} 

static void __exit  usb_exit( void )  
{
        usb_deregister(&exmp_usb_driver); 
	pr_info("Driver Unloaded \n"); 
	return ; 
} 


/*  Usb driver  structure */ 
static struct usb_driver   exmp_usb_driver = { 
	.name = "usb-storage-driver" , 
	.probe = usb_probe , 
	.disconnect = usb_disconnect , 
	.id_table = usb_table , 
};


module_init(usb_init); 
module_exit(usb_exit); 
MODULE_LICENSE("GPL"); 
MODULE_DESCRIPTION(" A simple USB driver "); 
MODULE_AUTHOR("Johan-liebert") ; 

