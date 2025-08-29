#include <linux/printk.h>
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
#define CBW_SIG 0x43425355 

/* Scsi command macros */ 
#define SCSI_TEST_UNIT_READY(cbw)   do{ \
	memset(&(cbw), 0 , sizeof(cbw));  \
	(cbw).dCBWSignature = cpu_to_le32(CBW_SIG); \
	(cbw).dCBWTag = cpu_to_le32(0x12345678) ; \
	(cbw).dCBWDataTransferLength = cpu_to_le32(0); \
	(cbw).bmCBWFlags = 0x00; \
	(cbw).bCBWLUN = 0 ;\
	(cbw).bCBWCBLength = 6 ; \
	(cbw).CBWCB[0] = 0x00; \
}while(0)

#define  SCSI_REQUEST_SENSE(cbw , alloc_len) do { \
	memset(&(cbw), 0 , sizeof(cbw)) ; \
	(cbw).dCBWSignature = cpu_to_le32(CBW_SIG) ;\
       	(cbw).dCBWTag = cpu_to_le32(0x12345678) ; \
	(cbw).dCBWDataTransferLength = cpu_to_le32(alloc_len); \
	(cbw).bmCBWFlags = 0x80; \
	(cbw).bCBWLUN = 0 ;\
	(cbw).bCBWCBLength = 6 ; \
        (cbw).CBWCB[0] = 0x03 ; \
	(cbw).CBWCB[4] =(alloc_len); \
}while(0) 

#define  SCSI_INQUIRY(cbw, alloc_len) do  { \
	memset(&(cbw), 0 , sizeof(cbw)) ; \
	(cbw).dCBWSignature = cpu_to_le32(CBW_SIG) ;\
	(cbw).dCBWTag = cpu_to_le32(0x12345678) ; \
	(cbw).dCBWDataTransferLength = cpu_to_le32(alloc_len); \
	(cbw).bmCBWFlags = 0x80; \
	(cbw).bCBWLUN = 0 ;\
	(cbw).bCBWCBLength = 6 ; \
        (cbw).CBWCB[0] = 0x12 ; \
        (cbw).CBWCB[1] = 0x00 ; \
        (cbw).CBWCB[2] = 0x00 ; \
        (cbw).CBWCB[3] = 0x00 ; \
	(cbw).CBWCB[4] =(alloc_len); \
        (cbw).CBWCB[5] = 0x00 ; \
}while(0) 

#define SCSI_READ_CAPACITY_10(cbw) do { \
	memset(&(cbw), 0 , sizeof(cbw)) ; \
	(cbw).dCBWSignature = cpu_to_le32(CBW_SIG) ;\
	(cbw).dCBWTag = cpu_to_le32(0x12345678) ; \
	(cbw).dCBWDataTransferLength = cpu_to_le32(8); \
	(cbw).bmCBWFlags = 0x80; \
	(cbw).bCBWLUN = 0 ;\
	(cbw).bCBWCBLength = 10  ; \
        (cbw).CBWCB[0] = 0x25 ; \
}while(0) 

#define SCSI_READ_10( cbw, lba  , num_blocks , block_size ) do { \
	memset(&(cbw), 0 , sizeof(cbw)) ; \
	(cbw).dCBWSignature = cpu_to_le32(CBW_SIG) ;\
	(cbw).dCBWTag = cpu_to_le32(0x12345678) ; \
	(cbw).dCBWDataTransferLength = cpu_to_le32((num_blocks)*  (block_size)); \
	(cbw).bmCBWFlags = 0x80; \
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

#define SCSIWRITE_10(cbw , lba , num_blocks, block_size) do {  \
	memset(&(cbw), 0 , sizeof(cbw)) ; \
	(cbw).dCBWSignature = cpu_to_le32(CBW_SIG) ;\
	(cbw).dCBWTag = cpu_to_le32(0x12345678) ; \
	(cbw).dCBWDataTransferLength = cpu_to_le32((num_blocks)*  (block_size)); \
	(cbw).bmCBWFlags = 0x00; \
	(cbw).bCBWLUN = 0 ;\
	(cbw).bCBWCBLength = 10  ; \
        (cbw).CBWCB[0] = 0x2A ; \
   	(cbw).CBWCB[1] = ((lba) >> 24 ) & 0xFF ; \
   	(cbw).CBWCB[2] = ((lba) >> 16 ) & 0xFF ; \
	(cbw).CBWCB[3] = ((lba) >> 8  ) & 0xFF ; \
	(cbw).CBWCB[4] =  (lba)  & 0xFF ; \
	(cbw).CBWCB[5] = ((num_blocks)  >> 8 ) & 0xFF ; \
	(cbw).CBWCB[6] = (num_blocks)  & 0xFF ; \
}while(0)   

