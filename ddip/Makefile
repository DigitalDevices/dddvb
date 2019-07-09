KDIR	?= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)

MODDEFS := CONFIG_DVB_DDIP=m 

all: 
	$(MAKE) -C $(KDIR) KBUILD_EXTMOD=$(PWD) $(MODDEFS) modules

dep:
	DIR=`pwd`; (cd $(TOPDIR); make KBUILD_EXTMOD=$$DIR dep)

install: all
	$(MAKE) -C $(KDIR) KBUILD_EXTMOD=$(PWD) modules_install

clean:
	rm -rf */*.o */*.ko */*.mod.c */.*.cmd .tmp_versions Module* modules*


