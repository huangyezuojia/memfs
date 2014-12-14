#
# Makefile for the FUSE filesystem.
#

CPUNR = $(shell cat /proc/cpuinfo |grep process |wc -l)
KERNELDIR ?= /lib/modules/`uname -r`/build
PWD ?= $(shell pwd)

obj-m := fuse.o
fuse-objs := dev.o dir.o file.o inode.o control.o

default:
	$(MAKE) -j $(CPUNR) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
