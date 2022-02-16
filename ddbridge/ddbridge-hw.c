// SPDX-License-Identifier: GPL-2.0
/*
 * ddbridge-hw.c: Digital Devices device information tables
 *
 * Copyright (C) 2010-2017 Digital Devices GmbH
 *                         Ralph Metzler <rjkm@metzlerbros.de>
 *                         Marcus Metzler <mocm@metzlerbros.de>
 *
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ddbridge.h"

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

static const struct ddb_regset octopus_mod_odma = {
	.base = 0x300,
	.num  = 0x0a,
	.size = 0x10,
};

static const struct ddb_regset octopus_mod_odma_buf = {
	.base = 0x2000,
	.num  = 0x0a,
	.size = 0x100,
};

static const struct ddb_regset octopus_mod_channel = {
	.base = 0x400,
	.num  = 0x0a,
	.size = 0x40,
};

/****************************************************************************/

static const struct ddb_regset octopus_mod_2_odma = {
	.base = 0x400,
	.num  = 0x18,
	.size = 0x10,
};

static const struct ddb_regset octopus_mod_2_odma_buf = {
	.base = 0x8000,
	.num  = 0x18,
	.size = 0x100,
};

static const struct ddb_regset octopus_mod_2_channel = {
	.base = 0x800,
	.num  = 0x18,
	.size = 0x40,
};

static const struct ddb_regset octopus_sdr_output = {
	.base = 0x240,
	.num  = 0x14,
	.size = 0x10,
};

/****************************************************************************/

static const struct ddb_regset octopus_input = {
	.base = 0x200,
	.num  = 0x08,
	.size = 0x10,
};

static const struct ddb_regset octopus_output = {
	.base = 0x280,
	.num  = 0x08,
	.size = 0x10,
};

static const struct ddb_regset octopus_idma = {
	.base = 0x300,
	.num  = 0x08,
	.size = 0x10,
};

static const struct ddb_regset octopus_idma_buf = {
	.base = 0x2000,
	.num  = 0x08,
	.size = 0x100,
};

static const struct ddb_regset octopus_odma = {
	.base = 0x380,
	.num  = 0x04,
	.size = 0x10,
};

static const struct ddb_regset octopus_odma_buf = {
	.base = 0x2800,
	.num  = 0x04,
	.size = 0x100,
};

static const struct ddb_regset octopus_i2c = {
	.base = 0x80,
	.num  = 0x04,
	.size = 0x20,
};

static const struct ddb_regset octopus_i2c_buf = {
	.base = 0x1000,
	.num  = 0x04,
	.size = 0x200,
};

/****************************************************************************/

static const struct ddb_regset max_mci = {
	.base = 0x500,
	.num  = 0x01,
	.size = 0x04,
};

static const struct ddb_regset max_mci_buf = {
	.base = 0x600,
	.num  = 0x01,
	.size = 0x100,
};

static const struct ddb_regset sdr_mci = {
	.base = 0x260,
	.num  = 0x01,
	.size = 0x04,
};

static const struct ddb_regset sdr_mci_buf = {
	.base = 0x300,
	.num  = 0x01,
	.size = 0x100,
};

/****************************************************************************/

static const struct ddb_regset octopro_input = {
	.base = 0x400,
	.num  = 0x14,
	.size = 0x10,
};

static const struct ddb_regset octopro_output = {
	.base = 0x600,
	.num  = 0x0a,
	.size = 0x10,
};

static const struct ddb_regset octopro_idma = {
	.base = 0x800,
	.num  = 0x40,
	.size = 0x10,
};

static const struct ddb_regset octopro_idma_buf = {
	.base = 0x4000,
	.num  = 0x40,
	.size = 0x100,
};

static const struct ddb_regset octopro_odma = {
	.base = 0xc00,
	.num  = 0x20,
	.size = 0x10,
};

static const struct ddb_regset octopro_odma_buf = {
	.base = 0x8000,
	.num  = 0x20,
	.size = 0x100,
};

static const struct ddb_regset octopro_i2c = {
	.base = 0x200,
	.num  = 0x0a,
	.size = 0x20,
};

static const struct ddb_regset octopro_i2c_buf = {
	.base = 0x2000,
	.num  = 0x0a,
	.size = 0x200,
};

static const struct ddb_regset octopro_gtl = {
	.base = 0xe00,
	.num  = 0x03,
	.size = 0x40,
};

/****************************************************************************/
/****************************************************************************/

