/*
 * ddbridge-mci.h: Digital Devices micro code interface
 *
 * Copyright (C) 2017 Digital Devices GmbH
 *                    Marcus Metzler <mocm@metzlerbros.de>
 *                    Ralph Metzler <rjkm@metzlerbros.de>
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

#ifndef _DDBRIDGE_MCI_H_
#define _DDBRIDGE_MCI_H_

#define MCI_CONTROL                         (0x500)
#define MCI_COMMAND                         (0x600)
#define MCI_RESULT                          (0x680)

#define MCI_COMMAND_SIZE                    (0x80)
#define MCI_RESULT_SIZE                     (0x80)

#define MCI_CONTROL_START_COMMAND           (0x00000001)
#define MCI_CONTROL_ENABLE_DONE_INTERRUPT   (0x00000002)
#define MCI_CONTROL_RESET                   (0x00008000)
#define MCI_CONTROL_READY                   (0x00010000)

#define SX8_TSCONFIG                        (0x280)

#define SX8_TSCONFIG_MODE_MASK              (0x00000003)
#define SX8_TSCONFIG_MODE_OFF               (0x00000000)
#define SX8_TSCONFIG_MODE_NORMAL            (0x00000001)
#define SX8_TSCONFIG_MODE_IQ                (0x00000003)


#define SX8_DEMOD_STOPPED       (0)
#define SX8_DEMOD_WAIT_SIGNAL   (2)
#define SX8_DEMOD_TIMEOUT       (14)
#define SX8_DEMOD_LOCKED        (15)

#define SX8_CMD_TUNER_ENABLE    (0x01)
#define SX8_CMD_TUNER_DISABLE   (0x02)
#define SX8_CMD_SEARCH          (0x03)
#define SX8_CMD_GETSTATUS       (0x04)
#define SX8_CMD_STOP            (0x05)
#define SX8_CMD_GETSIGNALINFO   (0x07)

#define SX8_CMD_SELECT_IQOUT    (0x10)
#define SX8_CMD_SELECT_TSOUT    (0x11)

#define SX8_CMD_DIAG_READ8      (0xE0)
#define SX8_CMD_DIAG_READ32     (0xE1)
#define SX8_CMD_DIAG_WRITE8     (0xE2)
#define SX8_CMD_DIAG_WRITE32    (0xE3)

#define SX8_CMD_DIAG_READV      (0xE8)
#define SX8_CMD_DIAG_WRITEV     (0xE9)


#define M4_CMD_DIAG_READX       (0xE0)
#define M4_CMD_DIAG_READT       (0xE1)
#define M4_CMD_DIAG_WRITEX      (0xE2)
#define M4_CMD_DIAG_WRITET      (0xE3)

#define M4_CMD_DIAG_READRF      (0xE8)
#define M4_CMD_DIAG_WRITERF     (0xE9)


#define SX8_ERROR_UNSUPPORTED   (0x80)
#define SX8_SUCCESS(status)     (status < SX8_ERROR_UNSUPPORTED)

struct mci_command {
    union {
        u32 command_word;
        struct {
            u8 command;
            u8 tuner;
            u8 demod;
            u8 output;
        };
    };
    union {
        u32 params[31];
        struct {
		u8  flags;
		u8  s2_modulation_mask;
		u8  rsvd;
		u8  retry;
		u32 frequency;
		u32 symbol_rate;
        } dvbs2_search;
    };
};

struct mci_result {
	union {
		u32 status_word;
		struct {
			u8 status;
			u8 rsvd;
			u16 time;
		};
	};
	union {
		u32 result[27];
		struct {
			u8  standard;
			u8  pls_code;   /* puncture rate for DVB-S */
			u8  roll_off;  /* 7-6: rolloff, 5-2: rsrvd, 1:short, 0:pilots */
			u8  rsvd;
			u32 frequency;
			u32 symbol_rate;
			s16 channel_power;
			s16 band_power;
			s16 signal_to_noise;
			s16 rsvd2;
			u32 packet_errors;
			u32 ber_numerator;
			u32 ber_denominator;		
		} dvbs2_signal_info;
	};
	u32 version[4];
};

struct mci_cfg {


};

struct dvb_frontend *ddb_mci_attach(struct ddb_input *input, int mci_type, int nr);
#endif
