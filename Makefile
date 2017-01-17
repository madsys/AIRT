#version 0.4

BOX     = `uname -r`
KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)
obj	:= sock_hunter process_hunter mod_hunter
CFLAGS	:= -O3 -I/lib/modules/`uname -r`/build/include/asm-i386/mach-default/


export CFLAGS

obj-m	+= sock_hunter.o 

dummy:
	@echo -e "\nall: \t   compile all the project"
	@echo -e "default:   compile all the project except for modumper"
	@echo -e "modumper:  compile the modumper only"
	@echo -e "clean:\t   clean all trash except the .ko files"
	@echo -e "clean_all: remove all the compiled file\n"

all:	default modumper

default:

	@echo -e "\nCompiling $(obj) on $(BOX)...\n"
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

modumper:
	@echo -e "\nCompiling modumper on $(BOX)...\n"
	cd mod_dumper && make
	
clean:
	rm -rf  *.o .*.cmd *.mod.* .tmp_versions
	cd mod_dumper && make clean
	
clean_all:
	rm -rf  *.o .*.cmd *.mod.* .tmp_versions *.ko 
	cd mod_dumper && make clean_all
