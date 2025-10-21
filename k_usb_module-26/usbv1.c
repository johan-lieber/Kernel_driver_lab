
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
#include <linux/mutex.h>

/* macrocs */
#define CSW_SIG 0x53425355
#define CBW_SIG 0x43425355 
#define CSW_LEN  13 
#define CBW_LEN 31 
#define DATA_LEN  512
#define MAX_FAIL_THRSHHOLD 2 
#define INQUIRY 0x12 

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

	/* scsi needed members */ 
	struct command_block_wrapper cbw ;	
	struct command_status_wrapper csw ;
	unsigned int bufferlength ; 
	unsigned  int direction ; 
	struct scsi_cmnd *active_scmd ; 	
	struct mutex    usb_lock ; 
  	unsigned int flag ; 	
	
	/* endpoints */ 
	__u8 bulk_in_endpointaddr ; 
	__u8 bulk_out_endpointaddr ; 
	__le32  cbw_tag ;
        dma_addr_t cbw_dma ; 
	dma_addr_t csw_dma ; 
	dma_addr_t data_dma ; 
	dma_addr_t sec_dma ;
        unsigned int count ; 	
	
	/* CBW */ 
	struct urb *cbw_urb; 	
	unsigned char *cbw_buffer ; 
	/* second cbw */ 
	struct urb *sec_urb ; 
	unsigned char *sec_buffer ;
	/* CSW  */ 
	struct urb *csw_urb ; 
	unsigned char *csw_buffer ; 
	/* data  buffer */ 
	struct urb *data_urb ; 
	unsigned  char *data_buffer; 
}; 

/* faling inquiey response */ 
static const unsigned char fake_inquiry_response[36] = { 
	0x00,
	0x80,
	0x05,
	0x02,
	0x1F,
	0x00,0x00,0x00,	
	'U','S','B',' ',' ','S','A','N',
	'D','I','S','K',' ',' ','1',':','1',' ',' ',' ',' ',' ',' ',' ',
	'0','0','0','1',
	}; 
static const unsigned char vpd_page_00[4]= 
	{ 
		0x00,
		0x00,
		0x00,
		0x00
	}; 
static const unsigned char no_sense_response[18] = { 
	0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x0A, 
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00 
	};



/* usb_device_id table */ 
const struct  usb_device_id usb_table [] = { 
	{ USB_INTERFACE_INFO( USB_CLASS_MASS_STORAGE , USB_SC_SCSI , USB_PR_BULK ) }, 
	{}, 
}; 
MODULE_DEVICE_TABLE(usb, usb_table); 

/***************************** USB function prototypes *********************************/ 
/*usb driver function */
static int usb_probe( struct usb_interface *interface ,  const struct usb_device_id  *id) ; 
static void  usb_disconnect( struct  usb_interface *interface); 

/*scsi  functions prototypes */ 
static int queue_command ( struct Scsi_Host *host , struct scsi_cmnd *scmd );
static void  cbw_callback( struct urb *urb ); 
static void csw_callback ( struct urb *urb ); 
static void data_callback ( struct urb *urb); 
int usb_pre_reset(struct usb_interface *iface);
int usb_post_reset(struct usb_interface *iface); 
int  usb_eh_abort( struct scsi_cmnd *cmd); 
int usb_eh_dev_reset( struct scsi_cmnd *cmd); 
int usb_eh_bus_reset( struct scsi_cmnd *cmd); 
int usb_eh_host_reset( struct scsi_cmnd *cmd); 

/* Utility functions prototypes */ 
void print_info( struct my_usb_storage *dev) ; 
void  sec_workfunction( struct work_struct *work) ;
int  clear_inflight_urb(struct my_usb_storage *dev) ;
int  deallocate_usb_resource( struct my_usb_storage *dev); 
int allocate_usb_resource( struct my_usb_storage *dev) ;
static int slave_alloc(struct scsi_device *dev, struct queue_limits *queue ); 


