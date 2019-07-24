static int32_t Log10x100(uint32_t x)
{
	static uint32_t LookupTable[100] = {
		101157945, 103514217, 105925373, 108392691, 110917482,
		113501082, 116144861, 118850223, 121618600, 124451461, // 800.5 - 809.5
		127350308, 130316678, 133352143, 136458314, 139636836,
		142889396, 146217717, 149623566, 153108746, 156675107, // 810.5 - 819.5
		160324539, 164058977, 167880402, 171790839, 175792361,
		179887092, 184077200, 188364909, 192752491, 197242274, // 820.5 - 829.5
		201836636, 206538016, 211348904, 216271852, 221309471,
		226464431, 231739465, 237137371, 242661010, 248313311, // 830.5 - 839.5
		254097271, 260015956, 266072506, 272270131, 278612117,
		285101827, 291742701, 298538262, 305492111, 312607937, // 840.5 - 849.5
		319889511, 327340695, 334965439, 342767787, 350751874,
		358921935, 367282300, 375837404, 384591782, 393550075, // 850.5 - 859.5
		402717034, 412097519, 421696503, 431519077, 441570447,
		451855944, 462381021, 473151259, 484172368, 495450191, // 860.5 - 869.5
		506990708, 518800039, 530884444, 543250331, 555904257,
		568852931, 582103218, 595662144, 609536897, 623734835, // 870.5 - 879.5
		638263486, 653130553, 668343918, 683911647, 699841996,
		716143410, 732824533, 749894209, 767361489, 785235635, // 880.5 - 889.5
		803526122, 822242650, 841395142, 860993752, 881048873,
		901571138, 922571427, 944060876, 966050879, 988553095, // 890.5 - 899.5
	};
	int32_t y = 800;
	int i = 0;

	if( x == 0 ) return 0;
	
	if( x >= 1000000000 ) {
		x /= 10;
		y += 100;
	}
	
	while (x < 100000000 ) {
		x *= 10;
		y -= 100;
	}
	
	while ( i < 100 && x > LookupTable[i] ) i += 1;
	y += i;
	return y;
}

static int32_t berq_rs(uint32_t BERNumerator, uint32_t BERDenominator)
{
     int32_t LogBER = Log10x100(BERDenominator) - Log10x100(BERNumerator);
     int32_t BERQual = 100;
     
     if ( BERNumerator == 0 )
	     return 100;
     if (LogBER < 700) {
	     if (LogBER <  300)
		     BERQual = 0;
	     else
		     BERQual = (LogBER + 5) / 5 - 40;
     }
     return BERQual;
}

static int32_t berq_bch(uint32_t BERNumerator, uint32_t BERDenominator)
{
     int32_t LogBER = Log10x100(BERDenominator) - Log10x100(BERNumerator);
     int32_t BERQual = 100;
     
     if (BERNumerator == 0 )
	     return 100;
     if (LogBER < 700) {
	     if( LogBER <  400 )
		     BERQual = 0;
	     else
		     BERQual = 40;
     }
     return BERQual;
}


static uint32_t ber_quality(struct dddvb_fe *fe)
{
	struct dtv_fe_stats ber;
	
	get_stat(fe->fd, DTV_STAT_PRE_ERROR_BIT_COUNT, &ber);
	get_stat(fe->fd, DTV_STAT_PRE_TOTAL_BIT_COUNT, &ber);
	
	return 100;
}

static int32_t dvbsq(uint32_t snr, uint32_t fec,
		     uint32_t ber_num, uint32_t ber_den)
{
	int32_t SignalToNoiseRel = -1000;
	int32_t Quality = 0;
	int32_t BERQuality = berq_rs(ber_num, ber_den);
	
	// SNR Values for quasi errorfree reception from Nordig 2.2
	static const int32_t DVBS_SN[] = {
		// 1/2 2/3 3/4    5/6    7/8
		0, 38, 56, 67, 0, 77, 0, 84
	};
	
	if (fec >= FEC_NONE && fec <= FEC_7_8 )
		SignalToNoiseRel = snr / 100 - DVBS_SN[fec];
	
	if( SignalToNoiseRel < -70 )
		Quality = 0;
	else if( SignalToNoiseRel < 30 )
		Quality = ((SignalToNoiseRel + 70) * BERQuality) / 100;
	else
		Quality = BERQuality;
	return Quality;
}


