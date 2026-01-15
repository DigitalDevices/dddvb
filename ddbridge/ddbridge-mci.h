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

#define MCI_STATUS_OK                 (0x00)
#define MCI_STATUS_UNSUPPORTED        (0x80)
#define MCI_STATUS_POWERED_DOWN       (0xF0)
#define MCI_STATUS_DISABLED           (0xF9)
#define MCI_STATUS_BUSY               (0xFA)
#define MCI_STATUS_HARDWARE_ERROR     (0xFB)
#define MCI_STATUS_INVALID_PARAMETER  (0xFC)
#define MCI_STATUS_RETRY              (0xFD)
#define MCI_STATUS_NOT_READY          (0xFE)
#define MCI_STATUS_ERROR              (0xFF)

// Receiver defines

#define MCI_CMD_STOP             (0x01)
#define MCI_CMD_GETSTATUS        (0x02)
#define MCI_CMD_GETSIGNALINFO    (0x03)
//#define MCI_CMD_RFPOWER          (0x04)

#define MCI_CMD_SET_INPUT_CONFIG  (0x05)

#define MCI_CMD_GET_CAPABILITIES  (0x0E)
#define MCI_CMD_GETBIST           (0x0F)

#define MCI_CMD_SEARCH_DVBS     (0x10)
#define MCI_CMD_SEARCH_ISDBS    (0x11)
#define MCI_CMD_SEARCH_ISDBS3   (0x12)

#define MCI_CMD_SEARCH_DVBC     (0x20)
#define MCI_CMD_SEARCH_DVBT     (0x21)
#define MCI_CMD_SEARCH_DVBT2    (0x22)
#define MCI_CMD_SEARCH_DVBC2    (0x23)

#define MCI_CMD_SEARCH_ISDBT    (0x24)
#define MCI_CMD_SEARCH_ISDBC    (0x25)
#define MCI_CMD_SEARCH_J83B     (0x26)

#define MCI_CMD_SEARCH_ATSC     (0x27)
#define MCI_CMD_SEARCH_ATSC3    (0x28)

#define MCI_CMD_GET_IQSYMBOL     (0x30)

#define MX_CMD_GET_L1INFO         (0x50)
#define MX_CMD_GET_IDS            (0x51)
#define MX_CMD_GET_DVBT_TPS       (0x52)
#define MCI_CMD_GET_BBHEADER      (0x53)
#define MX_CMD_GET_ISDBT_TMCC     (0x54)
#define MX_CMD_GET_ISDBS_TMCC     (0x55)
#define MX_CMD_GET_ISDBC_TSMF     (0x56)

#define MX_CMD_GET_BBHEADER       (MCI_CMD_GET_BBHEADER)

#define MCI_CMD_GET_SERIALNUMBER  (0xF0)
#define MCI_CMD_EXPORT_LICENSE    (0xF0)
#define MCI_CMD_IMPORT_LICENSE    (0xF1)

#define MCI_CMD_POWER_DOWN        (0xF2)
#define MCI_CMD_POWER_UP          (0xF3)

#define MX_T2_FLAGS_SETPLP_ID     (0x80)
#define MX_T2_FLAGS_SETPLP_INDEX  (0x10)

#define MX_L1INFO_SEL_PRE         (0)
#define MX_L1INFO_SEL_DSINFO      (1)
#define MX_L1INFO_SEL_PLPINFO     (2)
#define MX_L1INFO_SEL_PLPINFO_C   (3)
#define MX_L1INFO_SEL_SETID       (MX_T2_FLAGS_SETPLP_ID)
#define MX_L1INFO_MAGIC           (0x3254)

#define MCI_BANDWIDTH_UNKNOWN    (0)
#define MCI_BANDWIDTH_1_7MHZ     (1)
#define MCI_BANDWIDTH_5MHZ       (5)
#define MCI_BANDWIDTH_6MHZ       (6)
#define MCI_BANDWIDTH_7MHZ       (7)
#define MCI_BANDWIDTH_8MHZ       (8)
#define MCI_BANDWIDTH_10MHZ      (10)

#define MCI_BANDWIDTH_EXTENSION  (0x80)   // currently used only for J83B in Japan