static const struct ddb_regset gtl_mini_input = {
	.base = 0x400,
	.num  = 0x14,
	.size = 0x10,
};

static const struct ddb_regset gtl_mini_idma = {
	.base = 0x800,
	.num  = 0x40,
	.size = 0x10,
};

static const struct ddb_regset gtl_mini_idma_buf = {
	.base = 0x4000,
	.num  = 0x40,
	.size = 0x100,
};

static const struct ddb_regset gtl_mini_gtl = {
	.base = 0xe00,
	.num  = 0x03,
	.size = 0x40,
};

/****************************************************************************/
/****************************************************************************/

static const struct ddb_regmap octopus_map = {
	.irq_version = 1,
	.irq_base_i2c = 0,
	.irq_base_idma = 8,
	.irq_base_odma = 16,
	.i2c = &octopus_i2c,
	.i2c_buf = &octopus_i2c_buf,
	.idma = &octopus_idma,
	.idma_buf = &octopus_idma_buf,
	.odma = &octopus_odma,
	.odma_buf = &octopus_odma_buf,
	.input = &octopus_input,

	.output = &octopus_output,
};

static const struct ddb_regmap octopus_mci_map = {
	.irq_version = 1,
	.irq_base_i2c = 0,
	.irq_base_idma = 8,
	.irq_base_odma = 16,
	.irq_base_mci = 0,
	.i2c = &octopus_i2c,
	.i2c_buf = &octopus_i2c_buf,
	.idma = &octopus_idma,
	.idma_buf = &octopus_idma_buf,
	.odma = &octopus_odma,
	.odma_buf = &octopus_odma_buf,
	.input = &octopus_input,
	.output = &octopus_output,

	.mci = &max_mci,
	.mci_buf = &max_mci_buf,
};

static const struct ddb_regmap octopro_map = {
	.irq_version = 2,
	.irq_base_i2c = 32,
	.irq_base_idma = 64,
	.irq_base_odma = 128,
	.irq_base_gtl = 8,
	.i2c = &octopro_i2c,
	.i2c_buf = &octopro_i2c_buf,
	.idma = &octopro_idma,
	.idma_buf = &octopro_idma_buf,
	.odma = &octopro_odma,
	.odma_buf = &octopro_odma_buf,
	.input = &octopro_input,
	.output = &octopro_output,
	.gtl = &octopro_gtl,
};

static const struct ddb_regmap octopro_hdin_map = {
	.irq_version = 2,
	.irq_base_i2c = 32,
	.irq_base_idma = 64,
	.irq_base_odma = 128,
	.i2c = &octopro_i2c,
	.i2c_buf = &octopro_i2c_buf,
	.idma = &octopro_idma,
	.idma_buf = &octopro_idma_buf,
	.odma = &octopro_odma,
	.odma_buf = &octopro_odma_buf,
	.input = &octopro_input,
	.output = &octopro_output,
};

static const struct ddb_regmap octopus_mod_map = {
	.irq_version = 1,
	.irq_base_odma = 8,
	.irq_base_rate = 18,
	.output = &octopus_output,
	.odma = &octopus_mod_odma,
	.odma_buf = &octopus_mod_odma_buf,
	.channel = &octopus_mod_channel,
};

static const struct ddb_regmap octopus_mod_2_map = {
	.irq_version = 2,
	.irq_base_odma = 64,
	.irq_base_rate = 32,
	.irq_base_mci = 10,
	.output = &octopus_output,
	.odma = &octopus_mod_2_odma,
	.odma_buf = &octopus_mod_2_odma_buf,
	.channel = &octopus_mod_2_channel,

	.mci = &sdr_mci,
	.mci_buf = &sdr_mci_buf,
};

static const struct ddb_regmap octopus_sdr_map = {
	.irq_version = 2,
	.irq_base_odma = 64,
	.irq_base_rate = 32,
	.irq_base_mci = 10,
	.output = &octopus_sdr_output,
	.odma = &octopus_mod_2_odma,
	.odma_buf = &octopus_mod_2_odma_buf,
	.channel = &octopus_mod_2_channel,

	.mci = &sdr_mci,
	.mci_buf = &sdr_mci_buf,
};

static const struct ddb_regmap gtl_mini = {
	.irq_version = 2,
	.irq_base_i2c = 32,
	.irq_base_idma = 64,
	.irq_base_odma = 128,
	.irq_base_gtl = 8,
	.idma = &gtl_mini_idma,
	.idma_buf = &gtl_mini_idma_buf,
	.input = &gtl_mini_input,
	.gtl = &gtl_mini_gtl,
};

