
#include <linux/kernel.h> 
#include <linux/usb.h> 
#include <linux/module.h> 
#include <linux/usb.h> 
#include <linux/usb/ch9.h> 
#include <linux/usb/storage.h> 
#include <linux/slab.h> 
#include <linux/types.h> 
#include <linux/byteorder/generic.h> 
#include <linux/string.h> 
#include <scsi/scsi.h> 
#include <scsi/scsi_host.h> 
#include <scsi/scsi_cmnd.h> 
#include <scsi/scsi_device.h> 
#include <linux/workqueue.h> 
#include <scsi/scsi_cmnd.h>

#define CBW_SIG 0x43425355 
#define CSW_LEN  13 
#define CBW_LEN 31 


/* command_block_wrapper struct */ 
struct command_block_wrapper { 
	__le32  dCBWSignature ; 
	__le32  dCBWTag ; 
	__le32  dCBWDataTransferLength ; 
	__u8 bmCBWFlags ; 
	__u8 bCBWLUN; 
	__u8 bCBWCBLength ; 
	__u8 CBWCB[16] ; 
} __attribute__((packed)) ; 
/* command status wrapper struct */ 
struct command_status_wrapper { 
	__le32 dCSWSignature ; 
	__le32 dCSWTag ; 
	__le32 dCSWDataResidue; 
	__u8 bCSWStatus ; 
} __attribute__((packed)) ;

/* custom struct */ 
struct my_usb_storage { 
	struct usb_device *udev ; 
	struct usb_interface *intf ; 
	struct work_struct my_work ;
        struct command_block_wrapper cbw ;	
	struct command_status_wrapper csw ;
	unsigned int bufferlength ; 
	unsigned  int direction ; 
	struct scsi_cmnd *active_scmd ; 	
	/* endpoints */ 
	__u8 bulk_in_endpointaddr ; 
	__u8 bulk_out_endpointaddr ; 
	__le32  cbw_tag ; 
	/* CBW */ 
	struct urb *cbw_urb; 
	unsigned char *cbw_buffer ; 
	/* CSW  */ 
	struct urb *csw_urb ; 
	unsigned char *csw_buffer ; 
	/* data  buffer */ 
	struct urb *data_urb ; 
	unsigned  char *data_buffer; 
}; 

/* usb_device_id table */ 
const struct  usb_device_id usb_table [] = { 
	{ USB_INTERFACE_INFO( USB_CLASS_MASS_STORAGE , USB_SC_SCSI , USB_PR_BULK ) }, 
	{}, 
}; 
MODULE_DEVICE_TABLE(usb, usb_table); 


/***************************** USB function prototypes *********************************/ 
static int usb_probe( struct usb_interface *interface ,  const struct usb_device_id  *id) ; 
static void  usb_disconnect( struct  usb_interface *interface); 
int  deallocate_usb_resource( struct my_usb_storage *dev); 
int allocate_usb_resource( struct my_usb_storage *dev) ;
static int queue_command ( struct Scsi_Host *host , struct scsi_cmnd *scmd );
static void  cbw_callback( struct urb *urb ); 
static void csw_callback ( struct urb *urb ); 
static void data_callback ( struct urb *urb); 

/* scsi host template */ 
static struct scsi_host_template  my_sht ={
       .name= " usb-storage",
	.queuecommand = queue_command ,
       .can_queue = 1 , 
	.this_id  = -1 , 
     	.sg_tablesize = -1 , 
	.max_sectors = 240, 
	.cmd_per_lun = 1 , 
 	}; 

/*  Usb driver  structure */ 
static struct usb_driver   exmp_usb_driver = { 
	.name = "usb-storage-driver" , 
	.probe = usb_probe , 
	.disconnect = usb_disconnect , 
	.id_table = usb_table , 
};



/****************************** USB functions ******************************************/ 
/* usb_probe function */ 
static int usb_probe (struct usb_interface *interface  ,const   struct usb_device_id *id ) 
{
	struct my_usb_storage *dev ; 

	struct Scsi_Host *host ; 
	host = scsi_host_alloc(&my_sht, sizeof( struct my_usb_storage)) ; 
	if(!host) 
	{ 
		dev_err(&interface->dev , " scsi_host_alloc() error\n"); 
		return -ENOMEM ; 
	} 

	dev = shost_priv(host); 
 

	dev->udev = usb_get_dev(interface_to_usbdev(interface)); 
	dev->intf = interface ; 
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
	       dev_err(&interface->dev, "!dev and !iface_desc error \n"); 
		return -EINVAL ; 
	} 


	/* Calling allocation function */ 
	if(allocate_usb_resource(dev)) 
	{
	       dev_err(&interface->dev, "  allocate_usb_resources() error \n");	
		return -EINVAL ; 
	} 
	
	/* Setting up  bulk_in_endpointaddr and bulk_out_endpointaddr */
	for( int i = 0 ; i < iface_desc->desc.bNumEndpoints ; i ++ ) 
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
	dev_info(&interface->dev , " Bulk_out_endpointaddr [0x%02x]\n",dev->bulk_out_endpointaddr); 

	dev_info(&interface->dev , " USB  device attached \n"); 
	return 0; 
}

