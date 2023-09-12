/*
 * Driver for the Sony CXD2843ER DVB-T/T2/C/C2 demodulator.
 * Also supports the CXD2837ER DVB-T/T2/C, the
 * CXD2838ER ISDB-T demodulator and the 
 * CXD2854 DVB-T/T2/C/C2 ISDB-T demodulator.
 *
 * Copyright (C) 2013-2016 Digital Devices GmbH
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <asm/div64.h>

#include <media/dvb_frontend.h>
#include <media/dvb_math.h>
#include "cxd2843.h"

#define Log10x100(x) ((s32)(((((u64) intlog2(x) * 0x1e1a5e2e) >> 47 ) + 1) >> 1))

#define USE_ALGO 1

enum demod_type { CXD2843, CXD2837, CXD2838, CXD2854 };
enum demod_state { Unknown, Shutdown, Sleep, ActiveT,
		   ActiveT2, ActiveC, ActiveC2, ActiveIT };
enum t2_profile { T2P_Base, T2P_Lite };
enum omode { OM_NONE, OM_DVBT, OM_DVBT2, OM_DVBC,
	     OM_QAM_ITU_C, OM_DVBC2, OM_ISDBT };

struct cxd_state {
	struct dvb_frontend   frontend;
	struct i2c_adapter   *i2c;
	struct mutex          mutex;
	int repi2cerr;

	u8  adrt;
	u8  curbankt;

	u8  adrx;
	u8  curbankx;

	enum demod_type  type;
	enum demod_state state;
	enum t2_profile T2Profile;
	enum omode omode;

	u8    IF_FS;
	int   ContinuousClock;
	int   SerialMode;
	u8    SerialClockFrequency;

	u32   LockTimeout;
	u32   TSLockTimeout;
	u32   L1PostTimeout;
	u32   DataSliceID;
	int   FirstTimeLock;
	u32   plp;
	u32   last_status;

	u32   bandwidth;
	u32   bw;

	unsigned long tune_time;

	u32   LastBERNumerator;
	u32   LastBERDenominator;
	u8    BERScaleMax;
	u8    is2k14;
	u8    is24MHz;
};

static int i2c_write(struct i2c_adapter *adap, u8 adr, u8 *data, int len, int flag)
{
	struct i2c_msg msg = {
		.addr = adr, .flags = 0, .buf = data, .len = len}; 
	if (i2c_transfer(adap, &msg, 1) != 1) {
		if (flag)
			pr_err("cxd2843: i2c_write error adr %02x data %02x\n", adr, data[0]);
		return -1;
	}
	return 0;
}

static int writeregs(struct cxd_state *state, u8 adr, u8 reg,
		     u8 *regd, u16 len)
{
	u8 data[16];

	if (len >= 15) {
		pr_err("cxd2843: writeregs length %u too large\n", len);
		return -1;
	}
	data[0] = reg;
	memcpy(data + 1, regd, len);
	return i2c_write(state->i2c, adr, data, len + 1, state->repi2cerr); 
}

static int writereg(struct cxd_state *state, u8 adr, u8 reg, u8 dat)
{
	u8 mm[2] = {reg, dat};

	return i2c_write(state->i2c, adr, mm, 2, state->repi2cerr); 
}

static int i2c_read(struct i2c_adapter *adap,
		    u8 adr, u8 *msg, int len, u8 *answ, int alen)
{
	struct i2c_msg msgs[2] = { { .addr = adr, .flags = 0,
				     .buf = msg, .len = len},
				   { .addr = adr, .flags = I2C_M_RD,
				     .buf = answ, .len = alen } };
	if (i2c_transfer(adap, msgs, 2) != 2)
		return -1;
	return 0;
}

static int readregs(struct cxd_state *state, u8 adr, u8 reg,
		    u8 *val, int count)
{
	int ret = i2c_read(state->i2c, adr, &reg, 1, val, count);

	if (ret && state->repi2cerr)
		pr_err("cxd2843: i2c_read error\n");
	return ret;
}

static int readregst_unlocked(struct cxd_state *cxd, u8 bank,
			      u8 Address, u8 *pValue, u16 count)
{
	int status = 0;

	if (bank != 0xFF && cxd->curbankt != bank) {
		status = writereg(cxd, cxd->adrt, 0, bank);
		if (status < 0) {
			cxd->curbankt = 0xFF;
			return status;
		}
		cxd->curbankt = bank;
	}
	status = readregs(cxd, cxd->adrt, Address, pValue, count);
	return status;
}

static int readregst(struct cxd_state *cxd, u8 Bank,
		     u8 Address, u8 *pValue, u16 count)
{
	int status;

	mutex_lock(&cxd->mutex);
	status = readregst_unlocked(cxd, Bank, Address, pValue, count);
	mutex_unlock(&cxd->mutex);
	return status;
}

static int readregsx_unlocked(struct cxd_state *cxd, u8 Bank,
			      u8 Address, u8 *pValue, u16 count)
{
	int status = 0;

	if (Bank != 0xFF && cxd->curbankx != Bank) {
		status = writereg(cxd, cxd->adrx, 0, Bank);
		if (status < 0) {
			cxd->curbankx = 0xFF;
			return status;
		}
		cxd->curbankx = Bank;
	}
	status = readregs(cxd, cxd->adrx, Address, pValue, count);
	return status;
}

static int readregsx(struct cxd_state *cxd, u8 Bank,
		     u8 Address, u8 *pValue, u16 count)
{
	int status;

	mutex_lock(&cxd->mutex);
	status = readregsx_unlocked(cxd, Bank, Address, pValue, count);
	mutex_unlock(&cxd->mutex);
	return status;
}

static int writeregsx_unlocked(struct cxd_state *cxd, u8 Bank,
			       u8 Address, u8 *pValue, u16 count)
{
	int status = 0;

	if (Bank != 0xFF && cxd->curbankx != Bank) {
		status = writereg(cxd, cxd->adrx, 0, Bank);
		if (status < 0) {
			cxd->curbankx = 0xFF;
			return status;
		}
		cxd->curbankx = Bank;
	}
	status = writeregs(cxd, cxd->adrx, Address, pValue, count);
	return status;
}

static int writeregsx(struct cxd_state *cxd, u8 Bank, u8 Address,
		      u8 *pValue, u16 count)
{
	int status;

	mutex_lock(&cxd->mutex);
	status = writeregsx_unlocked(cxd, Bank, Address, pValue, count);
	mutex_unlock(&cxd->mutex);
	return status;
}

static int writeregx(struct cxd_state *cxd, u8 Bank, u8 Address, u8 val)
{
	return writeregsx(cxd, Bank, Address, &val, 1);
}

static int writeregst_unlocked(struct cxd_state *cxd, u8 Bank,
			       u8 Address, u8 *pValue, u16 count)
{
	int status = 0;

	if (Bank != 0xFF && cxd->curbankt != Bank) {
		status = writereg(cxd, cxd->adrt, 0, Bank);
		if (status < 0) {
			cxd->curbankt = 0xFF;
			return status;
		}
		cxd->curbankt = Bank;
	}
	status = writeregs(cxd, cxd->adrt, Address, pValue, count);
	return status;
}

static int writeregst(struct cxd_state *cxd, u8 Bank, u8 Address,
		      u8 *pValue, u16 count)
{
	int status;

	mutex_lock(&cxd->mutex);
	status = writeregst_unlocked(cxd, Bank, Address, pValue, count);
	mutex_unlock(&cxd->mutex);
	return status;
}

static int writeregt(struct cxd_state *cxd, u8 Bank, u8 Address, u8 val)
{
	return writeregst(cxd, Bank, Address, &val, 1);
}

static int writebitsx(struct cxd_state *cxd, u8 Bank, u8 Address,
		      u8 Value, u8 Mask)
{
	int status = 0;
	u8 tmp;

	mutex_lock(&cxd->mutex);
	status = readregsx_unlocked(cxd, Bank, Address, &tmp, 1);
	if (status < 0)
		goto out;
	tmp = (tmp & ~Mask) | Value;
	status = writeregsx_unlocked(cxd, Bank, Address, &tmp, 1);
out:
	mutex_unlock(&cxd->mutex);
	return status;
}

static int writebitst(struct cxd_state *cxd, u8 Bank, u8 Address,
		      u8 Value, u8 Mask)
{
	int status = 0;
	u8 Tmp = 0x00;

	mutex_lock(&cxd->mutex);
	status = readregst_unlocked(cxd, Bank, Address, &Tmp, 1);
	if (status < 0)
		goto out;
	Tmp = (Tmp & ~Mask) | Value;
	status = writeregst_unlocked(cxd, Bank, Address, &Tmp, 1);
out:
	mutex_unlock(&cxd->mutex);
	return status;
}

static int freeze_regst(struct cxd_state *cxd)
{
	mutex_lock(&cxd->mutex);
	return writereg(cxd, cxd->adrt, 1, 1);
}

static int unfreeze_regst(struct cxd_state *cxd)
{
	int status = 0;

	status = writereg(cxd, cxd->adrt, 1, 0);
	mutex_unlock(&cxd->mutex);
	return status;
}

static inline u32 MulDiv32(u32 a, u32 b, u32 c)
{
	u64 tmp64;

	tmp64 = (u64)a * (u64)b;
	do_div(tmp64, c);

	return (u32) tmp64;
}

/* TPSData[0] [7:6]  CNST[1:0] */
/* TPSData[0] [5:3]  HIER[2:0] */
/* TPSData[0] [2:0]  HRATE[2:0] */
/* TPSData[1] [7:5]  LRATE[2:0] */
/* TPSData[1] [4:3]  GI[1:0] */
/* TPSData[1] [2:1]  MODE[1:0] */
/* TPSData[2] [7:6]  FNUM[1:0] */
/* TPSData[2] [5:0]  LENGTH_INDICATOR[5:0] */
/* TPSData[3] [7:0]  CELLID[15:8] */
/* TPSData[4] [7:0]  CELLID[7:0] */
/* TPSData[5] [5:0]  RESERVE_EVEN[5:0] */
/* TPSData[6] [5:0]  RESERVE_ODD[5:0] */

static int read_tps(struct cxd_state *state, u8 *tps)
{
	if (state->last_status != 0x1f)
		return -1;

	freeze_regst(state);
	readregst_unlocked(state, 0x10, 0x2f, tps, 7);
	unfreeze_regst(state);
	return 0;
}

/* Read DVBT2 OFDM Info */
/*  OFDMInfo[0] [5]    OFDM_MIXED        */
/*  OFDMInfo[0] [4]    OFDM_MISO         */
/*  OFDMInfo[0] [2:0]  OFDM_FFTSIZE[2:0] */
/*  OFDMInfo[1] [6:4]  OFDM_GI[2:0]      */
/*  OFDMInfo[1] [2:0]  OFDM_PP[2:0]      */
/*  OFDMInfo[2] [4]    OFDM_BWT_EXT      */
/*  OFDMInfo[2] [3:0]  OFDM_PAPR[3:0]    */
/*  OFDMInfo[3] [3:0]  OFDM_NDSYM[11:8] */
/*  OFDMInfo[4] [7:0]  OFDM_NDSYM[7:0]  */

#if 0
static int read_t2_ofdm_info(struct cxd_state *state, u8 *ofdm)
{
	if (state->last_status != 0x1f)
		return -1;

	freeze_regst(state);
	readregst_unlocked(state, 0x20, 0x5c, ofdm, 5);
	unfreeze_regst(state);
	return 0;
}
#endif