#define MX_MODE_DVBSX            (2)
#define MX_MODE_DVBC             (3)
#define MX_MODE_DVBT             (4)
#define MX_MODE_DVBT2            (5)
#define MX_MODE_DVBC2            (6)
#define MX_MODE_J83B             (7)
#define MX_MODE_ISDBT            (8)
#define MX_MODE_ISDBC            (9)
#define MX_MODE_ISDBS            (10)
#define MX_MODE_ISDBS3           (11)
#define MX_MODE_ATSC             (12)
#define MX_MODE_ATSC3            (13)

#define MX_SUPPORT_DVBS       ( 1 << 0 )
#define MX_SUPPORT_DVBS2      ( 1 << 1 )
#define MX_SUPPORT_DVBS2X     ( 1 << 2 )

#define MX_SUPPORT_DVBC       ( 1 << 3 )
#define MX_SUPPORT_DVBT       ( 1 << 4 )
#define MX_SUPPORT_DVBT2      ( 1 << 5 )
#define MX_SUPPORT_DVBC2      ( 1 << 6 )

#define MX_SUPPORT_J83B       ( 1 << 7 )
#define MX_SUPPORT_ISDBT      ( 1 << 8 )
#define MX_SUPPORT_ISDBC      ( 1 << 9 )

#define MX_SUPPORT_ISDBS      ( 1 << 10 )
#define MX_SUPPORT_ISDBS3     ( 1 << 11 )
#define MX_SUPPORT_ATSC       ( 1 << 12 )
#define MX_SUPPORT_ATSC3      ( 1 << 13 )

#define MX_SUPPORT_J83A       ( MX_SUPPORT_DVBC  )
#define MX_SUPPORT_J83C       ( MX_SUPPORT_ISDBC )

#define MX_DVBC_CONSTELLATION_16QAM  (0)
#define MX_DVBC_CONSTELLATION_32QAM  (1)
#define MX_DVBC_CONSTELLATION_64QAM  (2)  // also valid for J83B and ISDB-C
#define MX_DVBC_CONSTELLATION_128QAM (3)
#define MX_DVBC_CONSTELLATION_256QAM (4)  // also valid for J83B and ISDB-C

#define MX_SIGNALINFO_FLAG_CHANGE (0x01)
#define MX_SIGNALINFO_FLAG_EWS    (0x02)

#define MX_DISABLE_INPUTS         (0)
#define MX_ENABLE_TER_INPUT       (1)
#define MX_ENABLE_SAT_INPUT       (2)

#define MX_CAP_FLAGS_SHARED_INPUT (0x0001)

#define SX8_CMD_INPUT_ENABLE      (0x40)
#define SX8_CMD_INPUT_DISABLE     (0x41)
#define SX8_CMD_START_IQ          (0x42)
#define SX8_CMD_STOP_IQ           (0x43)
#define SX8_CMD_ENABLE_IQOUTPUT   (0x44)
#define SX8_CMD_DISABLE_IQOUTPUT  (0x45)
#define SX8_CMD_PACKETFILTER      (0x46)
#define SX8_CMD_RFAGC_CONTROL     (0x47)

#define SX8_ROLLOFF_35  0
#define SX8_ROLLOFF_25  1
#define SX8_ROLLOFF_20  2
#define SX8_ROLLOFF_15  5
#define SX8_ROLLOFF_10  3
#define SX8_ROLLOFF_05  4

#define MCI_DEMOD_STOPPED        (0)
#define MCI_DEMOD_WAIT_SIGNAL    (2)
#define MCI_DEMOD_TIMEOUT        (14)
#define MCI_DEMOD_LOCKED         (15)

#define SX8_DEMOD_IQ_MODE        (1)
#define SX8_DEMOD_WAIT_MATYPE    (3)

#define MX_DEMOD_WAIT_TS         (6)
#define MX_DEMOD_C2SCAN          (16)

// Modulator defines

#define MOD_SETUP_CHANNELS        (0x60)
#define MOD_SETUP_OUTPUT          (0x61)
#define MOD_SETUP_STREAM          (0x62)
//#define MOD_SET_STREAM_CHANNEL    (0x63)
#define MOD_CLOCK_CORRECTION      (0x64)

#define MOD_SETUP_FLAG_FIRST      (0x01)
#define MOD_SETUP_FLAG_LAST       (0x02)
#define MOD_SETUP_FLAG_VALID      (0x80)

#define MOD_SETUP_STATUS_MORE     (0x01)

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