/* queue_command function */ 
static int queue_command ( struct Scsi_Host *host , struct scsi_cmnd *scmd ) 
{ 
	struct my_usb_storage *dev = shost_priv(host); 

	unsigned char *cdb  = scmd->cmnd ; 
	int direction  = scmd->sc_data_direction;
	unsigned int bufflen  = scsi_bufflen(scmd); 

	dev->bufferlength = bufflen ;
        dev->direction = direction ; 
	 	

	memset(&dev->cbw , 0 ,sizeof(dev->cbw)); 

	dev->cbw.dCBWSignature = cpu_to_le32(CBW_SIG); 
	dev->cbw.dCBWTag = cpu_to_le32(dev->cbw_tag++); 
	dev->cbw.dCBWDataTransferLength  = cpu_to_le32(bufflen); 

	 
	if( direction == DMA_FROM_DEVICE )
	{ 
		dev->cbw.bmCBWFlags = 0x80; 
	}else if (direction  == DMA_TO_DEVICE) 
	{ 
		dev->cbw.bmCBWFlags = 0x00 ; 
	} 
	dev->cbw.bCBWLUN =0 ; 
	dev->cbw.bCBWCBLength = scmd->cmd_len ; 

	pr_info(" scmd->cmd_len :%d\n",scmd->cmd_len); 

	memcpy( dev->cbw.CBWCB , cdb , scmd->cmd_len);
        memcpy( dev->cbw_buffer , &dev->cbw , CBW_LEN); 	

 usb_fill_bulk_urb(dev->cbw_urb , dev->udev , usb_sndbulkpipe(dev->udev , dev->bulk_out_endpointaddr), dev->cbw_buffer, CBW_LEN  , cbw_callback, dev) ;

	if( usb_submit_urb (dev->cbw_urb , GFP_KERNEL)) 
	{ 
		pr_info(" cbw_urb : usb_submit_urb() error\n"); 
		return -ENOMEM ; 
	} 


	return 0 ; 
} 


/*  cbw_callback function */ 

 static void  cbw_callback( struct urb *urb ) 
{ 
	struct my_usb_storage  *dev  = urb->context ; 
	if( urb->status) 
	{
	       pr_info(" status \n"); 
      		return ; 	       
	}

	struct command_block_wrapper  *cbw = (struct command_block_wrapper *) urb->transfer_buffer; 

	if(  le32_to_cpu(cbw->dCBWSignature)!= CBW_SIG)
	{ 
		pr_err(" Invalid CBW signature  \n"); 
		return ; 
	} 
 
	


	/* Submitting  data_urb and   csw_urb  if condition  value  gets less then zero   */ 	
	if( dev->bufferlength > 0 ) 
	{ 
		if(dev->direction == DMA_FROM_DEVICE) 
		{ 
			usb_fill_bulk_urb(dev->data_urb , dev->udev , usb_rcvbulkpipe(dev->udev , dev->bulk_in_endpointaddr) ,dev->data_buffer , CBW_LEN , data_callback, dev ) ; 
			int  ret = usb_submit_urb(dev->data_urb , GFP_ATOMIC);
			if(ret ) 
			{ 
				pr_info(" usb_submit_urb() error\n"); 
				return ; 
			} 
		} 

		if(dev->direction == DMA_TO_DEVICE) 
		{ 
			
			usb_fill_bulk_urb(dev->data_urb , dev->udev , usb_sndbulkpipe(dev->udev , dev->bulk_out_endpointaddr) ,dev->data_buffer , CBW_LEN , data_callback, dev ) ; 
			int retvalue = usb_submit_urb(dev->data_urb , GFP_ATOMIC); 
			if( retvalue) 
			{ 
				pr_info("usb_submit_urb() error\n"); 
				return ; 
			}
		} 
	}else{ 
		/* Submiting  csw   urb */ 

		usb_fill_bulk_urb(dev->csw_urb , dev->udev , usb_rcvbulkpipe(dev->udev , dev->bulk_in_endpointaddr) , dev->csw_buffer ,  CSW_LEN , csw_callback, dev ) ;   
		int csw_rtv = usb_submit_urb(dev->csw_urb, GFP_ATOMIC);
		if(csw_rtv) 
		{ 
			pr_info("csw_urb : usb_submit_urb() error\n"); 
			return ; 
		} 

	}
	return ; 
}