/* Read DVBT2 QAM, 
   Data PLP
  0  [7:0]        L1POST_PLP_ID[7:0]
  1  [2:0]        L1POST_PLP_TYPE[2:0]
  2  [4:0]        L1POST_PLP_PAYLOAD_TYPE[4:0]
  3  [0]          L1POST_FF_FLAG
  4  [2:0]        L1POST_FIRST_RF_IDX[2:0]
  5  [7:0]        L1POST_FIRST_FRAME_IDX[7:0]
  6  [7:0]        L1POST_PLP_GROUP_ID[7:0]
  7  [2:0]        L1POST_PLP_COD[2:0]
  8  [2:0]        L1POST_PLP_MOD[2:0]
  9  [0]          L1POST_PLP_ROTATION
 10  [1:0]        L1POST_PLP_FEC_TYPE[1:0]
 11  [1:0]        L1POST_PLP_NUM_BLOCKS_MAX[9:8]
 12  [7:0]        L1POST_PLP_NUM_BLOCKS_MAX[7:0]
 13  [7:0]        L1POST_FRAME_INTERVAL[7:0]
 14  [7:0]        L1POST_TIME_IL_LENGTH[7:0]
 15  [0]          L1POST_TIME_IL_TYPE
 16  [0]          L1POST_IN_BAND_FLAG
 17  [7:0]        L1POST_RESERVED_1[15:8]
 18  [7:0]        L1POST_RESERVED_1[7:0]
 19-37 same for common PLP
*/

#if 0
static int read_t2_tlp_info(struct cxd_state *state, u8 off, u8 count, u8 *tlp)
{
	if (state->last_status != 0x1f)
		return -1;

	freeze_regst(state);
	readregst_unlocked(state, 0x22, 0x54 + off, tlp, count);
	unfreeze_regst(state);
	return 0;
}
#endif

static void Active_to_Sleep(struct cxd_state *state)
{
	if (state->state <= Sleep)
		return;

	writeregt(state, 0x00, 0xC3, 0x01); /* Disable TS */
	writeregt(state, 0x00, 0x80, 0x3F); /* Enable HighZ 1 */
	writeregt(state, 0x00, 0x81, 0xFF); /* Enable HighZ 2 */
	writeregx(state, 0x00, 0x18, 0x01); /* Disable ADC 4 */
	writeregt(state, 0x00, 0x43, 0x0A); /* Disable ADC 2 */
	writeregt(state, 0x00, 0x41, 0x0A); /* Disable ADC 1 */
	writeregt(state, 0x00, 0x30, 0x00); /* Disable ADC Clock */
	writeregt(state, 0x00, 0x59, 0x00); /* Disable RF Monitor ADC */
	writeregt(state, 0x00, 0x2F, 0x00); /* Disable RF Monitor Clock */
	writeregt(state, 0x00, 0x2C, 0x00); /* Disable Demod Clock */
	state->state = Sleep;
}

static void ActiveT2_to_Sleep(struct cxd_state *state)
{
	if (state->state <= Sleep)
		return;

	writeregt(state, 0x00, 0xC3, 0x01); /* Disable TS */
	writeregt(state, 0x00, 0x80, 0x3F); /* Enable HighZ 1 */
	writeregt(state, 0x00, 0x81, 0xFF); /* Enable HighZ 2 */

	writeregt(state, 0x13, 0x83, 0x40);
	writeregt(state, 0x13, 0x86, 0x21);
	writebitst(state, 0x13, 0x9E, 0x09, 0x0F);
	writeregt(state, 0x13, 0x9F, 0xFB);

	writeregx(state, 0x00, 0x18, 0x01); /* Disable ADC 4 */
	writeregt(state, 0x00, 0x43, 0x0A); /* Disable ADC 2 */
	writeregt(state, 0x00, 0x41, 0x0A); /* Disable ADC 1 */
	writeregt(state, 0x00, 0x30, 0x00); /* Disable ADC Clock */
	writeregt(state, 0x00, 0x59, 0x00); /* Disable RF Monitor ADC */
	writeregt(state, 0x00, 0x2F, 0x00); /* Disable RF Monitor Clock */
	writeregt(state, 0x00, 0x2C, 0x00); /* Disable Demod Clock */
	state->state = Sleep;
}

static void ActiveIT_to_Sleep(struct cxd_state *state)
{
	if (state->state <= Sleep)
		return;

	writeregt(state, 0x00, 0xC3, 0x01); /* Disable TS */
	writeregt(state, 0x00, 0x80, 0x3F); /* Enable HighZ 1 */
	writeregt(state, 0x00, 0x81, 0xFF); /* Enable HighZ 2 */

	if (state->is2k14) {
		writebitst(state, 0x10, 0x69, 0x05, 0x07);
		writebitst(state, 0x10, 0x6b, 0x07, 0x07);
		writebitst(state, 0x10, 0x9d, 0x14, 0xff);
		writebitst(state, 0x10, 0xd3, 0x00, 0x1f);
		writebitst(state, 0x10, 0xed, 0x01, 0x01);
		writebitst(state, 0x10, 0xe2, 0x4e, 0x80);
		writebitst(state, 0x10, 0xf2, 0x03, 0x10);
		writebitst(state, 0x10, 0xde, 0x32, 0x3f);

		writebitst(state, 0x15, 0xde, 0x03, 0x03);
		writebitst(state, 0x1e, 0x73, 0x00, 0xff);
		writebitst(state, 0x63, 0x81, 0x01, 0x01);
	}
	
	writeregx(state, 0x00, 0x18, 0x01); /* Disable ADC 4 */
	writeregt(state, 0x00, 0x43, 0x0A); /* Disable ADC 2 */
	writeregt(state, 0x00, 0x41, 0x0A); /* Disable ADC 1 */
	writeregt(state, 0x00, 0x30, 0x00); /* Disable ADC Clock */
	writeregt(state, 0x00, 0x59, 0x00); /* Disable RF Monitor ADC */
	writeregt(state, 0x00, 0x2F, 0x00); /* Disable RF Monitor Clock */
	writeregt(state, 0x00, 0x2C, 0x00); /* Disable Demod Clock */
	state->state = Sleep;
}

static void ActiveC2_to_Sleep(struct cxd_state *state)
{
	if (state->state <= Sleep)
		return;

	writeregt(state, 0x00, 0xC3, 0x01); /* Disable TS */
	writeregt(state, 0x00, 0x80, 0x3F); /* Enable HighZ 1 */
	writeregt(state, 0x00, 0x81, 0xFF); /* Enable HighZ 2 */

	writeregt(state, 0x20, 0xC2, 0x11);
	writebitst(state, 0x25, 0x6A, 0x02, 0x03);
	{
		static u8 data[3] = { 0x07, 0x61, 0x36 };

		writeregst(state, 0x25, 0x89, data, sizeof(data));
	}
	writebitst(state, 0x25, 0xCB, 0x05, 0x07);
	{
		static u8 data[4] = { 0x2E, 0xE0, 0x2E, 0xE0 };

		writeregst(state, 0x25, 0xDC, data, sizeof(data));
	}
	writeregt(state, 0x25, 0xE2, 0x2F);
	writeregt(state, 0x25, 0xE5, 0x2F);
	writebitst(state, 0x27, 0x20, 0x00, 0x01);
	writebitst(state, 0x27, 0x35, 0x00, 0x01);
	writebitst(state, 0x27, 0xD9, 0x19, 0x3F);
	writebitst(state, 0x2A, 0x78, 0x01, 0x07);
	writeregt(state, 0x2A, 0x86, 0x08);
	writeregt(state, 0x2A, 0x88, 0x14);
	writebitst(state, 0x2B, 0x2B, 0x00, 0x1F);
	{
		u8 data[2] = { 0x75, 0x75 };
		u8 data24[2] = { 0x89, 0x89 };

		if (state->is24MHz) 
			writeregst(state, 0x2D, 0x24, data24, sizeof(data24));
		else
			writeregst(state, 0x2D, 0x24, data, sizeof(data));
	}

	writeregx(state, 0x00, 0x18, 0x01); /* Disable ADC 4 */
	writeregt(state, 0x00, 0x43, 0x0A); /* Disable ADC 2 */
	writeregt(state, 0x00, 0x41, 0x0A); /* Disable ADC 1 */
	writeregt(state, 0x00, 0x30, 0x00); /* Disable ADC Clock */
	writeregt(state, 0x00, 0x59, 0x00); /* Disable RF Monitor ADC */
	writeregt(state, 0x00, 0x2F, 0x00); /* Disable RF Monitor Clock */
	writeregt(state, 0x00, 0x2C, 0x00); /* Disable Demod Clock */
	state->state = Sleep;
}

static int ConfigureTS(struct cxd_state *state,
		       enum demod_state newDemodState)
{
	int status = 0;
	u8 OSERCKMODE = state->SerialMode ?  1 : 0;
	u8 OSERDUTYMODE = state->SerialMode ?  1 : 0;
	u8 OTSCKPERIOD = 8;
	u8 OREG_CKSEL_TSIF = state->SerialMode ?
		state->SerialClockFrequency : 0;

	if (state->SerialMode && state->SerialClockFrequency >= 3) {
		OSERCKMODE = 2;
		OSERDUTYMODE = 2;
		OTSCKPERIOD = 16;
		OREG_CKSEL_TSIF = state->SerialClockFrequency - 3;
	}
	writebitst(state, 0x00, 0xC4, OSERCKMODE, 0x03); /* OSERCKMODE */
	writebitst(state, 0x00, 0xD1, OSERDUTYMODE, 0x03); /* OSERDUTYMODE */
	writeregt(state, 0x00, 0xD9, OTSCKPERIOD); /* OTSCKPERIOD */
	writebitst(state, 0x00, 0x32, 0x00, 0x01); /* Disable TS IF */
	/* OREG_CKSEL_TSIF */
	writebitst(state, 0x00, 0x33, OREG_CKSEL_TSIF, 0x03);
	writebitst(state, 0x00, 0x32, 0x01, 0x01); /* Enable TS IF */

	if (newDemodState == ActiveT)
		writebitst(state, 0x10, 0x66, 0x01, 0x01);
	if (newDemodState == ActiveC)
		writebitst(state, 0x40, 0x66, 0x01, 0x01);

	return status;
}

#if 0
static int set_tr(struct cxd_state *state, u32 bw, u32 osc24)
{
	u64 tr = 7 *(osc24 ? 0x1800000000 : 0x1480000000);

	div64_32(tr, bw);
	printk("TR %016llx\n", tr);	
	return 0;
}
#endif

static void BandSettingT(struct cxd_state *state, u32 iffreq)
{
	u8 IF_data[3] = { (iffreq >> 16) & 0xff,
			  (iffreq >> 8) & 0xff, iffreq & 0xff};
	u8 data[] = { 0x01, 0x14 };

	writeregst(state, 0x13, 0x9c, data, sizeof(data));
	switch (state->bw) {
	default:
	case 8:
	{
		u8 NF_data[] = { 0x01, 0x02 };

		if (state->is24MHz) {
			u8 TR_data[] = { 0x15, 0x00, 0x00, 0x00, 0x00 };
			writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		} else {
			u8 TR_data[] = { 0x11, 0xF0, 0x00, 0x00, 0x00 };
			writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		}
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x00, 0x07);
		if (state->is24MHz) {
			u8 CL_data[] = { 0x15, 0x28 };
			writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		} else {
			u8 CL_data[] = { 0x01, 0xE0 };
			writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		}
		writeregst(state, 0x17, 0x38, NF_data, sizeof(NF_data));
		break;
	}
	case 7:
	{
		u8 NF_data[] = { 0x00, 0x03 };

		if (state->is24MHz) {
			u8 TR_data[] = { 0x18, 0x00, 0x00, 0x00, 0x00 };
			writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		} else {
			u8 TR_data[] = { 0x14, 0x80, 0x00, 0x00, 0x00 };
			writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		}
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x02, 0x07);
		if (state->is24MHz) {
			u8 CL_data[] = { 0x1f, 0xf8 };
			writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		} else {
			u8 CL_data[] = { 0x12, 0xF8 };
			writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		}
		writeregst(state, 0x17, 0x38, NF_data, sizeof(NF_data));
		break;
	}
	case 6:
	{
		u8 NF_data[] = { 0x00, 0x03 };

		if (state->is24MHz) {
			u8 TR_data[] = { 0x1c, 0x00, 0x00, 0x00, 0x00 };
			writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		} else {
			u8 TR_data[] = { 0x17, 0xEA, 0xAA, 0xAA, 0xAA };
			writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		}
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x04, 0x07);
		if (state->is24MHz) {
			u8 CL_data[] = { 0x25, 0x4c };
			writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		} else {
			u8 CL_data[] = { 0x1F, 0xDC };
			writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		}
		writeregst(state, 0x17, 0x38, NF_data, sizeof(NF_data));
		break;
	}
	case 5:
	{
		static u8 NF_data[] = { 0x00, 0x03 };

		if (state->is24MHz) {
			u8 TR_data[] = { 0x21, 0x99, 0x99, 0x99, 0x99 };
			writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		} else {
			static u8 TR_data[] = { 0x1C, 0xB3, 0x33, 0x33, 0x33 };
			writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		}
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x06, 0x07);
		if (state->is24MHz) {
			static u8 CL_data[] = { 0x2c, 0xc2 };
			writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		} else {
			static u8 CL_data[] = { 0x26, 0x3C };
			writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		}
		writeregst(state, 0x17, 0x38, NF_data, sizeof(NF_data));
		break;
	}
	}
}