int32_t dvbs2q(int32_t snr, uint32_t fec, uint32_t mod,
	       uint32_t ber_num, uint32_t ber_den)
{
	static const int32_t DVBS2_SN_QPSK[] = {
		// 1/2 2/3 3/4 4/5 5/6 6/7 7/8 8/9 AUT  3/5 9/10 2/5 
		0, 20, 41, 50, 57, 62,  0,  0, 72,  0,  32,  74,  7, 
	};
	static const int32_t DVBS2_SN_8PSK[] = {
		// 1/2 2/3 3/4 4/5  5/6 6/7 7/8  8/9 AUT  3/5 9/10 2/5 
		0,  0, 76, 89,  0, 104,  0,  0, 117,  0,  65, 120,  0, 
	};
	static const int32_t DVBS2_SN_16APSK[] = {
		// 1/2  2/3  3/4  4/5  5/6 6/7 7/8  8/9 AUT  3/5 9/10 2/5 
		0,  0, 100, 112, 120, 126,  0,  0, 139,  0,   0, 141,  0, 
	};
	static const int32_t DVBS2_SN_32APSK[] = {
		// 1/2 2/3  3/4  4/5  5/6 6/7 7/8  8/9 AUT  3/5 9/10 2/5 
		0,  0,  0, 137, 146, 153,  0,  0, 167,  0,   0, 171,  0, 
	};
	int32_t BERQuality = berq_bch(ber_num, ber_den);
	int32_t Quality = 0;
	int32_t SignalToNoiseRel = -1000, snc = 0;
	
	if (fec > FEC_2_5 )
		return 0;
	switch (mod) {
	case QPSK:
		snc = DVBS2_SN_QPSK[fec];
		break;
	case PSK_8:
		snc = DVBS2_SN_8PSK[fec];
		break;
	case APSK_16:
		snc = DVBS2_SN_16APSK[fec];
		break;
	case APSK_32:
		snc = DVBS2_SN_32APSK[fec];
		break;
	default:
		return 0;
	}
	SignalToNoiseRel = snr / 100 - snc;
	
	if (SignalToNoiseRel < -30 )
		Quality = 0;
	else if( SignalToNoiseRel < 30 )
		Quality = ((SignalToNoiseRel + 30) * BERQuality) / 60;
	else
		Quality = 100;
	return Quality;
}

static int32_t dvbcq(int32_t snr, uint32_t mod,
		     uint32_t ber_num, uint32_t ber_den)
{
	int32_t SignalToNoiseRel = 0;
	int32_t Quality = 0;
	int32_t BERQuality = berq_rs(ber_num, ber_den);
	
	switch (mod) {
	case QAM_16: SignalToNoiseRel = snr - 200; break;
	case QAM_32: SignalToNoiseRel = snr - 230; break;
	case QAM_64:  SignalToNoiseRel = snr - 260; break;
	case QAM_128: SignalToNoiseRel = snr - 290; break;
	case QAM_256: SignalToNoiseRel = snr - 320; break;
	}

	if (SignalToNoiseRel < -70)
		Quality = 0;
	else if (SignalToNoiseRel < 30)
		Quality = ((SignalToNoiseRel + 70) * BERQuality) / 100;
	else
		Quality = BERQuality;
	return Quality;
}

