kernelver ?= $(shell uname -r)
KDIR	?= /lib/modules/$(kernelver)/build
PWD	:= $(shell pwd)

MODDEFS := CONFIG_DVB_CORE=m CONFIG_DVB_DDBRIDGE=m CONFIG_DVB_DRXK=m CONFIG_DVB_TDA18271C2DD=m CONFIG_DVB_CXD2099=m CONFIG_DVB_LNBP21=m  CONFIG_DVB_STV090x=m CONFIG_DVB_STV6110x=m CONFIG_DVB_STV0367=m CONFIG_DVB_TDA18212=m CONFIG_DVB_STV0367DD=m CONFIG_DVB_TDA18212DD=m CONFIG_DVB_OCTONET=m CONFIG_DVB_CXD2843=m CONFIG_DVB_STV0910=m CONFIG_DVB_STV6111=m CONFIG_DVB_LNBH25=m CONFIG_DVB_MXL5XX=m CONFIG_DVB_NET=y DDDVB=y

KBUILD_EXTMOD = $(PWD)

ifeq ($(KERNEL_DVB_CORE),y)
DDDVB_INC = "--include=$(KBUILD_EXTMOD)/include/dd_compat.h -I$(KBUILD_EXTMOD)/frontends -I$(KBUILD_EXTMOD) -DKERNEL_DVB_CORE=y"
else
DDDVB_INC = "--include=$(KBUILD_EXTMOD)/include/dd_compat.h -I$(KBUILD_EXTMOD)/frontends -I$(KBUILD_EXTMOD)/include -I$(KBUILD_EXTMOD)/include/linux"
endif

all: 
	$(MAKE) -C $(KDIR) KBUILD_EXTMOD=$(PWD) $(MODDEFS) modules NOSTDINC_FLAGS=$(DDDVB_INC)
	$(MAKE) -C apps

libdddvb:
	$(MAKE) -C lib

libdddvb-install:
	$(MAKE) -C lib install

libdddvb-clean:
	$(MAKE) -C lib clean

dep:
	DIR=`pwd`; (cd $(TOPDIR); make KBUILD_EXTMOD=$$DIR dep)

install: all
	$(MAKE) -C $(KDIR) KBUILD_EXTMOD=$(PWD) modules_install
	depmod $(kernelver)

clean:
	rm -rf */.*.o.d */*.o */*.ko */*.mod.c */.*.cmd .tmp_versions Module* modules*