/***************************************** USB DEVICE TABLE TEMPORARY***********************************/ 

// USB DEVICE ID TABLE // 
const struct  usb_device_id  usb_table[] = { 
	{USB_INTERFACE_INFO(USB_CLASS_MASS_STORAGE , USB_SC_SCSI , USB_PR_BULK ) },	
       	{} };
MODULE_DEVICE_TABLE(usb,usb_table);



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


/* Custom structure for global variables */ 
struct  my_usb_storage { 
	struct usb_device *udev  ; 
	struct usb_interface *intf ; 
        struct command_block_wrapper  my_cbw ;
	struct command_status_wrapper my_csw; 

        struct work_struct my_work ; 	
        /* pipes */ 

	 __u8  bulk_in_endpointaddr ; 
	 __u8  bulk_out_endpointaddr  ; 
	 __le32  cbw_tag; 
	 /* CBW */ 

	struct urb  *cbw_urb ; 
	unsigned  char *cbw_buffer; 
	/* WRITE */ 

	unsigned char *write_buffer;  
	struct urb *write_urb  ;
	/* CSW */ 

	struct urb  *csw_urb  ; 
	unsigned  char *csw_buffer; 
       
	/* READ */ 

	struct urb  *read_urb ;  
	unsigned char *read_buf; 

	/* INQUIRY */ 
	
	struct urb *inquiry_urb ; 
	unsigned char *inquiry_buffer ;  

}; 

 
/***************************************** FUNCTION PROTOTYPE ******************************************/ 

static int usb_probe( struct usb_interface *interface ,  const struct  usb_device_id *id);
static void usb_disconnect( struct usb_interface *interface );
static   void cbw_callback ( struct urb *urb ); 
static  void data_callback( struct urb *urb) ; 
static void  csw_callback(struct urb *urb); 
void  init_usb_protocols( struct work_struct *work); 
int usb_clear_buffer_and_urb( struct my_usb_storage *dev);
int usb_alloc_buffer_and_urb( struct my_usb_storage *dev) ; 


 
/* USB_DRIVER STRUCTURES */ 
static struct   usb_driver   exmp_usb_driver  =
{ .name = "URBS DRIVER" ,
       	.probe = usb_probe ,
       	.disconnect = usb_disconnect ,
       	.id_table =  usb_table
}; 


/************************************************ USB  FUNCTIONS ****************************************/

static int usb_probe ( struct  usb_interface *interface ,  const struct usb_device_id  *id)
{
	pr_info(" USB PROBE \n"); 

	struct  my_usb_storage  *dev ;
       	dev = kzalloc(sizeof(*dev) , GFP_KERNEL) ; 
	if( dev ==NULL) 
	{ 
		pr_info("Dev_alloc_error\n"); 
		return  -ENOMEM ; 
	} 
	dev->udev =  usb_get_dev(interface_to_usbdev(interface)) ; 

	dev->intf = interface; 
	usb_set_intfdata(interface, dev); 

	dev = dev ; 
	
	struct usb_host_interface *iface_desc ; 
	iface_desc = interface->cur_altsetting ;
 

	if((!dev || !iface_desc))  
		
	{ 

		pr_info(" No my_dev  && iface_desc found \n"); 

		return -EINVAL ; 
	} 



	/* Calling allocation function */ 
        int  usb_alloc_return = 	usb_alloc_buffer_and_urb( dev) ; 
	if( usb_alloc_return ) 
	{ 
		pr_err("Buffer allocation error\n"); 
		return -EINVAL ;
	} 




	/* Setting up   bulk_in_endpointaddr and bulk_out_endpointaddr */ 
	for(int i = 0 ; i < iface_desc->desc.bNumEndpoints ; i++ ) 
	{ 
		struct usb_endpoint_descriptor  *epd = &iface_desc->endpoint[i].desc; 
		
		if(usb_endpoint_is_bulk_in(epd)) 
		{ 
			dev->bulk_in_endpointaddr =  epd->bEndpointAddress ; 
		} 

		if(usb_endpoint_is_bulk_out(epd))
		{ 
			dev->bulk_out_endpointaddr = epd->bEndpointAddress; 
		} 
	} 


	if(!dev->bulk_in_endpointaddr && !dev->bulk_out_endpointaddr) 
	{ 
		dev_err(&interface->dev , " Bad endpoint address  \n") ; 
		return  -ENODEV ; 
	
	} 


	pr_info("bulk_in_endpointaddr :[0x%02x]\n", dev->bulk_in_endpointaddr) ; 

	pr_info("bulk_out_endpointaddr :[0x%02x]\n", dev->bulk_out_endpointaddr) ; 


	/* Creating workqueue */ 
	INIT_WORK(&dev->my_work, init_usb_protocols) ;



	/* Scheduling  CBW   in  workqyeue */ 
	schedule_work(&dev->my_work); 

	 
	pr_info(" -- DEVICE  READY --\n"); 

	return 0 ; 
		
}


 