static void Sleep_to_ActiveT(struct cxd_state *state, u32 iffreq)
{
	ConfigureTS(state, ActiveT);
	writeregx(state, 0x00, 0x17, 0x01);   /* Mode */
	writeregt(state, 0x00, 0x2C, 0x01);   /* Demod Clock */
	writeregt(state, 0x00, 0x59, 0x00);   /* Disable RF Monitor ADC */
	writeregt(state, 0x00, 0x2F, 0x00);   /* Disable RF Monitor Clock */
	writeregt(state, 0x00, 0x30, 0x00);   /* Enable ADC Clock */
	writeregt(state, 0x00, 0x41, 0x1A);   /* Enable ADC1 */
	{
		u8 data[2] = { 0x09, 0x54 };  /* 20.5/24 MHz */
		/*u8 data[2] = { 0x0A, 0xD4 }; */  /* 41 MHz */

		writeregst(state, 0x00, 0x43, data, 2);   /* Enable ADC 2+3 */
	}
	writeregx(state, 0x00, 0x18, 0x00);   /* Enable ADC 4 */

	writebitst(state, 0x10, 0xD2, 0x0C, 0x1F); /* IF AGC Gain */
	writeregt(state, 0x11, 0x6A, 0x50); /* BB AGC Target Level */

	writebitst(state, 0x10, 0xA5, 0x00, 0x01); /* ASCOT Off */

	writebitst(state, 0x18, 0x36, 0x40, 0x07); /* Pre RS Monitoring */
	writebitst(state, 0x18, 0x30, 0x01, 0x01); /* FEC Autorecover */
	writebitst(state, 0x18, 0x31, 0x01, 0x01); /* FEC Autorecover */

	writebitst(state, 0x00, 0xCE, 0x01, 0x01); /* TSIF ONOPARITY */
	writebitst(state, 0x00, 0xCF, 0x01, 0x01);/*TSIF ONOPARITY_MANUAL_ON*/

	if (state->is24MHz) {
		u8 data[3] = { 0xdc, 0x6c, 0x00 };

		writeregt(state, 0x10, 0xbf, 0x60);
		writeregst(state, 0x18, 0x24, data, 3);
	}
	
	BandSettingT(state, iffreq);

	writebitst(state, 0x10, 0x60, 0x11, 0x1f); /* BER scaling */

	writeregt(state, 0x00, 0x80, 0x28); /* Disable HiZ Setting 1 */
	writeregt(state, 0x00, 0x81, 0x00); /* Disable HiZ Setting 2 */
}

static void BandSettingT2(struct cxd_state *state, u32 iffreq)
{
	u8 IF_data[3] = {(iffreq >> 16) & 0xff, (iffreq >> 8) & 0xff,
			 iffreq & 0xff};

	switch (state->bw) {
	default:
	case 8:
	{
		/* Timing recovery */
		if (state->is24MHz) {
			u8 TR_data[] = { 0x15, 0x00, 0x00, 0x00, 0x00 };

			writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		} else {
			u8 TR_data[] = { 0x11, 0xF0, 0x00, 0x00, 0x00 };
				
			writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		}
		/* Add EQ Optimisation for tuner here */
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		/* System Bandwidth */
		writebitst(state, 0x10, 0xD7, 0x00, 0x07);
	}
	break;
	case 7:
	{
		if (state->is24MHz) {
			u8 TR_data[] = { 0x18, 0x00, 0x00, 0x00, 0x00 };

			writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		} else {
			u8 TR_data[] = { 0x14, 0x80, 0x00, 0x00, 0x00 };

			writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		}
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x02, 0x07);
	}
	break;
	case 6:
	{
		if (state->is24MHz) {
			u8 TR_data[] = { 0x1c, 0x00, 0x00, 0x00, 0x00 };

			writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		} else {
			u8 TR_data[] = { 0x17, 0xEA, 0xAA, 0xAA, 0xAA };

			writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		}
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x04, 0x07);
	}
	break;
	case 5:
	{
		if (state->is24MHz) {
			u8 TR_data[] = { 0x21, 0x99, 0x99, 0x99, 0x99 };

			writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		} else {
			u8 TR_data[] = { 0x1C, 0xB3, 0x33, 0x33, 0x33 };

			writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		}
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x06, 0x07);
	}
	break;
	case 2: /* 1.7 MHz */
	{
		if (state->is24MHz) {
			u8 TR_data[] = { 0x68, 0x0f, 0xa2, 0x32, 0xd0 };

			writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		} else {
			u8 TR_data[] = { 0x58, 0xE2, 0xAF, 0xE0, 0xBC };

			writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		}
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x03, 0x07);
	}
	break;
	}
}


static void Sleep_to_ActiveT2(struct cxd_state *state, u32 iffreq)
{
	ConfigureTS(state, ActiveT2);

	writeregx(state, 0x00, 0x17, 0x02);   /* Mode */
	writeregt(state, 0x00, 0x2C, 0x01);   /* Demod Clock */
	writeregt(state, 0x00, 0x59, 0x00);   /* Disable RF Monitor ADC */
	writeregt(state, 0x00, 0x2F, 0x00);   /* Disable RF Monitor Clock */
	writeregt(state, 0x00, 0x30, 0x00);   /* Enable ADC Clock */
	writeregt(state, 0x00, 0x41, 0x1A);   /* Enable ADC1 */

	{
		u8 data[2] = { 0x09, 0x54 };  /* 20.5/24 MHz */
		/*u8 data[2] = { 0x0A, 0xD4 }; */  /* 41 MHz */

		writeregst(state, 0x00, 0x43, data, 2);   /* Enable ADC 2+3 */
	}
	writeregx(state, 0x00, 0x18, 0x00);   /* Enable ADC 4 */

	writebitst(state, 0x10, 0xD2, 0x0C, 0x1F); /* IFAGC  coarse gain */
	writeregt(state, 0x11, 0x6A, 0x50); /* BB AGC Target Level */
	writebitst(state, 0x10, 0xA5, 0x00, 0x01); /* ASCOT Off */

	writeregt(state, 0x20, 0x8B, 0x3C); /* SNR Good count */
	writebitst(state, 0x2B, 0x76, 0x20, 0x70); /* Noise Gain ACQ */
	writebitst(state, 0x23, 0xe6, 0x00, 0x03);

	writebitst(state, 0x00, 0xCE, 0x01, 0x01); /* TSIF ONOPARITY */
	writebitst(state, 0x00, 0xCF, 0x01, 0x01);/*TSIF ONOPARITY_MANUAL_ON*/

	writeregt(state, 0x13, 0x83, 0x10); /* T2 Inital settings */
	writeregt(state, 0x13, 0x86, 0x34);
	writebitst(state, 0x13, 0x9E, 0x09, 0x0F);
	writeregt(state, 0x13, 0x9F, 0xD8);
	writebitst(state, 0x23, 0x11, 0x20, 0x3F);


	if (state->is24MHz ) {
		static u8 data1[] = { 0xEB, 0x03, 0x3B };
		static u8 data2[] = { 0x5E, 0x5E, 0x47 };
		static u8 data3[] = { 0x3F, 0xFF };
		static u8 data4[] = { 0x0B, 0x72 };
		static u8 data5[] = { 0x93, 0xF3, 0x00 };
		static u8 data6[] = { 0x05, 0xB8, 0xD8 };
		static u8 data7[] = { 0x89, 0x89  };
		static u8 data8[] = { 0x24, 0x95  };
		
		writeregst(state, 0x11, 0x33, data1, sizeof(data1));
		writeregst(state, 0x20, 0x95, data2, sizeof(data2));
		writeregt(state, 0x20, 0x99, 0x18);
		writeregst(state, 0x20, 0xD9, data3, sizeof(data3));
		writeregst(state, 0x24, 0x34, data4, sizeof(data4));
		writeregst(state, 0x24, 0xD2, data5, sizeof(data5));
		writeregst(state, 0x24, 0xDD, data6, sizeof(data6));
		writeregt(state, 0x24, 0xE0, 0x00);
		writeregt(state, 0x25, 0xED, 0x60);
		writeregt(state, 0x27, 0xFA, 0x34);
		writeregt(state, 0x2B, 0x4B, 0x2F);
		writeregt(state, 0x2B, 0x9E, 0x0E);
		writeregst(state, 0x2D, 0x24, data7, sizeof(data7));
		writeregst(state, 0x5E, 0x8C, data8, sizeof(data8));
        }
	BandSettingT2(state, iffreq);

	writebitst(state, 0x20, 0x72, 0x08, 0x0f); /* BER scaling */

	writeregt(state, 0x00, 0x80, 0x28); /* Disable HiZ Setting 1 */
	writeregt(state, 0x00, 0x81, 0x00); /* Disable HiZ Setting 2 */
}


static void BandSettingC(struct cxd_state *state, u32 iffreq)
{
	u8 data[3];

	data[0] = (iffreq >> 16) & 0xFF;
	data[1] = (iffreq >>  8) & 0xFF;
	data[2] = (iffreq) & 0xFF;
	writeregst(state, 0x10, 0xB6, data, 3);
}

static void Sleep_to_ActiveC(struct cxd_state *state, u32 iffreq)
{
	ConfigureTS(state, ActiveC);

	writeregx(state, 0x00, 0x17, 0x04);   /* Mode */
	writeregt(state, 0x00, 0x2C, 0x01);   /* Demod Clock */
	writeregt(state, 0x00, 0x59, 0x00);   /* Disable RF Monitor ADC */
	writeregt(state, 0x00, 0x2F, 0x00);   /* Disable RF Monitor Clock */
	writeregt(state, 0x00, 0x30, 0x00);   /* Enable ADC Clock */
	writeregt(state, 0x00, 0x41, 0x1A);   /* Enable ADC1 */

	{
		u8 data[2] = { 0x09, 0x54 };  /* 20.5/24 MHz */
		/*u8 data[2] = { 0x0A, 0xD4 }; */  /* 41 MHz */

		writeregst(state, 0x00, 0x43, data, 2);   /* Enable ADC 2+3 */
	}
	writeregx(state, 0x00, 0x18, 0x00);   /* Enable ADC 4 */

	writebitst(state, 0x10, 0xD2, 0x09, 0x1F); /* IF AGC Gain */
	writeregt(state, 0x11, 0x6A, 0x48); /* BB AGC Target Level */
	writebitst(state, 0x10, 0xA5, 0x00, 0x01); /* ASCOT Off */

	writebitst(state, 0x40, 0xC3, 0x00, 0x04); /* OREG_BNDET_EN_64 */

	writebitst(state, 0x00, 0xCE, 0x01, 0x01); /* TSIF ONOPARITY */
	writebitst(state, 0x00, 0xCF, 0x01, 0x01);/*TSIF ONOPARITY_MANUAL_ON*/

        if (state->is24MHz) {
		u8 data1[2] = { 0x29, 0x09 };
		u8 data2[4] = { 0x08, 0x38, 0x83, 0x0E };
		u8 data3[3] = { 0xDC, 0x6C, 0x00 };
		u8 data4[2] = { 0x77, 0x00 };
		
		writeregst(state,0x40,0x54,data1,2);
		writeregst(state,0x40,0x8b,data2,4);
		writeregt(state,0x40,0xBF,0x60);
		writeregst(state,0x48,0x24,data3,2);
		writeregst(state,0x49,0x11,data4,2);
        }
	BandSettingC(state, iffreq);

	writebitst(state, 0x40, 0x60, 0x11, 0x1f); /* BER scaling */

	writeregt(state, 0x00, 0x80, 0x28); /* Disable HiZ Setting 1 */
	writeregt(state, 0x00, 0x81, 0x00); /* Disable HiZ Setting 2 */
}

