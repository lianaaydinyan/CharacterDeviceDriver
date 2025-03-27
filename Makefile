CONFIG_MODULE_SIG=n
CONFIG_MODULE_SIG_ALL=n

obj-m := drive.o
KERNELDIR := /lib/modules/$(shell uname -r)/build

all:
	@clear
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	@clear
	rm -f Module.symvers modules.order *.ko *.o *.mod *.mod.c
	make -C $(KERNELDIR) M=$(PWD) clean

load:
	@clear
	@echo Loading drive into kernel . . . 
	insmod drive.ko
	@echo ~DONE~

	@echo Wait to check . . .
	lsmod | grep drive
	@echo ~DONE~

	@echo drive ~ 
	dmesg | tail
	@echo ~DONE~

unload:
	@clear
	@echo Remove device 
	rm -f /dev/liana
	@echo ~DONE~

	@echo Unloading module from the kernel
	rmmod drive
	@echo ~DONE~
