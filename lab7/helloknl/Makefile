ifneq ($(KERNELRELEASE),) 
	module-objs := hellokernel.o 
	obj-m := hellokernel.o 
else
    # KERNELDIR := /lib/modules/$(shell uname -r)/build
	KERNELDIR := /home/panda/code/linux/build/rawkernel
    PWD := $(shell pwd)
modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif	
clean:
	rm -rf module.*
	rm -rf *.mod.*
	rm -rf .*.cmd
	rm -rf *.o *.ko
	rm -rf *.mod modules.order Module.symvers .tmp_versions