#define MOD_STANDARD_ATV_8        (0x40)
#define MOD_STANDARD_ATV_7        (0x41)
#define MOD_STANDARD_ATV_6        (0x42)

#define MOD_STANDARD_PAL_B        (0x44)
#define MOD_STANDARD_PAL_G        (0x45)
#define MOD_STANDARD_PAL_I        (0x46)
#define MOD_STANDARD_PAL_DK       (0x47)
#define MOD_STANDARD_PAL_DK1      (0x48)
#define MOD_STANDARD_PAL_DK2      (0x49)
#define MOD_STANDARD_PAL_DK3      (0x4A)

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

// Sub Commands for MOD_CLOCK_CORRECTION
// Only available on SDR Modulator (ATV,IQ,IQ2)
#define MOD_CLOCK_COR_RESET         (0)
#define MOD_CLOCK_COR_SET           (1)
#define MOD_CLOCK_COR_LEGACY_SET    (120)
#define MOD_CLOCK_COR_LEGACY_QUERY  (121)

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
	u8   rolloff;           /* Legal values:  12,13,15,18 (only used with MOD_STANDARD_GENERIC) */
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

struct dvbs2_search {
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
};

struct isdbs_search {
	u8   flags;  /* Bit 0:  0 = TSID is Transport Stream ID, 1 = TSID is relative stream number */
	u8   rsvd1[2];
	u8   retry;
	u32  frequency;
	u32  rsvd2;
	u16  rsvd3;
	u16  tsid;
};

struct dvbc_search {
	u8   flags;
	u8   bandwidth;
	u8   rsvd1;
	u8   retry;
	u32  frequency;
};

struct dvbt_search {
	u8   flags;       /* Bit 0: LP Stream */
	u8   bandwidth;
	u8   rsvd1;
	u8   retry;
	u32  frequency;
};

struct dvbt2_search {
	u8   flags;       /* Bit 0: T2 Lite Profile, 7: PLP, */
	u8   bandwidth;
	u8   rsvd1;
	u8   retry;
	u32  frequency;
	u32  reserved;
	u8   plp;
	u8   rsvd2[3];
};

struct dvbc2_search {
	u8   flags;
	u8   bandwidth;
	u8   rsvd1;
	u8   retry;
	u32  frequency;
	u32  reserved;
	u8   plp;
	u8   data_slice;
	u8   rsvd2[2];
};

struct isdbt_search {
	u8   flags;
	u8   bandwidth;
	u8   rsvd1;
	u8   retry;
	u32  frequency;
};

struct isdbc_search {
	u8   flags; /* Bit 0:  0 = TSID is Transport Stream ID, 1 = TSID is relative stream number */
	/* Bit 2..1:  0 = force single, 1 = force multi, 2 = auto detect */
	u8   bandwidth;
	u8   rsvd1;
	u8   retry;
	u32  frequency;
	u32  rsvd2;
	u16  onid;
	u16  tsid;
};

struct j83b_search {
	u8   flags;
	u8   bandwidth;
	u8   rsvd1;
	u8   retry;
	u32  frequency;
};

struct atsc_search {
	u8   flags;
	u8   rsvd0;
	u8   rsvd1;
	u8   retry;
	u32  frequency;
};

struct atsc3_search {
	u8   flags;
	u8   bandwidth;
	u8   rsvd1;
	u8   retry;
	u32  frequency;
	u32  reserved;
	u8   plp[4];
};

struct get_signalinfo {
	u8   flags; /*  Bit 0 : 1 = short info (1st 4 Bytes) */
};

struct get_iq_symbol {
	u8   tap;
	u8   rsvd;
	u16  point;
};

struct get_ids {
	u8   offset;        /* Offset into list, must be multiple of 64 */
	u8   select;        /* 0 = Slices, 1 = PLPs  (C2 Only) */
	u8   data_slice;    /* DataSlice to get PLPList (C2 Only) */
};

struct get_l1_info {
	u8   select;        /* 0 = Base, 1 = DataSilce, 2 = PLP,  Bit 7:  Set new ID */
	u8   id;            /* DataSliceID, PLPId */
};

struct get_bb_header {
	u8   select;        /* 0 = Data PLP, 1 = Common PLP, only DVB-T2 and DVB-C2 */
};

struct sx8_start_iq {
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
};