/* Data_callback function */ 
static void data_callback(  struct urb *urb ) 
{ 
	struct my_usb_storage  *dev  = urb->context ; 

	if( urb->status) 
	{ 
		pr_err(" data_urb  error \n"); 
	}
// 
// 	struct command__wrapper  *csw = (struct command_block_wrapper *) urb->transfer_buffer; 
// 
// 	if(  le32_to_cpu(csw->dCSWSignature)!= CBW_SIG)
// 	{ 
// 		pr_err(" Invalid CSW signature  \n"); 
// 		return ; 
// 	} 
//  

       	/* Submiting  csw   urb */ 
	usb_fill_bulk_urb(dev->csw_urb , dev->udev , usb_rcvbulkpipe(dev->udev , dev->bulk_in_endpointaddr) , dev->csw_buffer ,  CSW_LEN , csw_callback, dev ) ;   
	int csw_rtv = usb_submit_urb(dev->csw_urb, GFP_ATOMIC);
	if(csw_rtv) 
	{

		pr_info("csw_urb : usb_submit_urb() error\n"); 
		return ; 
	} 		
	
	return ; 


}


/* csw_callback function */ 
static void csw_callback(  struct urb *urb ) 
{ 
	struct my_usb_storage  *dev  = urb->context ; 
	struct scsi_cmnd *scmd  = dev->active_scmd ; 

	if( urb->status) 
	{ 
		pr_err(" data_urb  error \n"); 
	}

	struct command_status_wrapper  *csw = (struct command_status_wrapper *) urb->transfer_buffer; 

	if(  le32_to_cpu(csw->dCSWSignature)!= CBW_SIG)
	{ 
		pr_err(" Invalid CSW signature  \n"); 
		return ; 
	} 


	scsi_done(scmd); 
	dev->active_scmd = NULL; 

        	
	return ; 


}
/* usb_disconnect function */
static void  usb_disconnect( struct usb_interface *interface ) 
{
        struct Scsi_Host *host =  usb_get_intfdata(interface); 
	if( host ) 
	{ 
		struct my_usb_storage  *dev ; 
		dev = shost_priv(host) ; 
		scsi_remove_host(host); 
		usb_set_intfdata(interface , NULL) ; 
		
		dev->cbw_tag = 0; 
		if(deallocate_usb_resource(dev))
		{ 
			dev_err(&interface->dev, " Resource deallocation failed \n"); 
		        return ; 
		} 
		/* Clearing  endpoints */	
		usb_clear_halt(dev->udev , usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointaddr));
		usb_clear_halt(dev->udev , usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointaddr)); 	
	
		usb_put_dev(dev->udev); 

	
		scsi_host_put(host); 
	} 

	dev_info(&interface->dev , " USB  disconnected \n"); 
	return ; 
} 


/* USB  resource allocation function */ 
int allocate_usb_resource( struct my_usb_storage *dev) 
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

	 /*Allocating data  buffer */ 
	 dev->data_urb = usb_alloc_urb(0, GFP_KERNEL) ; 
	 if( dev->data_urb ==NULL) 
	 { 
		 pr_info(" data_urb alloc error \n"); 
		 return -ENOMEM ; 
	 } 
	 dev->data_buffer = kmalloc(CBW_LEN , GFP_KERNEL) ; 
	 if(dev->data_buffer ==NULL ) 
	 { 
		 pr_info("data_buffer alloc error \n"); 
		 return -ENOMEM ;
	 } 
	 dev_info(&dev->intf->dev , "USB  Resource allocation success \n");
	 return 0 ; 
} 


/* USB  resource allocation  cleanup function */ 
int  deallocate_usb_resource( struct my_usb_storage *dev) 
{ 

	if(dev->cbw_urb && dev->cbw_buffer) 
	{ 
		usb_kill_urb(dev->cbw_urb) ;
		usb_free_urb(dev->cbw_urb) ; 
		dev->cbw_urb = NULL ; 
		kfree(dev->cbw_buffer); 
		dev->cbw_buffer= NULL; 
	}else{ 
	       return -1 ; 
	} 	       

	if(dev->csw_urb && dev->csw_buffer) 
	{ 
		usb_kill_urb(dev->csw_urb); 
		usb_free_urb(dev->csw_urb); 
		dev->csw_urb =NULL ; 

		kfree(dev->csw_buffer); 
		dev->csw_buffer = NULL ; 
	}else { 
		return -1; 
	} 



	if(dev->data_urb && dev->data_buffer) 
	{ 
		usb_kill_urb(dev->data_urb); 
		usb_free_urb(dev->data_urb); 
		dev->data_urb =NULL ; 

		kfree(dev->data_buffer); 
		dev->data_buffer = NULL ; 
	}else { 
		return -1; 
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


module_init(usb_init); 
module_exit(usb_exit); 
MODULE_LICENSE("GPL"); 
MODULE_DESCRIPTION(" A simple USB driver "); 
MODULE_AUTHOR("Johan-liebert") ; 

