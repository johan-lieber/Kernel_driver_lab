
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
#define CSW_SIG 0x53425355
#define CBW_SIG 0x43425355 
#define CSW_LEN  13 
#define CBW_LEN 31 
#define DATA_LEN  512

/* command_block_wrapper struct */ 
struct command_block_wrapper { 
	__le32  dCBWSignature ; 
	__le32  dCBWTag ; 
	__le32  dCBWDataTransferLength ; 
	__u8 bmCBWFlags ; 
	__u8 bCBWLUN; 
	__u8 bCBWCBLength ; 
	__u8 CBWCB[16] ; 
}__attribute__((packed)) ; 
/* command status wrapper struct */ 
struct command_status_wrapper { 
	__le32 dCSWSignature ; 
	__le32 dCSWTag ; 
	__le32 dCSWDataResidue; 
	__u8 bCSWStatus ; 
}__attribute__((packed)) ;

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
        dma_addr_t cbw_dma ; 
	dma_addr_t csw_dma ; 
	dma_addr_t data_dma ; 
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
int  usb_eh_abort( struct scsi_cmnd *cmd); 
int usb_eh_dev_reset( struct scsi_cmnd *cmd); 
int usb_eh_bus_reset( struct scsi_cmnd *cmd); 
int usb_eh_host_reset( struct scsi_cmnd *cmd); 


/* scsi host template */ 
static struct scsi_host_template  my_sht ={
       .name= " usb-storage",
	.queuecommand = queue_command ,
       .can_queue = 1 , 
	.this_id  = -1 , 
     	.sg_tablesize = -1 , 
	.max_sectors = 240, 
	.cmd_per_lun = 1 ,
        .eh_abort_handler = usb_eh_abort, 
	.eh_device_reset_handler= usb_eh_dev_reset , 
 	.eh_bus_reset_handler  = usb_eh_bus_reset, 
	.eh_host_reset_handler = usb_eh_host_reset , 

 	}; 

/*  Usb driver  structure */ 
static struct usb_driver   exmp_usb_driver = { 
	.name = "usb-storage-meow" , 
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
	struct usb_host_interface  *iface_desc = interface->cur_altsetting ; 

	host = scsi_host_alloc(&my_sht, sizeof( struct my_usb_storage)) ; 
	if(!host) 
	{ 
		dev_err(&interface->dev , " scsi_host_alloc() error\n"); 
        	goto r_host ; 
	} 