static void BandSettingC2(struct cxd_state *state, u32 iffreq)
{
	u8 IF_data[3] = { (iffreq >> 16) & 0xff,
			  (iffreq >> 8) & 0xff, iffreq & 0xff};

	switch (state->bw) {
	default:
	case 8:
	{
		if (state->is24MHz) {
			u8 TR_data[] = { 0x15, 0x00, 0x00, 0x00, 0x00 };
			writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		} else {
			u8 TR_data[] = { 0x11, 0xF0, 0x00, 0x00, 0x00 };
			writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		}
		writebitst(state, 0x27, 0x7a, 0x00, 0x0f);

		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x00, 0x07);

		if (state->is24MHz) {
			u8 data[2] = { 0x14, 0xa0 };

			writeregst(state, 0x50, 0xEC, data, sizeof(data));
			writeregt(state, 0x50, 0xEF, 0x14);
			writeregt(state, 0x50, 0xF1, 0xa0);
		} else {
			u8 data[2] = { 0x11, 0x9E };

			writeregst(state, 0x50, 0xEC, data, sizeof(data));
			writeregt(state, 0x50, 0xEF, 0x11);
			writeregt(state, 0x50, 0xF1, 0x9E);
		}
	}
	break;
	case 6:
	{
		if (state->is24MHz) {
			u8 TR_data[] = { 0x1c, 0x00, 0x00, 0x00, 0x00 };
			writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		} else {
			u8 TR_data[] = { 0x17, 0xEA, 0xAA, 0xAA, 0xAA };
			writeregst(state, 0x20, 0x9F, TR_data, sizeof(TR_data));
		}
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x04, 0x07);
		if (state->is24MHz) {
			u8 data[2] = { 0x1b, 0x70 };

			writeregst(state, 0x50, 0xEC, data, sizeof(data));
			writeregt(state, 0x50, 0xEF, 0x1b);
			writeregt(state, 0x50, 0xF1, 0x70);
		} else {
			u8 data[2] = { 0x17, 0x70 };

			writeregst(state, 0x50, 0xEC, data, sizeof(data));
			writeregt(state, 0x50, 0xEF, 0x17);
			writeregt(state, 0x50, 0xF1, 0x70);
		}
	}
	break;
	}
}

static void Sleep_to_ActiveC2(struct cxd_state *state, u32 iffreq)
{
	ConfigureTS(state, ActiveC2);

	writeregx(state, 0x00, 0x17, 0x05);   /* Mode */
	writeregt(state, 0x00, 0x2C, 0x01);   /* Demod Clock */
	writeregt(state, 0x00, 0x59, 0x00);   /* Disable RF Monitor ADC */
	writeregt(state, 0x00, 0x2F, 0x00);   /* Disable RF Monitor Clock */
	writeregt(state, 0x00, 0x30, 0x00);   /* Enable ADC Clock */
	writeregt(state, 0x00, 0x41, 0x1A);   /* Enable ADC1 */

	{
		u8 data[2] = { 0x09, 0x54 };  /* 20.5/24 MHz */
		/*u8 data[2] = { 0x0A, 0xD4 }; */  /* 41 MHz */

		writeregst(state, 0x00, 0x43, data, sizeof(data));
		/* Enable ADC 2+3 */
	}
	writeregx(state, 0x00, 0x18, 0x00);   /* Enable ADC 4 */

	writebitst(state, 0x10, 0xD2, 0x0C, 0x1F); /* IFAGC  coarse gain */
	writeregt(state, 0x11, 0x6A, 0x50); /* BB AGC Target Level */
	writebitst(state, 0x10, 0xA5, 0x00, 0x01); /* ASCOT Off */

	writebitst(state, 0x00, 0xCE, 0x01, 0x01); /* TSIF ONOPARITY */
	writebitst(state, 0x00, 0xCF, 0x01, 0x01);/*TSIF ONOPARITY_MANUAL_ON*/

	writeregt(state, 0x20, 0xC2, 0x00);
	writebitst(state, 0x25, 0x6A, 0x00, 0x03);
	{
		u8 data[3] = { 0x0C, 0xD1, 0x40 };

		writeregst(state, 0x25, 0x89, data, sizeof(data));
	}
	writebitst(state, 0x25, 0xCB, 0x01, 0x07);
	{
		u8 data[4] = { 0x7B, 0x00, 0x7B, 0x00 };

		writeregst(state, 0x25, 0xDC, data, sizeof(data));
	}
	writeregt(state, 0x25, 0xE2, 0x30);
	writeregt(state, 0x25, 0xE5, 0x30);
	writebitst(state, 0x27, 0x20, 0x01, 0x01);
	writebitst(state, 0x27, 0x35, 0x01, 0x01);
	writebitst(state, 0x27, 0xD9, 0x18, 0x3F);
	writebitst(state, 0x2A, 0x78, 0x00, 0x07);
	writeregt(state, 0x2A, 0x86, 0x20);
	writeregt(state, 0x2A, 0x88, 0x32);
	writebitst(state, 0x2B, 0x2B, 0x10, 0x1F);
	{
		u8 data[2] = { 0x01, 0x01 };

		writeregst(state, 0x2D, 0x24, data, sizeof(data));
	}
        if (state->is24MHz) {
		u8 data1[3] = { 0xEB, 0x03, 0x3B };
		u8 data2[2] = { 0x3F, 0xFF };
		u8 data3[2] = { 0x0B, 0x72 };
		u8 data4[3] = { 0x93, 0xF3, 0x00 };
		u8 data5[4] = { 0x05, 0xB8, 0xD8, 0x00 };
		u8 data6[9] = { 0x18, 0x1E, 0x71, 0x5D, 0xA9, 0x5D, 0xA9, 0x46, 0x3F };
		
		writeregst(state,0x11,0x33,data1,sizeof(data1));
		writeregst(state,0x20,0xD9,data2,sizeof(data2));
		writeregst(state,0x24,0x34,data3,sizeof(data3));
		writeregst(state,0x24,0xD2,data4,sizeof(data4));
		writeregst(state,0x24,0xDD,data5,sizeof(data5));
		writeregt(state,0x25,0xED,0x60);
		writeregst(state,0x5E,0xDB,data6,sizeof(data6));
        }
	
	BandSettingC2(state, iffreq);

	writeregt(state, 0x00, 0x80, 0x28); /* Disable HiZ Setting 1 */
	writeregt(state, 0x00, 0x81, 0x00); /* Disable HiZ Setting 2 */
}


static void BandSettingIT(struct cxd_state *state, u32 iffreq)
{
	u8 IF_data[3] = { (iffreq >> 16) & 0xff,
			  (iffreq >> 8) & 0xff, iffreq & 0xff};

	switch (state->bw) {
	default:
	case 8:
	{
		if (state->is24MHz) {
			u8 TR_data[] = { 0x11, 0xb8, 0x00, 0x00, 0x00 }; /* 24 */
			writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		} else {
			u8 TR_data[] = { 0x0F, 0x22, 0x80, 0x00, 0x00 }; /* 20.5/41 */
			writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		}
		/* Add EQ Optimisation for tuner here */
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));

		writebitst(state, 0x10, 0xD7, 0x00, 0x07); /* System Bandwidth */
		if (state->is24MHz) {
			u8 CL_data[] = { 0x13, 0xfc };

			writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		} else {
			u8 CL_data[] = { 0x15, 0xA8 };
		
			writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		}
		writebitst(state, 0x12, 0x71, 0x03, 0x07);
		writeregt(state, 0x15, 0xbe, 0x03);
	}
	break;
	case 7:
	{
		if (state->is24MHz) {
			u8 TR_data[] = { 0x14, 0x40, 0x00, 0x00, 0x00 }; /* 24 */
			writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		} else {
			u8 TR_data[] = { 0x11, 0x4c, 0x00, 0x00, 0x00 };
			writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		}
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));

		writebitst(state, 0x10, 0xD7, 0x02, 0x07);
		if (state->is24MHz) {
			u8 CL_data[] = { 0x1a, 0xfa };

			writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		} else {
			u8 CL_data[] = { 0x1B, 0x5D };
		
			writeregst(state, 0x10, 0xD9, CL_data, sizeof(CL_data));
		}

		writebitst(state, 0x12, 0x71, 0x03, 0x07);
		writeregt(state, 0x15, 0xbe, 0x02);
	}
	break;
	case 6:
	{
		if (state->is24MHz) {
			u8 TR_data[] = { 0x17, 0xa0, 0x00, 0x00, 0x00 }; /* 24 */
			writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		} else {
			u8 TR_data[] = { 0x14, 0x2E, 0x00, 0x00, 0x00 };
			writeregst(state, 0x10, 0x9F, TR_data, sizeof(TR_data));
		}
		writeregst(state, 0x10, 0xB6, IF_data, sizeof(IF_data));
		writebitst(state, 0x10, 0xD7, 0x04, 0x07);

		if (state->is24MHz) {
			u8 CL_data[] = { 0x1f, 0x79 };
			
			writeregst(state, 0x10, 0xd9, CL_data, sizeof(CL_data));
		} else {
			if (state->is2k14) {
				u8 CL_data[] = { 0x1a, 0xe2 };
				
				writeregst(state, 0x10, 0xd9, CL_data, sizeof(CL_data));
			} else {
				u8 CL_data[] = { 0x1F, 0xec };
				
				writeregst(state, 0x10, 0xd9, CL_data, sizeof(CL_data));
			}
		}
		writebitst(state, 0x12, 0x71, 0x07, 0x07);
		writeregt(state, 0x15, 0xbe, 0x02);
	}
	break;
	}
}

