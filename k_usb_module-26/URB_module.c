#include <linux/kernel.h>
#include <linux/usb.h> 
#include <linux/module.h> 
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


#define VID  (0X0781)
#define PID  (0X5591)
#define BUF_SIZE 512 
#define CBW_LEN  31
#define CSW_LEN 13 


/* Scsi command macros */ 
#define SCSI_TEST_UNIT_READY(CBW) do  { \ 
	memset(&(cbw), 0 , sizeof(cbw)) ; \ 
	(cbw).dCBWSignature = cpu_to_le32(0x43425455) ;\ 
	(cbw).dCBWTag = cpu_to_le32(0x12345678) ; \ 
	(cbw).dCBWDataTransferLength = cpu_to_le32(0); \
	(cbw).bmCBWflags = 0x90; \
	(cbw).bCBWLUN = 0 ;\ 
	(cbw).CBWCB[0] = 0X00; 
}while(0)

#define  SCSI_REQUEST_SENSE(cbw , alloc_len) do { \ 
	memset(&(cbw), 0 , sizeof(cbw)) ; \ 
	(cbw).dCBWSignature = cpu_to_le32(0x43425455) ;\ 
	(cbw).dCBWTag = cpu_to_le32(0x12345678) ; \ 
	(cbw).dCBWDataTransferLength = cpu_to_le32(alloc_len); \
	(cbw).bmCBWflags = 0x80; \
	(cbw).bCBWLUN = 0 ;\ 
	(cbw).bCBWCBLength = 6 ; \ 
        (cbw).CBWCB[0] = 0x03 ; \ 
	(cbw).CBWCB[4] =(alloc_len); \ 
	
}while(0) 

#define  SCSI_INQUIRY(cbw, alloc_len) do  { \ 
	memset(&(cbw), 0 , sizeof(cbw)) ; \ 
	(cbw).dCBWSignature = cpu_to_le32(0x43425455) ;\ 
	(cbw).dCBWTag = cpu_to_le32(0x12345678) ; \ 
	(cbw).dCBWDataTransferLength = cpu_to_le32(alloc_len); \
	(cbw).bmCBWflags = 0x80; \
	(cbw).bCBWLUN = 0 ;\ 
	(cbw).bCBWCBLength = 6 ; \ 
        (cbw).CBWCB[0] = 0x12 ; \ 
	(cbw).CBWCB[4] =(alloc_len); \ 
	
}while(0) 

#define SCSI_READ_CAPACITY_10 (cbw) do { \ 
	memset(&(cbw), 0 , sizeof(cbw)) ; \ 
	(cbw).dCBWSignature = cpu_to_le32(0x43425455) ;\ 
	(cbw).dCBWTag = cpu_to_le32(0x12345678) ; \ 
	(cbw).dCBWDataTransferLength = cpu_to_le32(8); \
	(cbw).bmCBWflags = 0x80; \
	(cbw).bCBWLUN = 0 ;\ 
	(cbw).bCBWCBLength = 10  ; \ 
        (cbw).CBWCB[0] = 0x25 ; \ 
	
}while(0) 

#define SCSI_READ_10( cbw, lba  , num_blocks , block_size ) do { \ 
	memset(&(cbw), 0 , sizeof(cbw)) ; \ 
	(cbw).dCBWSignature = cpu_to_le32(0x43425455) ;\ 
	(cbw).dCBWTag = cpu_to_le32(0x12345678) ; \ 
	(cbw).dCBWDataTransferLength = cpu_to_le32((num_blocks)*  (block_size)); \
	(cbw).bmCBWflags = 0x80; \
	(cbw).bCBWLUN = 0 ;\ 
	(cbw).bCBWCBLength = 10  ; \ 
        (cbw).CBWCB[0] = 0x28 ; \ 
   	(cbw).CBWCB[1] = ((lba) >> 24 ) & 0xFF ; \ 
   	(cbw).CBWCB[2] = ((lba) >> 16 ) & 0xFF ; \ 
	(cbw).CBWCB[3] = ((lba) >> 8  ) & 0xFF ; \ 	
	(cbw).CBWCB[4] =  (lba)  & 0xFF ; \ 	
	(cbw).CBWCB[5] = ((num_blocks)  >> 8 ) & 0xFF ; \ 
	(cbw).CBWCB[6] = (num_blocks)  & 0xFF ; \ 
} while(0) 

