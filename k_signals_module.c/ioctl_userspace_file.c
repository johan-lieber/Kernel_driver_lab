#include <stdio.h> 
#include <fcntl.h> 
#include <sys/ioctl.h> 
#include <unistd.h>
#include <sys/types.h> 
#include <signal.h> 
#include <stdlib.h>




#define IO_READ  _IOR('a','b',int ) 
#define  IO_WRITE  _IOW('a', 'b' , int )
#define SIGNO  44 

static int done = 0 ; 
int check = 0 ; 

static int my_value ; 

void ctrl_c_handler ( int n , siginfo_t *info , void *unused) 
{ 
	if(n == SIGINT) 
	{ 
		printf( " \n Recieved ctril_c \n")  ; 
		done =1 ; 
	} 

} 


void sig_event_handler ( int n , siginfo_t *info , void *unused) 
{ 
	if(n== SIGNO) 
	{
		check = info->si_int ; 
		printf(" Recieved  signal from   kernel : %u\n", check ) ; 
	} 
} 





int main () 
{ 

	int fd ; 
	int value  , no ; 
	struct sigaction act ; 

	sigemptyset(&act.sa_mask) ; 
	act.sa_flags = (SA_SIGINFO | SA_RESETHAND); 
	act.sa_sigaction = ctrl_c_handler ; 
	sigaction( SIGINT , &act , NULL) ; 




	/*custom  signal handler */ 

	sigemptyset(&act.sa_mask) ; 
	act.sa_flags = (SA_SIGINFO | SA_RESTART) ; 
	act.sa_sigaction =sig_event_handler ; 
	sigaction(SIGNO , &act , NULL ) ; 


	printf(" installed signals from signo :%d \n", SIGNO) ; 





	/* OPEING DRIVER */ 

	 fd =  open( "/dev/mychardev" , O_RDWR) ; 
	 if( fd  < 0 ) 
	 { 
		 return -1 ; 
	 } 




	 printf(" REGESTER APPP\n") ; 

	 if(ioctl(fd , IO_WRITE,  my_value)< 0 ) 
	 { 
		 printf(" falied \n")  ; 
		 close(fd); 
		 exit(1); 
	 } 

	 printf("done " ) ; 



	 while (!done) 
	 { 
		 printf(" waiting for signal \n"); 

		 while(!done && !check) ; 
		 check = 0 ; 
	 } 


	 printf( "closing driver \n") ; 
	 close(fd) ; 

} 

