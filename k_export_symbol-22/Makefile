obj-m +=export_symbol.o
obj-m +=export_symbol_2.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd) 

all:
	$(MAKE) -C $(KDIR)  M=$(PWD)  modules 
clean:
	$(MAKE) -C $(KDIR)  M=$(PWD)  clean 