/* Init_usb_protocol */ 
void init_usb_protocols( struct work_struct *work) 
{  

	pr_info("Initializing protocols...\n"); 



	struct my_usb_storage *dev ; 

	dev = container_of(work, struct my_usb_storage , my_work) ; 


	

	if(!dev) 
	{ 
		pr_info(" no dev "); 
	} 
	if( !dev->udev) 
	{ 
		pr_info(" no udev too"); 
	} 


	memset(dev->cbw_buffer , 0 , CBW_LEN); 

	SCSI_INQUIRY(dev->my_cbw , 36 ) ; 


	
	print_hex_dump_bytes("cbw ;", DUMP_PREFIX_OFFSET ,&dev->my_cbw , sizeof(dev->my_cbw));

	memcpy(dev->cbw_buffer ,&dev->my_cbw, CBW_LEN) ; 	

       // SCSI_TEST_UNIT_READY(dev->my_cbw); 

 
	pr_info(" sending this CBW\n"); 
	print_hex_dump_bytes("cbw ;", DUMP_PREFIX_OFFSET ,dev->cbw_buffer , CBW_LEN);	

	usb_fill_bulk_urb(dev->cbw_urb, dev->udev , usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointaddr), dev->cbw_buffer , CBW_LEN , cbw_callback , dev ) ; 


	 int ret = usb_submit_urb(dev->cbw_urb , GFP_KERNEL); 
 
	 if(ret) 
	 { 
		 pr_info("Cbw submission error:[%d] \n", ret) ; 
	  
		 goto r_urb ; 
	 } 


	 pr_info("Cbw send successfully [ OK ]\n"); 


	return ;

/* Cleanups */
r_urb :
	if(dev->cbw_urb) 
	{ 

	 pr_info("cbw_urb_err : cleaning.... \n");

	 usb_kill_urb(dev->cbw_urb); 
	 usb_free_urb(dev->cbw_urb); 
	 kfree(dev->cbw_buffer) ;
	 dev->cbw_buffer = NULL; 
	 dev->cbw_urb =NULL; 
	 return  ; 

	}else{ 
	       pr_info(" No_cbw_urb_error \n"); 
	 	return ; 

	} 

}