static int32_t dvbtq(int32_t snr, uint32_t mod, uint32_t fec,
	      uint32_t ber_num, uint32_t ber_den)
{
	int32_t Quality = 0;
	int32_t BERQuality = berq_rs(ber_num, ber_den);
	int32_t SignalToNoiseRel = -1000, snc = 0;
	
	// SNR Values for quasi error free reception from Nordig 2.2
	static const int32_t DVBT_SN_QPSK[] = {
		// 1/2 2/3 3/4 4/5 5/6 6/7 7/8 8/9 AUT  3/5 9/10 2/5 
		0, 51, 69, 79,  0, 89,  0, 97,  0,  0,   0,   0,  0, 
	};
	static const int32_t DVBT_SN_QAM16[] = {
		//  1/2  2/3  3/4 4/5  5/6 6/7  7/8 8/9 AUT  3/5 9/10 2/5 
		0, 108, 131, 146,  0, 156,  0, 160,  0,  0,   0,   0,  0, 
	};
	static const int32_t DVBT_SN_QAM64[] = {
		//  1/2  2/3  3/4 4/5  5/6 6/7  7/8 8/9 AUT  3/5 9/10 2/5 
		0, 165, 187, 202,  0, 216,  0, 225,  0,  0,   0,   0,  0, 
	};

	if (fec > FEC_2_5 )
		return 0;
	switch (mod) {
	case QPSK:
		snc = DVBT_SN_QPSK[fec];
		break;
	case QAM_16:
		snc = DVBT_SN_QAM16[fec];
		break;
	case QAM_64:
		snc = DVBT_SN_QAM64[fec];
		break;
	default:
		break;
	}
	SignalToNoiseRel = snr / 100 - snc;

	if (SignalToNoiseRel < -70 )
		Quality = 0;
	else if (SignalToNoiseRel < 30)
		Quality = ((SignalToNoiseRel + 70) * BERQuality)/100;
	else
		Quality = BERQuality;
	
	return Quality;
}

static int32_t dvbt2q(int32_t snr, uint32_t mod, uint32_t fec, uint32_t trans, uint32_t pilot,
	       uint32_t ber_num, uint32_t ber_den)
{
	int32_t Quality = 0;
	int32_t BERQuality = berq_bch(ber_num, ber_den);
	int32_t SignalToNoiseRel = -1000, snc = 0;

	static const int32_t QE_SN_16K_QPSK[] = {
		// 1/2 2/3 3/4 4/5 5/6 6/7 7/8 8/9 AUT  3/5 9/10 2/5 1/4 1/3
		0, 32, 59, 68, 74, 80,  0,  0,  0,  0,  49,   0, 24,  0, 15 };
	static const int32_t QE_SN_16K_16QAM[] = {
		// 1/2 2/3 3/4 4/5 5/6 6/7 7/8 8/9 AUT  3/5 9/10 2/5 1/4 1/3
		0, 82,116,130,136,141,  0,  0,  0,  0, 104,   0, 74,  0, 62 };
	static const int32_t QE_SN_16K_64QAM[] = {
		// 1/2 2/3 3/4 4/5 5/6 6/7 7/8 8/9 AUT  3/5 9/10 2/5 1/4 1/3
		0,123,165,181,190,197,  0,  0,  0,  0, 151,   0,114,  0,101 };
	static const int32_t QE_SN_16K_256QAM[] = {
		// 1/2 2/3 3/4 4/5 5/6 6/7 7/8 8/9 AUT  3/5 9/10 2/5 1/4 1/3
		0,164,211,232,246,255,  0,  0,  0,  0, 202,   0,153,  0,137 };
	static const int32_t QE_SN_64K_QPSK[] = {
		// 1/2 2/3 3/4 4/5 5/6 6/7 7/8 8/9 AUT  3/5 9/10 2/5 1/4 1/3
		0, 35, 56, 66, 72, 77,  0,  0,  0,  0,  47,   0, 22,  0, 13 };
	static const int32_t QE_SN_64K_16QAM[] = {
		// 1/2 2/3 3/4 4/5 5/6 6/7 7/8 8/9 AUT  3/5 9/10 2/5 1/4 1/3
		0, 87,114,125,133,138,  0,  0,  0,  0, 101,   0, 72,  0, 60 };
	static const int32_t QE_SN_64K_64QAM[] = {
		// 1/2 2/3 3/4 4/5 5/6 6/7 7/8 8/9 AUT  3/5 9/10 2/5 1/4 1/3
		0,130,162,177,187,194,  0,  0,  0,  0, 148,   0,111,  0, 98 };
	static const int32_t QE_SN_64K_256QAM[] = {
		// 1/2 2/3 3/4 4/5 5/6 6/7 7/8 8/9 AUT  3/5 9/10 2/5 1/4 1/3
		0,170,208,229,243,251,  0,  0,  0,  0, 194,   0,148,  0,132 };

	if (trans == TRANSMISSION_MODE_16K) {
		switch (mod) {
		case QPSK:
			snc = QE_SN_16K_QPSK[fec];
			break;
		case QAM_16:
			snc = QE_SN_16K_16QAM[fec];
			break;
		case QAM_64:
			snc = QE_SN_16K_64QAM[fec];
			break;
		case QAM_256:
			snc = QE_SN_16K_256QAM[fec];
			break;
		default:
			break;
		}
	}
	if (trans == TRANSMISSION_MODE_C3780 + 1) { /*	TRANSMISSION_MODE_64K */
		switch (mod) {
		case QPSK:
			snc = QE_SN_64K_QPSK[fec];
			break;
		case QAM_16:
			snc = QE_SN_64K_16QAM[fec];
			break;
		case QAM_64:
			snc = QE_SN_64K_64QAM[fec];
			break;
		case QAM_256:
			snc = QE_SN_64K_256QAM[fec];
			break;
		default:
			break;
		}
	}

	if (snc) {
		SignalToNoiseRel = snr - snc;
#if 0  //FIXME
		if (PilotPattern >= DVBT2_PP3 &&
		    PilotPattern <= DVBT2_PP4 )
			SignalToNoiseRel += 5;
		else if
			( PilotPattern >= DVBT2_PP5 && PilotPattern <= DVBT2_PP8 )
			SignalToNoiseRel += 10;
#endif
	}
	if( SignalToNoiseRel < -30 )
		Quality = 0;
	else if (SignalToNoiseRel < 30 )
		Quality = ((SignalToNoiseRel + 30) * BERQuality)/60;
	else
		Quality = 100;
	
	return Quality;
}

