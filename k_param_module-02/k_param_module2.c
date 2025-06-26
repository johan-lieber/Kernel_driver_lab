#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>


MODULE_LICENSE("GPL");


static int my_int = 0 ;
static  short int myshort = 1 ; 
static char *mychar = "hello_world";
static int arr [2] = { 22 , 33 } ;
static int arr_store = 0  ; 

module_param(my_int , int , 0644);
MODULE_PARM_DESC(my_int , " an intefar ");

module_param(myshort , short  , 0644);
MODULE_PARM_DESC(myshort, "nyshoet ");


module_param(mychar , charp , 0644);
MODULE_PARM_DESC(mychar, "string ");

module_param_array(arr , int , &arr_store , 0644);
MODULE_PARM_DESC(arr, "array store");



static int __init  param_init_module(void)
{
	pr_info(" Initial value of my int : %d \n",my_int);
	pr_info(" Value of myshort : %d \n", myshort);
	pr_info(" content of the strings %s \n", mychar);

	// looping to print value 
	for (int i = 0 ; i< ARRAY_SIZE(arr); i++)
	{
		pr_info(" Index int :%d \n",mychar[i]);

		pr_info("  second unt is %d \n",arr_store);
		if(i  > 5 )
		{
			break;
		}

	}
	return 0 ;
}


static void __exit param_exit_module(void)
{
	printk("good bye you idiot");

}


module_init(param_init_module);
module_exit(param_exit_module);

