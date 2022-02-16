#
# Makefile for the kernel multimedia device drivers.
#

ifeq ($(KERNEL_DVB_CORE),y)
obj-y        := ddbridge/       \
		frontends/

else
obj-y        := dvb-core/	\
		ddbridge/       \
		frontends/
endif