static void calc_lq(struct dddvb_fe *fe)
{
	struct dtv_fe_stats st;
	int64_t str, snr;
	uint32_t mod, fec, ber_num, ber_den, trans, pilot = 0, quality = 0;
	
	get_stat(fe->fd, DTV_STAT_SIGNAL_STRENGTH, &st);
	str = st.stat[0].svalue;
	dbgprintf(DEBUG_DVB, "fe%d: str=%lld\n", fe->nr, str);
	fe->strength = str;
	str = (str * 48) / 10000 + 344;
	if (str < 0)
		str = 0;
	if (str > 255)
		str = 255;
	fe->level = str;
	// str: 0-255: -25dbm = 224, -65dbm = 32
	// qual: 0-15 15=BER<2*10^-4 PER<10^-7
	get_stat(fe->fd, DTV_STAT_CNR, &st);
	snr = st.stat[0].svalue;
	fe->cnr = snr;
	get_property(fe->fd, DTV_INNER_FEC, &fec);
	fe->param.param[PARAM_FEC] = fec;
	get_property(fe->fd, DTV_MODULATION, &mod);
	fe->param.param[PARAM_MTYPE] = mod;

	get_stat(fe->fd, DTV_STAT_PRE_ERROR_BIT_COUNT, &st);
	ber_num = st.stat[0].uvalue;
	get_stat(fe->fd, DTV_STAT_PRE_TOTAL_BIT_COUNT, &st);
	ber_den = st.stat[0].uvalue;

	dbgprintf(DEBUG_DVB, "fe%d: snr=%lld ber=%llu/%llu\n",
		  fe->nr, snr, ber_num, ber_den);
	dbgprintf(DEBUG_DVB, "fe%d: fec=%u mod=%u\n", fe->nr, fec, mod);
	switch (fe->n_param.param[PARAM_MSYS]) {
	case SYS_DVBS:
		quality = dvbsq(snr, fec, ber_num, ber_den);
		break;
	case SYS_DVBS2:
		quality = dvbs2q(snr, fec, mod, ber_num, ber_den);
		break;
	case SYS_DVBC_ANNEX_A:
		quality = dvbcq(snr, mod, ber_num, ber_den);
		break;
	case SYS_DVBT:
		quality = dvbtq(snr, mod, fec, ber_num, ber_den);
		break;
	case SYS_DVBT2:
		get_property(fe->fd, DTV_TRANSMISSION_MODE, &trans);
		dbgprintf(DEBUG_DVB, "fe%d: trans=%u pilot=%u\n", fe->nr, trans, pilot);
		quality = dvbt2q(snr, mod, fec, trans, pilot, ber_num, ber_den);
		break;
	case SYS_DVBC2:
		break;
	default:
		break;
	}
	fe->quality = quality;
	dbgprintf(DEBUG_DVB, "fe%d: level=%u quality=%u\n", fe->nr, fe->level, fe->quality);
}

