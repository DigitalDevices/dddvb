/*
 * ddbridge-mci.h: Digital Devices micro code interface
 *
 * Copyright (C) 2017-2018 Digital Devices GmbH
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

#ifndef _DDBRIDGE_MCI_H_
#define _DDBRIDGE_MCI_H_

#define SX8_TSINPUT             (0x280)
#define MIC_CONTROL             (0x500)
#define MIC_PROGMEM_OLD         (0x4000)
#define MIC_PROGMEM_OLD_SIZE    (0x4000)

#define MIC_PROGMEM             (0x8000)
#define MIC_PROGMEM_SIZE        (0x8000)

#define MIC_DATAMEM             (0x8000)
#define MIC_DATAMEM_SIZE        (0x2000)

#define MIC_INTERFACE_IN        (0x0600)
#define MIC_INTERFACE_OUT       (0x0680)
#define MIC_INTERFACE_VER       (0x06F0)

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

/*
 * IQMode only vailable on MaxSX8 on a single tuner
 *
 * IQ_MODE_SAMPLES
 *       sampling rate is 1550/24 MHz (64.583 MHz)
 *       channel agc is frozen, to allow stitching the FFT results together
 *
 * IQ_MODE_VTM
 *       sampling rate is the supplied symbolrate
 *       channel agc is active
 *
 *  in both cases down sampling is done with a RRC Filter (currently fixed to alpha = 0.05)
 *  which causes some (ca 5%) aliasing at the edges from outside the spectrum
*/


#define SX8_TSCONFIG_TSHEADER               (0x00000004)
#define SX8_TSCONFIG_BURST                  (0x00000008)

#define SX8_TSCONFIG_BURSTSIZE_MASK         (0x00000030)
#define SX8_TSCONFIG_BURSTSIZE_2K           (0x00000000)
#define SX8_TSCONFIG_BURSTSIZE_4K           (0x00000010)
#define SX8_TSCONFIG_BURSTSIZE_8K           (0x00000020)
#define SX8_TSCONFIG_BURSTSIZE_16K          (0x00000030)

/* additional TS input control bits on MaxSX8 DD01:0009 */
#define TS_INPUT_CONTROL_SIZEMASK           (0x00000030)
#define TS_INPUT_CONTROL_SIZE188            (0x00000000)
#define TS_INPUT_CONTROL_SIZE192            (0x00000010)
#define TS_INPUT_CONTROL_SIZE196            (0x00000020)


/********************************************************/

#define MCI_DEMOD_STOPPED        (0)
#define MCI_DEMOD_WAIT_SIGNAL    (2)
#define MCI_DEMOD_TIMEOUT        (14)
#define MCI_DEMOD_LOCKED         (15)

#define SX8_DEMOD_IQ_MODE        (1)
#define SX8_DEMOD_WAIT_MATYPE    (3)

#define M4_DEMOD_WAIT_TS         (6)
#define M4_DEMOD_C2SCAN          (16)

#define MCI_STATUS_OK                 (0x00)
#define MCI_STATUS_UNSUPPORTED        (0x80)
#define MCI_STATUS_INVALID_PARAMETER  (0xFC)
#define MCI_STATUS_RETRY              (0xFD)
#define MCI_STATUS_NOT_READY          (0xFE)
#define MCI_STATUS_ERROR              (0xFF)

#define MCI_CMD_STOP             (0x01)
#define MCI_CMD_GETSTATUS        (0x02)
#define MCI_CMD_GETSIGNALINFO    (0x03)
//#define MCI_CMD_RFPOWER          (0x04)

#define MCI_CMD_SEARCH_DVBS     (0x10)
#define MCI_CMD_SEARCH_ISDBS    (0x11)

#define MCI_CMD_SEARCH_DVBC     (0x20)
#define MCI_CMD_SEARCH_DVBT     (0x21)
#define MCI_CMD_SEARCH_DVBT2    (0x22)
#define MCI_CMD_SEARCH_DVBC2    (0x23)

#define MCI_CMD_SEARCH_ISDBT    (0x24)
#define MCI_CMD_SEARCH_ISDBC    (0x25)
#define MCI_CMD_SEARCH_J83B     (0x26)

#define MCI_CMD_GET_IQSYMBOL     (0x30)

#define MCI_BANDWIDTH_UNKNOWN    (0)
#define MCI_BANDWIDTH_1_7MHZ     (1)
#define MCI_BANDWIDTH_5MHZ       (5)
#define MCI_BANDWIDTH_6MHZ       (6)
#define MCI_BANDWIDTH_7MHZ       (7)
#define MCI_BANDWIDTH_8MHZ       (8)

#define SX8_CMD_GETBIST          (0x0F)
#define SX8_CMD_INPUT_ENABLE     (0x40)
#define SX8_CMD_INPUT_DISABLE    (0x41)
#define SX8_CMD_START_IQ         (0x42)
#define SX8_CMD_STOP_IQ          (0x43)
#define SX8_CMD_ENABLE_IQOUTPUT  (0x44)
#define SX8_CMD_DISABLE_IQOUTPUT (0x45)