/* Cbw_callback function */ 
static   void cbw_callback ( struct urb *urb )
{ 
	pr_info("-- Cbw_callback function -- \n");

	struct my_usb_storage *dev  =  urb->context ;
        	       
	pr_info(" bulk_in_endpointaddr :[0x%02x]\n", dev->bulk_in_endpointaddr) ; 

	if(urb->status)
	{

		if( urb->status == -EPIPE ) 
		{ 

			usb_clear_halt(dev->udev, usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointaddr));
			usb_clear_halt(dev->udev , usb_sndbulkpipe(dev->udev , dev->bulk_out_endpointaddr)); 
	
			pr_info(" ENDPOINT STALLED \n"); 

	
			return ; 
		} 

		pr_err("  CBW URB  FAILED WIH STATUS  : %d\n  && (%s) \n", urb->status , 
		urb->status == -ESHUTDOWN ? "Device removed/shutdown" : 
		urb->status == -ENOENT ? " Urb  killed befroe submissio " : "Unkown err" ); 
		return ; 
	}  
	

 	 struct command_block_wrapper *cbw  =  (struct  command_block_wrapper *)  urb->transfer_buffer; 


	if(cbw->dCBWSignature !=cpu_to_le32(CBW_SIG)) 
	{ 
		pr_err(" Invalid   cbw signature \n"); 
		return ; 
	}


	dev->cbw_tag = le32_to_cpu(cbw->dCBWTag);  
	

	switch(cbw->CBWCB[0]) 
	{
	       // SCSI  WRITE COMMAND // 	
		case 0x2A : 
			pr_info(" SCSI WRITE COMMAD BEDU  COMMAND REVIEVED \n") ; 


			usb_fill_bulk_urb(dev->write_urb ,dev->udev, usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointaddr), dev->write_buffer ,  15  ,  data_callback, dev) ; 

			int ret = usb_submit_urb(dev->write_urb , GFP_KERNEL) ; 
			if(ret) 

			{ 
				pr_info("  SCSI_WRITE_URB_SUB_ERR\n");   
				usb_kill_urb(dev->write_urb); 
				usb_free_urb(dev->write_urb);
				kfree(dev->write_buffer); 
				dev->write_buffer =NULL; 
				dev->write_urb = NULL; 
				return ; 

			} 


			
			pr_info(" WRITE_URB_SUB_SUCCESS \n");
			
		       	break ; 


		/* READ SCSIC COMMAND */ 
		case 0x28 :  

			pr_info("SCSI  READ (10) \n") ;


			usb_fill_bulk_urb(dev->read_urb ,dev->udev, usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointaddr), dev->read_buf ,  512   ,  data_callback,dev) ; 

			int result  = usb_submit_urb(dev->read_urb , GFP_KERNEL) ; 
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
			
usb_fill_bulk_urb(dev->inquiry_urb , dev->udev , usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointaddr), dev->inquiry_buffer , 36 , data_callback,dev) ; 

			int urb_inquiry_result = usb_submit_urb(dev->inquiry_urb , GFP_ATOMIC) ; 
			if( urb_inquiry_result) 
			{ 
				pr_info("Inquiry_urb_alloc error\n"); 
				usb_kill_urb(dev->inquiry_urb); 
				usb_free_urb(dev->inquiry_urb); 
				kfree(dev->inquiry_buffer) ;
				dev->inquiry_buffer = NULL ;
			        dev->inquiry_urb = NULL; 

				return ; 
			} 

			
			pr_info("Inquiry response recieved [ ok ]\n");

			break ; 


		default :
			pr_warn(" SCSI UNKNOWN COMMAND :0x%02x\n", cbw->CBWCB[0]); 
			break ; 
	}

	return ; 



r_urb : 
usb_kill_urb(dev->read_urb); 
usb_free_urb(dev->read_urb); 
kfree(dev->read_buf);
dev->read_urb=NULL; 
dev->read_buf = NULL ; 
return ; 

	
} 
 


/* data call_back function */ 
static   void data_callback  ( struct urb *urb )
{ 

 	if(urb->status == -EPIPE) 
  	{ 
		pr_info("endpoint stalled \n"); 
		return ; 
	 } 	
	
	pr_info("  -DATA CALLBACK FN -\n");
	struct my_usb_storage *dev = urb->context ;  

      // unsigned char *buffer = urb->transfer_buffer; 
//
//	uint32_t  max_lba  = be32_to_cpu(*(uint32_t *) &buffer[0]); 
//
//	uint32_t  block_size  = be32_to_cpu(*(uint32_t *) &buffer[4]);
//
//
//	pr_info(" max_lba :%u\n", max_lba); 
//	pr_info(" BLOCK sizev: %u bytes \n", block_size); 
//
	
usb_fill_bulk_urb(dev->csw_urb,dev->udev,usb_rcvbulkpipe(dev->udev  , dev->bulk_in_endpointaddr), dev->csw_buffer, CSW_LEN , csw_callback, dev) ; 
	 

	int ret = usb_submit_urb(dev->csw_urb, GFP_ATOMIC); 

	if( ret) 
	{ 
		pr_info("Csw_urb_alloc error\n"); 
		return  ; 
	} 




	pr_info("Csw response recieved  [ ok ]\n"); 


	return ; 




}