static void Sleep_to_ActiveIT(struct cxd_state *state, u32 iffreq)
{
	ConfigureTS(state, ActiveIT);

	/* writeregx(state, 0x00,0x17,0x01); */  /* 2838 has only one Mode */
	if (state->is2k14)
		writeregx(state, 0x00, 0x17, 0x06);
	writeregt(state, 0x00, 0x2C, 0x01);   /* Demod Clock */
	if (state->is2k14) {
		writeregt(state, 0x00, 0x59, 0x00);   /* Disable RF Monitor ADC */
		writeregt(state, 0x00, 0x2F, 0x00);   /* Disable RF Monitor Clock */
	}
	writeregt(state, 0x00, 0x30, 0x00);   /* Enable ADC Clock */
	writeregt(state, 0x00, 0x41, 0x1A);   /* Enable ADC1 */

	{
		u8 data[2] = { 0x09, 0x54 };  /* 20.5 MHz/24 MHz */
		/*u8 data[2] = { 0x0A, 0xD4 }; */  /* 41 MHz */

		writeregst(state, 0x00, 0x43, data, 2);   /* Enable ADC 2+3 */
	}
	writeregx(state, 0x00, 0x18, 0x00);   /* Enable ADC 4 */

	
	if (state->is2k14) {
		writebitst(state, 0x10, 0xd2, 0x0c, 0x1f);
		writeregt(state, 0x11, 0x6a, 0x50);

		writebitst(state, 0x10, 0xA5, 0x00, 0x01); /* ASCOT Off */

		writebitst(state, 0x18, 0x30, 0x01, 0x01);
		writebitst(state, 0x18, 0x31, 0x00, 0x01);
		writebitst(state, 0x00, 0xce, 0x00, 0x01);
		writebitst(state, 0x00, 0xcf, 0x00, 0x01);

		writebitst(state, 0x10, 0x69, 0x04, 0x07);
		writebitst(state, 0x10, 0x6b, 0x03, 0x07);
		writebitst(state, 0x10, 0x9d, 0x50, 0xff);
		writebitst(state, 0x10, 0xd3, 0x06, 0x1f);
		writebitst(state, 0x10, 0xed, 0x00, 0x01);
		writebitst(state, 0x10, 0xe2, 0xce, 0x80);
		writebitst(state, 0x10, 0xf2, 0x13, 0x10);
		writebitst(state, 0x10, 0xde, 0x2e, 0x3f);

		writebitst(state, 0x15, 0xde, 0x02, 0x03);
		writebitst(state, 0x1e, 0x73, 0x68, 0xff);
		writebitst(state, 0x63, 0x81, 0x00, 0x01);
	}
        if (state->is24MHz) {
		static u8 TSIF_data[2] = { 0x60,0x00 } ; // 24 MHz
		static u8 data[3] = { 0xB7,0x1B,0x00 };  // 24 MHz
		
		writeregst(state, 0x10, 0xBF, TSIF_data, sizeof(TSIF_data));
		writeregst(state, 0x60, 0xA8, data, sizeof(data));
        } else {
		u8 TSIF_data[2] = { 0x61, 0x60 } ; /* 20.5/41 MHz */
		u8 data[3] = { 0xB9, 0xBA, 0x63 };  /* 20.5/41 MHz */

		writeregst(state, 0x10, 0xBF, TSIF_data, sizeof(TSIF_data));
		writeregst(state, 0x60, 0xa8, data, sizeof(data));
	}
	
	if (!state->is2k14) {
		writeregt(state, 0x10, 0xE2, 0xCE); /* OREG_PNC_DISABLE */
		writebitst(state, 0x10, 0xA5, 0x00, 0x01); /* ASCOT Off */
	}
	BandSettingIT(state, iffreq);

	writeregt(state, 0x00, 0x80, 0x28); /* Disable HiZ Setting 1 */
	writeregt(state, 0x00, 0x81, 0x00); /* Disable HiZ Setting 2 */
}

static void T2_SetParameters(struct cxd_state *state)
{
	u8 Profile = 0x01;    /* Profile Base */
	u8 notT2time = state->is24MHz ? 24 : 12;    /* early unlock detection time */

	if (state->T2Profile == T2P_Lite) {
		Profile = 0x05;
		notT2time = state->is24MHz ? 46 : 40;
	}

	if (state->plp != 0xffffffff) {
		state->T2Profile = ((state->plp & 0x100) != 0) ?
			T2P_Lite : T2P_Base;
		writeregt(state, 0x23, 0xAF, state->plp);
		writeregt(state, 0x23, 0xAD, 0x01);
	} else {
		state->T2Profile = T2P_Base;
		writeregt(state, 0x23, 0xAD, 0x00);
	}

	writebitst(state, 0x2E, 0x10, Profile, 0x07);
	writeregt(state, 0x2B, 0x19, notT2time);
}

static void C2_ReleasePreset(struct cxd_state *state)
{
	{
		static u8 data[2] = { 0x02, 0x80};

		writeregst(state, 0x27, 0xF4, data, sizeof(data));
	}
	writebitst(state, 0x27, 0x51, 0x40, 0xF0);
	writebitst(state, 0x27, 0x73, 0x07, 0x0F);
	writebitst(state, 0x27, 0x74, 0x19, 0x3F);
	writebitst(state, 0x27, 0x75, 0x19, 0x3F);
	writebitst(state, 0x27, 0x76, 0x19, 0x3F);
	if (state->bw == 6) {
		static u8 data[5] = { 0x17, 0xEA, 0xAA, 0xAA, 0xAA};

		writeregst(state, 0x20, 0x9F, data, sizeof(data));
	} else {
		static u8 data[5] = { 0x11, 0xF0, 0x00, 0x00, 0x00};

		writeregst(state, 0x20, 0x9F, data, sizeof(data));
	}
	writebitst(state, 0x27, 0xC9, 0x07, 0x07);
	writebitst(state, 0x20, 0xC2, 0x11, 0x33);
	{
		static u8 data[10] = { 0x16, 0xF0, 0x2B, 0xD8,
				       0x16, 0x16, 0xF0, 0x2C, 0xD8, 0x16 };

		writeregst(state, 0x2A, 0x20, data, sizeof(data));
	}
	{
		static u8 data[4] = { 0x00, 0x00, 0x00, 0x00 };

		writeregst(state, 0x50, 0x6B, data, sizeof(data));
	}
	writebitst(state, 0x50, 0x6F, 0x00, 0x40); /* Disable Preset */
}

static void C2_DemodSetting2(struct cxd_state *state)
{
	u8 data[6];
	u32 TunePosition =
		state->frontend.dtv_property_cache.frequency / 1000;

	if (state->bw == 6)
		TunePosition = ((TunePosition * 1792) / 3) / 1000;
	else
		TunePosition = (TunePosition * 448) / 1000;

	TunePosition = ((TunePosition + 6) / 12) * 12;

	pr_info("TunePosition = %u\n", TunePosition);

	data[0] = ((TunePosition >> 16) & 0xFF);
	data[1] = ((TunePosition >>  8) & 0xFF);
	data[2] = (TunePosition & 0xFF);
	data[3] = 0x02;
	data[4] = (state->DataSliceID & 0xFF);
	data[5] = (state->plp & 0xFF);
	writeregst(state, 0x50, 0x7A, data, sizeof(data));
	writebitst(state, 0x50, 0x87, 0x01, 0x01); /* Preset Clear */
}

static void Stop(struct cxd_state *state)
{

	writeregt(state, 0x00, 0xC3, 0x01); /* Disable TS */
}

static void ShutDown(struct cxd_state *state)
{
	switch (state->state) {
	case ActiveT2:
		ActiveT2_to_Sleep(state);
		break;
	case ActiveC2:
		ActiveC2_to_Sleep(state);
		break;
	case ActiveIT:
		ActiveIT_to_Sleep(state);
		break;
	default:
		Active_to_Sleep(state);
		break;
	}
}

static int gate_ctrl(struct dvb_frontend *fe, int enable)
{
	struct cxd_state *state = fe->demodulator_priv;

	return writebitsx(state, 0xFF, 0x08, enable ? 0x01 : 0x00, 0x01);
}

static void release(struct dvb_frontend *fe)
{
	struct cxd_state *state = fe->demodulator_priv;

	Stop(state);
	ShutDown(state);
	kfree(state);
}

static int sleep(struct dvb_frontend *fe)
{
	struct cxd_state *state = fe->demodulator_priv;

	Stop(state);
	ShutDown(state);
	return 0;
}

static int Start(struct cxd_state *state, u32 IntermediateFrequency)
{
	enum demod_state newDemodState = Unknown;
	u32 iffreq;

	if (state->state < Sleep)
		return -EINVAL;

	iffreq = MulDiv32(IntermediateFrequency, 16777216, state->is24MHz ? 48000000 : 41000000);

	switch (state->omode) {
	case OM_DVBT:
		if (state->type == CXD2838)
			return -EINVAL;
		newDemodState = ActiveT;
		break;
	case OM_DVBT2:
		if (state->type == CXD2838)
			return -EINVAL;
		newDemodState = ActiveT2;
		break;
	case OM_DVBC:
	case OM_QAM_ITU_C:
		if (state->type == CXD2838)
			return -EINVAL;
		newDemodState = ActiveC;
		break;
	case OM_DVBC2:
		if (state->type != CXD2843 && state->type != CXD2854)
			return -EINVAL;
		newDemodState = ActiveC2;
		break;
	case OM_ISDBT:
		if (state->type != CXD2838 && state->type != CXD2854)
			return -EINVAL;
		if (state->type == CXD2854 && !state->is24MHz && state->bw != 6)
			return -EINVAL;
		newDemodState = ActiveIT;
		break;
	default:
		return -EINVAL;
	}

	state->LockTimeout = 0;
	state->TSLockTimeout = 0;
	state->L1PostTimeout = 0;
	state->last_status = 0;
	state->FirstTimeLock = 1;
	state->LastBERNumerator = 0;
	state->LastBERDenominator = 1;
	state->BERScaleMax = 19;

	if (state->state == newDemodState) {
		writeregt(state, 0x00, 0xC3, 0x01);   /* Disable TS Output */
		switch (newDemodState) {
		case ActiveT:
			/* Stick with HP ( 0x01 = LP ) */
			writeregt(state, 0x10, 0x67, 0x00);
			BandSettingT(state, iffreq);
			state->BERScaleMax = 18;
			break;
		case ActiveT2:
			T2_SetParameters(state);
			BandSettingT2(state, iffreq);
			state->BERScaleMax = 12;
			break;
		case ActiveC:
			BandSettingC(state, iffreq);
			state->BERScaleMax = 19;
			break;
		case ActiveC2:
			BandSettingC2(state, iffreq);
			C2_ReleasePreset(state);
			C2_DemodSetting2(state);
			break;
		case ActiveIT:
			BandSettingIT(state, iffreq);
			break;
		default:
			break;
		}
	} else {
		if (state->state > Sleep) {
			switch (state->state) {
			case ActiveT2:
				ActiveT2_to_Sleep(state);
				break;
			case ActiveC2:
				ActiveC2_to_Sleep(state);
				break;
			case ActiveIT:
				ActiveIT_to_Sleep(state);
				break;
			default:
				Active_to_Sleep(state);
				break;
			}
		}
		switch (newDemodState) {
		case ActiveT:
			/* Stick with HP ( 0x01 = LP ) */
			writeregt(state, 0x10, 0x67, 0x00);
			Sleep_to_ActiveT(state, iffreq);
			state->BERScaleMax = 18;
			break;
		case ActiveT2:
			T2_SetParameters(state);
			Sleep_to_ActiveT2(state, iffreq);
			state->BERScaleMax = 12;
			break;
		case ActiveC:
			Sleep_to_ActiveC(state, iffreq);
			state->BERScaleMax = 19;
			break;
		case ActiveC2:
			Sleep_to_ActiveC2(state, iffreq);
			C2_ReleasePreset(state);
			C2_DemodSetting2(state);
			break;
		case ActiveIT:
			Sleep_to_ActiveIT(state, iffreq);
			break;
		default:
			break;
		}
	}
	state->state = newDemodState;
	writeregt(state, 0x00, 0xFE, 0x01);   /* SW Reset */
	writeregt(state, 0x00, 0xC3, 0x00);   /* Enable TS Output */

	return 0;
}

static int set_parameters(struct dvb_frontend *fe)
{
	int stat;
	struct cxd_state *state = fe->demodulator_priv;
	u32 IF;

	switch (fe->dtv_property_cache.delivery_system) {
	case SYS_DVBC_ANNEX_A:
		state->omode = OM_DVBC;
		break;
	case SYS_DVBC2:
		state->omode = OM_DVBC2;
		break;
	case SYS_DVBT:
		state->omode = OM_DVBT;
		break;
	case SYS_DVBT2:
		state->omode = OM_DVBT2;
		break;
	case SYS_ISDBT:
		state->omode = OM_ISDBT;
		break;
	default:
		return -EINVAL;
	}
	if (fe->ops.tuner_ops.set_params)
		fe->ops.tuner_ops.set_params(fe);
	state->bandwidth = fe->dtv_property_cache.bandwidth_hz;
	state->bw = (fe->dtv_property_cache.bandwidth_hz + 999999) / 1000000;
	if (fe->dtv_property_cache.stream_id == NO_STREAM_ID_FILTER) {
		state->DataSliceID = 0xffffffff;
		state->plp = 0xffffffff;
	} else {
		state->DataSliceID = (fe->dtv_property_cache.stream_id >> 8)
			& 0xff;
		state->plp = fe->dtv_property_cache.stream_id & 0xff;
	}
	/* printk("PLP = %08x, bw = %u\n", state->plp, state->bw); */
	if (fe->ops.tuner_ops.get_if_frequency)
		fe->ops.tuner_ops.get_if_frequency(fe, &IF);
	stat = Start(state, IF);
	return stat;
}


