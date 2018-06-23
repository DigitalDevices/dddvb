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

#define SX8_DEMOD_STOPPED        (0)
#define SX8_DEMOD_IQ_MODE        (1)
#define SX8_DEMOD_WAIT_SIGNAL    (2)
#define SX8_DEMOD_WAIT_MATYPE    (3)
#define SX8_DEMOD_TIMEOUT        (14)
#define SX8_DEMOD_LOCKED         (15)

#define M4_DEMOD_STOPPED         (0)
#define M4_DEMOD_WAIT_SIGNAL     (1)
#define M4_DEMOD_TIMEOUT         (14)
#define M4_DEMOD_LOCKED          (15)

#define MCI_CMD_STOP             (0x01)
#define MCI_CMD_GETSTATUS        (0x02)
#define MCI_CMD_GETSIGNALINFO    (0x03)
#define MCI_CMD_RFPOWER          (0x04)

#define MCI_CMD_SEARCH_DVBS     (0x10)
#define MCI_CMD_SEARCH_DVBC     (0x20)
#define MCI_CMD_SEARCH_DVBT     (0x21)
#define MCI_CMD_SEARCH_DVBT2    (0x22)
#define MCI_CMD_SEARCH_DVBC2    (0x23)
#define MCI_CMD_SEARCH_ISDBT    (0x24)

#define MCI_CMD_GET_IQSYMBOL     (0x30)

#define MCI_BANDWIDTH_UNKNOWN    (0)
#define MCI_BANDWIDTH_1_7MHZ     (1)
#define MCI_BANDWIDTH_5MHZ       (5)
#define MCI_BANDWIDTH_6MHZ       (6)
#define MCI_BANDWIDTH_7MHZ       (7)
#define MCI_BANDWIDTH_8MHZ       (8)

#define M4_MODE_DVBSX            (2)
#define M4_MODE_DVBC             (3)
#define M4_MODE_DVBT             (4)
#define M4_MODE_DVBT2            (5)
#define M4_MODE_DVBC2            (6)
#define M4_MODE_ISDBT            (7)

#define SX8_CMD_INPUT_ENABLE     (0x40)
#define SX8_CMD_INPUT_DISABLE    (0x41)
#define SX8_CMD_START_IQ         (0x42)
#define SX8_CMD_STOP_IQ          (0x43)
#define SX8_CMD_ENABLE_IQOUTPUT  (0x44)
#define SX8_CMD_DISABLE_IQOUTPUT (0x45)

#define M4_CMD_GET_T2_L1INFO      (0x50)
#define M4_CMD_GET_C2_L1P2        (0x50)
#define M4_CMD_GET_IDS            (0x51)

#define MCI_STATUS_OK             (0x00)
#define MCI_STATUS_UNSUPPORTED    (0x80)
#define MCI_STATUS_RETRY          (0xFD)
#define MCI_STATUS_NOT_READY      (0xFE)
#define MCI_STATUS_ERROR          (0xFF)