#define SCSIWRITE_10 ( cbw , lba , num_blocks, block_size) do {  \ 
	memset(&(cbw), 0 , sizeof(cbw)) ; \ 
	(cbw).dCBWSignature = cpu_to_le32(0x43425455) ;\ 
	(cbw).dCBWTag = cpu_to_le32(0x12345678) ; \ 
	(cbw).dCBWDataTransferLength = cpu_to_le32((num_blocks)*  (block_size)); \
	(cbw).bmCBWflags = 0x00; \
	(cbw).bCBWLUN = 0 ;\ 
	(cbw).bCBWCBLength = 10  ; \ 
        (cbw).CBWCB[0] = 0x2A ; \ 
   	(cbw).CBWCB[1] = ((lba) >> 24 ) & 0xFF ; \ 
   	(cbw).CBWCB[2] = ((lba) >> 16 ) & 0xFF ; \ 
	(cbw).CBWCB[3] = ((lba) >> 8  ) & 0xFF ; \ 	
	(cbw).CBWCB[4] =  (lba)  & 0xFF ; \ 	
	(cbw).CBWCB[5] = ((num_blocks)  >> 8 ) & 0xFF ; \ 
	(cbw).CBWCB[6] = (num_blocks)  & 0xFF ; \ 
}

/***************************************** FUNCTION PROTOTYPE ******************************************/ 

static int usb_probe( struct usb_interface *interface ,  const struct  usb_device_id *id);
static void usb_disconnect( struct usb_interface *interface );
static   void cbw_callback ( struct urb *urb ); 
static  void data_callback( struct urb *urb) ; 
static void  csw_callback(struct urb *urb); 
void  init_usb_protocols( struct work_struct *work); 

/***************************************** USB DEVICE TABLE TEMPORARY***********************************/ 

// USB DEVICE ID TABLE // 
const struct  usb_device_id  usb_table[] = { 
	{USB_INTERFACE_INFO(USB_CLASS_MASS_STORAGE , USB_SC_SCSI , USB_PR_BULK ) },	
       	{} };
MODULE_DEVICE_TABLE(usb,usb_table);



/* USB_DRIVER STRUCTURES */ 
static struct   usb_driver   exmp_usb_driver  =
{ .name = "URBS DRIVER" ,
       	.probe = usb_probe ,
       	.disconnect = usb_disconnect ,
       	.id_table =  usb_table
}; 


/*  command block wrapper structure */ 
struct command_block_wrapper   { 
	__le32 dCBWSignature ; 
	__le32 dCBWTag ; 
	__le32 dCBWDataTransferLength ; 
	__u8 bmCBWFlags ; 
	__u8 bCBWLUN ; 
	__u8 bCBWCBLength ; 
	__u8 CBWCB[16];
} __attribute__((packed)); 


struct command_status_wrapper  {
	__le32 dCSWSignature ;
	__le32 dCSWTag ; 
  	__le32 dCSWDataResidue ;
	__u8 bCSWStatus ; 
} __attribute__((packed)); 



DECLARE_WORK(cbw_work, init_usb_protocols); 
struct usb_device *dev = NULL ; 

/* Custom structuer for global variables */ 
struct  my_usb_storage { 
	struct usb_device *udev  ; 
	struct usb_interface *uinterface  ; 
        struct command_block_wrapper  my_cbw ; 
        /* pipes */ 

	__u8  bulk_in_endpointaddr = 0  ; 
	 __u8  bulk_out_endpointaddr  =  0 ; 
	 __le32  cbw_tag  =  0 ; 
	 /* CBW */ 

	struct urb  *cbw_urb ; 
	unsigned  char *cbw_buffer =NULL ; 
	/* WRITE */ 

	unsigned char *write_buffer = NULL;  
	struct urb *write_urb = NULL ;
	/* CSW */ 

