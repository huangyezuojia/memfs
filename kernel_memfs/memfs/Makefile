CPUNR = $(shell cat /proc/cpuinfo |grep processor |wc -l)

KERNELDIR  ?=/lib/modules/`uname -r`/build
PWD ?= $(shell pwd)

obj-m := memfs.o

default:
	$(MAKE) -j $(CPUNR) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

