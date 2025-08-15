#include <linux/kernel.h>
#include <linux/usb.h> 
#include <linux/module.h> 
#include <linux/usb/ch9.h> 
#include <linux/usb/storage.h> 
#include <linux/slab.h> 
#include <linux/types.h> 
#include <linux/byteorder/generic.h> 
#include <linux/string.h>


#define  VID  (0X0781)
#define PID  (0X5591)
#define BUF_SIZE 512 
#define CBW_LEN  31 

/******************************************** GLOABLA VARIBLES ****************************************/
 __u8  bulk_in_endpointaddr = 0  ; 
 __u8  bulk_out_endpointaddr  =  0 ; 
 __le32  cbw_tag  =  0 ;

 /* cbw variable */ 
 struct urb  *cbw_urb ; 
 unsigned  char *cbw_buffer =NULL ; 

// scsi write varible // 
unsigned char *write_buffer = NULL;  
struct urb *write_urb = NULL ;

// csw variable // 
struct urb  *csw_urb ; 
static char *csw_buffer = NULL ; 

// Scsi  read variable // 
struct urb  *read_urb= NULL ;  
unsigned char *read_buf = NULL ;

/* scsi inquiry command variables */ 
struct urb *inquiry_urb ; 
unsigned char *inquiry_buffer =NULL; 

 static  struct command_block_wrapper  my_cbw ; 
 

/***************************************** FUNCTION PROTOTYPE ******************************************/ 

static int usb_probe( struct usb_interface *interface ,  const struct  usb_device_id *id);
static void usb_disconnect( struct usb_interface *interface );
static   void cbw_callback ( struct urb *urb ); 
static  void data_callback( struct urb *urb) ; 
static void  csw_callback(struct urb *urb); 


/***************************************** USB DEVICE TABLE TEMPORARY***********************************/ 

/* USB  DEVICE ID TABLE  */ 
/*const struct  usb_device_id  usb_table[] = 
{  { .match_flags = USB_DEVICE_ID_MATCH_INT_INFO, 
  .bInterfaceClass  = USB_CLASS_MASS_STORAGE, 
  .bInterfaceSubClass  = USB_SC_SCSI, 
  .bInterfaceProtocol  = USB_PR_BULK ,  },  
	{}
}; */ 


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





/************************************************ USB  FUNCTIONS ****************************************/