#define M4_CMD_GET_L1INFO         (0x50)
#define M4_CMD_GET_IDS            (0x51)
#define M4_CMD_GET_DVBT_TPS       (0x52)
#define MCI_CMD_GET_BBHEADER      (0x53)
#define M4_CMD_GET_ISDBT_TMCC     (0x54)
#define M4_CMD_GET_ISDBS_TMCC     (0x55)
#define M4_CMD_GET_ISDBC_TSMF     (0x56)

#define M4_CMD_GET_BBHEADER       (MCI_CMD_GET_BBHEADER)

#define M4_L1INFO_SEL_PRE         (0)
#define M4_L1INFO_SEL_DSINFO      (1)
#define M4_L1INFO_SEL_PLPINFO     (2)
#define M4_L1INFO_SEL_PLPINFO_C   (3)
#define M4_L1INFO_SEL_SETID       (0x80)

#define MCI_BANDWIDTH_EXTENSION  (0x80)   // currently used only for J83B in Japan

#define M4_MODE_DVBSX            (2)
#define M4_MODE_DVBC             (3)
#define M4_MODE_DVBT             (4)
#define M4_MODE_DVBT2            (5)
#define M4_MODE_DVBC2            (6)
#define M4_MODE_J83B             (7)
#define M4_MODE_ISDBT            (8)
#define M4_MODE_ISDBC            (9)
#define M4_MODE_ISDBS            (10)

#define M4_DVBC_CONSTELLATION_16QAM  (0)
#define M4_DVBC_CONSTELLATION_32QAM  (1)
#define M4_DVBC_CONSTELLATION_64QAM  (2)  // also valid for J83B and ISDB-C
#define M4_DVBC_CONSTELLATION_128QAM (3)
#define M4_DVBC_CONSTELLATION_256QAM (4)  // also valid for J83B and ISDB-C

#define M4_SIGNALINFO_FLAG_CHANGE (0x01)
#define M4_SIGNALINFO_FLAG_EWS    (0x02)

#define SX8_ROLLOFF_35  0
#define SX8_ROLLOFF_25  1
#define SX8_ROLLOFF_20  2
#define SX8_ROLLOFF_15  5
#define SX8_ROLLOFF_10  3
#define SX8_ROLLOFF_05  4

#define MCI_SUCCESS(status)     ((status & MCI_STATUS_UNSUPPORTED) == 0)

/********************************************************/

#define MOD_SETUP_CHANNELS        (0x60)
#define MOD_SETUP_OUTPUT          (0x61)
#define MOD_SETUP_STREAM          (0x62)
#define MOD_SET_STREAM_CHANNEL    (0x63)

#define MOD_SETUP_FLAG_FIRST      (0x01)
#define MOD_SETUP_FLAG_LAST       (0x02)
#define MOD_SETUP_FLAG_VALID      (0x80)

#define MOD_STANDARD_GENERIC      (0x00)
#define MOD_STANDARD_DVBT_8       (0x01)
#define MOD_STANDARD_DVBT_7       (0x02)
#define MOD_STANDARD_DVBT_6       (0x03)
#define MOD_STANDARD_DVBT_5       (0x04)

#define MOD_STANDARD_DVBC_8       (0x08)
#define MOD_STANDARD_DVBC_7       (0x09)
#define MOD_STANDARD_DVBC_6       (0x0A)

#define MOD_STANDARD_J83A_8       (MOD_STANDARD_DVBC_8)
#define MOD_STANDARD_J83A_7       (MOD_STANDARD_DVBC_7)
#define MOD_STANDARD_J83A_6       (MOD_STANDARD_DVBC_6)

#define MOD_STANDARD_J83B_QAM64   (0x0B)
#define MOD_STANDARD_J83B_QAM256  (0x0C)

#define MOD_STANDARD_ISDBC_QAM64  (0x0D)
#define MOD_STANDARD_ISDBC_QAM256 (0x0E)

#define MOD_STANDARD_J83C_QAM64   (MOD_STANDARD_ISDBC_QAM64 )
#define MOD_STANDARD_J83C_QAM256  (MOD_STANDARD_ISDBC_QAM256)

#define MOD_CONNECTOR_OFF         (0x00)
#define MOD_CONNECTOR_F           (0x01)
#define MOD_CONNECTOR_SMA         (0x02)

#define MOD_UNIT_DBUV             (0x00)
#define MOD_UNIT_DBM              (0x01)

#define MOD_FORMAT_DEFAULT        (0x00)
#define MOD_FORMAT_IQ16           (0x01)
#define MOD_FORMAT_IQ8            (0x02)
#define MOD_FORMAT_IDX8           (0x03)
#define MOD_FORMAT_TS             (0x04)

#define MOD_DVBT_FFT_8K           (0x01)
#define MOD_DVBT_GI_1_32          (0x00)
#define MOD_DVBT_GI_1_16          (0x01)
#define MOD_DVBT_GI_1_8           (0x02)
#define MOD_DVBT_GI_1_4           (0x03)

