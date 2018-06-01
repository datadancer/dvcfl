#KERNELDIR should be set as your kernel build directory.
KERNELDIR := /workspace/difuze/AndroidKernels/huawei/mate9/Code_Opensource/out-hello
PWD := $(shell pwd)
ARCH := arm64
#ARCH := arm

CROSS_COMPILE := aarch64-linux-android-
#CROSS_COMPILE := arm-linux-gnueabi-
CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld

obj-m := dvcfl.o

all: getdvcfl modules

getdvcfl: getdvcfl.c
	aarch64-linux-gnu-gcc -static getdvcfl.c -o getdvcfl

modules:
	$(MAKE) -C $(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules 
 
clean:
	rm -f *.o *.ko *.mod.c *.markers *.order *.symvers getdvcfl
