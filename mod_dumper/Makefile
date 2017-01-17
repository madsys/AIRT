CC	= gcc -O3 -Wall
ifneq ($(KERNELRELEASE),)
obj-m       += modumper.o

else
KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)
BOX	= `uname -n`

all: 	modumper dismod
modumper:
	@echo -e "\nCompiling modumper on $(BOX)...\n"
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

dismod:	i386-dis.c utils.h bfd.h opintl.h dismod.c dis-asm.h
	$(CC) i386-dis.c dismod.c -o dismod


clean_all:
	rm -rf dismod *.ko *.o *.mod.* .*.cmd .tmp_versions
clean:
	rm -rf dismod *.o *.mod.* .*.cmd .tmp_versions
	
endif