struct sx8_input_enable {
	u8   flags;
	/*   Bit 0:1 Preamp Mode;  0 = Preamp AGC, 1 == Minimum (~ -17dB) ,
	     2 = Medium, 3 = Maximum gain {~ 15dB}
	     Bit 2: Bypass Input LNA (6 dB less gain) (Note this is after Preamp)
	     Bit 4: Set RF Gain
	     Bit 5: Freeze RF Gain (Turn AGC off at current gain, only when already enabled)
	     Bit 7: Optimize RF Gain and freeze for FFT */
	u8   rf_gain;       /*   0 .. 50 dB */
};

struct sx8_packet_filter {
	u8   Cmd;
	u8   Offset;
	u8   Length;
	u8   Rsvd1;
	u32  Rsvd2[2];
	u8   Data[96];
};

struct sx8_rfagc_control {
	u8   cmd;
	u8   rsvd;
	u16  param;
};

struct set_license {
	u8   ID[8];
	u8   LK[24];
};

struct mx_capabilities {
	u8   type;                 // 1 = MX
	u8   version;              // Current 1
	u8   mum_demods;           // 
	u8   num_sat_inputs;       // either 0,1 or NumDemods
	u16  standards;
	u16  flags;
	u16  sat_min_frequency;      // Note:
	u16  sat_max_frequency;      //  Frequencies in MHz
	u16  ter_cable_min_frequency; //  Minimum tuning frequency is xxMinFrequeny + Bandwidth/2
	u16  ter_cable_max_frequency; //  Maximum tuning frequency is xxMinFrequeny - Bandwidth/2
};

struct common_signal_info {
	u8  Rsvd0[3];
	u8  Flags;
	
	u32 frequency;          /* actual frequency in Hz */
	u32 symbol_rate;        /* actual symbolrate in Hz */
	s16 channel_power;      /* channel power in dBm x 100 */
	s16 rsvd2;
	s16 signal_to_noise;    /* SNR in dB x 100, Note: negativ values are valid in DVB-S2 */
	u16 signal_loss_counter;/* Counts signal losses and automatic retunes */
	u32 packet_errors;      /* Counter for packet errors. (set to 0 on Start command) */
	u32 ber_numerator;      /* Bit error rate: PreRS in DVB-S, PreBCH in DVB-S2X */
	u32 ber_denominator;
	u32 ber_rsvd1;
	u32 ber_rsvd2;
	u32 ber_rsvd3;
	u32 ber_rsvd4;
	u32 packet_counter;
	u32 ber_rsvd5;          /* reserved to extend PacketCounter to 64 bit */
	u32 packet_loss_counter;
	u32 packet_error_counter;
};

struct dvbs2_signal_info {
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
};

struct isdbs_signal_info {
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
};

struct dvbc_signal_info {
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
};

struct dvbt_signal_info {
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
};

struct dvbt2_signal_info {
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
};

struct dvbc2_signal_info {
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
};

struct isdbt_signal_info {
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
};

struct j83b_signal_info {
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
};

struct isdbc_signal_info {
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
};

struct atsc_signal_info {
	u8  rsvd0[3];
	u8  flags;
	
	u32 frequency;         // actual frequency in Hz
	u32 rsvd1;             //
	s16 channel_power;      // channel power in dBm x 100
	s16 band_power;         // band power in dBm x 100
	s16 signal_to_noise;     // SNR in dB x 100, Note: negativ values are valid in DVB-S2
	u16 signal_loss_counter; // Counts signal losses and automatic retunes
	u32 packet_errors;     // Counter for packet errors. (set to 0 on Start command)
	u32 ber_numerator;     // Bit error rate: PreRS
	u32 ber_denominator;
};

struct atsc3_signal_info {
	u8  rsvd0[3];
	u8  flags;
	
	u32 frequency;         // actual frequency in Hz
	u32 rsvd1;             //
	s16 channel_power;      // channel power in dBm x 100
	s16 band_power;         // band power in dBm x 100
	s16 signal_to_noise;     // SNR in dB x 100, Note: negativ values are valid in DVB-S2
	u16 signal_loss_counter; // Counts signal losses and automatic retunes
	u32 packet_errors;     // Counter for packet errors. (set to 0 on Start command)
	u32 ber_numerator;     // Bit error rate: PreRS
	u32 ber_denominator;
};

struct iq_symbol {
	s16 i;
	s16 q;
};

struct dvbt_tps_info {
	u8   tps_info[7];
	// u16 tps_cell_id;
};