static void init(struct cxd_state *state)
{
	u8 data[2] = {0x00, 0x00}; /* 20.5 MHz */

	state->omode = OM_NONE;
	state->state   = Unknown;

	writeregx(state, 0xFF, 0x02, 0x00);
	usleep_range(4000, 5000);
	writeregx(state, 0x00, 0x15, 0x01);
	if (state->type != CXD2838)
		writeregx(state, 0x00, 0x17, 0x01);
	usleep_range(4000, 5000);

	writeregx(state, 0x00, 0x10, 0x01);

	writeregx(state, 0x00, 0x15, 0x00);
	usleep_range(3000, 4000);

	writeregsx(state, 0x00, 0x13, data, 0);
	if (state->is24MHz)
		writeregx(state, 0x00, 0x12, 0x00);

	writeregx(state, 0x00, 0x14, state->is24MHz ? 0x03 : 0x00);

	
	writeregx(state, 0x00, 0x10, 0x00);
	usleep_range(2000, 3000);

	state->curbankx = 0xFF;
	state->curbankt = 0xFF;

	writeregt(state, 0x00, 0x43, 0x0A);
	writeregt(state, 0x00, 0x41, 0x0A);
	if (state->type == CXD2838)
		writeregt(state, 0x60, 0x5A, 0x00);

	if (state->type == CXD2854) {
		writeregt(state, 0x00, 0x63, 0x16);
		writeregt(state, 0x00, 0x65, 0x27);
		writeregt(state, 0x00, 0x69, 0x06);
	}
	
	writebitst(state, 0x10, 0xCB, 0x00, 0x40);
	writeregt(state, 0x10, 0xCD, state->IF_FS);

	writebitst(state, 0x00, 0xC4, state->SerialMode ? 0x80 : 0x00, 0x98);
	writebitst(state, 0x00, 0xC5, 0x01, 0x07);
	writebitst(state, 0x00, 0xCB, 0x00, 0x01);
	writebitst(state, 0x00, 0xC6, 0x00, 0x1D);
	writebitst(state, 0x00, 0xC8, 0x01, 0x1D);
	writebitst(state, 0x00, 0xC9, 0x00, 0x1D);
	writebitst(state, 0x00, 0x83, 0x00, 0x07);
	writeregt(state, 0x00, 0x84, 0x00);
	writebitst(state, 0x00, 0xD3,
		   (state->type == CXD2838) ? 0x01 : 0x00, 0x01);
	writebitst(state, 0x00, 0xDE, 0x00, 0x01);

	state->state = Sleep;
}


static void init_state(struct cxd_state *state, struct cxd2843_cfg *cfg)
{
	state->adrt = cfg->adr;
	state->adrx = cfg->adr + 0x02;
	state->curbankt = 0xff;
	state->curbankx = 0xff;
	mutex_init(&state->mutex);

	state->SerialMode = cfg->parallel ? 0 : 1;
	state->ContinuousClock = 1;
	state->SerialClockFrequency =
		(cfg->ts_clock >= 1 && cfg->ts_clock <= 5) ?
		cfg->ts_clock :  1; /* 1 = fastest (82 MBit/s), 5 = slowest */
	/* IF Fullscale 0x50 = 1.4V, 0x39 = 1V, 0x28 = 0.7V */
	state->IF_FS = 0x50;
	state->is24MHz = (cfg->osc == 24000000) ? 1 : 0;
	printk("is24Mhz = %u, adr = %02x\n", state->is24MHz, cfg->adr);
}

static int get_tune_settings(struct dvb_frontend *fe,
			     struct dvb_frontend_tune_settings *sets)
{
	switch (fe->dtv_property_cache.delivery_system) {
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		/*return c_get_tune_settings(fe, sets);*/
	default:
		/* DVB-T: Use info.frequency_stepsize. */
		return -EINVAL;
	}
}

static int read_snr(struct dvb_frontend *fe, u16 *snr);

static int get_stats(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	u16 val;
	s64 str;

	if (fe->ops.tuner_ops.get_rf_strength)
		fe->ops.tuner_ops.get_rf_strength(fe, &val);
	else
		val = 0;

	str = 1000 * (s64) (s16) val;
	str -= 108750;
	p->strength.len = 1;
	p->strength.stat[0].scale = FE_SCALE_DECIBEL;
	p->strength.stat[0].svalue = str;
	
	read_snr(fe, &val);
	p->cnr.len = 1;
	p->cnr.stat[0].scale = FE_SCALE_DECIBEL;
	p->cnr.stat[0].svalue = 100 * (s64) (s16) val;
	return 0;
}


static int read_status(struct dvb_frontend *fe, enum fe_status *status)
{
	struct cxd_state *state = fe->demodulator_priv;
	u8 rdata;

	get_stats(fe);

	*status = 0;
	switch (state->state) {
	case ActiveC:
		readregst(state, 0x40, 0x88, &rdata, 1);
		if (rdata & 0x02)
			break;
		if (rdata & 0x01) {
			*status |= 0x07;
			readregst(state, 0x40, 0x10, &rdata, 1);
			if (rdata & 0x20)
				*status |= 0x1f;
		}
		if (*status == 0x1f && state->FirstTimeLock) {
			readregst(state, 0x40, 0x19, &rdata, 1);
			rdata &= 0x07;
			state->BERScaleMax = ( rdata < 2 ) ? 18 : 19;
			state->FirstTimeLock = 0;
		}
		break;
	case ActiveT:
		readregst(state, 0x10, 0x10, &rdata, 1);
		if (rdata & 0x10)
			break;
		if ((rdata & 0x07) == 0x06) {
			*status |= 0x07;
			if (rdata & 0x20)
				*status |= 0x1f;
		}
		if (*status == 0x1f && state->FirstTimeLock) {
			u8 tps[7];
			
			read_tps(state, tps);
			state->BERScaleMax =
				(((tps[0] >> 6) & 0x03) < 2 ) ? 17 : 18;
			if ((tps[0] & 7) < 2)
				state->BERScaleMax--;
			state->FirstTimeLock = 0;
		}
		break;
	case ActiveT2:
		readregst(state, 0x20, 0x10, &rdata, 1);
		if (rdata & 0x10)
			break;
		if ((rdata & 0x07) == 0x06) {
			*status |= 0x07;
			if (rdata & 0x20)
				*status |= 0x08;
		}
		if (*status & 0x08) {
			readregst(state, 0x22, 0x12, &rdata, 1);
			if (rdata & 0x01)
				*status |= 0x10;
		}
		break;
	case ActiveC2:
		readregst(state, 0x20, 0x10, &rdata, 1);
		if (rdata & 0x10)
			break;
		if ((rdata & 0x07) == 0x06) {
			*status |= 0x07;
			if (rdata & 0x20)
				*status |= 0x18;
		}
		if ((*status & 0x10) && state->FirstTimeLock) {
			u8 data;

			/* Change1stTrial */
			readregst(state, 0x28, 0xE6, &rdata, 1);
			data = rdata & 1;
			readregst(state, 0x50, 0x15, &rdata, 1);
			data |= ((rdata & 0x18) >> 2);
			/*writebitst(state, 0x50,0x6F,rdata,0x07);*/
			state->FirstTimeLock = 0;
		}
		break;
	case ActiveIT:
		readregst(state, 0x60, 0x10, &rdata, 1);
		if (rdata & 0x10)
			break;
		if (rdata & 0x02) {
			*status |= 0x07;
			if (rdata & 0x01)
				*status |= 0x18;
		}
		if (*status == 0x1f && state->FirstTimeLock) {
			/* readregst(state, 0x40, 0x19, &rdata, 1); */
			/* rdata &= 0x07; */
			/* state->BERScaleMax = ( rdata < 2 ) ? 18 : 19; */
			state->FirstTimeLock = 0;
		}
		break;
	default:
		break;
	}
	state->last_status = *status;
	return 0;
}

static int get_ber_t(struct cxd_state *state, u32 *n, u32 *d)
{
	u8 BERRegs[3];
	u8 Scale;

	*n = 0;
	*d = 1;

	readregst(state, 0x10, 0x62, BERRegs, 3);
	readregst(state, 0x10, 0x60, &Scale, 1);
	Scale &= 0x1F;

	if (BERRegs[0] & 0x80) {
		state->LastBERNumerator = (((u32) BERRegs[0] & 0x3F) << 16) |
			(((u32) BERRegs[1]) << 8) | BERRegs[2];
		state->LastBERDenominator = 1632 << Scale;
		if (state->LastBERNumerator < 256 &&
		    Scale < state->BERScaleMax) {
			writebitst(state, 0x10, 0x60, Scale + 1, 0x1F);
		} else if (state->LastBERNumerator > 512 && Scale > 11)
			writebitst(state, 0x10, 0x60, Scale - 1, 0x1F);
	}
	*n = state->LastBERNumerator;
	*d = state->LastBERDenominator;

	return 0;
}

static int get_ber_t2(struct cxd_state *state, u32 *n, u32 *d)
{
	u8 BERRegs[4];
	u8 Scale;
	u8 FECType;
	u8 CodeRate;
	static const u32 nBCHBitsLookup[2][8] = {
		/* R1_2   R3_5   R2_3   R3_4   R4_5   R5_6   R1_3   R2_5 */
		{7200,  9720,  10800, 11880, 12600, 13320, 5400,  6480}, /* 16K FEC */
		{32400, 38880, 43200, 48600, 51840, 54000, 21600, 25920} /* 64k FEC */
	};
	
	*n = 0;
	*d = 1;
	freeze_regst(state);
	readregst_unlocked(state, 0x24, 0x40, BERRegs, 4);
	readregst_unlocked(state, 0x22, 0x5e, &FECType, 1);
	readregst_unlocked(state, 0x22, 0x5b, &CodeRate, 1);

	FECType &= 0x03;
	CodeRate &= 0x07;
	unfreeze_regst(state);
	if (FECType > 1)
		return 0;


	readregst(state, 0x20, 0x72, &Scale, 1);
	Scale &= 0x0F;
	if (BERRegs[0] & 0x01) {
		state->LastBERNumerator = (((u32) BERRegs[1] & 0x3F) << 16) |
			(((u32) BERRegs[2]) << 8) | BERRegs[3];
		state->LastBERDenominator = nBCHBitsLookup[FECType][CodeRate] << Scale;
		if (state->LastBERNumerator < 256 &&
		    Scale < state->BERScaleMax) {
			writebitst(state, 0x20, 0x72, Scale + 1, 0x0F);
		} else if (state->LastBERNumerator > 512 && Scale > 8)
			writebitst(state, 0x20, 0x72, Scale - 1, 0x0F);
	}
	*n = state->LastBERNumerator;
	*d = state->LastBERDenominator;
	return 0;
}

static int get_ber_c(struct cxd_state *state, u32 *n, u32 *d)
{
	u8 BERRegs[3];
	u8 Scale;

	*n = 0;
	*d = 1;

	readregst(state, 0x40, 0x62, BERRegs, 3);
	readregst(state, 0x40, 0x60, &Scale, 1);
	Scale &= 0x1F;

	if (BERRegs[0] & 0x80) {
		state->LastBERNumerator = (((u32) BERRegs[0] & 0x3F) << 16) |
			(((u32) BERRegs[1]) << 8) | BERRegs[2];
		state->LastBERDenominator = 1632 << Scale;
		if (state->LastBERNumerator < 256 &&
		    Scale < state->BERScaleMax) {
			writebitst(state, 0x40, 0x60, Scale + 1, 0x1F);
		} else if (state->LastBERNumerator > 512 && Scale > 11)
			writebitst(state, 0x40, 0x60, Scale - 1, 0x1F);
	}
	*n = state->LastBERNumerator;
	*d = state->LastBERDenominator;

	return 0;
}