#define MOD_DVBT_PR_1_2           (0x00)
#define MOD_DVBT_PR_2_3           (0x01)
#define MOD_DVBT_PR_3_4           (0x02)
#define MOD_DVBT_PR_5_6           (0x03)
#define MOD_DVBT_PR_7_8           (0x04)

#define MOD_DVBT_QPSK             (0x00)
#define MOD_DVBT_16QAM            (0x01)
#define MOD_DVBT_64QAM            (0x02)

#define MOD_QAM_DVBC_16           (0x00)
#define MOD_QAM_DVBC_32           (0x01)
#define MOD_QAM_DVBC_64           (0x02)
#define MOD_QAM_DVBC_128          (0x03)
#define MOD_QAM_DVBC_256          (0x04)

#define MOD_QAM_J83B_64           (0x05)
#define MOD_QAM_J83B_256          (0x06)

#define MOD_QAM_GENERIC           (0x07)

#define MOD_QAM_ISDBC_64          (0x08)
#define MOD_QAM_ISDBC_256         (0x09)

#define CMD_GET_SERIALNUMBER    (0xF0)
#define CMD_EXPORT_LICENSE      (0xF0)

struct mod_setup_channels {
	u8   flags;
	u8   standard;
	u8   num_channels;
	u8   rsvd;
	u32  frequency;
	u32  offset;            /* used only when Standard == 0 */
	u32  bandwidth;         /* used only when Standard == 0 */
};

struct mod_ofdm_parameter {
	u8   fft_size;           /* 0 = 2K, 1 = 8K  (2K not yet supported) */
	u8   guard_interval;     /* 0 = 1/32, 1 = 1/16, 2 = 1/8, 3 = 1/4  (DVB-T Encoding) */
	u8   puncture_rate;      /* 0 = 1/2, 1 = 2/3, 2 = 3/4, 3 = 5/6, 4 = 7/8  (DVB-T Encoding) */
	u8   constellation;      /* MOD_DVBT_QPSK, MOD_DVBT_16QAM, MOD_DVBT_64QAM */
	u8   rsvd2[2];           /* Reserved for DVB-T hierarchical */
	u16  cell_identifier;
};

struct mod_qam_parameter {
	u8   modulation;
	u8   rolloff;           /* Legal values:  12,13,15,18 */
};

struct mod_setup_stream {
	u8   standard;
	u8   stream_format;
	u8   rsvd1[2];
	u32  symbol_rate;        /* only used when Standard doesn't define a fixed symbol rate */
	union {
		struct mod_ofdm_parameter ofdm;
		struct mod_qam_parameter qam;
	};
};

struct mod_setup_output {
	u8   connector;         /* 0 = OFF, 1 = F, 2 = SMA */
	u8   num_channels;      /* max active channels, determines max power for each channel. */
	u8   unit;              /* 0 = dBµV, 1 = dBm, */
	u8   rsvd;
	s16  channel_power;
};

/********************************************************/

struct mci_command {
	union {
		u32 command_word;
		struct {
			u8 command;
			u8 tuner;
			u8 demod;
			u8 output;
		};
		struct {
			u8 mod_command;
			u8 mod_channel;
			u8 mod_stream;
			u8 mod_rsvd1;
		};
	};
	union {
		u32 params[31];
		u8  params8[31*4];
		struct {
			u8  flags; /* Bit 0: DVB-S Enabled, 1: DVB-S2 Enabled,
				      5: ChannelBonding, 6: FrequencyRange, 7: InputStreamID */
			u8  s2_modulation_mask; /* Bit 0 : QPSK, 1: 8PSK/8APSK,
						   2 : 16APSK, 3: 32APSK, 4: 64APSK,
						   5: 128APSK, 6: 256APSK */
			u8  rsvd1;
			u8  retry;
			u32 frequency;
			u32 symbol_rate;
			u8  input_stream_id;
			u8  rsvd2[3];
			u32 scrambling_sequence_index;
			u32 frequency_range;
			u8  channel_bonding_config; /* Bit 7: IsSlave,  Bit 5..4: MasterDemod,
						       bit 0:  Num channels - 2.
						       (must be set on all channels to same value) */
		} dvbs2_search;

		struct {
			u8   flags;  /* Bit 0:  0 = TSID is Transport Stream ID, 1 = TSID is relative stream number */
			u8   rsvd1[2];
			u8   retry;
			u32  frequency;
			u32  rsvd2;
			u16  rsvd3;
			u16  tsid;
		} isdbs_search;

		struct {
			u8   flags;
			u8   bandwidth;
			u8   rsvd1;
			u8   retry;
			u32  frequency;
		} dvbc_search;

		struct {
			u8   flags;       /* Bit 0: LP Stream */
			u8   bandwidth;
			u8   rsvd1;
			u8   retry;
			u32  frequency;
		} dvbt_search;