/****************************************************************************/
/****************************************************************************/

static const struct ddb_info ddb_none = {
	.type     = DDB_NONE,
	.name     = "unknown Digital Devices device, install newer driver",
	.regmap   = &octopus_map,
};

static const struct ddb_info ddb_octopus = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Octopus DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
};

static const struct ddb_info ddb_octopusv3 = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Octopus V3 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
};

static const struct ddb_info ddb_octopus_le = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Octopus LE DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 2,
	.i2c_mask = 0x03,
};

static const struct ddb_info ddb_octopus_oem = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Octopus OEM",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.led_num  = 1,
	.fan_num  = 1,
	.temp_num = 1,
	.temp_bus = 0,
};

static const struct ddb_info ddb_octopus_mini = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Octopus Mini",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
};

static const struct ddb_info ddb_v6 = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Cine S2 V6 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 3,
	.i2c_mask = 0x07,
};

static const struct ddb_info ddb_v6_5 = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Cine S2 V6.5 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
};

static const struct ddb_info ddb_v7a = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Cine S2 V7 Advanced DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.board_control   = 2,
	.board_control_2 = 4,
	.ts_quirks = TS_QUIRK_REVERSED,
	.hw_min    = 0x010007,
};

static const struct ddb_info ddb_v7 = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Cine S2 V7 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.board_control   = 2,
	.board_control_2 = 4,
	.ts_quirks = TS_QUIRK_REVERSED,
	.hw_min    = 0x010007,
};

static const struct ddb_info ddb_ctv7 = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Cine CT V7 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.board_control   = 3,
	.board_control_2 = 4,
};

static const struct ddb_info ddb_satixs2v3 = {
	.type     = DDB_OCTOPUS,
	.name     = "Mystique SaTiX-S2 V3 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 3,
	.i2c_mask = 0x07,
};

static const struct ddb_info ddb_ci = {
	.type     = DDB_OCTOPUS_CI,
	.name     = "Digital Devices Octopus CI",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x03,
};

static const struct ddb_info ddb_cis = {
	.type     = DDB_OCTOPUS_CI,
	.name     = "Digital Devices Octopus CI single",
	.regmap   = &octopus_map,
	.port_num = 3,
	.i2c_mask = 0x03,
};

static const struct ddb_info ddb_ci_s2_pro = {
	.type     = DDB_OCTOPUS_CI,
	.name     = "Digital Devices Octopus CI S2 Pro",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x01,
	.board_control   = 2,
	.board_control_2 = 4,
	.hw_min    = 0x010007,
};

static const struct ddb_info ddb_ci_s2_pro_a = {
	.type     = DDB_OCTOPUS_CI,
	.name     = "Digital Devices Octopus CI S2 Pro Advanced",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x01,
	.board_control   = 2,
	.board_control_2 = 4,
	.hw_min    = 0x010007,
};

static const struct ddb_info ddb_dvbct = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices DVBCT V6.1 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 3,
	.i2c_mask = 0x07,
};

/****************************************************************************/

static const struct ddb_info ddb_mod = {
	.type     = DDB_MOD,
	.name     = "Digital Devices DVB-C modulator",
	.regmap   = &octopus_mod_map,
	.port_num = 10,
	.temp_num = 1,
};

static const struct ddb_info ddb_mod_4 = {
	.type     = DDB_MOD,
	.name     = "Digital Devices DVB-C modulator",
	.regmap   = &octopus_mod_map,
	.port_num = 4,
	.temp_num = 1,
};

static const struct ddb_info ddb_mod_fsm_24 = {
	.type     = DDB_MOD,
	.version  = 2,
	.name     = "Digital Devices DVB-C modulator FSM-24",
	.regmap   = &octopus_mod_2_map,
	.port_num = 24,
	.temp_num = 1,
	.tempmon_irq = 8,
	.lostlock_irq = 9,
};

static const struct ddb_info ddb_mod_fsm_16 = {
	.type     = DDB_MOD,
	.version  = 2,
	.name     = "Digital Devices DVB-C modulator FSM-16",
	.regmap   = &octopus_mod_2_map,
	.port_num = 16,
	.temp_num = 1,
	.tempmon_irq = 8,
	.lostlock_irq = 9,
};

