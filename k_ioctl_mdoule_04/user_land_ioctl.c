#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "ioctl_header.h"
#include <unistd.h>


struct my_data d ; 
 




int main () 
{

	d.a = 23 ; 
	d.b = 33 ; 
	int fd = open("/dev/mychardev" , O_RDWR) ; 
	if(fd < 0) 
	{
		perror("OPEN_ERR");
		return 0 ; 
	}


	ioctl(fd , IOCTL_CMD_RESET) ; 

	int my_value = 63; 

	ioctl(fd , IOCTL_CMD_SET_VAL, &my_value) ;

	my_value = 0 ;

	ioctl(fd , IOCTL_CMD_GET_VAL , &my_value) ;

	printf("  Value: %d\n", my_value ) ;

	ioctl(fd , IOCTL_CMD_CALC ,  &d ) ;

	printf("Result : %d \n", d.result) ;

	close(fd);

	return 0;
}

