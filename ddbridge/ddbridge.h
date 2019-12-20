/*
 * ddbridge.h: Digital Devices PCIe bridge driver
 *
 * Copyright (C) 2010-2019 Digital Devices GmbH
 *                         Marcus Metzler <mocm@metzlerbros.de>
 *                         Ralph Metzler <rjkm@metzlerbros.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 only, as published by the Free Software Foundation.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, point your browser to
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef _DDBRIDGE_H_
#define _DDBRIDGE_H_

#define DDB_USE_WORK
/*#define DDB_TEST_THREADED*/

#include <linux/version.h>

#if (KERNEL_VERSION(3, 8, 0) <= LINUX_VERSION_CODE)
#define __devexit
#define __devinit
#define __devinitconst
#endif

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/io.h>
#include <linux/pci.h>
/*#include <linux/pci_ids.h>*/
#include <linux/timer.h>
#include <linux/i2c.h>
#include <linux/swab.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/completion.h>

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#include <linux/dvb/ca.h>
#include <linux/socket.h>
#include <linux/device.h>
#include <linux/io.h>

#include "dvb_netstream.h"
#include "dmxdev.h"
#include "dvbdev.h"
#include "dvb_demux.h"
#include "dvb_frontend.h"
#include "dvb_ringbuffer.h"
#include "dvb_ca_en50221.h"
#include "dvb_net.h"

#include "tda18271c2dd.h"
#include "stv6110x.h"
#include "stv090x.h"
#include "lnbh24.h"
#include "drxk.h"
#include "stv0367dd.h"
#include "tda18212dd.h"
#include "cxd2843.h"
#include "cxd2099.h"
#include "stv0910.h"
#include "stv6111.h"
#include "lnbh25.h"
#include "mxl5xx.h"

#include "ddbridge-regs.h"
#include "ddbridge-mci.h"

#define DDB_MAX_I2C    32
#define DDB_MAX_PORT   32
#define DDB_MAX_INPUT  64
#define DDB_MAX_OUTPUT 32
#define DDB_MAX_LINK    4
#define DDB_LINK_SHIFT 28

#define DDB_LINK_TAG(_x) ((_x) << DDB_LINK_SHIFT)

struct ddb_regset {
	u32 base;
	u32 num;
	u32 size;
};

struct ddb_regmap {
	u32 irq_version;
	u32 irq_base_i2c;
	u32 irq_base_idma;
	u32 irq_base_odma;
	u32 irq_base_gtl;
	u32 irq_base_rate;

	const struct ddb_regset *i2c;
	const struct ddb_regset *i2c_buf;
	const struct ddb_regset *idma;
	const struct ddb_regset *idma_buf;
	const struct ddb_regset *odma;
	const struct ddb_regset *odma_buf;

	const struct ddb_regset *input;
	const struct ddb_regset *output;

	const struct ddb_regset *channel;
	const struct ddb_regset *gtl;
};

struct ddb_ids {
	u16 vendor;
	u16 device;
	u16 subvendor;
	u16 subdevice;

	u32 hwid;
	u32 regmapid;
	u32 devid;
	u32 mac;
};

struct ddb_info {
	u32   type;
#define DDB_NONE             0
#define DDB_OCTOPUS          1
#define DDB_OCTOPUS_CI       2
#define DDB_MOD              3
#define DDB_OCTONET          4
#define DDB_OCTOPUS_MAX      5
#define DDB_OCTOPUS_MAX_CT   6
#define DDB_OCTOPRO          7
#define DDB_OCTOPRO_HDIN     8
#define DDB_OCTOPUS_MCI      9
	u32   version;
	char *name;
	u32   i2c_mask;
	u32   board_control;
	u32   board_control_2;

	u8    port_num;
	u8    led_num;
	u8    fan_num;
	u8    temp_num;
	u8    temp_bus;
	u8    ns_num;
	u8    con_clock; /* use a continuous clock */
	u8    ts_quirks;
#define TS_QUIRK_SERIAL    1
#define TS_QUIRK_REVERSED  2
#define TS_QUIRK_NO_OUTPUT 4
#define TS_QUIRK_ALT_OSC   8
	u8    mci_ports;
	u8    mci_type;

	u32   tempmon_irq;
	u32   lostlock_irq;
	u32   mdio_base;
	u32   hw_min;
	const struct ddb_regmap *regmap;
};