struct dvbt2_l1_pre {
	u8   Type;
	u8   BWExtension;
	u8   S1;
	u8   S2;
	
	u8   L1RepetitionFlag;
	u8   GuardInterval;
	u8   PAPR;
	u8   L1Mod;
	
	u8   L1Cod;
	u8   L1FECType;
	
	u8   Align1[2];
	
	u32  L1PostSize;
	u32  L1PostInfoSize;
	
	u8   PilotPattern;
	u8   TXIDAvailabilty;
	u16  CellID;
	
	u16  NetworkID;
	u16  T2SystemID;
	
	u8   NumT2Frames;
	u8   Align2[1];
	
	u16  NumDataSymbols;
	
	u8   RegenFlag;
	u8   L1PostExtension;
	u8   NumRF;
	u8   CurrentRFIndex;
	u8   T2Version;
	u8   PostScrambled;
	u8   BaseLite;
	u8   Rsvd;
	
	// u8   T2Version_PostScrambled_BaseLite_Rsvd[2]; // 4,1,1,4 bit
};

struct dvbt2_l1_post {
	u16  SubSlicesPerFrame;
	u8   NumPLP;
	u8   NumAux;
	
	u8   AuxConfigRFU;
	u8   RFIndex;
	u8   Align3[2];
	
	u32  Frequency;
	u8   FEFType;
	u8   Align4[3];
	u32  FEFLength;
	u8   FEFInterval;
};

struct dvbt2_l1_post_plp {
	u8   PLPID;
	u8   Type;
	u8   PayloadType;
	u8   FFFlag;
	
	u8   FirstRFIndex;
	u8   FirstFrameIndex;
	u8   GroupID;
	u8   Cod;
	
	u8   Mod;
	u8   Rotation;
	u8   FECType;  
	u8   Align1[1];
	
	u16  NumBlocksMax;
	u8   FrameInterval;
	u8   TimeILLength;
	
	u8   TimeILType;
	u8   InBandAFlag;
	u8   InBandBFlag;
	u8   Mode;
	u8   StaticFlag;
	u8   StaticPaddingFlag;
};

struct dvbt2_l1_info {
	struct  {
		u16  magic;   // Set to MX_L1INFO_MAGIC (0x3254)
		u8   version; // Set to 0x01
		u8   reserved;
	} header;
	struct dvbt2_l1_pre dvbt2_l1_pre;
	struct dvbt2_l1_post dvbt2_l1_post;
};

struct dvbt2_plp_info {
	struct  {
		u16  magic;   // Set to MX_L1INFO_MAGIC (0x3254)
		u8   version; // Set to 0x01
		u8   reserved;
	} header;
	struct dvbt2_l1_post_plp dvbt2_l1_post_plp;
};

struct dvbt2_l1_info_old {
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
};

struct dvbt2_plp_info_old {
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
};

struct dvbc2_l1_part2 {
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
};

struct dvbc2_id_list {
	u8  NumIDs;
	u8  Offset;
	u8  IDs[64];
};

struct dvbc2_slice_info {
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
};

struct dvbc2_plp_info {
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
};

struct bb_header {
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
};

struct isdbt_tmcc_info {
	u8  Mode;          // FFT Mode   1,2,3
	u8  GuardInterval; // 1/32, 1/16, 1/8, /14
	u8  TMCCInfo[13];  // TMCC B20 - B121,  byte 0 bit 7: B20,  byte 12 bit 2: B121
};

struct isdbs_tmcc_info {
	u8   change;  // 5 bits, increments with every change
	struct {
		u8 modcodd;    // 4 bits
		u8 num_slots;  // 6 bits
	} mode[4];
	u8 rel_ts_id[24];  // bit 6..4 Relative TSID for slot i*2 + 1, bit 2..0 Relative TSID for slot i*2 + 2
	struct {
		u8 high_byte;
		u8 low_byte;
	} ts_id[8];
	u8   flags;  // Bit 5: EWS flag, bit 4: Site Diversity flag, bit 3..1: Site Diversity information, bit 0: Extension flag
	u8   extension[8];   // 61 bits, right aligned
};

struct sx8_bist {
	u8  cut;
	u8  avs_code;
	u8  temperature;
	u8  rsvd[13];
};

struct sx8_packet_filter_status {
	u8   status;
	u8   offset;
	u8   length;
	u8   rsvd2;
	u32  rsvd3[2];
	u8   data[96];
};

