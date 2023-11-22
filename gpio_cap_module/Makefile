
PWD := $(shell pwd)

obj-m += gpio-capture.o
obj-m += gpio-capture-platform.o


.PHONY: all clean

# COMPILE=/home/cywang/ws/mad35/MA35D1_Buildroot/output/host/bin/aarch64-nuvoton-linux-gnu-

LINUX_DIR=/home/user/NUC970_Buildroot-master/output/build/linux-master
#LINUX_DIR=/home/derek/workspace/nuc980/work/nuc980bsp/linux-4.4.y/kernel-out

COMPILE = arm-linux-


all:
	make CROSS_COMPILE=$(COMPILE) -C $(LINUX_DIR) M=$(PWD) modules

clean:
	make CROSS_COMPILE=$(COMPILE) -C $(LINUX_DIR) M=$(PWD) clean 



