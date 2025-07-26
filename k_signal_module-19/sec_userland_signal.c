#include <stdio.h>
#include <fcntl.h> 
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h> 


#define SIGNO  44 
#define IO_READ  _IOR('a', 'b', int ) 
#define IO_WRITE  _IOW('a', 'b', int )
static int done = 0 ; 
static int check = 0 ; 


void ctrl_c_handler  ( int n, siginfo_t *t , void *unused ) 
{ 

	printf(" CTRL_C HANDLER \n"); 
	if ( n == SIGINT ) 
	{ 
		printf("Recieved  ctrl_c  \n");
	       done = 1 ; 	
	} 

	return ; 
} 


void  my_signal_handler( int n,  siginfo_t *t , void *unused)  
{ 
	printf(" MY_SIG HANDLER \n");
	if(n = SIGNO) 
	{ 
		printf(" Recieved  my_signal \n") ;
	       check = 1 ; 	
	} 
	return ; 
} 



int main () 

{

	int my_value ;


	struct  sigaction  act ; 

	/*  settings for  ctrl */ 
	sigemptyset(&act.sa_mask);
       	act.sa_flags  = ( SA_SIGINFO  | SA_RESTART) ; 
	act.sa_sigaction = ctrl_c_handler; 
	sigaction(SIGINT , &act , NULL ) ; 


	/*  settings for   custom signal  */ 
	sigemptyset(&act.sa_mask);
       	act.sa_flags  = ( SA_SIGINFO   | SA_RESTART) ; 
	act.sa_sigaction =  my_signal_handler; 
	sigaction(SIGNO  , &act , NULL ) ; 

	

	int fd = open("/dev/mychardev", O_RDWR); 
	if( fd < 0) 
	{ 
		printf(" ALLOC_ERR\n"); 
		return -1 ; 
	} 




	printf("  SENDIND CMD \n") ;
	 
	if(ioctl( fd , IO_WRITE , my_value )<0) 
	{ 
		printf(" FAILED TO WRITE \n") ; 
		close(fd); 
	} 

	while (!done) 
	{ 

	printf(" WATING FOR SIGNAL \n"); 
	while (!done && !check) ; 
	check = 0 ; 
	} 



	close(fd) ; 
	printf(" CLOSING  DRIVER \n") ; 
	return 0 ; 

	
} 