struct extended_status {
	u8   version;  /* 0 = none, 1 = SX8 */
	u8   flags;    /* Bit 0: 1 = Tuner Valid, Bit 1: 1 = Output Valid */
	u8   tuner;
	u8   output;
};

struct get_serial_number {
	u8   reserved;
	u8   serial_number[17];
};

struct get_license {
	u8   flags;
	u8   serial_number[17];
	u16  code;
	u8   ID[8];
	u8   LK[24];
};

struct mci_command_header {
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
		u16 params16[31 * 2];
		u8  params8[31 * 4];
		struct mod_setup_channels mod_setup_channels[4];
		struct mod_setup_stream   mod_setup_stream;
		struct mod_setup_output   mod_setup_output;
		struct dvbs2_search       dvbs2_search;
		struct isdbs_search       isdbs_search;
		struct dvbc_search        dvbc_search;
		struct dvbt_search        dvbt_search;
		struct dvbt2_search       dvbt2_search;
		struct dvbc2_search       dvbc2_search;
		struct isdbt_search       isdbt_search;
		struct isdbc_search       isdbc_search;
		struct j83b_search        j83b_search;
		struct atsc_search        atsc_search;
		struct atsc3_search       atsc3_search;
		
		struct get_signalinfo     get_signalinfo;
		struct get_iq_symbol      get_iq_symbol;
		struct get_ids            get_ids;
		struct get_l1_info        get_l1_info;
		struct get_bb_header      get_bb_header;

		struct sx8_start_iq       sx8_start_iq;
		struct sx8_input_enable   sx8_input_enable;
		struct sx8_packet_filter  sx8_packet_filter;
		struct sx8_rfagc_control  sx8_rfagc_control;
		struct set_license        set_license;
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
		u32 result32[27];
		u16 result16[27 * 2];
		u8 result8[27 * 4];
		struct mx_capabilities          mx_capabilities;
		struct common_signal_info       common_signal_info;
		struct dvbs2_signal_info        dvbs2_signal_info;
		struct isdbs_signal_info        isdbs_signal_info;
		struct dvbc_signal_info         dvbc_signal_info;
		struct dvbt_signal_info         dvbt_signal_info;
		struct dvbt2_signal_info        dvbt2_signal_info;
		struct dvbc2_signal_info        dvbc2_signal_info;
		struct isdbt_signal_info        isdbt_signal_info;
		struct j83b_signal_info         j83b_signal_info;
		struct isdbc_signal_info        isdbc_signal_info;
		struct atsc_signal_info         atsc_signal_info;
		struct atsc3_signal_info        atsc3_signal_info;
		struct iq_symbol                iq_symbol;
		struct dvbt_tps_info            dvbt_tps_info;
		struct dvbt2_l1_info            dvbt2_l1_info;
		struct dvbt2_plp_info           dvbt2_plp_info;
		struct dvbt2_l1_info_old        dvbt2_l1_info_old;
		struct dvbt2_plp_info_old       dvbt2_plp_info_old;
		struct dvbc2_l1_part2           dvbc2_l1_part2;
		struct dvbc2_id_list            dvbc2_id_list;
		struct dvbc2_slice_info         dvbc2_slice_info;
		struct dvbc2_plp_info           dvbc2_plp_info;
		struct bb_header                bb_header;
		struct isdbt_tmcc_info          isdbt_tmcc_info;
		struct isdbs_tmcc_info          isdbs_tmcc_info;
		struct sx8_bist                 sx8_bist;
		struct sx8_packet_filter_status sx8_packet_filter_status;
		struct extended_status          extended_status;
		struct get_serial_number        get_serial_number;
		struct get_license              get_license;
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
	struct mutex         tuner_lock;
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

	struct mutex         lock;
	struct mci_command   cmd;
	struct mci_result    result;
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
int ddb_mci_cmd_link_simple(struct ddb_link *link, u8 command, u8 demod, u8 value);
int ddb_mci_get_status(struct mci *mci, struct mci_result *res);
int ddb_mci_get_snr(struct dvb_frontend *fe);
int ddb_mci_get_info(struct mci *mci);
int ddb_mci_get_strength(struct dvb_frontend *fe);
void ddb_mci_proc_info(struct mci *mci, struct dtv_frontend_properties *p);
int mci_init(struct ddb_link *link);

#endif

#endif