static int get_ber_c2(struct cxd_state *state, u32 *n, u32 *d)
{
	*n = 0;
	*d = 1;
	return 0;
}

static int get_ber_it(struct cxd_state *state, u32 *n, u32 *d)
{
	*n = 0;
	*d = 1;
	return 0;
}

static int read_ber(struct dvb_frontend *fe, u32 *ber)
{
	struct cxd_state *state = fe->demodulator_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	u32 n = 0, d = 1;
	int s = 0;

	*ber = 0;
	switch (state->state) {
	case ActiveT:
		s = get_ber_t(state, &n, &d);
		break;
	case ActiveT2:
		s = get_ber_t2(state, &n, &d);
		break;
	case ActiveC:
		s = get_ber_c(state, &n, &d);
		break;
	case ActiveC2:
		s = get_ber_c2(state, &n, &d);
		break;
	case ActiveIT:
		s = get_ber_it(state, &n, &d);
		break;
	default:
		break;
	}
	if (s)
		return s;
	
	p->pre_bit_error.len = 1;
	p->pre_bit_error.stat[0].scale = FE_SCALE_COUNTER;
	p->pre_bit_error.stat[0].uvalue = n;
	p->pre_bit_count.len = 1;
	p->pre_bit_count.stat[0].scale = FE_SCALE_COUNTER;
	p->pre_bit_count.stat[0].uvalue = d;
	if (d)
		*ber = (n * 1000) / d;
	return 0;
}

static int read_signal_strength(struct dvb_frontend *fe, u16 *strength)
{
	if (fe->ops.tuner_ops.get_rf_strength)
		fe->ops.tuner_ops.get_rf_strength(fe, strength);
	else
		*strength = 0;
	return 0;
}

#if 0
+NTSTATUS CCXD2843ER::GetT2PLPIds(DD_T2_PLPIDS* pT2_PLPIDS)
 {
     NTSTATUS status = STATUS_SUCCESS;
-    *pReturned = 0;
+
     if( m_DemodState != ActiveT2 ) return STATUS_NOT_IMPLEMENTED;
-    if( m_LastLockStatus < TSLock || m_LastLockStatus == Unlock ) return status;
+    if( m_LastLockStatus < TSLock ) return status;
 
     do
     {
+        u8 tmp;
+
         CHK_ERROR(FreezeRegsT());
 
+        CHK_ERROR(ReadRegT(0x20,0x5C,&tmp)); // OFDM Info
+
+        if( tmp & 0x20 ) pT2_PLPIDS->Flags |= DD_T2_PLPIDS_FEF;
+        if( m_T2Profile == T2P_Lite ) pT2_PLPIDS->Flags |= DD_T2_PLPIDS_LITE;
+
+        CHK_ERROR(ReadRegT(0x22,0x54,&tmp));
+        pT2_PLPIDS->PLPID = tmp;
+
+        CHK_ERROR(ReadRegT(0x22,0x54 + 19 + 13,&tmp));    // Interval
+        if( tmp > 0 )
+        {
+            CHK_ERROR(ReadRegT(0x22,0x54 + 19,&tmp));
+            pT2_PLPIDS->CommonPLPID = tmp;
+        }
+
         u8 nPids = 0;
         CHK_ERROR(ReadRegT(0x22,0x7F,&nPids));
 
-        pValues[0] = nPids;
-        if( nPids >= nValues ) nPids = nValues - 1;
+        pT2_PLPIDS->NumPLPS = nPids;
+        CHK_ERROR(ReadRegT(0x22,0x80,&pT2_PLPIDS->PLPList[0], nPids > 128 ? 128 : nPids));
 
-        CHK_ERROR(ReadRegT(0x22,0x80,&pValues[1], nPids > 128 ? 128 : nPids));
-
         if( nPids > 128 )
         {
-            CHK_ERROR(ReadRegT(0x23,0x10,&pValues[129], nPids - 128));
+            CHK_ERROR(ReadRegT(0x23,0x10,&pT2_PLPIDS->PLPList[128], nPids - 128));
         }
 
-        *pReturned = nPids + 1;
+
     }
     while(0);
     UnFreezeRegsT();

static void GetPLPIds(struct cxd_state *state, u32 nValues,
		      u8 *Values, u32 *Returned)
{
	u8 nPids = 0, tmp;

	*Returned = 0;
	if (state->state != ActiveT2)
		return;
	if (state->last_status != 0x1f)
		return;

	freeze_regst(state);
	readregst_unlocked(state, 0x22, 0x7F, &nPids, 1);

	Values[0] = nPids;
	if (nPids >= nValues)
		nPids = nValues - 1;

	readregst_unlocked(state, 0x22, 0x80, &Values[1],
			   nPids > 128 ? 128 : nPids);

	if (nPids > 128)
		readregst_unlocked(state, 0x23, 0x10, &Values[129],
				   nPids - 128);

	*Returned = nPids + 1;

	unfreeze_regst(state);
}
#endif

static void GetSignalToNoiseIT(struct cxd_state *state, u32 *SignalToNoise)
{
	u8 Data[2];
	u32 reg;

	*SignalToNoise = 0;
	freeze_regst(state);
	readregst_unlocked(state, 0x60, 0x28, Data, sizeof(Data));
	unfreeze_regst(state);

	reg = (Data[0] << 8) | Data[1];
	if (reg == 0)
		return;
	if (reg > 51441)
		reg = 51441;
	
	if (state->bw == 8) {
		if (reg > 1143)
			reg = 1143;
		*SignalToNoise = (Log10x100(reg) -
				  Log10x100(1200 - reg)) + 220;
	} else
		*SignalToNoise = Log10x100(reg) - 90;
}

static void GetSignalToNoiseC2(struct cxd_state *state, u32 *SignalToNoise)
{
	u8 Data[2];
	u32 reg;

	*SignalToNoise = 0;
	freeze_regst(state);
	readregst_unlocked(state, 0x20, 0x28, Data, sizeof(Data));
	unfreeze_regst(state);

	reg = (Data[0] << 8) | Data[1];
	if (reg == 0)
		return;
	if (reg > 51441)
		reg = 51441;

	*SignalToNoise = (Log10x100(reg) - Log10x100(55000 - reg)) + 384;
}


static void GetSignalToNoiseT2(struct cxd_state *state, u32 *SignalToNoise)
{
	u8 Data[2];
	u32 reg;

	*SignalToNoise = 0;
	freeze_regst(state);
	readregst_unlocked(state, 0x20, 0x28, Data, sizeof(Data));
	unfreeze_regst(state);

	reg = (Data[0] << 8) | Data[1];
	if (reg == 0)
		return;
	if (reg > 10876)
		reg = 10876;

	*SignalToNoise = (Log10x100(reg) - Log10x100(12600 - reg)) + 320;
}

static void GetSignalToNoiseT(struct cxd_state *state, u32 *SignalToNoise)
{
	u8 Data[2];
	u32 reg;

	*SignalToNoise = 0;
	freeze_regst(state);
	readregst_unlocked(state, 0x10, 0x28, Data, sizeof(Data));
	unfreeze_regst(state);

	reg = (Data[0] << 8) | Data[1];
	if (reg == 0)
		return;
	if (reg > 4996)
		reg = 4996;

	*SignalToNoise = (Log10x100(reg) - Log10x100(5350 - reg)) + 285;
}

static void GetSignalToNoiseC(struct cxd_state *state, u32 *SignalToNoise)
{
	u8 Data[2];
	u8 Constellation = 0;
	u32 reg;

	*SignalToNoise = 0;
	freeze_regst(state);
	readregst_unlocked(state, 0x40, 0x19, &Constellation, 1);
	readregst_unlocked(state, 0x40, 0x4C, Data, sizeof(Data));
	unfreeze_regst(state);

	reg = ((u32)(Data[0] & 0x1F) << 8) | (Data[1]);
	if (reg == 0)
		return;

	switch (Constellation & 0x07) {
	case 0: /* QAM 16 */
	case 2: /* QAM 64 */
	case 4: /* QAM 256 */
		if (reg < 126)
			reg = 126;
		*SignalToNoise = ((439 - Log10x100(reg)) * 2134 + 500) / 1000;
		break;
	case 1: /* QAM 32 */
	case 3: /* QAM 128 */
		if (reg < 69)
			reg = 69;
		*SignalToNoise = ((432 - Log10x100(reg)) * 2015 + 500) / 1000;
		break;
	}
}

static int read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct cxd_state *state = fe->demodulator_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	u32 SNR = 0;

	*snr = 0;
	if (state->last_status != 0x1f)
		return 0;

	switch (state->state) {
	case ActiveC:
		GetSignalToNoiseC(state, &SNR);
		break;
	case ActiveC2:
		GetSignalToNoiseC2(state, &SNR);
		break;
	case ActiveT:
		GetSignalToNoiseT(state, &SNR);
		break;
	case ActiveT2:
		GetSignalToNoiseT2(state, &SNR);
		break;
	case ActiveIT:
		GetSignalToNoiseIT(state, &SNR);
		break;
	default:
		break;
	}
	*snr = SNR;
	p->cnr.len = 1;
	p->cnr.stat[0].scale = FE_SCALE_DECIBEL;
	p->cnr.stat[0].svalue = 10 * (s64) SNR;
	return 0;
}

static int read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}

static int tune(struct dvb_frontend *fe, bool re_tune,
		unsigned int mode_flags,
		unsigned int *delay, enum fe_status *status)
{
	struct cxd_state *state = fe->demodulator_priv;
	int r;

	if (re_tune) {
		r = set_parameters(fe);
		if (r)
			return r;
		state->tune_time = jiffies;

	}
	r = read_status(fe, status);
	if (r)
		return r;
	if (*status & FE_HAS_LOCK) {
		*delay = HZ;
		return 0;
	}
	return 0;
}

static enum dvbfe_search search(struct dvb_frontend *fe)
{
	int r;
	u32 loops = 20, i;
	enum fe_status status;

	r = set_parameters(fe);

	for (i = 0; i < loops; i++)  {
		msleep(50);
		r = read_status(fe, &status);
		if (r)
			return DVBFE_ALGO_SEARCH_ERROR;
		if (status & FE_HAS_LOCK)
			break;
	}

	if (status & FE_HAS_LOCK)
		return DVBFE_ALGO_SEARCH_SUCCESS;
	else
		return DVBFE_ALGO_SEARCH_AGAIN;
}

static enum dvbfe_algo get_algo(struct dvb_frontend *fe)
{
	return DVBFE_ALGO_HW;
}

static int get_fe_t2(struct cxd_state *state, struct dtv_frontend_properties *p)
{
	//struct dvb_frontend *fe = &state->frontend;
	u8 ofdm[5], modcod[2];
	
	freeze_regst(state);
	readregst_unlocked(state, 0x20, 0x5c, ofdm, 5);
	readregst_unlocked(state, 0x22, 0x5b, modcod, 2);
	unfreeze_regst(state);
	
        switch (modcod[0] & 0x07) {
	case 0:
		p->fec_inner = FEC_1_2;
		break;
	case 1:
		p->fec_inner = FEC_3_5;
		break;
	case 2:
		p->fec_inner = FEC_2_3;
		break;
	case 3:
		p->fec_inner = FEC_3_4;
		break;
	case 4:
		p->fec_inner = FEC_4_5;
		break;
	case 5:
		p->fec_inner = FEC_5_6;
		break;
	case 6:
		p->fec_inner = FEC_1_3;
		break;
	case 7:
		p->fec_inner = FEC_2_5;
		break;
	}
	
        switch (modcod[1] & 0x07) {
	case 0:
		p->modulation = QPSK;
		break;
	case 1:
		p->modulation = QAM_16;
		break;
	case 2:
		p->modulation = QAM_64;
		break;
	case 3:
		p->modulation = QAM_256;
		break;
	}
	
	switch (ofdm[0] & 0x07) {
	case 0:
		p->transmission_mode = TRANSMISSION_MODE_2K;
		break;
	case 1:
		p->transmission_mode = TRANSMISSION_MODE_8K;
		break;
	case 2:
		p->transmission_mode = TRANSMISSION_MODE_4K;
		break;
	case 3:
		p->transmission_mode = TRANSMISSION_MODE_1K;
		break;
	case 4:
		p->transmission_mode = TRANSMISSION_MODE_16K;
		break;
	case 5:
		p->transmission_mode = TRANSMISSION_MODE_32K;
		break;
	}

        switch ((ofdm[1] >> 4) & 0x07) {
	case 0:
		p->guard_interval = GUARD_INTERVAL_1_32;
		break;
	case 1:
		p->guard_interval = GUARD_INTERVAL_1_16;
		break;
	case 2:
		p->guard_interval = GUARD_INTERVAL_1_8;
		break;
	case 3:
		p->guard_interval = GUARD_INTERVAL_1_4;
		break;
	case 4:
		p->guard_interval = GUARD_INTERVAL_1_128;
		break;
	case 5:
		p->guard_interval = GUARD_INTERVAL_19_128;
		break;
	case 6:
		p->guard_interval = GUARD_INTERVAL_19_256;
		break;
	}
	
	return 0;
}