static const struct ddb_info ddb_mod_fsm_8 = {
	.type     = DDB_MOD,
	.name     = "Digital Devices DVB-C modulator FSM-8",
	.version  = 2,
	.regmap   = &octopus_mod_2_map,
	.port_num = 8,
	.temp_num = 1,
	.tempmon_irq = 8,
	.lostlock_irq = 9,
};

static const struct ddb_info ddb_mod_fsm_4 = {
	.type     = DDB_MOD,
	.name     = "Digital Devices DVB-C modulator FSM-4",
	.version  = 2,
	.regmap   = &octopus_mod_2_map,
	.port_num = 4,
	.temp_num = 1,
	.tempmon_irq = 8,
	.lostlock_irq = 9,
};

static const struct ddb_info ddb_sdr_atv = {
	.type     = DDB_MOD,
	.name     = "Digital Devices SDR ATV",
	.version  = 16,
	.regmap   = &octopus_sdr_map,
	.port_num = 16,
	.temp_num = 1,
	.tempmon_irq = 8,
};

static const struct ddb_info ddb_sdr_iq = {
	.type     = DDB_MOD,
	.name     = "Digital Devices SDR IQ",
	.version  = 17,
	.regmap   = &octopus_sdr_map,
	.port_num = 16,
	.temp_num = 1,
	.tempmon_irq = 8,
};

static const struct ddb_info ddb_sdr_iq2 = {
	.type     = DDB_MOD,
	.name     = "Digital Devices SDR IQ2",
	.version  = 17,
	.regmap   = &octopus_sdr_map,
	.port_num = 4,
	.temp_num = 1,
	.tempmon_irq = 8,
};

static const struct ddb_info ddb_sdr_dvbt = {
	.type     = DDB_MOD,
	.name     = "Digital Devices DVBT",
	.version  = 18,
	.regmap   = &octopus_sdr_map,
	.port_num = 16,
	.temp_num = 1,
	.tempmon_irq = 8,
};

static const struct ddb_info ddb_octopro_hdin = {
	.type     = DDB_OCTOPRO_HDIN,
	.name     = "Digital Devices OctopusNet Pro HDIN",
	.regmap   = &octopro_hdin_map,
	.port_num = 10,
	.i2c_mask = 0x3ff,
	.mdio_base = 0x10020,
};

static const struct ddb_info ddb_octopro = {
	.type     = DDB_OCTOPRO,
	.name     = "Digital Devices OctopusNet Pro",
	.regmap   = &octopro_map,
	.port_num = 10,
	.i2c_mask = 0x3ff,
	.mdio_base = 0x10020,
};

static const struct ddb_info ddb_s2_48 = {
	.type     = DDB_OCTOPUS_MAX,
	.name     = "Digital Devices MAX S8 4/8",
	.regmap   = &octopus_mci_map,
	.port_num = 4,
	.i2c_mask = 0x01,
	.board_control = 1,
	.tempmon_irq = 24,
};

static const struct ddb_info ddb_ct2_8 = {
	.type     = DDB_OCTOPUS_MAX_CT,
	.name     = "Digital Devices MAX A8 CT2",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.board_control   = 0x0ff,
	.board_control_2 = 0xf00,
	.ts_quirks = TS_QUIRK_SERIAL,
	.tempmon_irq = 24,
};

static const struct ddb_info ddb_c2t2_8 = {
	.type     = DDB_OCTOPUS_MAX_CT,
	.name     = "Digital Devices MAX A8 C2T2",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.board_control   = 0x0ff,
	.board_control_2 = 0xf00,
	.ts_quirks = TS_QUIRK_SERIAL,
	.tempmon_irq = 24,
};

static const struct ddb_info ddb_isdbt_8 = {
	.type     = DDB_OCTOPUS_MAX_CT,
	.name     = "Digital Devices MAX A8 ISDBT",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.board_control   = 0x0ff,
	.board_control_2 = 0xf00,
	.ts_quirks = TS_QUIRK_SERIAL,
	.tempmon_irq = 24,
};

static const struct ddb_info ddb_c2t2i_v0_8 = {
	.type     = DDB_OCTOPUS_MAX_CT,
	.name     = "Digital Devices MAX A8 C2T2I V0",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.board_control   = 0x0ff,
	.board_control_2 = 0xf00,
	.ts_quirks = TS_QUIRK_SERIAL | TS_QUIRK_ALT_OSC,
	.tempmon_irq = 24,
};

static const struct ddb_info ddb_c2t2i_8 = {
	.type     = DDB_OCTOPUS_MAX_CT,
	.name     = "Digital Devices MAX A8 C2T2I",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.board_control   = 0x0ff,
	.board_control_2 = 0xf00,
	.ts_quirks = TS_QUIRK_SERIAL,
	.tempmon_irq = 24,
};