/* scsi host template */ 
static struct scsi_host_template  my_sht ={
       .name= " usb-storage",
	.queuecommand = queue_command ,
        .can_queue = 1 , 
	.this_id  = -1 , 
     	.sg_tablesize = -1 , 
	.max_sectors = 240, 
	.cmd_per_lun = 1 ,
	.sdev_configure = slave_alloc,
        .eh_abort_handler = usb_eh_abort, 
	.eh_device_reset_handler= usb_eh_dev_reset , 
 	.eh_bus_reset_handler  = usb_eh_bus_reset, 
	.eh_host_reset_handler = usb_eh_host_reset , 

 	}; 

/*  Usb driver  structure */ 
static struct usb_driver   exmp_usb_driver = { 
	.name = "usb-storage-meow" , 
	.probe = usb_probe , 
	.pre_reset = usb_pre_reset, 
	.post_reset = usb_post_reset,
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

	/* usb resource allocation */ 
	if(allocate_usb_resource(dev)) 
	{ 
		dev_err(&dev->intf->dev," allocate_usb_resource() error\n"); 
		return -ENOMEM ; 
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
        
	/*  Initializing   mutex lock */ 
	mutex_init(&dev->usb_lock);	


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



/**************************************   SCSI NEEDED FUNCTIONS  *************************************/

/* queue_command function */ 
static int queue_command ( struct Scsi_Host *host , struct scsi_cmnd *scmd ) 
{  	
	struct my_usb_storage *dev = shost_priv(host); 
	int direction  = scmd->sc_data_direction;
	unsigned int bufflen  = scsi_bufflen(scmd);

	if(!dev) 
	{ 
		pr_info("queue_command() error no dev \n");
	        return -ENODEV ; 
	} 
	dev_info(&dev->intf->dev,"queue_command()...\n"); 

	/* Setting value to custom struct my_usb_storage */ 
	dev->bufferlength = bufflen ;
        dev->direction = direction ; 
	dev->active_scmd = scmd ;  		

	if (dev->bufferlength ) 
	{	
 		dev->data_buffer = usb_alloc_coherent(dev->udev,   bufflen   , GFP_KERNEL, &dev->data_dma);
	 	dev->data_buffer = kmalloc(bufflen , GFP_KERNEL) ; 
	 	if(dev->data_buffer ==NULL ) 
	 	{ 
			 pr_info("data_buffer usb_alloc_coheret() error \n"); 
		  	return -ENOMEM ;
	 	}	 
	}

	if(dev->count == 1 ) 
	{
	       dev_info(&dev->intf->dev,"queue_command() using sec cbw \n");	
	} 

	/* Wrapping cbd  into cbw */
	memset(&dev->cbw , 0 , CBW_LEN);
	dev->cbw.dCBWSignature = cpu_to_le32(CBW_SIG); 
	dev->cbw.dCBWTag = cpu_to_le32(dev->cbw_tag++); 
	dev->cbw.dCBWDataTransferLength  = cpu_to_le32(bufflen); 
	dev->cbw.bmCBWFlags=  (  direction ==DMA_FROM_DEVICE) ? 0x80 : 0x00 ; 
	dev->cbw.bCBWLUN =0 ; 
	dev->cbw.bCBWCBLength =min_t(u8 , scmd->cmd_len, sizeof(dev->cbw.CBWCB)); 

	if( !dev->cbw_buffer ||  !dev->cbw_urb)  
	{ 
		dev_err(&dev->intf->dev , " queue_command () Resource not allocated \n"); 
		return -ENOMEM  ;
	} 
	
	/* Changing first urb   to second urb
	 * for clean urb submission   */	
	if ( dev->count == 0 ) 
	{ 	
		memcpy( dev->cbw.CBWCB ,  scmd->cmnd , dev->cbw.bCBWCBLength);
        	memcpy(  dev->cbw_buffer  ,&dev->cbw, CBW_LEN); 	
	}else {
 		memcpy( dev->cbw.CBWCB ,  scmd->cmnd , dev->cbw.bCBWCBLength);
        	memcpy(  dev->sec_buffer  ,&dev->cbw, CBW_LEN); 	
	}

	/* Calling print_info functions to print infos  */
        print_info(dev); 	
	
	/* filling struct urb  for urb submission */
	if(dev->count ==0 ) 
	{ 

		usb_fill_bulk_urb(dev->cbw_urb, dev->udev , usb_sndbulkpipe(dev->udev , dev->bulk_out_endpointaddr), dev->cbw_buffer , CBW_LEN  , cbw_callback, dev) ;
	}else 
	{
		usb_fill_bulk_urb(dev->sec_urb, dev->udev , usb_sndbulkpipe(dev->udev , dev->bulk_out_endpointaddr), dev->sec_buffer , CBW_LEN  , cbw_callback, dev) ;
	} 

	/* Dma buffer  for urb submission  */
	if(dev->count ==  0) 
	{ 
  		dev->cbw_urb->transfer_dma = dev->cbw_dma ; 
		dev->cbw_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP ; 
	}else{
		dev->sec_urb->transfer_dma = dev->sec_dma ; 
		dev->sec_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP ; 
	}

	if(dev->count ==0 ) 
	{ 

 		int cbw_urb_rtv = usb_submit_urb(dev->cbw_urb , GFP_ATOMIC); 
		if( cbw_urb_rtv) 
		{
			pr_info(" cbw_urb : usb_submit_urb() error :%d\n", cbw_urb_rtv); 
			scmd->result =  (DID_ERROR << 16 ) ;
		       	scsi_done(dev->active_scmd); 
		       	return -ENOMEM ; 
		}
        	scmd->result = SAM_STAT_GOOD ; 	
        	dev_info(&dev->intf->dev,  " queue_command() cbw send... \n"); 	
	}else{
		int cbw_urb_rtv = usb_submit_urb(dev->sec_urb , GFP_ATOMIC); 
		if( cbw_urb_rtv) 
		{
			pr_info(" sec_urb : usb_submit_urb() error :%d\n", cbw_urb_rtv); 		
			scmd->result =  (DID_ERROR << 16 ) ;
		       	scsi_done(dev->active_scmd); 
		       	return -ENOMEM ; 
		}
		
		/* sendin success code to scsi layer */
        	scmd->result = SAM_STAT_GOOD ; 	
        	dev_info(&dev->intf->dev,  " queue_command() vpd send... \n"); 	
	
	} 
	return 0 ; 
} 




/*  cbw_callback function  */ 
static void  cbw_callback( struct urb *urb ) 
{
	/* Extracting dev from urb->contect 
	 * from previous urb filling  and
	 * other info from  dev */ 
	struct my_usb_storage  *dev  = urb->context ; 
	struct scsi_data_buffer *sdb = &dev->active_scmd->sdb ;
        unsigned char *buf  = sg_virt(sdb->table.sgl); 
	unsigned int len = min(sdb->length , dev->bufferlength); 	
	
	if(!dev) 
	{ 
		pr_info("cbw_callback() error no dev \n"); 
		return ; 
	}
	
	dev_info(&dev->intf->dev, "cbw_callback()... \n"); 	

	if( !dev->data_buffer || !dev->data_urb) 
	{ 
		pr_info(" cbw_callback() no data_buffer \n"); 
	}

	dev_info(&dev->intf->dev,"csw_urb status				:%d", urb->status);
	dev_info(&dev->intf->dev,"csw_urb actual length 			:%d", urb->actual_length);

	dev_info(&dev->intf->dev," data_direction			:%d", dev->direction);
	dev_info(&dev->intf->dev," Endpoints addr			:0x%02x",dev->direction ==DMA_FROM_DEVICE ? dev->bulk_in_endpointaddr:dev->bulk_out_endpointaddr);
	dev_info(&dev->intf->dev,"Buffer length				:%d", dev->bufferlength ); 

	/*   type casting  cbw from  urb->transfer_buffer */ 
	struct command_block_wrapper  *cbw = (struct command_block_wrapper *) urb->transfer_buffer; 

	/*  Checking for previos  urb  submission   status for further  urb submission */
	if( urb->status  < 0 ) 
	{
		dev_err(&dev->intf->dev, "cbw_callback()  Bad cbw submission:%d\n", urb->status ); 
		usb_clear_halt(dev->udev, usb_sndbulkpipe(dev->udev, dev->bulk_in_endpointaddr)); 
	        dev->active_scmd->result =  ( DID_ERROR << 16 ) ; 
	        scsi_done(dev->active_scmd); 
      		return ; 	       
	}
	
	if(  le32_to_cpu(cbw->dCBWSignature)!= CBW_SIG)
	{ 
		pr_err("cbw_callback() Invalid CBW signature  \n"); 
		return ; 
	}


	/*  Checking  which command scsi_scmd *scmd  provided */ 
	switch(cbw->CBWCB[0])  
	{ 
	
case 0x12 : /* Inquiry command */ 
		dev_info(&dev->intf->dev ,"--INQUIRY--\n"); 
	
		/* Checking if it  is again asking for ( Inquiry VPD)  command 
		 * sometimes   scsi layer asks for it and  we need to provide 
		 * either by providing or faking  in case of no response from
		 *  usb-device	*/ 

   		if(cbw->CBWCB[1] && 0x01) 
		{  
		        pr_info("INQUIRY EVPD\n");	
		         memcpy(buf, vpd_page_00, min(len  , sizeof(vpd_page_00))); 
		}else { 
	        	 memcpy(buf, fake_inquiry_response, len ); 
		}

		/* Letting know the scsi layer   of the status */ 
		scsi_set_resid(dev->active_scmd, sdb->length - len);
		dev->active_scmd->result = SAM_STAT_GOOD;

		/* Trying to manully call this function callback 
		 * to manully trigger  csw tranaction for complete 
		 * bot protocol */ 

	 	pr_info("Calling data_callback manually \n");
	        dev->data_urb->actual_length = len; 
       		dev->data_urb->status = 0 ;
	        dev->data_urb->context = dev ; 	
		data_callback(dev->data_urb); 		
		return ; 	
		break ; 

case 0x00:  /* Test unit ready command  */ 

		dev_info(&dev->intf->dev , "-- TEST_UNIT_READY --\n");

		/* In case of  test unit ready   there is no  data phase 
		 * between the   usb-device and  scsi    so  we direcly info 
		 * the scsi layer  and submit the csw  direclt  after cbw  */

		scsi_set_resid(dev->active_scmd, 0);  /* no data returned */ 
		dev->active_scmd->result =  SAM_STAT_GOOD ;
		break; 
	
case 0x03: /* Request sense */ 
		
		dev_info(&dev->intf->dev , "-- REQUESR_SENSE --\n"); 
		memcpy(buf , no_sense_response , len); 
		scsi_set_resid(dev->active_scmd, sdb->length - len);
		dev->active_scmd->result = SAM_STAT_GOOD; 
		scsi_done(dev->active_scmd); 
		break ; 

case 0x25 : /* Read capacity */ 
	        dev_info(&dev->intf->dev," -- READ CAPACITY --\n");	
		break ; 

	default :
		 pr_info("cbw_callback() Unkown cbw command \n"); 
		 break ; 
	}   

	/* submitting data_urb or 
	 * csw urb based on the 
	 * direction */ 

	if( dev->bufferlength > 0 ) 
	{
		if(dev->direction == DMA_FROM_DEVICE) 
		{
		        dev_info(&dev->intf->dev , "Direction <- DMA_FROM_DEVICE\n"); 

//			usb_fill_bulk_urb(dev->data_urb , dev->udev , usb_rcvbulkpipe(dev->udev , dev->bulk_in_endpointaddr) , buf , len , data_callback, dev ) ; 
			
		  	dev->data_urb->transfer_dma = dev->data_dma ; 
			dev->data_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP ; 

//			int  ret = usb_submit_urb(dev->data_urb , GFP_ATOMIC);
//			if(ret ) 
//			{
//			        dev_err(&dev->intf->dev , " data_urb usb_submit_urb() error %d \n", ret ); 	
//				dev->active_scmd->result = ( DID_ERROR << 16 ) ; 
//				scsi_done(dev->active_scmd) ; 
//				return ; 
//			}
//		        dev->active_scmd->result = SAM_STAT_GOOD; 
				

			dev->count = 1 ; 
			dev_info(&dev->intf->dev , " cbw_callbackl() data_urb submitted... \n");
			return ;
		} 

		if(dev->direction == DMA_TO_DEVICE) 
		{
			
		        dev_info(&dev->intf->dev , "Direction -> DMA_TO_DEVICE\n"); 
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
		/* Some command do not have any data exchange phase 
		 * and in that case we have to submit the csw directly 
		 * skipping data  phase */ 

		dev_info(&dev->intf->dev, "cbw_callack() No direction specified \n");
	      	dev_info(&dev->intf->dev," cbw_callback() Submitting csw direclty...\n"); 
	 		 
		/* csw_urn submission  */ 
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

		dev_info(&dev->intf->dev,"cbw_calback() csw_urb submitted ...\n");

	}
	return ; 
} 




/* Data_callback function */ 
static void data_callback(  struct urb *urb ) 
{ 	
	struct my_usb_storage  *dev  = urb->context ;
        struct scsi_cmnd *scmd  = dev->active_scmd ;  	
	struct scsi_data_buffer *sdb = &dev->active_scmd->sdb ;
        unsigned char *buf  = sg_virt(sdb->table.sgl); 
	unsigned int len = min(sdb->length , dev->bufferlength); 	

	pr_info(" len in data_callback is :%d\n",len); 

	if(!dev) 
	{ 
		pr_info("data_callback() error no dev \n");
	        return  ; 
	} 


	dev_info(&dev->intf->dev," data_callback() \n");
        dev_info(&dev->intf->dev," data_callback actual_length :%d\n",urb->actual_length); 	

	if( urb->status < 0 ) 
	{ 
                dev_err(&dev->intf->dev, "data_callback() Bad dataurb submission %d\n", urb->status );  
		dev->active_scmd->result = (DID_ABORT << 16 ) ; 
		scsi_done(dev->active_scmd); 
	}
	
	if( scmd && urb->actual_length > 0 ) 
	{ 
		scsi_sg_copy_from_buffer( scmd , dev->data_buffer , urb->actual_length); 
	} 

	if( scmd)
	{ 
		unsigned int resid  =0 ; 
		if(dev->bufferlength > urb->actual_length ) 
		{ 
			resid = dev->bufferlength - urb->actual_length ; 
			scsi_set_resid(scmd , resid); 
	 		pr_info(" set_ scsu resid = %u\n", resid); 
		} 
	} 	


	dev_info(&dev->intf->dev," faking csw :( \n"); 
 	struct command_status_wrapper csw ; 
	csw.dCSWSignature = cpu_to_le32(CSW_SIG);
	csw.dCSWTag = cpu_to_le32(dev->cbw_tag) ;
	csw.dCSWDataResidue = cpu_to_le32(0); 
	csw.bCSWStatus = 0x00; 
	memcpy(dev->csw_buffer, &csw , sizeof(csw)); 
	dev->active_scmd->result =  (  DID_OK  << 16 ) |  SAM_STAT_GOOD; 
	dev->csw_urb->actual_length = CSW_LEN; 
       	dev->csw_urb->status = 0 ;
	dev->csw_urb->context = dev ; 
	csw_callback(dev->csw_urb); 
	pr_info(" donw donw ===============\n"); 	
     	
	return ; 		
       	
	/* csw submission  */ 
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

	dev->active_scmd->result = SAM_STAT_GOOD ; 
	scsi_done(dev->active_scmd); 
	dev_info(&dev->intf->dev," data_callback() csw sumitted ...\n");
	return ; 
}



/* csw_callback function */ 
static void csw_callback(  struct urb *urb ) 
{ 
	struct my_usb_storage  *dev  = urb->context ; 
	struct scsi_cmnd *scmd  = dev->active_scmd ; 
	
	if(!dev) 
	{ 
		pr_info("csw_callback() error no dev \n");
		return ; 
	} 
	dev_info(&dev->intf->dev,"csw_callback()\n"); 

	if( urb->status < 0) 
	{ 
		dev_err(&dev->intf->dev," csw_callback() Bad data_urb submission :%d\n", urb->status) ;
	        scmd->result = ( DID_ERROR << 16);
		scsi_done(dev->active_scmd); 
 	}

	struct command_status_wrapper  *csw = (struct command_status_wrapper *) urb->transfer_buffer; 

	if(  le32_to_cpu(csw->dCSWSignature)!= CSW_SIG)
	{ 
		pr_err(" csw_callback() Invalid CSW signature  \n");
	        dev->active_scmd->result = (DID_ERROR << 16 )  ; 
		scsi_done(dev->active_scmd); 	
		return ; 
	}
				


	dev->active_scmd->result =  (  DID_OK  << 16 ) |  SAM_STAT_GOOD; 
	scsi_done(scmd); 
	dev->active_scmd = NULL; 
        dev_info(&dev->intf->dev,"csw_callback() handshake finished \n");	
	return ; 


}


/* usb_eh_abort function */ 
int  usb_eh_abort( struct scsi_cmnd *scmd) 
{ 
	pr_info("usb_eh_abort() \n");
        struct Scsi_Host *host = scmd->device->host ; 	
	struct my_usb_storage *dev = shost_priv(host); 

	if(!dev) 
	{ 
		pr_info("usb_eh_abort() error no dev \n");
	        return -ENODEV ; 
	} 


	int ret ; 
	dev_info(&dev->intf->dev , "eh_abort: Aborting command \n"); 
	ret  = usb_clear_halt(dev->udev, usb_sndbulkpipe(dev->udev , dev->bulk_out_endpointaddr )); 
	if( ret) 
	{ 
		dev_err(&dev->intf->dev ,"eh_abort : failed to clear bulk out :%d\n", ret);
	}	
	ret =  usb_clear_halt(dev->udev, usb_rcvbulkpipe(dev->udev , dev->bulk_in_endpointaddr )); 		
	if( ret) 
	{ 
		dev_err(&dev->intf->dev ,"eh_abort : failed to clear bulk in  :%d\n", ret); 
	} 
	   
	if(dev->cbw_urb) 
	{ 
		usb_kill_urb(dev->cbw_urb); 
	} 

	if(dev->csw_urb) 
	{ 
		usb_kill_urb(dev->csw_urb); 
	} 

	if(dev->data_urb) 
	{ 
		usb_kill_urb(dev->data_urb); 
	} 

	scmd->result = (DID_ABORT <<16) ; 
	scsi_done(scmd); 

	return  SUCCESS ; 
} 



/* usb pre  resetting function */
int usb_pre_reset( struct usb_interface *iface ) 
{ 
        struct Scsi_Host *host = usb_get_intfdata(iface); 
	struct my_usb_storage *dev = shost_priv(host); 
	if(dev) 
	{ 
		dev_info(&dev->intf->dev," usb_pre_reset() \n"); 
	} 

	mutex_lock(&dev->usb_lock); 	
 	return 0; 
} 


/* Usb post resetting  function */ 
int usb_post_reset( struct usb_interface *iface) 
{ 
        struct Scsi_Host *host = usb_get_intfdata(iface); 
	struct my_usb_storage *dev = shost_priv(host);
	if(dev) 
	{ 
		dev_info(&dev->intf->dev," usb_post_reset() \n"); 
	} 


 	mutex_unlock(&dev->usb_lock); 	
		
	return 0 ; 
} 

/* usb_eh_dev_reset function */ 
int usb_eh_dev_reset( struct scsi_cmnd *scmd) 
{ 
        struct Scsi_Host  *host = scmd->device->host ; 
	struct my_usb_storage  *dev = shost_priv(host); 
	if(dev) 
	{ 
		dev_info(&dev->intf->dev," usb_eh_dev_reset() \n"); 
	} 


	int ret ; 
	ret = usb_control_msg(dev->udev, usb_sndctrlpipe(dev->udev, 0 ),0xFF, USB_TYPE_CLASS | USB_RECIP_INTERFACE , 0 , dev->intf->cur_altsetting->desc.bInterfaceNumber , NULL , 0, USB_CTRL_SET_TIMEOUT);
	if(ret < 0 ) 
	{ 
	 	dev_err(&dev->intf->dev, "BOBS  device reset failed %d\n", ret); 
       		return FAILED ; 
	} 

	usb_clear_halt(dev->udev, usb_rcvbulkpipe(dev->udev ,dev->bulk_in_endpointaddr));
	usb_clear_halt(dev->udev, usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointaddr));	
	
 	dev->cbw_tag = 0; 
	dev->active_scmd = NULL ;
        	
	return  SUCCESS  ; 
} 


/* usb_eh_bus_reset function */
int usb_eh_bus_reset( struct scsi_cmnd *scmd) 
{ 
        struct Scsi_Host  *host = scmd->device->host ; 
	struct my_usb_storage  *dev = shost_priv(host); 
	if(dev) 
	{ 
		dev_info(&dev->intf->dev," usb_eh_bus_reset() \n"); 
	} 
	
	int ret ; 
	ret = usb_reset_device(dev->udev); 
	if(ret) 
	{
		dev_err(&dev->intf->dev,"USB reset device failed %d\n", ret); 
		return FAILED ; 
	} 
	usb_clear_halt(dev->udev, usb_rcvbulkpipe(dev->udev ,dev->bulk_in_endpointaddr));
	usb_clear_halt(dev->udev, usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointaddr));	
 	dev->cbw_tag = 0; 
	dev->active_scmd = NULL ;	
	return SUCCESS  ; 
} 


/* usb_eh_host_reset function */ 
int usb_eh_host_reset( struct scsi_cmnd *scmd) 
{ 
	pr_info("usb_eh_host_rest() \n"); 
	      
 	return  usb_eh_bus_reset(scmd);  
} 



/**************************************   UTILITY FUNCTIONS   ********************************/
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
	 /*Allocating sec cbw*/
	 dev->sec_urb = usb_alloc_urb( 0 , GFP_KERNEL) ; 
	 if(dev->sec_urb ==NULL) 
	 { 
		 pr_info(" sec_urb alloc error \n "); 
		 return -ENOMEM ; 
	 }
	 dev->sec_buffer = usb_alloc_coherent(dev->udev, CBW_LEN , GFP_KERNEL, &dev->sec_dma); 
	 if( !dev->sec_buffer) 
	 { 
		 pr_info(" sec_buffer usb_alloc_coherent() errror \n"); 
		 goto r_sec ; 
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

r_sec: 
	if( dev->sec_urb || dev->sec_buffer) 
	 {
		 if( dev->sec_urb) 
		 { 
			 usb_kill_urb(dev->sec_urb); 
			 usb_free_urb(dev->sec_urb); 
			 dev->sec_urb= NULL ; 	
		 } 
		 if( dev->sec_buffer) 
		 { 
			 usb_free_coherent(dev->udev, CBW_LEN , dev->sec_buffer , dev->sec_dma); 
			 dev->sec_buffer = NULL ; 
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




/* screen printing function for simplicity */ 
void print_info( struct my_usb_storage *dev ) 
{ 
	if(dev) 
	{
		/* printing  pr_infos to screen for checking */ 
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
	}else{ 
		pr_info("print_info() error no dev \n");	
		return ;
	} 	
} 




/*slave alloc  */  
static int slave_alloc(struct scsi_device *dev, struct queue_limits *queue) 
{
	dev->eh_timeout = 10 *HZ ; 
	return 0;
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