static int usb_probe ( struct  usb_interface *interface ,  const struct usb_device_id  *id)
{
	pr_info(" USB PROBE \n"); 


	struct usb_device *dev  =  interface_to_usbdev(interface) ; 
	struct usb_host_interface  *iface_desc = interface->cur_altsetting ; 


	if((!dev || !iface_desc))  
		
	{ 

		pr_info(" NO DEV ||  NO IFACE_DESC \n"); 
		return 0; 
	} 


	usb_get_dev(dev) ; 


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


	pr_info(" bulk_in_endpointaddr :0x%02x\n", bulk_in_endpointaddr) ; 

	pr_info(" bulk_out_endpointaddr :0x%02x\n", bulk_out_endpointaddr) ; 


	 /*Allocating buffer  for CW_LEN */ 
	 cbw_buffer = kmalloc(CBW_LEN , GFP_KERNEL) ; 
	 if( cbw_buffer == NULL  ) 
	 { 
		 pr_info(" CW_BUF_ALLOC_ERR\n"); 
		 return -ENOMEM ; 
	 } 



	 /* Allocating and  urb */ 
	 cbw_urb = usb_alloc_urb(0, GFP_KERNEL) ; 
	 
	 if(!cbw_urb ) 
	 {
		 pr_info(" MY_URB ALLOC_ERRR\n"); 

		 return -ENOMEM ; 
	 } 


	  memset(&my_cbw , 0 , sizeof(my_cbw)) ; 
	 my_cbw.dCBWSignature = cpu_to_le32(0x43425355); 
	 my_cbw.dCBWTag = cpu_to_le32(0x12345678); 
       	 my_cbw.dCBWDataTransferLength = cpu_to_le32(512); 
         my_cbw.bmCBWFlags = 0x80;
         my_cbw.bCBWLUN = 0 ; 
	 my_cbw.bCBWCBLength = 10;

	my_cbw.CBWCB[0] = 0x28;
	my_cbw.CBWCB[1] = 0x00;
	my_cbw.CBWCB[2] = 0x00 ;
	my_cbw.CBWCB[3] = 0x00 ;
	my_cbw.CBWCB[4] =  0x00  ;
	my_cbw.CBWCB[5] = 0x00;
	my_cbw.CBWCB[6] = 0x00;
	my_cbw.CBWCB[7] = 0x00;
	my_cbw.CBWCB[8] = 0x01;
	my_cbw.CBWCB[9] = 0x00;



	memcpy(cbw_buffer , &my_cbw, sizeof(my_cbw)); 
 
	 usb_clear_halt(dev, usb_rcvbulkpipe(dev, bulk_in_endpointaddr));		 	 
	 usb_clear_halt(dev, usb_sndbulkpipe(dev, bulk_out_endpointaddr));	

	
	 usb_fill_bulk_urb(cbw_urb, dev , usb_sndbulkpipe(dev, bulk_out_endpointaddr), cbw_buffer , CBW_LEN , cbw_callback , NULL ) ; 


	 int ret = usb_submit_urb(cbw_urb , GFP_KERNEL); 
	 
	 if(ret) 
	 { 
		 pr_info(" URB SUBMISSION  ERR %d \n", ret) ; 
		  
		 goto r_urb ; 
	 } 


 
	 pr_info(" CBW SEND SUCCESSFULL  \n"); 

	 return 0 ; 


/* cleanups */
r_urb :
	if(cbw_urb) 
	{ 

	 pr_info("CBW_URB_ERR - CLEANING CWB_URB \n");

	 usb_kill_urb(cbw_urb); 
	 usb_free_urb(cbw_urb); 
         kfree(cbw_buffer) ;
	 cbw_buffer = NULL; 
	 cbw_urb =NULL; 
 
	 usb_clear_halt(dev, usb_rcvbulkpipe(dev, bulk_in_endpointaddr));		 	 
	 usb_clear_halt(dev, usb_sndbulkpipe(dev, bulk_out_endpointaddr));	
      
	 return 0 ; 
	
	}else{ 
	       pr_info(" NO CBW_URB_ERR \n"); 
	 	return 0 ; 
	} 
	return 0 ; 



		
} 


/* Usb  Disconnect */
static void usb_disconnect ( struct usb_interface  *interface ) 
{ 


 
	struct usb_device *dev = interface_to_usbdev(interface) ; 

	/*   -- CLEAN UPS ON DISCONNECT  -- */  

	if(cbw_urb) 
	{	
	usb_kill_urb(cbw_urb) ; 
	usb_free_urb(cbw_urb) ; 
	kfree(cbw_buffer) ; 
        cbw_urb = NULL ; 
	cbw_buffer = NULL ;
	} 

	if(write_urb) 
	{ 
	usb_kill_urb(write_urb); 
	usb_free_urb(write_urb); 
	kfree(write_buffer);
        write_urb = NULL ; 
	write_buffer= NULL ; 
	} 
	

	if(csw_urb)
	{
	usb_kill_urb(csw_urb); 
	usb_free_urb(csw_urb); 
	kfree(csw_buffer); 
        csw_urb = NULL ; 
	csw_buffer = NULL ; 
	} 

	
	if(read_buf) 
	{ 
	usb_kill_urb(read_urb); 
	usb_free_urb(read_urb); 
	kfree(read_buf); 
        read_urb = NULL ; 
	read_buf = NULL ; 
	} 
 

	if( inquiry_urb) 
	{ 
	usb_kill_urb(inquiry_urb); 
	usb_free_urb(inquiry_urb); 
	kfree(inquiry_buffer) ;
	inquiry_urb = NULL ; 
	inquiry_buffer = NULL ; 
	} 


	
	cbw_tag = 0 ; 	

	usb_set_intfdata(interface, NULL) ; 

	usb_clear_halt(dev, usb_rcvbulkpipe(dev, bulk_in_endpointaddr));		 	 
	usb_clear_halt(dev, usb_sndbulkpipe(dev, bulk_out_endpointaddr));	

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



