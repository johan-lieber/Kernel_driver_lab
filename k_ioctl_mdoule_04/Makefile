obj-m +=ioctl_second_module.o

KDIR := /lib/modules/$(shell uname -r)/build/

PWD  := $(shell pwd) 

all:
	$(MAKE) -C $(KDIR)  M=$(PWD) modules

clean : 
	$(MAKE) -C $(KFIR)  M=$(PWD)  clean 