	struct urb  *csw_urb  = NULL ; 
	static char *csw_buffer = NULL ; 
       
	/* READ */ 

	struct urb  *read_urb= NULL ;  
	unsigned char *read_buf = NULL ; 

	/* INQUIRY */ 
	`
	struct urb *inquiry_urb ; 
	unsigned char *inquiry_buffer =NULL;  

} 

 
/************************************************ USB  FUNCTIONS ****************************************/

static int usb_probe ( struct  usb_interface *interface ,  const struct usb_device_id  *id)
{
	pr_info(" USB PROBE \n"); 

	struct  my_usb_storage  *dev ;
       	dev = kzalloc(sizeof(&dev) , GFP_KERNEL) ; 
	if( dev ==NULL) 
	{ 
		pr_info("Dev_alloc_error\n"); 
		return  -ENOMEM ; 
	} 
	dev->udev =  usb_get_dev(interface_to_udev(interface)) ; 

	dev->intf = interface; 
	usb_set_intfdata(interface, dev); 

	
	struct usb_host_interface *iface_desc ; 
	iface_desc = interface->cur_altsetting ;
 

	if((!dev || !iface_desc))  
		
	{ 

		pr_info(" No my_dev  && iface_desc found \n"); 

		return -EINVAL ; 
	} 


 

	// Allocating   buffers and URBS    
	
	/* For Inquriry scsi command */
	dev->cbw_buffer = kmalloc(CBW_LEN , GFP_KERNEL) ; 
	if( dev->cbw_buffer ==NULL) 
	{ 
		pr_err(" cbw_buffer_alloc_err\n"); 
	 	return  -ENOMEM ; 
	} 
	dev->cbw_urb = usb_alloc_urb(0 , GFP_KERNEL); 
        if ( dev->cbw_urb ==NULL )
	{
       		pr_err(" cbw_urb_alloc_err\n"); 
		goto r_cbw; 
	} 

		

	/* For CSW scsi command */
 	dev->csw_buffer = kmalloc(CSW_LEN , GFP_KERNEL) ; 
 	if( dev->csw_buffer == NULL  ) 
 	{ 
		 pr_info(" Csw_buffer_alloc_err\n"); 
		 return  -ENOMEM ;  
	} 

 	dev->csw_urb = usb_alloc_urb(0, GFP_KERNEL) ; 
 
 	if(!dev->csw_urb ) 
	 {
		 pr_info(" MY_URB ALLOC_ERRR\n");
		 goto r_csw  
		 return ; 
 	 }
	


	/* For Read  scsi command */
	dev->read_buf = kmalloc(36 , GFP_KERNEL) ; 
 	if( dev->read_buffer == NULL  ) 
 	{ 
		 pr_info(" read_buffer_alloc_err\n"); 
		 return - ENOMEM ;  
	} 

 	dev->read_urb = usb_alloc_urb(0, GFP_KERNEL) ;  
 	if(!dev->read_urb ) 
	 {
		 pr_info("  read_urb_alloc_err\n"); 
		 goto r_read  
 	 }



	/* For Inquriry scsi command */
	dev->inquiry_buffer = kmalloc(36 , GFP_KERNEL) ; 
 	if( dev->inquiry_buffer == NULL  ) 
 	{ 
		 pr_info(" inquiry_buffer_alloc_err\n"); 
		 return -ENOMEM ; 

	} 

 	dev->inquiry_urb = usb_alloc_urb(0, GFP_KERNEL) ; 
 
 	if(!dev->inquiry_urb ) 
	 {
		 pr_info("  inquiry_urb_alloc_err\n"); 
		  goto r_inquiry ; 

 	 }



	/* For Write scsi command */
	dev->write_buffer = kmalloc(CSW_LEN , GFP_KERNEL) ; 
 	if(!dev->write_buffer == NULL  ) 
 	{ 
		 pr_info("write_buffer_alloc_err\n"); 
		 return -ENOMEM ; 

	} 

 	dev->write_urb = usb_alloc_urb(0, GFP_KERNEL) ; 
 
 	if(!dev->write_urb ) 
	 {
		 pr_info("  write_urb_alloc_err\n"); 
		  goto  r_write ; 
 	 }


	



	/* Setting up   bulk_in_endpointaddr and bulk_out_endpointaddr */ 
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
		dev_err(&interface->dev , " Bad endpoint address  \n") ; 
		return  -ENODEV ; 
	
	} 


	pr_info(" bulk_in_endpointaddr :0x%02x\n", bulk_in_endpointaddr) ; 

	pr_info(" bulk_out_endpointaddr :0x%02x\n", bulk_out_endpointaddr) ; 



	/* Scheduling  CBW   in  workqyeue */ 
	schedule_work(&cbw_work); 

	 
	pr_info(" -- DEVICE  READY --\n"); 

	return 0 ; 


r_cbw: 

	 if(!cbw_urb || !cbw_buffer) 
	 { 

		usb_kill_urb(cbw_urb); 
	 	usb_free_urb(cbw_urb);
		kfree(cbw_buffer) ;
	 	cbw_urb = NULL ;
	 	cbw_buffer = NULL;
	        return -EINVAL ; 	
	 } 

r_csw: 

	 if(!csw_urb || !csw_buffer) 
	 { 

		usb_kill_urb(csw_urb); 
	 	usb_free_urb(csw_urb);
		kfree(csw_buffer) ;
	 	csw_urb = NULL ;
	 	csw_buffer = NULL;
	        return -EINVAL ; 	
	 } 

r_inquiry: 


	 if(!inquiry_urb || !inquiry_buffer) 
	 { 

		usb_kill_urb(inquiry_urb); 
	 	usb_free_urb(inquiry_urb);
		kfree(inquiry_buffer) ;
	 	inquiry_urb = NULL ;
	 	inquiry_buffer = NULL;
	        return -EINVAL ; 	
	 } 


r_read : 

	 if(!read_urb || !read_buf) 
	 { 

		usb_kill_urb(read_urb); 
	 	usb_free_urb(read_urb);
		kfree(read_buf) ;
	 	read_urb = NULL ;
	 	read_buf = NULL;
	        return -EINVAL ; 	
	 } 

r_write: 

	
	 if(!write_urb || !write_buffer) 
	 { 

		usb_kill_urb(write_urb); 
	 	usb_free_urb(write_urb);
		kfree(write_buffer) ;
	 	write_urb = NULL ;
	 	write_buffer = NULL;
	        return -EINVAL ; 	
	 } 

 

		
} 



/* Init_usb_protocol */ 
void init_usb_protocols( struct work_struct *work) 
{  

	pr_info(" -USB_WORKQUEUE_FUNCTION-\n"); 



	struct my_usb_storage *dev ; 
	dev = usb_get_intfdata(intf);
        	       


        SCSI_INQUIRY(dev->mycbw ,36 ); 

 

	usb_fill_bulk_urb(dev->cbw_urb, dev , usb_sndbulkpipe(dev, bulk_out_endpointaddr), dev->cbw_buffer , CBW_LEN , cbw_callback , NULL ) ; 


	 int ret = usb_submit_urb(dev->cbw_urb , GFP_KERNEL); 
 
	 if(ret) 
	 { 
		 pr_info(" CBW submission error :[%d] \n", ret) ; 
	  
		 goto r_urb ; 
	 } 


	 pr_info("CBW send successfully [ OK ] \n"); 


	return ;

/* Cleanups */
r_urb :
	if(cbw_urb) 
	{ 

	 pr_info("cbw_urb_err : cleaning.... \n");

	 usb_kill_urb(cbw_urb); 
	 usb_free_urb(cbw_urb); 
	 kfree(cbw_buffer) ;
	 cbw_buffer = NULL; 
	 cbw_urb =NULL; 
	 return  ; 

	}else{ 
	       pr_info(" No_cbw_urb_error \n"); 
	 	return ; 

	} 

}




/* Usb  Disconnect */
static void usb_disconnect ( struct usb_interface  *interface ) 
{ 


 
	struct usb_device *devt = interface_to_usbdev(interface) ; 

	/*   -- CLEAN UPS ON DISCONNECT  -- */  

	struct my_usb_storage  *dev ; 
	dev =  usb_get_intfdata ( intf, NULL ); 


	if (dev) 
	{ 
		if(dev->cbw_urb) 
	
		{	
		usb_kill_urb(dev->cbw_urb) ; 
		usb_free_urb(dev->cbw_urb) ; 
		kfree(dev->cbw_buffer) ; 
	        dev->cbw_urb = NULL ; 
		dev->cbw_buffer = NULL ;
		} 
	
		if(dev->rite_urb) 
		{ 
		usb_kill_urb(dev->write_urb); 
		usb_free_urb(dev->write_urb); 
		kfree(dev->write_buffer);
	        dev->write_urb = NULL ; 
		dev->write_buffer= NULL ; 
		} 
	

		if(dev->csw_urb)
		{
		usb_kill_urb(dev->csw_urb); 
		usb_free_urb(dev->csw_urb); 
		kfree(dev->csw_buffer); 
	        dev->csw_urb = NULL ; 
		dev->csw_buffer = NULL ; 
		} 

	
		if(dev->read_buf) 
		{ 
		usb_kill_urb(dev->read_urb); 
		usb_free_urb(dev->read_urb); 
		kfree(dev->read_buf); 
	        dev->read_urb = NULL ; 
		dev->read_buf = NULL ; 
		} 
 

		if( dev->inquiry_urb) 
		{ 
		usb_kill_urb(dev->inquiry_urb); 
		usb_free_urb(dev->inquiry_urb); 
		kfree(dev->inquiry_buffer) ;
		dev->inquiry_urb = NULL ; 
		dev->inquiry_buffer = NULL ; 
		} 
	} 


	usb_put_dev(dev->udev) ; 
	kfree(dev)l 

	cbw_tag = 0 ; 	

	usb_set_intfdata(interface, NULL) ; 

	usb_clear_halt(devt, usb_rcvbulkpipe(dev, bulk_in_endpointaddr));		 	 
	usb_clear_halt(devt, usb_sndbulkpipe(dev, bulk_out_endpointaddr));	

	pr_info(" -- USB DISCONNECTED -- \n"); 
	return ; 
}





/* Cbw_callback function */ 
static   void cbw_callback ( struct urb *urb )
{ 
	pr_info(" Callback usb functions \n"); 


	if(urb->status)
	{

		if( urb->status == EPIPE ) 
		{ 

			pr_info(" ENDPOINT STALLED \n"); 

	
			usb_clear_halt(urb->dev, usb_rcvbulkpipe(urb->dev, bulk_in_endpointaddr));		 	 
			usb_clear_halt(urb->dev, usb_sndbulkpipe(urb->dev, bulk_out_endpointaddr));	


			return ; 
		} 

		pr_err("  CBW URB  FAILED WIH STATUS  : %d\n  && (%s) \n", urb->status , 
		urb->status == -ESHUTDOWN ? "Device removed/shutdown" : 
		urb->status == -ENOENT ? " Urb  killed befroe submissio " : "Unkown err" ); 
		return ; 
	}  
	

	struct command_block_wrapper *cbw =  (struct  command_block_wrapper *)  urb->transfer_buffer; 


	if( cbw->dCBWSignature !=cpu_to_le32(0x43425355)) 
	{ 
		pr_err(" Imvalid CBW signature \n"); 
		return ; 
	}


	cbw_tag = cbw->dCBWTag; 
	

	switch(cbw->CBWCB[0]) 
	{
	       // SCSI  WRITE COMMAND // 	
		case 0x2A : 
			pr_info(" SCSI WRITE COMMAD BEDU  COMMAND REVIEVED \n") ; 

			write_buffer = kmalloc(15 , GFP_KERNEL) ; 
			if( write_buffer ==NULL) 
			{ 
				pr_info(" DATA_BUFF ALLLOC_ERR\n") ; 
				return ;
			} 


			unsigned char *new_Buff = "helloworld" ; 
	

	       		 strcpy(write_buffer , new_Buff) ; 

			write_urb = usb_alloc_urb(0, GFP_KERNEL); 
			if( !write_urb) 
			{ 
				pr_info(" DATA_URB_ALL_ERR\n"); 
				return ; 
			} 



			usb_fill_bulk_urb(write_urb ,urb->dev, usb_sndbulkpipe(urb->dev, bulk_out_endpointaddr), write_buffer ,  15  ,  data_callback, NULL) ; 

			int ret = usb_submit_urb(write_urb , GFP_KERNEL) ; 
			if(ret) 

			{ 
				pr_info("  SCSI_WRITE_URB_SUB_ERR\n");   
				usb_kill_urb(write_urb); 
				usb_free_urb(write_urb);
				kfree(write_buffer); 
				write_buffer =NULL; 
				write_urb = NULL; 
				return ; 

			} 


			
			pr_info(" WRITE_URB_SUB_SUCCESS \n");
			
		       	break ; 


		/* READ SCSIC COMMAND */ 
		case 0x28 :  

			pr_info("SCSI  READ (10) \n") ;

			read_buf = kmalloc(512 , GFP_KERNEL) ; 

	
			if( read_buf ==NULL) 
			{ 
				pr_info(" read _BUFF ALLLOC_ERR\n") ; 
				return ;
			} 


			read_urb = usb_alloc_urb(0, GFP_KERNEL); 
			if( !read_urb) 
			{ 
				
				pr_info(" READ_URB_ALL_ERR\n");
			       	return ; 
			} 



			usb_fill_bulk_urb(read_urb ,urb->dev, usb_rcvbulkpipe(urb->dev, bulk_in_endpointaddr), read_buf ,  512   ,  data_callback, NULL) ; 

			int result  = usb_submit_urb(read_urb , GFP_KERNEL) ; 
			if(result) 

			{ 

				pr_info(" READ_URB_SUB_ERR\n"); 
				goto r_urb ; 
			} 


			pr_info(" SEC URB SUMBIT SUCCESS \n"); 

			break ; 


		// SCSI  TEST UNIT COMMAND // 
		case 0x1A : 
			pr_info(" SCSI MODE SENSE COMMAND RECIEVED \n"); 
			break ;


		// SCSI  INQUIRY COMMAND // 	
		case 0x12  : 
			
			pr_info(" SCSI-INQUIRY_CMD REQ\n");  

			inquiry_buffer = kmalloc( 36 , GFP_KERNEL); 
			if(inquiry_buffer ==NULL ) 
			{ 
				pr_info("INQUIRY_BUF_ALLOC_ERR\n"); 
				return ; 
			} 

			inquiry_urb = usb_alloc_urb(0, GFP_KERNEL); 
			if(inquiry_urb==NULL) 
			{ 
				pr_info("INQUIRY_URB_ALLOC_ERR\n"); 
				usb_kill_urb(inquiry_urb); 
				usb_free_urb(inquiry_urb); 
				kfree(inquiry_buffer) ;
				inquiry_buffer = NULL ;
			        inquiry_urb =NULL ; 

				 
				return ; 
			} 

			usb_fill_bulk_urb(inquiry_urb , urb->dev , usb_rcvbulkpipe(urb->dev , bulk_in_endpointaddr), inquiry_buffer , 36 , data_callback,NULL) ; 

			int urb_inquiry_result = usb_submit_urb(inquiry_urb , GFP_KERNEL) ; 
			if( urb_inquiry_result) 
			{ 
				pr_info(" INQUIRY_URB_SUBMIT_ERR\n"); 
				usb_kill_urb(inquiry_urb); 
				usb_free_urb(inquiry_urb); 
				kfree(inquiry_buffer) ;
				inquiry_buffer = NULL ;
			        inquiry_urb = NULL; 

				usb_clear_halt(urb->dev, bulk_in_endpointaddr); 
				return ; 
			} 

			
			pr_info(" INQUIRY_URB_SUBMISSION_SUCCESS\n"); 
			break ; 


		default :
			pr_warn(" SCSI UNKNOWN COMMAND :0x%02x\n", cbw->CBWCB[0]); 
			break ; 
	}

	return ; 



r_urb : 
usb_kill_urb(read_urb); 
usb_free_urb(read_urb); 
kfree(read_buf);
read_urb=NULL; 
read_buf = NULL ; 
return ; 

	
} 
 


/* data call_back function */ 
static   void data_callback  ( struct urb *urb )
{ 


	pr_info("  -DATA CALLBACK FN -\n");

       unsigned char *buffer = urb->transfer_buffer; 

	uint32_t  max_lba  = be32_to_cpu(*(uint32_t *) &buffer[0]); 

	uint32_t  block_size  = be32_to_cpu(*(uint32_t *) &buffer[4]);


	pr_info(" max_lba :%u\n", max_lba); 
	pr_info(" BLOCK sizev: %u bytes \n", block_size); 


 	csw_buffer = kmalloc(13, GFP_KERNEL) ; 
	if(csw_buffer == NULL ) 
	{ 
		pr_info (" CSW_BUF_ALLOC_ERR\n"); 
		return ; 
	} 


	csw_urb   = usb_alloc_urb(0, GFP_KERNEL); 
        if(csw_urb==NULL ) 
	{ 
		usb_kill_urb(csw_urb); 
		usb_free_urb(csw_urb);
		kfree(csw_buffer); 
		csw_buffer= NULL; 
		csw_urb = NULL ;
	} 	 	

	
	usb_clear_halt(urb->dev, usb_rcvbulkpipe(urb->dev, bulk_in_endpointaddr)); 
       	usb_clear_halt(urb->dev, usb_sndbulkpipe(urb->dev, bulk_out_endpointaddr)); 
	
	usb_fill_bulk_urb(csw_urb, urb->dev,usb_rcvbulkpipe(urb->dev , bulk_in_endpointaddr), csw_buffer, 13 , csw_callback, NULL) ; 
	int ret = usb_submit_urb(csw_urb, GFP_KERNEL); 

	if( ret) 
	{ 
		pr_info(" SUBMISSION ERR : 3 \n"); 
		usb_kill_urb(csw_urb); 
		usb_free_urb(csw_urb); 
		kfree(csw_buffer);
	       	csw_buffer = NULL ; 
		csw_urb = NULL ; 	
		return  ; 
	} 




	pr_info(" - CSW RECIEVED -  \n"); 


	return ; 




}



/* csw_callback function */ 
static   void csw_callback( struct urb *urb ) 
{ 

	pr_info(" csw callback function \n");
	pr_info("  CSW CALLBACK STATUS : %d\n",urb->status); 


	if(urb->status)
	{	
		urb->status == -ENOENT  ? usb_clear_halt(urb->dev, usb_rcvbulkpipe(urb->dev, bulk_in_endpointaddr)) , usb_clear_halt(urb->dev, usb_sndbulkpipe(urb->dev, bulk_out_endpointaddr)) : 
	
			
		pr_err("  CBW URB  FAILED WIH STATUS  : %d\n  && (%s) \n", urb->status , 
		urb->status == -ESHUTDOWN ? "Device removed/shutdown" : 
		urb->status == -EPIPE ?  "Endpoint stalled" : " HEHE " ) ;  
	//	urb->status == -ENOENT ? " Urb  killed befroe submissio " : "Unkown err" ); 
		return ; 
	} 
	struct command_status_wrapper *csw =  (struct  command_status_wrapper *)  urb->transfer_buffer; 

	if( csw->dCSWSignature !=cpu_to_le32(0x53425355)) 
	{ 
		pr_err(" Imvalid CBW signature \n"); 
		return ;
	} 
	if(csw->dCSWTag != cbw_tag) 
	{ 
		pr_info(" TAG NOT MATCHED \n"); 
		return  ;
	} 


	pr_info("something went good \n"); 

	
	return ; 
} 

/************************************************   MODULE FUNCTIONS ***************************************/ 

static int __init urb_init(void) 
{ 
       int ret =  usb_register_driver(&exmp_usb_driver,  THIS_MODULE , "URB_driver") ;
      if( ret) 
      { 
	     pr_info("-USB REGISTER FAILED\n"); 
	     return 0 ; 
      } 


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