static int get_fe_t(struct cxd_state *state, struct dtv_frontend_properties *p)
{
	//struct dvb_frontend *fe = &state->frontend;
	u8 tps[7];

	read_tps(state, tps);

/*  TPSData[0] [7:6]  CNST[1:0]
    TPSData[0] [5:3]  HIER[2:0]
    TPSData[0] [2:0]  HRATE[2:0]
*/
	switch ((tps[0] >> 6) & 0x03) {
	case 0:
		p->modulation = QPSK;
		break;
	case 1:
		p->modulation = QAM_16;
		break;
	case 2:
		p->modulation = QAM_64;
		break;
	}
	switch ((tps[0] >> 3) & 0x07) {
	case 0:
		p->hierarchy = HIERARCHY_NONE;
		break;
	case 1:
		p->hierarchy = HIERARCHY_1;
		break;
	case 2:
		p->hierarchy = HIERARCHY_2;
		break;
	case 3:
		p->hierarchy = HIERARCHY_4;
		break;
	}
	switch ((tps[0] >> 0) & 0x07) {
	case 0:
		p->code_rate_HP = FEC_1_2;
		break;
	case 1:
		p->code_rate_HP = FEC_2_3;
		break;
	case 2:
		p->code_rate_HP = FEC_3_4;
		break;
	case 3:
		p->code_rate_HP = FEC_5_6;
		break;
	case 4:
		p->code_rate_HP = FEC_7_8;
		break;
	}

/*  TPSData[1] [7:5]  LRATE[2:0]
    TPSData[1] [4:3]  GI[1:0]
    TPSData[1] [2:1]  MODE[1:0]
*/
	switch ((tps[1] >> 5) & 0x07) {
	case 0:
		p->code_rate_LP = FEC_1_2;
		break;
	case 1:
		p->code_rate_LP = FEC_2_3;
		break;
	case 2:
		p->code_rate_LP = FEC_3_4;
		break;
	case 3:
		p->code_rate_LP = FEC_5_6;
		break;
	case 4:
		p->code_rate_LP = FEC_7_8;
		break;
	}
	switch ((tps[1] >> 3) & 0x03) {
	case 0:
		p->guard_interval = GUARD_INTERVAL_1_32;
		break;
	case 1:
		p->guard_interval = GUARD_INTERVAL_1_16;
		break;
	case 2:
		p->guard_interval = GUARD_INTERVAL_1_8;
		break;
	case 3:
		p->guard_interval = GUARD_INTERVAL_1_4;
		break;
	}
	switch ((tps[1] >> 1) & 0x03) {
	case 0:
		p->transmission_mode = TRANSMISSION_MODE_2K;
		break;
	case 1:
		p->transmission_mode = TRANSMISSION_MODE_8K;
		break;
	}

	return 0;
}

static int get_fe_c(struct cxd_state *state,  struct dtv_frontend_properties *p)
{
	//struct dvb_frontend *fe = &state->frontend;
	u8 qam;

	freeze_regst(state);
	readregst_unlocked(state, 0x40, 0x19, &qam, 1);
	unfreeze_regst(state);
	p->modulation = 1 + (qam & 0x07);
	return 0;
}

static int get_frontend(struct dvb_frontend *fe, struct dtv_frontend_properties *p)
{
	struct cxd_state *state = fe->demodulator_priv;

	if (state->last_status != 0x1f)
		return 0;

	switch (state->state) {
	case ActiveT:
		get_fe_t(state, p);
		break;
	case ActiveT2:
		get_fe_t2(state, p);
		break;
	case ActiveC:
		get_fe_c(state, p);
		break;
	case ActiveC2:
		break;
	case ActiveIT:
		break;
	default:
		break;
	}
	return 0;
}

static struct dvb_frontend_ops common_ops_2854 = {
	.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBT, SYS_DVBT2, SYS_DVBC2, SYS_ISDBT },
	.info = {
		.name = "CXD2854 DVB-C/C2 DVB-T/T2 ISDB-T",
		.frequency_stepsize_hz = 166667,	/* DVB-T only */
		.frequency_min_hz = 47000000,	/* DVB-T: 47125000 */
		.frequency_max_hz = 865000000,	/* DVB-C: 862000000 */
		.symbol_rate_min = 870000,
		.symbol_rate_max = 11700000,
		.caps = FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_32 |
		        FE_CAN_QAM_64 | FE_CAN_QAM_128 | FE_CAN_QAM_256 |
		        FE_CAN_QAM_AUTO | 
			FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_4_5 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS | FE_CAN_2G_MODULATION |
		        FE_CAN_MULTISTREAM
	},
	.release = release,
	.sleep = sleep,
	.i2c_gate_ctrl = gate_ctrl,
	.set_frontend = set_parameters,

	.get_tune_settings = get_tune_settings,
	.read_status = read_status,
	.read_ber = read_ber,
	.read_signal_strength = read_signal_strength,
	.read_snr = read_snr,
	.read_ucblocks = read_ucblocks,
	.get_frontend = get_frontend,
};

static struct dvb_frontend_ops common_ops_2843 = {
	.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBT, SYS_DVBT2, SYS_DVBC2 },
	.info = {
		.name = "CXD2843 DVB-C/C2 DVB-T/T2",
		.frequency_stepsize_hz = 166667,	/* DVB-T only */
		.frequency_min_hz = 47000000,	/* DVB-T: 47125000 */
		.frequency_max_hz = 865000000,	/* DVB-C: 862000000 */
		.symbol_rate_min = 870000,
		.symbol_rate_max = 11700000,
		.caps = FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_32 |
		        FE_CAN_QAM_64 | FE_CAN_QAM_128 | FE_CAN_QAM_256 |
		        FE_CAN_QAM_AUTO | 
			FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_4_5 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS | FE_CAN_2G_MODULATION |
		        FE_CAN_MULTISTREAM
	},
	.release = release,
	.sleep = sleep,
	.i2c_gate_ctrl = gate_ctrl,
	.set_frontend = set_parameters,

	.get_tune_settings = get_tune_settings,
	.read_status = read_status,
	.read_ber = read_ber,
	.read_signal_strength = read_signal_strength,
	.read_snr = read_snr,
	.read_ucblocks = read_ucblocks,
	.get_frontend = get_frontend,
#ifdef USE_ALGO
	.get_frontend_algo = get_algo,
	.search = search,
	.tune = tune,
#endif
};

static struct dvb_frontend_ops common_ops_2837 = {
	.delsys = { SYS_DVBC_ANNEX_A, SYS_DVBT, SYS_DVBT2 },
	.info = {
		.name = "CXD2837 DVB-C DVB-T/T2",
		.frequency_stepsize_hz = 166667,	/* DVB-T only */
		.frequency_min_hz = 47000000,	/* DVB-T: 47125000 */
		.frequency_max_hz = 865000000,	/* DVB-C: 862000000 */
		.symbol_rate_min = 870000,
		.symbol_rate_max = 11700000,
		.caps = FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_32 |
		        FE_CAN_QAM_64 | FE_CAN_QAM_128 | FE_CAN_QAM_256 |
		        FE_CAN_QAM_AUTO | 
			FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_4_5 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS | FE_CAN_2G_MODULATION |
		        FE_CAN_MULTISTREAM
	},
	.release = release,
	.sleep = sleep,
	.i2c_gate_ctrl = gate_ctrl,
	.set_frontend = set_parameters,

	.get_tune_settings = get_tune_settings,
	.read_status = read_status,
	.read_ber = read_ber,
	.read_signal_strength = read_signal_strength,
	.read_snr = read_snr,
	.read_ucblocks = read_ucblocks,
	.get_frontend = get_frontend,
#ifdef USE_ALGO
	.get_frontend_algo = get_algo,
	.search = search,
	.tune = tune,
#endif
};

static struct dvb_frontend_ops common_ops_2838 = {
	.delsys = { SYS_ISDBT },
	.info = {
		.name = "CXD2838 ISDB-T",
		.frequency_stepsize_hz = 166667,
		.frequency_min_hz = 47000000,
		.frequency_max_hz = 865000000,
		.symbol_rate_min = 870000,
		.symbol_rate_max = 11700000,
		.caps = FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 | FE_CAN_QAM_AUTO |
			FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_4_5 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO | FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER | FE_CAN_MUTE_TS | FE_CAN_2G_MODULATION
	},
	.release = release,
	.sleep = sleep,
	.i2c_gate_ctrl = gate_ctrl,
	.set_frontend = set_parameters,

	.get_tune_settings = get_tune_settings,
	.read_status = read_status,
	.read_ber = read_ber,
	.read_signal_strength = read_signal_strength,
	.read_snr = read_snr,
	.read_ucblocks = read_ucblocks,
#ifdef USE_ALGO
	.get_frontend_algo = get_algo,
	.search = search,
	.tune = tune,
#endif
};

static int probe(struct cxd_state *state)
{
	u8 ChipID = 0x00;
	int status;

	status = readregst(state, 0x00, 0xFD, &ChipID, 1);
	if (status)
		status = readregsx(state, 0x00, 0xFD, &ChipID, 1);
	if (status)
		return status;
	
	state->repi2cerr = 1;
	//pr_info("cxd2843: ChipID  = %02X\n", ChipID);
	switch (ChipID) {
	case 0xa4:
		state->type = CXD2843;
		memcpy(&state->frontend.ops, &common_ops_2843,
		       sizeof(struct dvb_frontend_ops));
		break;
	case 0xb1:
		state->type = CXD2837;
		memcpy(&state->frontend.ops, &common_ops_2837,
		       sizeof(struct dvb_frontend_ops));
		break;
	case 0xb0:
		state->type = CXD2838;
		memcpy(&state->frontend.ops, &common_ops_2838,
		       sizeof(struct dvb_frontend_ops));
		break;
	case 0xc1:
		state->type = CXD2854;
		memcpy(&state->frontend.ops, &common_ops_2854,
		       sizeof(struct dvb_frontend_ops));
		state->is2k14 = 1;
		break;
	default:
		return -1;
	}
	state->frontend.demodulator_priv = state;
	return 0;
}

struct dvb_frontend *cxd2843_attach(struct i2c_adapter *i2c,
				    struct cxd2843_cfg *cfg)
{
	struct cxd_state *state = NULL;

	state = kzalloc(sizeof(struct cxd_state), GFP_KERNEL);
	if (!state)
		return NULL;

	state->i2c = i2c;
	init_state(state, cfg);
	if (probe(state) == 0) {
		init(state);
		return &state->frontend;
	}
	pr_err("cxd2843: not found\n");
	kfree(state);
	return NULL;
}
EXPORT_SYMBOL_GPL(cxd2843_attach);

MODULE_DESCRIPTION("CXD2843/37/38 driver");
MODULE_AUTHOR("Ralph Metzler, Manfred Voelkel");
MODULE_LICENSE("GPL v2");
