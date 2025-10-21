#include <linux/ioctl.h>
#include <linux/types.h>

#define IO   _IO() 
#define IO_WRITE  _IOW('a','b',int32_t)
#define IO_READ   _IOR('a','b',int32_t)
#define IO_BOTH   _IOWR('a','b',int32_t) 