/* csw_callback function */ 
static   void csw_callback( struct urb *urb ) 
{ 

	struct my_usb_storage *dev = urb->context ;  


 

	pr_info(" csw callback function \n");
	pr_info("  CSW CALLBACK STATUS : %d\n",urb->status); 


	if(urb->status)
	{	
		urb->status == -ENOENT  ? usb_clear_halt(dev->udev, usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointaddr)) , usb_clear_halt(dev->udev , usb_sndbulkpipe(dev->udev , dev->bulk_out_endpointaddr)) : 
	
			
		pr_err("  CBW URB  FAILED WIH STATUS  : %d\n  && (%s) \n", urb->status , 
		urb->status == -ESHUTDOWN ? "Device removed/shutdown" : 
		urb->status == -EPIPE ?  "Endpoint stalled" : " HEHE " ) ;  
	//	urb->status == -ENOENT ? " Urb  killed befroe submissio " : "Unkown err" ); 
		return ; 
	} 
	struct command_status_wrapper *csw =  (struct  command_status_wrapper *)  urb->transfer_buffer; 

	if( csw->dCSWSignature !=cpu_to_le32(CBW_SIG)) 
	{ 
		pr_err(" Imvalid CBW signature \n"); 
		return ;
	} 
	if(le32_to_cpu(csw->dCSWTag)  != dev->cbw_tag) 
	{ 
		pr_info("Invalid  tags  \n"); 
		return  ;
	} 


	pr_info("something went good \n"); 

	
	return ; 
} 




/* Usb  Disconnect  function  */
static void usb_disconnect ( struct usb_interface  *interface ) 
{ 

	/*   -- CLEAN UPS ON DISCONNECT  -- */  

	struct my_usb_storage  *dev ; 
	dev =  usb_get_intfdata ( interface);
	if( dev) 
	{
		dev->cbw_tag = 0 ; 	
		/* Calling usb_clear_buffer_and_urb function */ 
	 	int  usb_clear_return =	usb_clear_buffer_and_urb( dev) ;
		if( usb_clear_return) 
		{ 
			pr_err("Allocation cleanup error\n") ; 
			return ; 
		} 

		/* Clearing bulk endpoints */ 
		usb_clear_halt(dev->udev, usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointaddr));		 	 
		usb_clear_halt(dev->udev, usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointaddr));	

	} 

       	usb_set_intfdata(interface , NULL) ; 
	usb_put_dev(dev->udev) ; 
	kfree(dev); 
	pr_info(" -- USB DISCONNECTED -- \n"); 
	return ; 
}