#define MCI_SUCCESS(status)     ((status & MCI_STATUS_UNSUPPORTED) == 0)


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
	};
	union {
		u32 params[31];
		struct {
			u8  flags;    /* Bit 0: DVB-S Enabled, 1: DVB-S2 Enabled, 7: InputStreamID*/
                        /* Bit 0 : QPSK, 1: 8PSK/8APSK, 2 : 16APSK, 3: 32APSK, 4: 64APSK, 5: 128APSK, 6: 256APSK */
			u8  s2_modulation_mask;
			u8  rsvd1;
			u8  retry;
			u32 frequency;
			u32 symbol_rate;
			u8  input_stream_id;
			u8  rsvd2[3];
			u32 scrambling_sequence_index;
			u32 frequency_range;
		} dvbs2_search;

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
			u8   tap;
			u8   rsvd;
			u16  point;
		} get_iq_symbol;

		struct {
			u8   flags;              /*  Bit 0 : 0 = VTM, 1 = SCAN.  Bit 1: Set Gain */
			u8   roll_off;
			u8   rsvd1;
			u8   rsvd2;              
			u32  frequency;
			u32  symbol_rate;         /* Only in VTM mode. */
			u16  gain;
		} sx8_start_iq;
		
		struct {
			u8     flags; /* Bit 1:0 = STVVGLNA Gain.  0 = AGC, 1 = 0dB, 2 = Minimum, 3 = Maximum */
		} sx8_input_enable;

		struct {
			u8   Offset;        // Offset into list, must be multiple of 64
			u8   Select;        // 0 = Slices, 1 = PLPs  (C2 Only)
			u8   DataSlice;     // DataSlice to get PLPList (C2 Only)
		} Get_IDs;

		struct {
			u8   Select;        // 0 = Base, 1 = DataSilce, 2 = PLP,  Bit 7:  Set new ID
			u8   ID;            // DataSliceID, PLPId 
		} Get_L1Info;
		
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
		struct {
			u8  standard; /* 1 = DVB-S, 2 = DVB-S2X */
			u8  pls_code; /* puncture rate for DVB-S */
			u8  roll_off;           /* 2-0: rolloff */
			u8  rsvd;
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
			u8  modulation;
			u8  rsvd1[3];
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
			u8  tps_25_32;         /* Constellation (2), Hierarchy (3), Coderate HP (3) */
			u8  tps_33_39;         /* Coderate LP (3), Guardinterval (2), FFT (2), 0 (1) */
			u16 tps_cell_id;       /* Cell Identifier */
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
			u32 rsvd0;
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

		struct {  // Work in Progress
			u32 rsvd0     ;        // Cell Identifier
			
			u32 frequency;         // actual frequency in Hz
			u32 rsvd1;             //
			s16 channel_power;      // channel power in dBm x 100
			s16 band_power;         // band power in dBm x 100
			s16 signal_to_noise; // SNR in dB x 100, Note: negativ values are valid in DVB-S2
			s16 rsvd2;
			u32 packet_errors;      // Counter for packet errors. (set to 0 on Start command)
			u32 ber_numerator;      // Bit error rate: PreBCH
			u32 ber_denominator;              
		} DVBC2_SignalInfo;

		struct {
			u32 rsvd0;
			
			u32 frequency;         // actual frequency in Hz
			u32 rsvd1;             //
			s16 channel_power;      // channel power in dBm x 100
			s16 band_power;         // band power in dBm x 100
			s16 signal_to_noise;     // SNR in dB x 100, Note: negativ values are valid in DVB-S2
			s16 rsvd2;
			u32 packet_errors;      // Counter for packet errors. (set to 0 on Start command)
			u32 ber_numerator;      // Bit error rate: PreRS 
			u32 ber_denominator;
			
			u8  tmcc_info[13];      // TMCC B20 - B121
		} isdbt_signal_info;

		struct {
			s16 i;
			s16 q;
		} iq_symbol;
		struct {
			u8  t2_l1_pre[37];
			u8  t2_l1_post[15];
			u8  t2_l1_post_d[19];
			u8  t2_l1_post_c[19];
		} dvbt2_l1_info;

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
			u8  FEC_Type;
			u8  Mod;
			u8  Cod;
			u8  PSISIReprocessing;
			u8  TransportstreamID[2];
			u8  OrginalNetworkID[2];
			u8  Reserved1;
		} DVBC2_PLPInfo;
	};
	u32 version[4];
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

#define L1POST_SUB_SLICES_PER_FRAME(p)     (((u16)(p)[ 0] & 0x7F) | (p)[ 1])
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

struct mci_base {
	struct list_head     mci_list;
	void                *key;
	struct ddb_link     *link;
	struct completion    completion;
	struct i2c_adapter  *i2c;
	struct mutex         i2c_lock;
	struct mutex         tuner_lock;
	struct mutex         mci_lock;
	int                  count;
	int                  type;
};

struct mci {
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
int ddb_mci_cmd_raw(struct mci *state, struct mci_command *command, u32 command_len,
		    struct mci_result *result, u32 result_len);
int ddb_mci_config(struct mci *state, u32 config);
int ddb_mci_get_status(struct mci *mci, struct mci_result *res);
int ddb_mci_get_snr(struct dvb_frontend *fe);
int ddb_mci_get_info(struct mci *mci);
int ddb_mci_get_strength(struct dvb_frontend *fe);

#endif
