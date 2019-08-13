#
# Makefile for the kernel DVB device drivers.
#

dvb-core-objs := dvbdev.o dmxdev.o dvb_demux.o dvb_filter.o 	\
		 dvb_ca_en50221.o dvb_frontend.o 		\
		 dvb_net.o dvb_ringbuffer.o dvb_math.o

obj-$(CONFIG_DVB_CORE) += dvb-core.o

EXTRA_CFLAGS += -DCONFIG_DVB_DYNAMIC_MINORS -DCONFIG_DVB_NET
NOSTDINC_FLAGS += -I$(KBUILD_EXTMOD)/include -I$(KBUILD_EXTMOD)/dvb-core