/* USB URB and buffer allocation function */ 
int usb_alloc_buffer_and_urb( struct my_usb_storage *dev) 
{ 

 	pr_info(" Allocating buffer....\n"); 

		
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
 
 	if(dev->csw_urb==NULL ) 
	 {
		 pr_info(" MY_URB ALLOC_ERRR\n");
		 goto r_csw ; 
 	 }
	


	/* For Read  scsi command */
	dev->read_buf = kmalloc(36 , GFP_KERNEL) ; 
 	if( dev->read_buf == NULL  ) 
 	{ 
		 pr_info(" read_buffer_alloc_err\n"); 
		 return - ENOMEM ;  
	} 

 	dev->read_urb = usb_alloc_urb(0, GFP_KERNEL) ;  
 	if(!dev->read_urb ) 
	 {
		 pr_info("  read_urb_alloc_err\n"); 
		 goto r_read ;  
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
 	if(!dev->write_buffer  ) 
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



	pr_info("Allocation successful [ ok ] \n") ; 
	
	return 0 ;	



/* Cleanups */ 


r_cbw: 

	 if(dev->cbw_urb || dev->cbw_buffer) 
	 {
		if(dev->cbw_urb) 
		{

			usb_kill_urb(dev->cbw_urb); 
		 	usb_free_urb(dev->cbw_urb);
	 		dev->cbw_urb = NULL ;
		} 
		if( dev->cbw_buffer) 
		{ 

			kfree(dev->cbw_buffer) ;
	 		dev->cbw_buffer = NULL; 
		} 

	        return -EINVAL ; 	
	 } 

r_csw: 

	 if(dev->csw_urb || dev->csw_buffer) 
	 { 

		 if( dev->csw_urb) 
		 {

			usb_kill_urb(dev->csw_urb); 
	 		usb_free_urb(dev->csw_urb);
	 		dev->csw_urb = NULL ;
		 }
		if(dev->csw_buffer) 
		{
			kfree(dev->csw_buffer) ; 
	 		dev->csw_buffer = NULL;

		}
	        return -EINVAL ; 	
	 } 

r_inquiry: 


	 if(dev->inquiry_urb || dev->inquiry_buffer) 
	 { 
		 if( dev->inquiry_urb) 
		 {

			usb_kill_urb(dev->inquiry_urb); 
	 		usb_free_urb(dev->inquiry_urb);
	 		dev->csw_urb = NULL ;
		 }
		if(dev->inquiry_buffer) 
		{
			kfree(dev->inquiry_buffer) ; 
	 		dev->inquiry_buffer = NULL;

		}
	
		 return -EINVAL ; 	
	 } 


r_read : 

	 if(dev->read_urb || dev->read_buf) 
	 { 
		 if( dev->read_urb) 
		 {

			usb_kill_urb(dev->read_urb); 
	 		usb_free_urb(dev->read_urb);
	 		dev->read_urb = NULL ;
		 }
		if(dev->read_buf) 
		{
			kfree(dev->read_buf) ; 
	 		dev->read_buf = NULL;

		}
	        return -EINVAL ; 	
	 } 

r_write: 

	
	 if(dev->write_urb || dev->write_buffer) 
	 { 
		 if( dev->write_urb) 
		 {

			usb_kill_urb(dev->write_urb); 
	 		usb_free_urb(dev->write_urb);
	 		dev->write_urb = NULL ;
		 }
		if(dev->write_buffer) 
		{
			kfree(dev->write_buffer) ; 
	 		dev->write_buffer = NULL;
		} 
		
	        return -EINVAL ; 	
	 } 


	return  -EINVAL;
} 




/* USB URB and buffer cleanup function */  
int usb_clear_buffer_and_urb( struct my_usb_storage *dev) 
{

	// Clearing  allocated  buffers and urbs // 
	pr_info("Clearing allocations....\n") ;

	if (dev) 
	{
		 /* Clearing cbw_urb and cbw_buffer */ 
		 if(dev->cbw_urb || dev->cbw_buffer) 
		 {
			if(dev->cbw_urb) 
			{

				usb_kill_urb(dev->cbw_urb); 
		 		usb_free_urb(dev->cbw_urb);
	 			dev->cbw_urb = NULL ;
			} 
			if( dev->cbw_buffer) 
			{ 

				kfree(dev->cbw_buffer) ;
	 			dev->cbw_buffer = NULL; 
			} 

	 	} 

		 /* Clearing csw_urb and csw_buffer */ 
		 if(dev->csw_urb || dev->csw_buffer) 
		 { 

			 if( dev->csw_urb) 
		 	{

				usb_kill_urb(dev->csw_urb); 
	 			usb_free_urb(dev->csw_urb);
	 			dev->csw_urb = NULL ;
			 }
			if(dev->csw_buffer) 
			{
				kfree(dev->csw_buffer) ; 
	 			dev->csw_buffer = NULL;

			}
	 	} 

		 /* Clearing  inquiry_urb and inquiry_buffer */ 
		 if(dev->inquiry_urb || dev->inquiry_buffer) 
		 { 
			 if( dev->inquiry_urb) 
			 {

				usb_kill_urb(dev->inquiry_urb); 
	 			usb_free_urb(dev->inquiry_urb);
	 			dev->csw_urb = NULL ;
			 }
			if(dev->csw_buffer) 
			{
				kfree(dev->csw_buffer) ; 
		 		dev->csw_buffer = NULL;
			}
		 } 

		 /* Clearing read_urb and read_buffer */ 
		 if(dev->read_urb || dev->read_buf) 
		 { 
			 if( dev->read_urb) 
			 {

				usb_kill_urb(dev->read_urb); 
	 			usb_free_urb(dev->read_urb);
	 			dev->read_urb = NULL ;
			 }
			if(dev->read_buf) 
				{
				kfree(dev->read_buf) ; 
	 			dev->read_buf = NULL;

			}
		 } 

		 /* Clearing write_urb and write_buffer */ 
		 if(dev->write_urb || dev->write_buffer) 
		 { 
			 if( dev->write_urb) 
			 {

				usb_kill_urb(dev->write_urb); 
		 		usb_free_urb(dev->write_urb);
		 		dev->write_urb = NULL ;
			 }
			if(dev->write_buffer) 
			{
				kfree(dev->write_buffer) ; 
	 			dev->write_buffer = NULL;
			} 
		 } 

	 pr_info("Allocation cleared [ ok ] \n");
	 return 0 ;

	}
		 return -EINVAL ;
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
MODULE_DESCRIPTION("A simple  USB driver "); 
MODULE_AUTHOR("Johan-liebert") ;


