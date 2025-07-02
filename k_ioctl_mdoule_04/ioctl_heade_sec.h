#define MY_IOCTL 'X' 
#define IOCTL_CMD_RESET _IO(MY_IOCTL, 1 ) 
#define IOCTL_CMD_SET_VAL _IOW(MY_IOCTL ,2 , int ) 
#define IOCTL_CMD_GET_VAL _IOR(MY_IOCTL , 3 , int ) 

