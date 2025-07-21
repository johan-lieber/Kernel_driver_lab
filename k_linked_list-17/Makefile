obj-m +=linked_list.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd) 

all:
	$(MAKE) -C $(KDIR)  M=$(PWD)  modules 
clean:
	$(MAKE) -C $(KDIR)  M=$(PWD)  clean 