#define DMA_MAX_BUFS 32      /* hardware table limit */
#define OUTPUT_DMA_BUFS_SDR 32
#define OUTPUT_DMA_SIZE_SDR (256 * 1024)

struct ddb;
struct ddb_port;

struct ddb_dma {
	void                  *io;
	u32                    regs;
	u32                    bufregs;

	dma_addr_t             pbuf[DMA_MAX_BUFS];
	u8                    *vbuf[DMA_MAX_BUFS];
	u32                    num;
	u32                    size;
	u32                    div;
	u32                    bufval;

#ifdef DDB_USE_WORK
	struct work_struct     work;
#else
	struct tasklet_struct  tasklet;
#endif
	spinlock_t             lock; /* DMA lock */
	wait_queue_head_t      wq;
	int                    running;
	u32                    stat;
	u32                    ctrl;
	u32                    cbuf;
	u32                    coff;
};

struct ddb_dvb {
	struct dvb_adapter    *adap;
	int                    adap_registered;
	struct dvb_device     *dev;
	struct dvb_frontend   *fe;
	struct dvb_frontend   *fe2;
	struct dmxdev          dmxdev;
	struct dvb_demux       demux;
	struct dvb_net         dvbnet;
	struct dvb_netstream   dvbns;
	struct dmx_frontend    hw_frontend;
	struct dmx_frontend    mem_frontend;
	int                    users;
	u32                    attached;
	u8                     input;

	enum fe_sec_tone_mode  tone;
	enum fe_sec_voltage    voltage;

	int (*i2c_gate_ctrl)(struct dvb_frontend *, int);
	int (*set_voltage)(struct dvb_frontend *fe,
			   enum fe_sec_voltage voltage);
	int (*set_input)(struct dvb_frontend *fe, int input);
	int (*diseqc_send_master_cmd)(struct dvb_frontend *fe,
				      struct dvb_diseqc_master_cmd *cmd);
};

struct ddb_ci {
	struct dvb_ca_en50221  en;
	struct ddb_port       *port;
	u32                    nr;
};

struct ddb_io {
	struct ddb_port       *port;
	u32                    nr;
	u32                    regs;
	struct ddb_dma        *dma;
	struct ddb_io         *redo;
	struct ddb_io         *redi;
};

#define ddb_output ddb_io
#define ddb_input ddb_io

struct ddb_i2c {
	struct ddb            *dev;
	u32                    nr;
	u32                    regs;
	u32                    link;
	struct i2c_adapter     adap;
	u32                    rbuf;
	u32                    wbuf;
	u32                    bsize;
	struct completion      completion;
};

struct ddb_port {
	struct ddb            *dev;
	u32                    nr;
	u32                    pnr;
	u32                    regs;
	u32                    lnr;
	struct ddb_i2c        *i2c;
	struct mutex           i2c_gate_lock; /* I2C access lock */
	u32                    class;
#define DDB_PORT_NONE           0
#define DDB_PORT_CI             1
#define DDB_PORT_TUNER          2
#define DDB_PORT_LOOP           3
#define DDB_PORT_MOD            4
	char                   *name;
	char                   *type_name;
	u32                     type;
#define DDB_TUNER_DUMMY          0xffffffff
#define DDB_TUNER_NONE           0
#define DDB_TUNER_DVBS_ST        1
#define DDB_TUNER_DVBS_ST_AA     2
#define DDB_TUNER_DVBCT_TR       3
#define DDB_TUNER_DVBCT_ST       4
#define DDB_CI_INTERNAL          5
#define DDB_CI_EXTERNAL_SONY     6
#define DDB_TUNER_DVBCT2_SONY_P  7
#define DDB_TUNER_DVBC2T2_SONY_P 8
#define DDB_TUNER_ISDBT_SONY_P   9
#define DDB_TUNER_DVBS_STV0910_P 10
#define DDB_TUNER_MXL5XX         11
#define DDB_CI_EXTERNAL_XO2      12
#define DDB_CI_EXTERNAL_XO2_B    13
#define DDB_TUNER_DVBS_STV0910_PR 14
#define DDB_TUNER_DVBC2T2I_SONY_P 15

#define DDB_TUNER_XO2            32
#define DDB_TUNER_DVBS_STV0910   (DDB_TUNER_XO2 + 0)
#define DDB_TUNER_DVBCT2_SONY    (DDB_TUNER_XO2 + 1)
#define DDB_TUNER_ISDBT_SONY     (DDB_TUNER_XO2 + 2)
#define DDB_TUNER_DVBC2T2_SONY   (DDB_TUNER_XO2 + 3)
#define DDB_TUNER_ATSC_ST        (DDB_TUNER_XO2 + 4)
#define DDB_TUNER_DVBC2T2I_SONY  (DDB_TUNER_XO2 + 5)

#define DDB_TUNER_MCI            48
#define DDB_TUNER_MCI_SX8       (DDB_TUNER_MCI + 0)
#define DDB_TUNER_MCI_M4        (DDB_TUNER_MCI + 1)

