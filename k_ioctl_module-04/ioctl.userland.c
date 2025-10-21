#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "ioctl_headers.h"
#include <unistd.h>
#include <stdint.h>


int main () 
{ 
	int a = 11; 
	int b = 22; 
	int c ; 
	int fd = open("/dev/ioctl_drv",O_RDWR) ;
	if( fd < 0)
	{
		printf("open() error\n"); 
		return -1;
	} 

	ioctl(fd,IO_WRITE,&a) ; 

	printf("write() success \n"); 

	ioctl(fd,IO_READ,&c);

	printf("Read() %d\n",c); 

	printf(" ioctl successful\n"); 

	return 0; 
} 