/****************************************************************************/

static const struct ddb_info ddb_s2x_48 = {
	.type     = DDB_OCTOPUS_MCI,
	.name     = "Digital Devices MAX SX8",
	.regmap   = &octopus_mci_map,
	.port_num = 4,
	.i2c_mask = 0x00,
	.tempmon_irq = 24,
	.mci_ports = 4,
	.mci_type = 0,
	.temp_num = 1,
};

static const struct ddb_info ddb_s2x_48_b = {
	.type     = DDB_OCTOPUS_MCI,
	.name     = "Digital Devices MAX SX8 Basic",
	.regmap   = &octopus_mci_map,
	.port_num = 4,
	.i2c_mask = 0x00,
	.tempmon_irq = 24,
	.mci_ports = 4,
	.mci_type = 0,
	.temp_num = 1,
};

static const struct ddb_info ddb_m4 = {
	.type     = DDB_OCTOPUS_MCI,
	.name     = "Digital Devices MAX M4",
	.regmap   = &octopus_mci_map,
	.port_num = 2,
	.i2c_mask = 0x00,
	.tempmon_irq = 24,
	.mci_ports = 2,
	.mci_type = 1,
	.temp_num = 1,
};

/****************************************************************************/

static const struct ddb_info ddb_gtl_mini = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Octopus GT Mini",
	.regmap   = &gtl_mini,
	.port_num = 0,
	.i2c_mask = 0x00,
	.ns_num   = 0,
};

/****************************************************************************/
/****************************************************************************/

static const struct ddb_regmap octopus_net_map = {
	.irq_version = 1,
	.irq_base_i2c = 0,
	.i2c = &octopus_i2c,
	.i2c_buf = &octopus_i2c_buf,
	.input = &octopus_input,
	.output = &octopus_output,
};

static const struct ddb_regset octopus_gtl = {
	.base = 0x180,
	.num  = 0x01,
	.size = 0x20,
};

static const struct ddb_regmap octopus_net_gtl = {
	.irq_version = 1,
	.irq_base_i2c = 0,
	.irq_base_gtl = 10,
	.i2c = &octopus_i2c,
	.i2c_buf = &octopus_i2c_buf,
	.input = &octopus_input,
	.output = &octopus_output,
	.gtl = &octopus_gtl,
};

static const struct ddb_info ddb_octonet = {
	.type     = DDB_OCTONET,
	.name     = "Digital Devices OctopusNet network DVB adapter",
	.regmap   = &octopus_net_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.ns_num   = 12,
	.mdio_base = 0x20,
};

static const struct ddb_info ddb_octonet_jse = {
	.type     = DDB_OCTONET,
	.name     = "Digital Devices OctopusNet network DVB adapter JSE",
	.regmap   = &octopus_net_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.ns_num   = 15,
	.mdio_base = 0x20,
};

static const struct ddb_info ddb_octonet_gtl = {
	.type     = DDB_OCTONET,
	.name     = "Digital Devices OctopusNet GTL",
	.regmap   = &octopus_net_gtl,
	.port_num = 4,
	.i2c_mask = 0x05,
	.ns_num   = 12,
	.mdio_base = 0x20,
	.con_clock = 1,
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

struct ddb_device_id {
	u16 vendor;
	u16 device;
	u16 subvendor;
	u16 subdevice;
	const struct ddb_info *info;
};

#define DDB_DEVID(_device, _subdevice, _info) { \
		.vendor = 0xdd01, \
		.device = _device, \
		.subvendor = 0xdd01, \
		.subdevice = _subdevice, \
		.info = &(_info) }

