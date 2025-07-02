
struct my_data  {
	int a ;
	int b ; 
	int result ; 
};



#define  MY_IOCTL_MAGIC 'X' 
#define  IOCTL_CMD_RESET     _IO(MY_IOCTL_MAGIC, 1 ) 
#define  IOCTL_CMD_SET_VAL   _IOW(MY_IOCTL_MAGIC , 2 , int ) 
#define  IOCTL_CMD_GET_VAL   _IOR(MY_IOCTL_MAGIC , 3 , int ) 
#define  IOCTL_CMD_CALC      _IOWR(MY_IOCTL_MAGIC , 3, struct my_data ) 