	dev = shost_priv(host); 
	dev->udev = usb_get_dev(interface_to_usbdev(interface)); 
	dev->intf = interface ; 
	usb_set_intfdata(interface , host); 
	dev->cbw_tag = 1 ; 
	if(!dev ||  !iface_desc)  
	{
	       dev_err(&interface->dev," No dev && iface_desc found  \n"); 
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

	dev_info(&interface->dev , "Bulk_in_endpoint  [0x%02x]\n" ,dev->bulk_in_endpointaddr); 
	dev_info(&interface->dev , "Bulk_out_endpoint [0x%02x]\n",dev->bulk_out_endpointaddr); 

	
	/* Adding host to scsi */ 
	if( scsi_add_host(host, &interface->dev))
	{
		dev_err(&interface->dev , " scsi_add_host() error\n");
	        goto  r_scsi ; 	
	} 

	scsi_scan_host(host); 

	dev_info(&interface->dev , " USB  device attached \n"); 
	return 0; 

r_host: 
	if(host)
	{	
		scsi_remove_host(host); 
		if(deallocate_usb_resource(dev)) 
		{ 
			dev_err(&dev->intf->dev, " error in init_protocol cleanup \n"); 
		}  		
			scsi_host_put(host);  
			return -1 ;	
	} 
			

r_scsi:
       		if(host) 
		{	
			scsi_remove_host(host); 
			if(deallocate_usb_resource(dev)) 
			{ 
				dev_err(&dev->intf->dev, " error in init_protocol cleanup \n"); 
			}  		
			scsi_host_put(host);  
			return -1 ;
		} 
		return -1 ;
} 



/* queue_command function */ 
static int queue_command ( struct Scsi_Host *host , struct scsi_cmnd *scmd ) 
{ 
	struct my_usb_storage *dev = shost_priv(host); 
	//unsigned char *cdb  = scmd->cmnd ; 
        	
	int direction  = scmd->sc_data_direction;
	unsigned int bufflen  = scsi_bufflen(scmd); 

	/* Setting value to custom struct my_usb_storage */ 
	dev->bufferlength = bufflen ;
        dev->direction = direction ; 
	dev->active_scmd = scmd ;  	

	if (dev->bufferlength ) 
	{ 
		 dev->data_buffer = usb_alloc_coherent(dev->udev, dev->bufferlength  , GFP_KERNEL, &dev->data_dma); 
		 if(dev->data_buffer ==NULL ) 
		 { 
			 pr_info("data_buffer usb_alloc_coheret() error \n"); 
			  return -ENOMEM ;
		 } 
	} 

	memset(&dev->cbw , 0 , CBW_LEN);
	dev->cbw.dCBWSignature = cpu_to_le32(CBW_SIG); 
	dev->cbw.dCBWTag = cpu_to_le32(dev->cbw_tag++); 
	dev->cbw.dCBWDataTransferLength  = cpu_to_le32(bufflen); 
	dev->cbw.bmCBWFlags=  (  direction ==DMA_FROM_DEVICE) ? 0x80 : 0x00 ; 
	dev->cbw.bCBWLUN =0 ; 
	dev->cbw.bCBWCBLength =min_t(u8 , scmd->cmd_len, sizeof(dev->cbw.CBWCB)); 

	if( !dev->cbw_buffer ||  !dev->cbw_urb)  
	{ 
		dev_err(&dev->intf->dev , "Resource not allocated \n"); 
		return -ENOMEM  ;
	} 

	
	memcpy( dev->cbw.CBWCB ,  scmd->cmnd , dev->cbw.bCBWCBLength);
        memcpy( dev->cbw_buffer , &dev->cbw , CBW_LEN); 	

	pr_info(" CBW-----------------------------\n"); 
	pr_info("dCBWSignature		:0x%08x\n", le32_to_cpu(dev->cbw.dCBWSignature)); 
	pr_info("dCBWTag		:0x%08x\n",le32_to_cpu(dev->cbw.dCBWTag)); 
	pr_info("dCBWDataTranferLength  :%u\n",le32_to_cpu(dev->cbw.dCBWDataTransferLength)); 
	pr_info("dCBWFlags 		:0x%02x (%s)\n", dev->cbw.bmCBWFlags,(dev->cbw.bmCBWFlags & 0x80)? "DATA-IN":"DATA-OUT"); 
	pr_info("bCBWLUN 		:%u\n", dev->cbw.bCBWLUN); 
	pr_info("bCBWCBLength 		:%u\n", dev->cbw.bCBWCBLength);
	pr_info(" CBWCB			:	"); 
	for( int i = 0 ; i < dev->cbw.bCBWCBLength ; i++) 
	{
		pr_cont("%02x ", dev->cbw.CBWCB[i]);
		pr_cont("\n"); 
	} 



	usb_fill_bulk_urb(dev->cbw_urb , dev->udev , usb_sndbulkpipe(dev->udev , dev->bulk_out_endpointaddr), dev->cbw_buffer, CBW_LEN  , cbw_callback, dev) ;

	/* Submitting  DMA  buffer */	
  	dev->cbw_urb->transfer_dma = dev->cbw_dma ; 
	dev->cbw_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP ; 

 	int cbw_urb_rtv = usb_submit_urb(dev->cbw_urb , GFP_KERNEL); 
	if( cbw_urb_rtv) 
	{
		pr_info(" cbw_urb : usb_submit_urb() error :%d\n", cbw_urb_rtv); 
		
		scmd->result =  (DID_ERROR << 16 ) ;
	       	scsi_done(dev->active_scmd); 
	       	return -ENOMEM ; 
	}
      	
        dev_info(&dev->intf->dev,  " CBW send ---->> \n"); 	
	return 0 ; 
} 


/*  cbw_callback function */ 
static void  cbw_callback( struct urb *urb ) 
{ 
	struct my_usb_storage  *dev  = urb->context ; 
        dev_info(&dev->intf->dev, " Reached data_callback() \n"); 	
	struct command_block_wrapper  *cbw = (struct command_block_wrapper *) urb->transfer_buffer; 
	
	if( urb->status  < 0 ) 
	{
		dev_err(&dev->intf->dev, " Bad cbw submission:%d\n", urb->status );
	        dev->active_scmd->result =  ( DID_ERROR << 16 ) ; 
	        scsi_done(dev->active_scmd); 
      		return ; 	       
	}


	if(  le32_to_cpu(cbw->dCBWSignature)!= CBW_SIG)
	{ 
		pr_err(" Invalid CBW signature  \n"); 
		return ; 
	} 
 
		

	if(!dev) 
	{ 
		pr_info(" no dev \n"); 
		return ; 
	}

        dev_info(&dev->intf->dev, "  submitting urbs ---- \n"); 
	pr_info(" buffer  length is :%d \n", dev->bufferlength ); 
	if( !dev->data_buffer) 
	{ 
		pr_info(" no data_buffer \n"); 
	} 

	pr_info(" data urb setup : dir=%d , ep=0x%02x  , len=%d\n", dev->direction , dev->bulk_in_endpointaddr, dev->bufferlength); 
	/* Submitting  data_urb and   csw_urb  if condition  value  gets less then zero   */ 	
	if( dev->bufferlength > 0 ) 
	{
		 pr_info("  bufferlength is greater > 0 =:%d\n", dev->bufferlength); 

		if(dev->direction == DMA_FROM_DEVICE) 
		{
		       pr_info(" DMA_FROM_DEVICE"); 

usb_fill_bulk_urb(dev->data_urb , dev->udev , usb_rcvbulkpipe(dev->udev , dev->bulk_in_endpointaddr) ,dev->data_buffer , dev->bufferlength , data_callback, dev ) ; 
			
		  	dev->data_urb->transfer_dma = dev->data_dma ; 
			dev->data_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP ; 


			int  ret = usb_submit_urb(dev->data_urb , GFP_ATOMIC);
			if(ret ) 
			{
			        dev_err(&dev->intf->dev , " data_urb usb_submit_urb() error %d \n", ret ); 	
				dev->active_scmd->result = ( DID_ERROR << 16 ) ; 
				scsi_done(dev->active_scmd) ; 
				return ; 
			} 
		} 

		if(dev->direction == DMA_TO_DEVICE) 
		{
		       pr_info(" DMA-TO-DEVICE \n");	
			
usb_fill_bulk_urb(dev->data_urb , dev->udev , usb_sndbulkpipe(dev->udev , dev->bulk_out_endpointaddr) ,dev->data_buffer , dev->bufferlength , data_callback, dev ) ; 
			
  			dev->data_urb->transfer_dma = dev->data_dma ; 
			dev->data_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP ; 


			int retvalue = usb_submit_urb(dev->data_urb , GFP_ATOMIC); 
			if( retvalue) 
			{ 
			        dev_err(&dev->intf->dev , "data_urb: usb_submit_urb() error:%d \n", retvalue ); 	
				dev->active_scmd->result = ( DID_ERROR << 16 ) ; 
			 	scsi_done(dev->active_scmd) ; 
				return ; 
			}
		} 
	}else{
	       pr_info("  direcly submitting csw now \n"); 	
		/* Submiting  csw   urb */ 
	usb_fill_bulk_urb(dev->csw_urb , dev->udev , usb_rcvbulkpipe(dev->udev , dev->bulk_in_endpointaddr) , dev->csw_buffer ,  CSW_LEN , csw_callback, dev ) ;   
		
  		dev->csw_urb->transfer_dma = dev->csw_dma ; 
		dev->csw_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP ; 


		int csw_rtv = usb_submit_urb(dev->csw_urb, GFP_ATOMIC);
		if(csw_rtv) 
		{
			dev_err(&dev->intf->dev , "csw_urb: usb_submit_urb() error:%d \n", csw_rtv );  /* rtv  = return value */  	
			dev->active_scmd->result = ( DID_ERROR << 16 ) ; 
			scsi_done(dev->active_scmd); 	
			return ; 
		} 

	}
	return ; 
}


/* Data_callback function */ 
static void data_callback(  struct urb *urb ) 
{
	struct my_usb_storage  *dev  = urb->context ;
        dev_info(&dev->intf->dev, " Reached data_callback() \n"); 	
        	

	if( urb->status < 0 ) 
	{ 
                dev_err(&dev->intf->dev, "Bad data_urb submission %d\n", urb->status );  
		dev->active_scmd->result = (DID_ERROR >> 16 ) ; 
		scsi_done(dev->active_scmd); 
	} 

       	/* Submiting  csw   urb */ 
	usb_fill_bulk_urb(dev->csw_urb , dev->udev , usb_rcvbulkpipe(dev->udev , dev->bulk_in_endpointaddr) , dev->csw_buffer ,  CSW_LEN , csw_callback, dev ) ;   
	
  	dev->csw_urb->transfer_dma = dev->csw_dma ; 
	dev->csw_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP ; 


	int csw_rtv = usb_submit_urb(dev->csw_urb, GFP_ATOMIC);
	if(csw_rtv) 
	{
                dev_err(&dev->intf->dev, " csw_urb : usb_submit_urb() error :%d \n", csw_rtv ); /*  csw_rtv    mean  csw_returnvalue in short */ 
		dev->active_scmd->result = (DID_ERROR << 16 ) ; 
	        scsi_done(dev->active_scmd); 	
		return ; 
	} 		
	
	return ; 


}


/* csw_callback function */ 
static void csw_callback(  struct urb *urb ) 
{ 
	struct my_usb_storage  *dev  = urb->context ; 
	struct scsi_cmnd *scmd  = dev->active_scmd ; 


	if( urb->status < 0) 
	{ 
		dev_err(&dev->intf->dev," Bad data_urb submission :%d\n", urb->status) ;
	        scmd->result = ( DID_ERROR << 16);
		scsi_done(dev->active_scmd); 
 	}

	struct command_status_wrapper  *csw = (struct command_status_wrapper *) urb->transfer_buffer; 

	if(  le32_to_cpu(csw->dCSWSignature)!= CSW_SIG)
	{ 
		pr_err(" Invalid CSW signature  \n");
	        dev->active_scmd->result = (DID_ERROR << 16 )  ; 
		scsi_done(dev->active_scmd); 	
		return ; 
	}

	switch( csw->dCSWSignature) 
	{ 
		case 0:
			dev->active_scmd = ( DID_OK <<16 ) | SAM_STAT_GOOD; 
			 break ; 
		case 1:
			 dev->active_scmd->result = ( DID_ERROR << 16 ) | SAM_STAT_CHECK_CONDITION ; 
			 break ; 
		case 2 :
			 dev->active_scmd->result = ( DID_ERROR << 16 ) | SAM_STAT_CHECK_CONDITION; 
			 break; 
		default:
			 dev->active_scmd->result = ( DID_ERROR << 16); 
			 break ; 
	} 

				


	dev->active_scmd->result =  (  DID_OK  << 16 ) |  SAM_STAT_GOOD; 
	scsi_done(scmd); 
	dev->active_scmd = NULL; 

        	
	return ; 


}


int  usb_eh_abort( struct scsi_cmnd *cmd) 
{ 
	pr_info("  usb_eh_abort () \n"); 
	return 0 ; 
} 


int usb_eh_dev_reset( struct scsi_cmnd *cmd) 
{ 
	pr_info(" usb_eh_dev_reset() \n"); 
	return  0 ; 
} 

int usb_eh_bus_reset( struct scsi_cmnd *cmd) 
{ 
	pr_info("usb_eh_bus_rest() \n"); 
	return 0 ; 
} 

int usb_eh_host_reset( struct scsi_cmnd *cmd) 
{ 
	pr_info("usb_eh_host_rest() \n"); 
 	return 0 ; 
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
	 dev->cbw_buffer = usb_alloc_coherent(dev->udev, CBW_LEN , GFP_KERNEL, &dev->cbw_dma); 
	 if( !dev->cbw_buffer) 
	 { 
		 pr_info(" cbw_buffer usb_alloc_coherent() errror \n"); 
		 goto r_cbw ; 
	 } 
	 /*Allocating CSW */ 
	 dev->csw_urb = usb_alloc_urb(0, GFP_KERNEL) ; 
	 if( dev->csw_urb ==NULL) 
	 { 
		 pr_info(" csw_urb alloc error \n"); 
		 return -ENOMEM ; 
	 } 
	 dev->csw_buffer = usb_alloc_coherent(dev->udev, CSW_LEN , GFP_KERNEL, &dev->csw_dma); 
	 if(dev->csw_buffer ==NULL ) 
	 { 
		 pr_info("csw_buffer usb_alloc_coherent() error \n"); 
		 goto r_csw ; 
	 } 

	 /*Allocating data  buffer */ 
	 dev->data_urb = usb_alloc_urb(0, GFP_KERNEL) ; 
	 if( dev->data_urb ==NULL) 
	 { 
		 pr_info(" data_urb alloc error \n"); 
		 return -ENOMEM ; 
	 } 
	 dev_info(&dev->intf->dev , "USB  Resource allocation success \n");
	 return 0 ;

/*  Cleanups */
r_cbw: 
	if( dev->cbw_urb || dev->cbw_buffer) 
	 {
		 if( dev->cbw_urb) 
		 { 
			 usb_kill_urb(dev->cbw_urb); 
			 usb_free_urb(dev->cbw_urb); 
			 dev->cbw_urb= NULL ; 	
		 } 
		 if( dev->cbw_buffer) 
		 { 
			 usb_free_coherent(dev->udev, CBW_LEN , dev->cbw_buffer , dev->cbw_dma); 
			 dev->cbw_buffer = NULL ; 
		 } 
	  return -1 ; 
	 } 

r_csw: 

	if( dev->csw_urb || dev->csw_buffer) 
	 {
		 if( dev->csw_urb) 
		 { 
			 usb_kill_urb(dev->csw_urb); 
			 usb_free_urb(dev->csw_urb); 
			 dev->csw_urb= NULL ; 	
		 } 
		 if( dev->csw_buffer) 
		 { 
			 usb_free_coherent(dev->udev, CSW_LEN , dev->csw_buffer , dev->csw_dma); 
			 dev->csw_buffer = NULL ; 
		 } 
	  return -1 ;
	 } 

	return -1 ; 
}


/* USB  resource allocation  cleanup function */ 
int  deallocate_usb_resource( struct my_usb_storage *dev) 
{ 

	if(dev->cbw_urb && dev->cbw_buffer) 
	{ 
		usb_kill_urb(dev->cbw_urb) ;
		usb_free_urb(dev->cbw_urb) ;
	    	usb_free_coherent(dev->udev, CBW_LEN , dev->cbw_buffer, dev->cbw_dma); 	
		dev->cbw_urb = NULL ;
		dev->cbw_buffer= NULL; 
	 
	} 	       

	if(dev->csw_urb && dev->csw_buffer) 
	{ 
		usb_kill_urb(dev->csw_urb); 
		usb_free_urb(dev->csw_urb); 
	    	usb_free_coherent(dev->udev, CSW_LEN , dev->csw_buffer, dev->csw_dma); 	
		dev->csw_urb =NULL ; 
		dev->csw_buffer = NULL ; 
	}


	if(dev->data_urb && dev->data_buffer) 
	{ 
		usb_kill_urb(dev->data_urb); 
		usb_free_urb(dev->data_urb); 
	    	usb_free_coherent(dev->udev, DATA_LEN , dev->data_buffer, dev->data_dma); 	
		dev->data_urb =NULL ; 
		dev->data_buffer = NULL ; 
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