		struct {
			u8   flags;       /* Bit 0: T2 Lite Profile, 7: PLP, */
			u8   bandwidth;
			u8   rsvd1;
			u8   retry;
			u32  frequency;
			u32  reserved;
			u8   plp;
			u8   rsvd2[3];
		} dvbt2_search;

		struct {
			u8   flags;
			u8   bandwidth;
			u8   rsvd1;
			u8   retry;
			u32  frequency;
			u32  reserved;
			u8   plp;
			u8   data_slice;
			u8   rsvd2[2];
		} dvbc2_search;

		struct {
			u8   flags;
			u8   bandwidth;
			u8   rsvd1;
			u8   retry;
			u32  frequency;
		} isdbt_search;

		struct {
			u8   flags; /* Bit 0:  0 = TSID is Transport Stream ID, 1 = TSID is relative stream number */
			/* Bit 2..1:  0 = force single, 1 = force multi, 2 = auto detect */
			u8   bandwidth;
			u8   rsvd1;
			u8   retry;
			u32  frequency;
			u32  rsvd2;
			u16  onid;
			u16  tsid;
		} isdbc_search;

		struct {
			u8   flags;
			u8   bandwidth;
			u8   rsvd1;
			u8   retry;
			u32  frequency;
		} j83b_search;

		struct {
			u8   flags; /*  Bit 0 : 1 = short info (1st 4 Bytes) */
		} get_signalinfo;

		struct {
			u8   tap;
			u8   rsvd;
			u16  point;
		} get_iq_symbol;

		struct {
			u8   flags; /*  Bit 0 : 0 = VTM/SDR, 1 = SCAN,
					     Bit 1: 1 = Disable AGC,
					     Bit 2: 1 = Set Gain.   */
			u8   roll_off;
			u8   rsvd1;
			u8   rsvd2;
			u32  frequency;
			u32  symbol_rate; /* Only in VTM/SDR mode, SCAN Mode uses exactly 1550/24 MSymbols/s.*/
			u8   gain;         /* Gain in 0.25 dB Steps */
			/* Frequency, symbolrate and gain can be schanged while running */
		} sx8_start_iq;

		struct {
			u8   flags;
			/*   Bit 0:1 Preamp Mode;  0 = Preamp AGC, 1 == Minimum (~ -17dB) ,
			     2 = Medium, 3 = Maximum gain {~ 15dB}
			     Bit 2: Bypass Input LNA (6 dB less gain) (Note this is after Preamp)
			     Bit 4: Set RF Gain
			     Bit 5: Freeze RF Gain (Turn AGC off at current gain, only when already enabled)
			     Bit 7: Optimize RF Gain and freeze for FFT */
			u8   rf_gain;       /*   0 .. 50 dB */
		} sx8_input_enable;

		struct {
			u8   offset;        /* Offset into list, must be multiple of 64 */
			u8   select;        /* 0 = Slices, 1 = PLPs  (C2 Only) */
			u8   data_slice;    /* DataSlice to get PLPList (C2 Only) */
		} get_ids;

		struct {
			u8   select;        /* 0 = Base, 1 = DataSilce, 2 = PLP,  Bit 7:  Set new ID */
			u8   id;            /* DataSliceID, PLPId */
		} get_l1_info;

		struct {
			u8   select;        /* 0 = Data PLP, 1 = Common PLP, only DVB-T2 and DVB-C2 */
		} get_bb_header;

		struct mod_setup_channels mod_setup_channels[4];
		struct mod_setup_stream mod_setup_stream;
		struct mod_setup_output mod_setup_output;
	};
};

struct mci_result {
	union {
		u32 status_word;
		struct {
			u8  status;
			u8  mode;
			u16 time;
		};
	};

	union {
		u32 result[27];
		u8 result8[27 * 4];
		struct {
			u8  Rsvd0[3];
			u8  Flags;

			u32 frequency;          /* actual frequency in Hz */
			u32 rsvd1;
			s16 channel_power;      /* channel power in dBm x 100 */
			s16 rsvd2;
			s16 signal_to_noise;    /* SNR in dB x 100, Note: negativ values are valid in DVB-S2 */
			u16 signal_loss_counter;/* Counts signal losses and automatic retunes */
			u32 rsvd4;
			u32 ber_numerator;      /* Bit error rate: PreRS in DVB-S, PreBCH in DVB-S2X */
			u32 ber_denominator;
			u32 ber_rsvd1;          /* Place holder for modulation bit error rate */
			u32 ber_rsvd2;
		} common_signal_info;

