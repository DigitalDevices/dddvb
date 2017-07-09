kernelver ?= $(shell uname -r)
KDIR	?= /lib/modules/$(kernelver)/build
PWD	:= $(shell pwd)

MODDEFS := CONFIG_DVB_CORE=m CONFIG_DVB_DDBRIDGE=m CONFIG_DVB_DRXK=m CONFIG_DVB_TDA18271C2DD=m CONFIG_DVB_CXD2099=m CONFIG_DVB_LNBP21=m  CONFIG_DVB_STV090x=m CONFIG_DVB_STV6110x=m CONFIG_DVB_STV0367=m CONFIG_DVB_TDA18212=m CONFIG_DVB_STV0367DD=m CONFIG_DVB_TDA18212DD=m CONFIG_DVB_OCTONET=m CONFIG_DVB_CXD2843=m CONFIG_DVB_STV0910=m CONFIG_DVB_STV6111=m CONFIG_DVB_LNBH25=m CONFIG_DVB_MXL5XX=m

all: 
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) $(MODDEFS) modules
	$(MAKE) -C apps

dep:
	DIR=`pwd`; (cd $(TOPDIR); make SUBDIRS=$$DIR dep)

install: all
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules_install

clean:
	rm -rf */*.o */*.ko */*.mod.c */.*.cmd */.*.o.d .tmp_versions Module* modules*


