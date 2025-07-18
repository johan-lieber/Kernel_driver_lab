obj-m +=spinlock_module.o
obj-m +=spinlock_with_interrupts.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd) 

all:
	$(MAKE) -C $(KDIR)  M=$(PWD)  modules 
clean:
	$(MAKE) -C $(KDIR)  M=$(PWD)  clean 