		struct {
			u8  standard; /* 1 = DVB-S, 2 = DVB-S2X */
			u8  pls_code; /* PLS code for DVB-S2/S2X, puncture rate for DVB-S */
			u8  roll_off; /* 2-0: rolloff, 7: spectrum inversion */
			u8  flags;
			u32 frequency;         /* actual frequency in Hz */
			u32 symbol_rate;       /* actual symbolrate in Hz */
			s16 channel_power;     /* channel power in dBm x 100 */
			s16 band_power;        /*/ band power in dBm x 100 */
			s16 signal_to_noise;   /* SNR in dB x 100, Note: negativ values are valid in DVB-S2 */
			s16 rsvd2;
			u32 packet_errors;     /* Counter for packet errors. (set to 0 on Start command) */
			u32 ber_numerator;     /* Bit error rate: PreRS in DVB-S, PreBCH in DVB-S2X */
			u32 ber_denominator;
		} dvbs2_signal_info;

		struct {
			u8  modcod;
			u8  rsvd0[2];
			u8  flags;             /* Bit 0: TMCC changed, Bit 1: EWS */
			u32 frequency;         /* actual frequency in Hz */
			u32 symbol_rate;       /* actual symbolrate in Hz */
			s16 channel_power;     /* channel power in dBm x 100 */
			s16 band_power;        /*/ band power in dBm x 100 */
			s16 signal_to_noise;   /* SNR in dB x 100, Note: negativ values are valid in DVB-S2 */
			s16 rsvd2;
			u32 packet_errors;     /* Counter for packet errors. (set to 0 on Start command) */
			u32 ber_numerator;     /* Bit error rate: PreRS in DVB-S, PreBCH in DVB-S2X */
			u32 ber_denominator;
		} isdbs_signal_info;

		struct {
			u8  constellation;
			u8  rsvd0[2];
			u8  flags;
			u32 frequency;         /* actual frequency in Hz */
			u32 symbol_rate;       /* actual symbolrate in Hz */
			s16 channel_power;     /* channel power in dBm x 100 */
			s16 band_power;        /* band power in dBm x 100 */
			s16 signal_to_noise;   /* SNR in dB x 100, Note: negativ values are valid in DVB-S2 */
			s16 rsvd2;
			u32 packet_errors;     /* Counter for packet errors. (set to 0 on Start command) */
			u32 ber_numerator;     /* Bit error rate: PreRS */
			u32 ber_denominator;
		} dvbc_signal_info;

		struct {
			u8  modulation1;        // bit 7..6: Constellation, bit 5..3 Hierachy, bit 2..0 CodeRate High
			u8  modulation2;        // bit 7..5: CodeRate Low, bit 4..3 Guard Interval, bit 2..1 FFT Mode
			u8  rsvd0;
			u8  flags;
			u32 frequency;         /* actual frequency in Hz */
			u32 rsvd1;
			s16 channel_power;     /* channel power in dBm x 100 */
			s16 band_power;        /* band power in dBm x 100 */
			s16 signal_to_noise;   /* SNR in dB x 100, Note: negativ values are valid in DVB-S2 */
			s16 rsvd2;
			u32 packet_errors;     /* Counter for packet errors. (set to 0 on Start command) */
			u32 ber_numerator;     /* Bit error rate: PreRS */
			u32 ber_denominator;
		} dvbt_signal_info;

		struct {
			u8  rsvd0[3];
			u8  flags;
			u32 frequency;         /* actual frequency in Hz */
			u32 rsvd1;
			s16 channel_power;      /* channel power in dBm x 100 */
			s16 band_power;         /* band power in dBm x 100 */
			s16 signal_to_noise;     /* SNR in dB x 100, Note: negativ values are valid in DVB-S2 */
			s16 rsvd2;
			u32 packet_errors;      /* Counter for packet errors. (set to 0 on Start command) */
			u32 ber_numerator;      /* Bit error rate: PreRS */
			u32 ber_denominator;
		} dvbt2_signal_info;

		struct {
			u8  rsvd0[3];
			u8  flags;

			u32 frequency;         // actual frequency in Hz
			u32 rsvd1;             //
			s16 channel_power;      // channel power in dBm x 100
			s16 band_power;         // band power in dBm x 100
			s16 signal_to_noise; // SNR in dB x 100, Note: negativ values are valid in DVB-S2
			s16 rsvd2;
			u32 packet_errors;      // Counter for packet errors. (set to 0 on Start command)
			u32 ber_numerator;      // Bit error rate: PreBCH
			u32 ber_denominator;
		} dvbc2_signal_info;

		struct {
			u8  rsvd0[3];
			u8  flags;

			u32 frequency;         // actual frequency in Hz
			u32 rsvd1;             //
			s16 channel_power;      // channel power in dBm x 100
			s16 band_power;         // band power in dBm x 100
			s16 signal_to_noise;     // SNR in dB x 100, Note: negativ values are valid in DVB-S2
			s16 rsvd2;
			u32 packet_errors;      // Counter for packet errors. (set to 0 on Start command)
			u32 ber_numerator;     // Bit error rate: PreRS Segment A
			u32 ber_denominator;
			u32 ber_rsvd1;          // Place holder for modulation bit error rate
			u32 ber_rsvd2;
			u32 ber_numeratorB;     // Bit error rate: PreRS Segment B
			u32 ber_numeratorC;     // Bit error rate: PreRS Segment C
		} isdbt_signal_info;