	struct ddb_input      *input[2];
	struct ddb_output     *output;
	struct dvb_ca_en50221 *en;
	struct ddb_dvb         dvb[2];
	u32                    gap;
	u32                    obr;
	u8                     creg;
};

struct mod_base {
	u32                    frequency;
	u32                    flat_start;
	u32                    flat_end;
};

struct ddb_mod {
	struct ddb_port       *port;

	u32                    frequency;
	u32                    modulation;
	u32                    symbolrate;

	u64                    obitrate;
	u64                    ibitrate;
	u32                    pcr_correction;

	u32                    rate_inc;
	u32                    Control;
	u32                    State;
	u32                    StateCounter;
	s32                    LastPCRAdjust;
	s32                    PCRAdjustSum;
	s32                    InPacketsSum;
	s32                    OutPacketsSum;
	s64                    PCRIncrement;
	s64                    PCRDecrement;
	s32                    PCRRunningCorr;
	u32                    OutOverflowPacketCount;
	u32                    InOverflowPacketCount;
	u32                    LastOutPacketCount;
	u32                    LastInPacketCount;
	u64                    LastOutPackets;
	u64                    LastInPackets;
	u32                    MinInputPackets;
};

#define CM_STARTUP_DELAY 2
#define CM_AVERAGE  20
#define CM_GAIN     10

#define HW_LSB_SHIFT    12
#define HW_LSB_MASK     0x1000

#define CM_IDLE    0
#define CM_STARTUP 1
#define CM_ADJUST  2

#define TS_CAPTURE_LEN  (4096)

/* net streaming hardware block */

#define DDB_NS_MAX 15

struct ddb_ns {
	struct ddb_input      *input;
	int                    nr;
	struct ddb_input      *fe;
	u32                    rtcp_udplen;
	u32                    rtcp_len;
	u32                    ts_offset;
	u32                    udplen;
	u8                     p[512];
};

struct ddb_lnb {
	struct mutex           lock; /* lock lnb access */
	u32                    tone;
	enum fe_sec_voltage    oldvoltage[4];
	u32                    voltage[4];
	u32                    voltages;
	u32                    fmode;
};

struct ddb_irq {
	void                   (*handler)(void *);
	void                   *data;
};

struct ddb_link {
	struct ddb            *dev;
	const struct ddb_info *info;
	u32                    nr;
	u32                    regs;
	spinlock_t             lock; /* lock link access */
	struct mutex           flash_mutex; /* lock flash access */
	struct ddb_lnb         lnb;
	struct tasklet_struct  tasklet;
	struct ddb_ids         ids;

	spinlock_t             temp_lock; /* lock temp chip access */
	int                    over_temperature_error;
	u8                     temp_tab[11];
	struct ddb_irq         irq[256];
};

struct ddb {
	struct pci_dev        *pdev;
	struct platform_device *pfdev;
	struct device         *dev;

	int                    msi;
	struct workqueue_struct *wq;
	u32                    has_dma;
	u32                    has_ns;

	struct ddb_link        link[DDB_MAX_LINK];
	unsigned char         *regs;
	u32                    regs_len;
	u32                    port_num;
	struct ddb_port        port[DDB_MAX_PORT];
	u32                    i2c_num;
	struct ddb_i2c         i2c[DDB_MAX_I2C];
	struct ddb_input       input[DDB_MAX_INPUT];
	struct ddb_output      output[DDB_MAX_OUTPUT];
	struct dvb_adapter     adap[DDB_MAX_INPUT];
	struct ddb_dma         idma[DDB_MAX_INPUT];
	struct ddb_dma         odma[DDB_MAX_OUTPUT];

	struct device         *ddb_dev;
	u32                    ddb_dev_users;
	u32                    nr;
	u8                     iobuf[1028];

	u8                     leds;
	u32                    ts_irq;
	u32                    i2c_irq;

