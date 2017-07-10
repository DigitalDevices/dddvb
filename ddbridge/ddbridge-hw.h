
/*
 * ddbridge-hw.h: Digital Devices bridge hardware maps
 *
 * Copyright (C) 2010-2017 Digital Devices GmbH
 *                         Ralph Metzler <rjkm@metzlerbros.de>
 *                         Marcus Metzler <mocm@metzlerbros.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 only, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _DDBRIDGE_HW_H_
#define _DDBRIDGE_HW_H_

#include "ddbridge.h"

/******************************************************************************/

extern struct ddb_info ddb_none;
extern struct ddb_info ddb_octopus;
extern struct ddb_info ddb_octopusv3;
extern struct ddb_info ddb_octopus_le;
extern struct ddb_info ddb_octopus_oem;
extern struct ddb_info ddb_octopus_mini;
extern struct ddb_info ddb_v6;
extern struct ddb_info ddb_v6_5;
extern struct ddb_info ddb_v7;
extern struct ddb_info ddb_v7a;
extern struct ddb_info ddb_ctv7;
extern struct ddb_info ddb_satixS2v3;
extern struct ddb_info ddb_ci;
extern struct ddb_info ddb_cis;
extern struct ddb_info ddb_ci_s2_pro;
extern struct ddb_info ddb_ci_s2_pro_a;
extern struct ddb_info ddb_dvbct;

/****************************************************************************/

extern struct ddb_info ddb_mod;
extern struct ddb_info ddb_mod_fsm_24;
extern struct ddb_info ddb_mod_fsm_16;
extern struct ddb_info ddb_mod_fsm_8;
extern struct ddb_info ddb_sdr;

/****************************************************************************/

extern struct ddb_info ddb_octopro_hdin;
extern struct ddb_info ddb_octopro;
extern struct ddb_info ddb_octonet;
extern struct ddb_info ddb_octonet_jse;
extern struct ddb_info ddb_octonet_gtl;
extern struct ddb_info ddb_octonet_tbd;

/****************************************************************************/

extern struct ddb_info ddb_ct2_8;
extern struct ddb_info ddb_c2t2_8;
extern struct ddb_info ddb_isdbt_8;
extern struct ddb_info ddb_c2t2i_v0_8;
extern struct ddb_info ddb_c2t2i_8;

/****************************************************************************/

extern struct ddb_info ddb_s2_48;

#endif /* _DDBRIDGE_HW_H */