		struct {
			u8  constellation;
			u8  rsvd0[2];
			u8  flags;

			u32 frequency;         // actual frequency in Hz
			u32 symbol_rate;       // actual symbolrate in Hz
			s16 channel_power;     // channel power in dBm x 100
			s16 band_power;        // band power in dBm x 100
			s16 signal_to_noise;   // SNR in dB x 100, Note: negativ values are valid in DVB-S2
			s16 rsvd2;
			u32 packet_errors;     // Counter for packet errors. (set to 0 on Start command)
			u32 ber_numerator;     // Bit error rate: PreRS in DVB-S, PreBCH in DVB-S2X
			u32 ber_denominator;
		} isdbc_signal_info;

		struct {
			u8  constellation;
			u8  interleaving;
			u8  rsvd0;
			u8  flags;

			u32 frequency;         // actual frequency in Hz
			u32 symbol_rate;        // actual symbolrate in Hz
			s16 channel_power;      // channel power in dBm x 100
			s16 band_power;         // band power in dBm x 100
			s16 signal_to_noise;   // SNR in dB x 100, Note: negativ values are valid in DVB-S2
			s16 rsvd2;
			u32 packet_errors;      // Counter for packet errors. (set to 0 on Start command)
			u32 ber_numerator;      // Bit error rate: PreRS in DVB-S, PreBCH in DVB-S2X
			u32 ber_denominator;
		} j83b_signal_info;

		struct {
			s16 i;
			s16 q;
		} iq_symbol;

		struct {
			u8   tps_info[7];
			// u16 tps_cell_id;
		} dvbt_tps_info;

		struct {
			struct  {
				u8 type;
				u8 BWExtension;
				u8 S1;
				u8 S2;
				u8 L1RepetitionFlag;
				u8 GuardInterval;
				u8 PAPR;
				u8 L1Mod;
				u8 L1Cod;
				u8 L1FECType;
				u8 L1PostSize[3];
				u8 L1PostInfoSize[3];
				u8 PilotPattern;
				u8 TXIDAvailabilty;
				u8 CellID[2];
				u8 NetworkID[2];
				u8 T2SystemID[2];
				u8 NumT2Frames;
				u8 NumDataSymbols[2];
				u8 RegenFlag;
				u8 L1PostExtension;
				u8 NumRF;
				u8 CurrentRFIndex;
				u8 T2Version_PostScrambled_BaseLite_Rsvd[2]; // 4,1,1,4 bit
				u8 CRC32[4];
			} dvbt2_l1_pre;

			struct  {
				u8 SubSlicesPerFrame[2];
				u8 NumPLP;
				u8 NumAux;
				u8 AuxConfigRFU;
				u8 RFIndex;
				u8 Frequency[4];
				u8 FEFType;
				u8 FEFLength[3];
				u8 FEFInterval;
			} dvbt2_l1_post;
		} dvbt2_l1_info;

		struct {
			u8 PLPID;
			u8 Type;
			u8 PayloadType;
			u8 FFFlag;
			u8 FirstRFIndex;
			u8 FirstFrameIndex;
			u8 GroupID;
			u8 Cod;
			u8 Mod;
			u8 Rotation;
			u8 FECType;
			u8 NumBlocksMax[2];
			u8 FrameInterval;
			u8 TimeILLength;
			u8 TimeILType;
			u8 InBandAFlag;
			u8 InBandBFlag_Rsvd1_Mode_StaticFlag_StaticPaddingFlag[2];  // 1,11,2,1,1
		} dvbt2_plp_info;

		struct {
			u8  NetworkID[2];
			u8  C2SystemID[2];
			u8  StartFrequency[3];
			u8  C2BandWidth[2];
			u8  GuardInterval;
			u8  C2FrameLength[2];
			u8  L1P2ChangeCounter;
			u8  NumDataSlices;
			u8  NumNotches;
			struct {
				u8 Start[2];
				u8 Width[2];
				u8 Reserved3;
			} NotchData[15];
			u8  ReservedTone;
			u8  Reserved4[2];        // EWS 1 bit, C2_Version 4 bit, Rsvd 11 bit
		} DVBC2_L1Part2;

		struct {
			u8  NumIDs;
			u8  Offset;
			u8  IDs[64];
		} DVBC2_IDList;

		struct {
			u8  SliceID;
			u8  TunePosition[2];
			u8  OffsetLeft[2];
			u8  OffsetRight[2];
			u8  TIDepth;
			u8  Type;
			u8  FECHeaderType;
			u8  ConstConf;
			u8  LeftNotch;
			u8  NumPLP;
			u8  Reserved2;
		} DVBC2_SliceInfo;

		struct {
			u8  PLPID;
			u8  Bundled;
			u8  Type;
			u8  PayloadType;
			u8  GroupID;
			u8  Start[2];
			u8  FECType;
			u8  Mod;
			u8  Cod;
			u8  PSISIReprocessing;
			u8  TransportstreamID[2];
			u8  OrginalNetworkID[2];
			u8  Reserved1;
		} DVBC2_PLPInfo;