	int                    ns_num;
	struct ddb_ns          ns[DDB_NS_MAX];
	int                    vlan;
	struct mutex           mutex; /* lock accces to global ddb array */

	struct dvb_device     *nsd_dev;
	u8                     tsbuf[TS_CAPTURE_LEN];

	struct mod_base        mod_base;
	struct ddb_mod         mod[24];
	struct mutex           ioctl_mutex; /* lock extra ioctls */
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

#define dd_uint8    u8
#define dd_uint16   u16
#define dd_int16    s16
#define dd_uint32   u32
#define dd_int32    s32
#define dd_uint64   u64
#define dd_int64    s64

#define DDMOD_FLASH_START  0x1000

struct DDMOD_FLASH_DS {
	dd_uint32   Symbolrate;             /* kSymbols/s */
	dd_uint32   DACFrequency;           /* kHz        */
	dd_uint16   FrequencyResolution;    /* kHz        */
	dd_uint16   IQTableLength;
	dd_uint16   FrequencyFactor;
	dd_int16    PhaseCorr;              /* TBD        */
	dd_uint32   Control2;
	dd_uint16   PostScaleI;
	dd_uint16   PostScaleQ;
	dd_uint16   PreScale;
	dd_int16    EQTap[11];
	dd_uint16   FlatStart;
	dd_uint16   FlatEnd;
	dd_uint32   FlashOffsetPrecalculatedIQTables;       /* 0 = none */
	dd_uint8    Reserved[28];

};

struct DDMOD_FLASH {
	dd_uint32   Magic;
	dd_uint16   Version;
	dd_uint16   DataSets;

	dd_uint16   VCORefFrequency;    /* MHz */
	dd_uint16   VCO1Frequency;      /* MHz */
	dd_uint16   VCO2Frequency;      /* MHz */

	dd_uint16   DACAux1;    /* TBD */
	dd_uint16   DACAux2;    /* TBD */

	dd_uint8    Reserved1[238];

	struct DDMOD_FLASH_DS DataSet[1];
};

#define DDMOD_FLASH_MAGIC   0x5F564d5F

int ddbridge_flashread(struct ddb *dev, u32 link, u8 *buf, u32 addr, u32 len);

#define DDBRIDGE_VERSION "0.9.38"

/* linked function prototypes */

const struct ddb_info *get_ddb_info(u16 vendor, u16 device,
				    u16 subvendor, u16 subdevice);
int netstream_init(struct ddb_input *input);
int ddb_dvb_ns_input_start(struct ddb_input *input);
int ddb_dvb_ns_input_stop(struct ddb_input *input);

int ddbridge_mod_do_ioctl(struct file *file, unsigned int cmd, void *parg);
int ddbridge_mod_init(struct ddb *dev);
void ddbridge_mod_output_stop(struct ddb_output *output);
int ddbridge_mod_output_start(struct ddb_output *output);
void ddbridge_mod_rate_handler(void *);

void ddb_device_destroy(struct ddb *dev);
void ddb_nsd_detach(struct ddb *dev);
void ddb_ports_detach(struct ddb *dev);
void ddb_ports_release(struct ddb *dev);
void ddb_buffers_free(struct ddb *dev);
void ddb_unmap(struct ddb *dev);
irqreturn_t ddb_irq_handler0(int irq, void *dev_id);
irqreturn_t ddb_irq_handler1(int irq, void *dev_id);
irqreturn_t ddb_irq_handler(int irq, void *dev_id);
irqreturn_t ddb_irq_handler_v2(int irq, void *dev_id);
void ddb_reset_ios(struct ddb *dev);
int ddb_init(struct ddb *dev);
int ddb_exit_ddbridge(int stage, int error);
int ddb_init_ddbridge(void);

int ddb_i2c_init(struct ddb *dev);
void ddb_i2c_release(struct ddb *dev);

int ddb_ci_attach(struct ddb_port *port, u32 bitrate);

int ddb_fe_attach_mxl5xx(struct ddb_input *input);
int ddb_fe_attach_mci(struct ddb_input *input, u32 type);
int ddb_lnb_init_fmode(struct ddb *dev, struct ddb_link *link, u32 fm);

struct ddb_irq *ddb_irq_set(struct ddb *dev, u32 link, u32 nr,
			    void (*handler)(void *), void *data);

struct dvb_frontend *ddb_mci_attach(struct ddb_input *input, struct mci_cfg *cfg, int nr, int tuner);

#endif
