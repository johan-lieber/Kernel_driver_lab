#include <linux/kernel.h>
#include <linux/usb.h> 
#include <linux/module.h> 
#include <linux/usb/ch9.h> 
#include <linux/usb/storage.h> 
#include <linux/slab.h> 
#include <linux/types.h> 
#include <linux/byteorder/generic.h> 


#define  VID  (0X0781)
#define PID  (0X5591)
#define BUF_SIZE 512 
#define CBW_LEN  31 


/* GLOBAL VARIBALE */ 
unsigned  char *bulk_buf ; 
struct urb *my_urb ; 
unsigned char *cbw_buffer ; 




/* FUNCTION PROTOTYPE */ 
static int usb_probe( struct usb_interface *interface ,  const struct  usb_device_id *id);
static void usb_disconnect( struct usb_interface *interface );
static   void cbw_callback ( struct urb *urb ); 

/* USB  DEVICE ID TABLE  */ 
const struct  usb_device_id  usb_table[] = 
{  {  USB_INTERFACE_INFO( USB_CLASS_MASS_STORAGE , USB_SC_SCSI , USB_PR_BULK) },  
	{}
};

MODULE_DEVICE_TABLE(usb,usb_table); 



/* USB_DRIVER STRUCTURES */ 
 static struct   usb_driver   exmp_usb_driver  = 
{ 
	.name = "URBS DRIVER" , 
	.probe = usb_probe , 
	.disconnect = usb_disconnect , 
	.id_table =  usb_table 
}; 

/*  command block wrapper structure */ 
struct command_block_wrapper { 
	__le32 dCBWSignature ; 
	__le32 dCBWTag ; 
	__le32 dCBWDataTransferLength ; 
	__u8 bmCBWFlags ; 
	__u8 bCBWLUN ; 
	__u8 bCBWCBLength ; 
	__u8 CBWCB[16];
} __attribute__((packed)); 


/************************************************ USB  FUNCTIONS ****************************************/

static int usb_probe ( struct  usb_interface *interface ,  const struct usb_device_id  *id)
{
	pr_info(" USB PROBE \n"); 


	struct usb_device *dev  =  interface_to_usbdev(interface) ; 
	struct usb_host_interface  *iface_desc = interface->cur_altsetting ; 


 	__u8  bulk_in_endpointaddr = 0  ; 
	__u8  bulk_out_endpointaddr  =  0 ; 
	
	

	for(int i = 0 ; i < iface_desc->desc.bNumEndpoints ; i++ ) 
	{ 
		struct usb_endpoint_descriptor  *epd = &iface_desc->endpoint[i].desc; 
		
		if(usb_endpoint_is_bulk_in(epd)) 
		{ 
			bulk_in_endpointaddr =  epd->bEndpointAddress ; 
		} 

		if(usb_endpoint_is_bulk_out(epd))
		{ 
			bulk_out_endpointaddr = epd->bEndpointAddress; 
		} 
	} 


	if(!bulk_in_endpointaddr && !bulk_out_endpointaddr) 
	{ 
		dev_err(&interface->dev , " BAD END POINTS ADD \n") ; 
		return  -ENODEV ; 
	} 

	 bulk_buf =  kmalloc(BUF_SIZE , GFP_KERNEL); 
	 if(bulk_buf == NULL ) 
	 { 
		 return -ENOMEM  ; 
	 } 



	 /*Allocating buffer  for CW_LEN */ 
	 cbw_buffer = kmalloc(CBW_LEN , GFP_KERNEL) ; 
	 if( cbw_buffer == NULL ) 
	 { 
		 pr_info(" CW_BUF_ALLOC_ERR\n"); 
		 return ENOMEM ; 
	 } 



	 /* Allocating and  urb */ 
	 my_urb = usb_alloc_urb(0, GFP_KERNEL) ; 
	 
	 if(my_urb == NULL ) 
	 {
		 return - ENOMEM ; 
	 } 




	 usb_fill_bulk_urb(my_urb, dev , usb_rcvbulkpipe(dev, bulk_out_endpointaddr), cbw_buffer , CBW_LEN , cbw_callback , NULL ) ; 


	 int ret = usb_submit_urb(my_urb , GFP_KERNEL); 
	 
	 if(ret) 
	 { 
		 pr_info(" URB SUBMISSION  ERR \n") ; 
		  
		 goto r_urb ; 
	 } 



	 return 0 ; 

r_urb : 
	 usb_kill_urb(my_urb); 
	 usb_free_urb(my_urb); 
	 kfree(bulk_buf);  
	 return 0 ; 



		
} 


/* Usb  Disconnect */
static void usb_disconnect ( struct usb_interface  *interface ) 
{ 


	pr_info(" USB - DISCONNECTED - \n"); 

	usb_kill_urb(my_urb) ; 
	usb_free_urb(my_urb) ; 
	kfree(bulk_buf) ; 

	return ; 
}



/* cbw_callback function */ 

static   void cbw_callback ( struct urb *urb )
{ 
	pr_info(" Callback usb functions \n"); 


	if(urb->status)
	{
		pr_err("  CBW URB  FAILED WIH STATUS  : %d\n", urb->status) ; 
		return ; 
	} 

	struct command_block_wrapper *cbw =  (struct  command_block_wrapper *)  urb->transfer_buffer; 

	if( cbw->dCBWSignature !=cpu_to_le32(0x43425355)) 
	{ 
		pr_err(" Imvalid CBW signature \n"); 
		return ; 
	} 

//	pr_info("Received  CBW - Command : 0x%02\n", cbw->CBWCB[0]); 

	switch(cbw->CBWCB[0]) 
	{ 
		case 0x12 : 
			pr_info(" SCSI  INQUIRY command  Recieved \n") ;
			break ; 
		case 0x00:
			pr_info(" SCSI TEST UNIT READY \n"); 
			break ; 
		case 0x28 : 
			pr_info("SCSI  READ (10) \n") ;
			break ;
		case 0x1A : 
			pr_info(" SCSI MODE SENSE COMMAND RECIEVED \n"); 
			break ; 
		case 0x2A : 
			pr_info(" SCSI WRITE COmmand RECIEVED \n"); 
			break ; 

		default :
			pr_warn(" Unhandled SCsi command :0x%02x\n", cbw->CBWCB[0]); 
			break ; 
	}

	return ; 
	
} 
 





/************************************************   MODULE FUNCTIONS ***************************************/ 

static int __init urb_init(void) 
{ 
	usb_register(&exmp_usb_driver) ; 
	pr_info(" -USB  REGISTERED-  \n");

	return 0 ; 
} 


static void  __exit urb_exit(void) 
{ 

	usb_deregister(&exmp_usb_driver) ; 
	pr_info(" -USB  DEREGISTERED-\n") ; 
	
	return  ; 
} 





module_init(urb_init); 
module_exit(urb_exit); 
MODULE_LICENSE("GPL"); 