		struct {
			u8  valid;
			u8  matype_1;
			u8  matype_2;
			u8  upl[2];
			u8  dfl[2];
			u8  sync;
			u8  syncd[2];
			u8  rsvd;
			u8  issy[3];
			u8  min_input_stream_id;
			u8  max_input_stream_id;
		} bb_header;

		struct {
			u8  Mode;          // FFT Mode   1,2,3
			u8  GuardInterval; // 1/32, 1/16, 1/8, /14
			u8  TMCCInfo[13];  // TMCC B20 - B121,  byte 0 bit 7: B20,  byte 12 bit 2: B121
		} ISDBT_TMCCInfo;

		struct {
			u8   Change;  // 5 bits, increments with every change
			struct {
				u8 ModCod;    // 4 bits
				u8 NumSlots;  // 6 bits
			} Mode[4];
			u8   RelTSID[24];  // bit 6..4 Relative TSID for slot i*2 + 1, bit 2..0 Relative TSID for slot i*2 + 2
			struct {
				u8 highByte;
				u8 lowByte;
			} TSID[8];
			u8   Flags;  // Bit 5: EWS flag, bit 4: Site Diversity flag, bit 3..1: Site Diversity information, bit 0: Extension flag
			u8   Extension[8];   // 61 bits, right aligned
		} ISDBS_TMCCInfo;

		struct {
			u8  cut;
			u8  avs_code;
			u8  temperature;
			u8  rsvd[13];
		} sx8_bist;

		struct {
			u8   status;
			u8   offset;
			u8   length;
			u8   rsvd2;
			u32  rsvd3[2];
			u8   data[96];
		} SX8_packet_filter_status;

		struct {
			u8   version;  /* 0 = none, 1 = SX8 */
			u8   flags;    /* Bit 0: 1 = Tuner Valid, Bit 1: 1 = Output Valid */
			u8   tuner;
			u8   output;
		} extended_status;

		struct {
		        u8   reserved;
		        u8   serial_number[17];
		} serial_number;

		struct {
		        u8   flags;
		        u8   serial_number[17];
		        u16  code;
		        u8   ID[8];
		        u8   LK[24];
		} license;

	};
	u32 version[3];
	u8  version_rsvd;
	u8  version_major;
	u8  version_minor;
	u8  version_sub;
};



/* Helper Macros */

/* DVB-T2 L1-Pre Signalling Data   ( ETSI EN 302 755 V1.4.1 Chapter 7.2.2 ) */

#define L1PRE_TYPE(p)                     ((p)[0] & 0xFF)
#define L1PRE_BWT_EXT(p)                  ((p)[1] & 0x01)
#define L1PRE_S1(p)                       ((p)[2] & 0x07)
#define L1PRE_S2(p)                       ((p)[3] & 0x0F)
#define L1PRE_L1_REPETITION_FLAG(p)       ((p)[4] & 0x01)
#define L1PRE_GUARD_INTERVAL(p)           ((p)[5] & 0x07)
#define L1PRE_PAPR(p)                     ((p)[6] & 0x0F)
#define L1PRE_L1_MOD(p)                   ((p)[7] & 0x0F)
#define L1PRE_L1_COD(p)                   ((p)[8] & 0x03)
#define L1PRE_L1_FEC_TYPE(p)              ((p)[9] & 0x03)
#define L1PRE_L1_POST_SIZE(p)             (((u32)((p)[10] & 0x03) << 16) | ((u32)(p)[11] << 8) | (p)[12])
#define L1PRE_L1_POST_INFO_SIZE(p)        (((u32)((p)[13] & 0x03) << 16) | ((u32)(p)[14] << 8) | (p)[15])
#define L1PRE_PILOT_PATTERN(p)            ((p)[16] & 0x0F)
#define L1PRE_TX_ID_AVAILABILITY(p)       ((p)[17] & 0xFF)
#define L1PRE_CELL_ID(p)                  (((u16)(p)[18] << 8) |  (p)[19])
#define L1PRE_NETWORK_ID(p)               (((u16)(p)[20] << 8) |  (p)[21])
#define L1PRE_T2_SYSTEM_ID(p)             (((u16)(p)[22] << 8) |  (p)[23])
#define L1PRE_NUM_T2_FRAMES(p)            ((p)[24] & 0xFF)
#define L1PRE_NUM_DATA_SYMBOLS(p)         (((u16)((p)[25] & 0x0F) << 8) |  (p)[26])
#define L1PRE_REGEN_FLAG(p)               ((p)[27] & 0x07)
#define L1PRE_L1_POST_EXTENSION(p)        ((p)[28] & 0x01)
#define L1PRE_NUM_RF(p)                   ((p)[29] & 0x07)
#define L1PRE_CURRENT_RF_IDX(p)           ((p)[30] & 0x07)
#define L1PRE_T2_VERSION(p)               ((((p)[31] & 0x03) << 2) | (((p)[32] & 0xC0) >> 6))
#define L1PRE_L1_POST_SCRAMBLED(p)        (((p)[32] & 0x20) >> 5)
#define L1PRE_T2_BASE_LITE(p)             (((p)[32] & 0x10) >> 4)