static const struct ddb_device_id ddb_device_ids[] = {
	/* OctopusNet */
	DDB_DEVID(0x0300, 0xffff, ddb_octonet),
	DDB_DEVID(0x0301, 0xffff, ddb_octonet_jse),
	DDB_DEVID(0x0307, 0xffff, ddb_octonet_gtl),

	/* PCIe devices */

	/* DVB tuners and demodulators */
	DDB_DEVID(0x0002, 0x0001, ddb_octopus),
	DDB_DEVID(0x0003, 0x0001, ddb_octopus),
	DDB_DEVID(0x0005, 0x0004, ddb_octopusv3),
	DDB_DEVID(0x0003, 0x0002, ddb_octopus_le),
	DDB_DEVID(0x0003, 0x0003, ddb_octopus_oem),
	DDB_DEVID(0x0003, 0x0010, ddb_octopus_mini),
	DDB_DEVID(0x0005, 0x0011, ddb_octopus_mini),
	DDB_DEVID(0x0003, 0x0020, ddb_v6),
	DDB_DEVID(0x0003, 0x0021, ddb_v6_5),
	DDB_DEVID(0x0006, 0x0022, ddb_v7),
	DDB_DEVID(0x0006, 0x0024, ddb_v7a),
	DDB_DEVID(0x0003, 0x0030, ddb_dvbct),
	DDB_DEVID(0x0003, 0xdb03, ddb_satixs2v3),
	DDB_DEVID(0x0006, 0x0031, ddb_ctv7),
	DDB_DEVID(0x0006, 0x0032, ddb_ctv7),
	DDB_DEVID(0x0006, 0x0033, ddb_ctv7),
	DDB_DEVID(0x0007, 0x0023, ddb_s2_48),
	DDB_DEVID(0x0008, 0x0034, ddb_ct2_8),
	DDB_DEVID(0x0008, 0x0035, ddb_c2t2_8),
	DDB_DEVID(0x0008, 0x0036, ddb_isdbt_8),
	DDB_DEVID(0x0008, 0x0037, ddb_c2t2i_v0_8),
	DDB_DEVID(0x0008, 0x0038, ddb_c2t2i_8),
	DDB_DEVID(0x0009, 0x0025, ddb_s2x_48),
	DDB_DEVID(0x0006, 0x0039, ddb_ctv7),
	DDB_DEVID(0x000a, 0x0050, ddb_m4),
	DDB_DEVID(0x000b, 0x0026, ddb_s2x_48_b),
	DDB_DEVID(0x0011, 0x0040, ddb_ci),
	DDB_DEVID(0x0011, 0x0041, ddb_cis),
	DDB_DEVID(0x0012, 0x0042, ddb_ci),
	DDB_DEVID(0x0013, 0x0043, ddb_ci_s2_pro),
	DDB_DEVID(0x0013, 0x0044, ddb_ci_s2_pro_a),
	DDB_DEVID(0x0020, 0x0012, ddb_gtl_mini),

	/* Modulators */
	DDB_DEVID(0x0201, 0x0001, ddb_mod),
	DDB_DEVID(0x0201, 0x0002, ddb_mod),
	DDB_DEVID(0x0201, 0x0004, ddb_mod_4),  /* dummy entry ! */
	DDB_DEVID(0x0203, 0x0001, ddb_mod),
	DDB_DEVID(0x0210, 0x0000, ddb_mod_fsm_4), /* dummy entry ! */
	DDB_DEVID(0x0210, 0x0001, ddb_mod_fsm_24),
	DDB_DEVID(0x0210, 0x0002, ddb_mod_fsm_16),
	DDB_DEVID(0x0210, 0x0003, ddb_mod_fsm_8),
	DDB_DEVID(0x0220, 0x0001, ddb_sdr_atv),
	DDB_DEVID(0x0221, 0x0001, ddb_sdr_iq),
	DDB_DEVID(0x0222, 0x0001, ddb_sdr_dvbt),
	DDB_DEVID(0x0223, 0x0001, ddb_sdr_iq2),
	DDB_DEVID(0xffff, 0xffff, ddb_sdr_iq2),

	/* testing on OctopusNet Pro */
	DDB_DEVID(0x0320, 0xffff, ddb_octopro_hdin),
	DDB_DEVID(0x0321, 0xffff, ddb_none),
	DDB_DEVID(0x0322, 0xffff, ddb_octopro),
	DDB_DEVID(0x0323, 0xffff, ddb_none),
	DDB_DEVID(0x0328, 0xffff, ddb_none),
	DDB_DEVID(0x0329, 0xffff, ddb_octopro_hdin),

	DDB_DEVID(0xffff, 0xffff, ddb_none),
};

const struct ddb_info *get_ddb_info(u16 vendor, u16 device,
				    u16 subvendor, u16 subdevice)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ddb_device_ids); i++) {
		const struct ddb_device_id *id = &ddb_device_ids[i];

		if (vendor == id->vendor &&
		    device == id->device &&
		    (subvendor == id->subvendor ||
		     id->subvendor == 0xffff) &&
		    (subdevice == id->subdevice ||
		     id->subdevice == 0xffff))
			return id->info;
	}
	return &ddb_none;
}