/* DVB-T2 L1-Post Signalling Data   ( ETSI EN 302 755 V1.4.1 Chapter 7.2.3 ) */

#define L1POST_SUB_SLICES_PER_FRAME(p)     (((u16)(p)[0] & 0x7F) | (p)[1])
#define L1POST_NUM_PLP(p)                  ((p)[2] & 0xFF)
#define L1POST_NUM_AUX(p)                  ((p)[3] & 0x0F)
#define L1POST_AUX_CONFIG_RFU(p)           ((p)[4] & 0xFF)
#define L1POST_RF_IDX(p)                   ((p)[5] & 0x07)
#define L1POST_FREQUENCY(p)                (((u32)(p)[6] << 24) | ((u32)(p)[7] << 16) | ((u32)(p)[8] << 8) | (p)[9])
#define L1POST_FEF_TYPE(p)                 ((p)[10] & 0x0F)
#define L1POST_FEF_LENGTH(p)               (((u32)(p)[11] << 16) | ((u32)(p)[12] << 8) | (p)[13])
#define L1POST_FEF_INTERVAL(p)             ((p)[14] & 0xFF)

/* Repeated for each PLP, */
/* Hardware is restricted to retrieve only values for current data PLP and common PLP */

#define L1POST_PLP_ID(p)                   ((p)[0] & 0xFF)
#define L1POST_PLP_TYPE(p)                 ((p)[1] & 0x07)
#define L1POST_PLP_PAYLOAD_TYPE(p)         ((p)[2] & 0x1F)
#define L1POST_FF_FLAG(p)                  ((p)[3] & 0x01)
#define L1POST_FIRST_RF_IDX(p)             ((p)[4] & 0x07)
#define L1POST_FIRST_FRAME_IDX(p)          ((p)[5] & 0xFF)
#define L1POST_PLP_GROUP_ID(p)             ((p)[6] & 0xFF)
#define L1POST_PLP_COD(p)                  ((p)[7] & 0x07)
#define L1POST_PLP_MOD(p)                  ((p)[8] & 0x07)
#define L1POST_PLP_ROTATION(p)             ((p)[9] & 0x01)
#define L1POST_PLP_FEC_TYPE(p)             ((p)[10] & 0x03)
#define L1POST_PLP_NUM_BLOCKS_MAX(p)       (((u16)((p)[11] & 0x03) << 8) | (p)[12])
#define L1POST_FRAME_INTERVAL(p)           ((p)[13] & 0xFF)
#define L1POST_TIME_IL_LENGTH(p)           ((p)[14] & 0xFF)
#define L1POST_TIME_IL_TYPE(p)             ((p)[15] & 0x01)
#define L1POST_IN_BAND_A_FLAG(p)           ((p)[16] & 0x01)
#define L1POST_IN_BAND_B_FLAG(p)           (((p)[17] >> 7) & 0x01)
#define L1POST_RESERVED_1(p)               (((u16)((p)[17] & 0x7F) << 4) | ((p)[18] & 0xF0) >> 4)
#define L1POST_PLP_MODE(p)                 (((p)[18] >> 2) & 0x03)
#define L1POST_STATIC_FLAG(p)              (((p)[18] >> 1) & 0x01)
#define L1POST_STATIC_PADDING_FLAG(p)      (((p)[18] >> 1) & 0x01)

#ifdef __KERNEL__

struct mci_base {
	struct list_head     mci_list;
	void                *key;
	struct ddb_link     *link;
//	struct completion    completion;
	struct mutex         tuner_lock;
//	struct mutex         mci_lock;
	int                  count;
	int                  type;
};

struct mci {
	struct ddb_io       *input;
	struct mci_base     *base;
	struct dvb_frontend  fe;
	int                  nr;
	int                  demod;
	int                  tuner;

	struct mci_result    signal_info;
};

struct mci_cfg {
	int type;
	struct dvb_frontend_ops *fe_ops;
	u32 base_size;
	u32 state_size;
	int (*init)(struct mci *mci);
	int (*base_init)(struct mci_base *mci_base);
};

int ddb_mci_cmd(struct mci *state, struct mci_command *command, struct mci_result *result);
int ddb_mci_cmd_link(struct ddb_link *link, struct mci_command *command, struct mci_result *result);
int ddb_mci_get_status(struct mci *mci, struct mci_result *res);
int ddb_mci_get_snr(struct dvb_frontend *fe);
int ddb_mci_get_info(struct mci *mci);
int ddb_mci_get_strength(struct dvb_frontend *fe);
void ddb_mci_proc_info(struct mci *mci, struct dtv_frontend_properties *p);
int mci_init(struct ddb_link *link);
int mci_cmd_val(struct ddb_link *link, u32 cmd, u32 val);

#endif

#endif
