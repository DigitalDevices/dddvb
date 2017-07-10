/*
 * ddbridge-hw.c: Digital Devices bridge hardware maps
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

#include "ddbridge.h"

/****************************************************************************/

static struct ddb_regset octopus_mod_odma = {
	.base = 0x300,
	.num  = 0x0a,
	.size = 0x10,
};

static struct ddb_regset octopus_mod_odma_buf = {
	.base = 0x2000,
	.num  = 0x0a,
	.size = 0x100,
};

static struct ddb_regset octopus_mod_channel = {
	.base = 0x400,
	.num  = 0x0a,
	.size = 0x40,
};

/****************************************************************************/

static struct ddb_regset octopus_mod_2_odma = {
	.base = 0x400,
	.num  = 0x18,
	.size = 0x10,
};

static struct ddb_regset octopus_mod_2_odma_buf = {
	.base = 0x8000,
	.num  = 0x18,
	.size = 0x100,
};

static struct ddb_regset octopus_mod_2_channel = {
	.base = 0x800,
	.num  = 0x18,
	.size = 0x40,
};

static struct ddb_regset octopus_sdr_output = {
	.base = 0x240,
	.num  = 0x14,
	.size = 0x10,
};

/****************************************************************************/

static struct ddb_regset octopus_input = {
	.base = 0x200,
	.num  = 0x08,
	.size = 0x10,
};

static struct ddb_regset octopus_output = {
	.base = 0x280,
	.num  = 0x08,
	.size = 0x10,
};

static struct ddb_regset octopus_idma = {
	.base = 0x300,
	.num  = 0x08,
	.size = 0x10,
};

static struct ddb_regset octopus_idma_buf = {
	.base = 0x2000,
	.num  = 0x08,
	.size = 0x100,
};

static struct ddb_regset octopus_odma = {
	.base = 0x380,
	.num  = 0x04,
	.size = 0x10,
};

static struct ddb_regset octopus_odma_buf = {
	.base = 0x2800,
	.num  = 0x04,
	.size = 0x100,
};

static struct ddb_regset octopus_i2c = {
	.base = 0x80,
	.num  = 0x04,
	.size = 0x20,
};

static struct ddb_regset octopus_i2c_buf = {
	.base = 0x1000,
	.num  = 0x04,
	.size = 0x200,
};

/****************************************************************************/

static struct ddb_regset octopro_input = {
	.base = 0x400,
	.num  = 0x14,
	.size = 0x10,
};

static struct ddb_regset octopro_output = {
	.base = 0x600,
	.num  = 0x0a,
	.size = 0x10,
};

static struct ddb_regset octopro_idma = {
	.base = 0x800,
	.num  = 0x40,
	.size = 0x10,
};

static struct ddb_regset octopro_idma_buf = {
	.base = 0x4000,
	.num  = 0x40,
	.size = 0x100,
};

static struct ddb_regset octopro_odma = {
	.base = 0xc00,
	.num  = 0x20,
	.size = 0x10,
};

static struct ddb_regset octopro_odma_buf = {
	.base = 0x8000,
	.num  = 0x20,
	.size = 0x100,
};

static struct ddb_regset octopro_i2c = {
	.base = 0x200,
	.num  = 0x0a,
	.size = 0x20,
};

static struct ddb_regset octopro_i2c_buf = {
	.base = 0x2000,
	.num  = 0x0a,
	.size = 0x200,
};

static struct ddb_regset octopro_gtl = {
	.base = 0xe00,
	.num  = 0x03,
	.size = 0x40,
};

static struct ddb_regset octopus_gtl = {
	.base = 0x180,
	.num  = 0x01,
	.size = 0x20,
};

/****************************************************************************/

static struct ddb_regmap octopus_map = {
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

static struct ddb_regmap octopro_map = {
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

static struct ddb_regmap octopro_hdin_map = {
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

static struct ddb_regmap octopus_mod_map = {
	.irq_version = 1,
	.irq_base_odma = 8,
	.irq_base_rate = 18,
	.output = &octopus_output,
	.odma = &octopus_mod_odma,
	.odma_buf = &octopus_mod_odma_buf,
	.channel = &octopus_mod_channel,
};

static struct ddb_regmap octopus_mod_2_map = {
	.irq_version = 2,
	.irq_base_odma = 64,
	.irq_base_rate = 32,
	.output = &octopus_output,
	.odma = &octopus_mod_2_odma,
	.odma_buf = &octopus_mod_2_odma_buf,
	.channel = &octopus_mod_2_channel,
};

static struct ddb_regmap octopus_sdr_map = {
	.irq_version = 2,
	.irq_base_odma = 64,
	.irq_base_rate = 32,
	.output = &octopus_sdr_output,
	.odma = &octopus_mod_2_odma,
	.odma_buf = &octopus_mod_2_odma_buf,
	.channel = &octopus_mod_2_channel,
};

/****************************************************************************/

static struct ddb_regmap octopus_net_map = {
	.irq_version = 1,
	.irq_base_i2c = 0,
	.i2c = &octopus_i2c,
	.i2c_buf = &octopus_i2c_buf,
	.input = &octopus_input,
	.output = &octopus_output,
};

static struct ddb_regmap octopus_net_gtl = {
	.irq_version = 1,
	.irq_base_i2c = 0,
	.irq_base_gtl = 10,
	.i2c = &octopus_i2c,
	.i2c_buf = &octopus_i2c_buf,
	.input = &octopus_input,
	.output = &octopus_output,
	.gtl = &octopus_gtl,
};

/****************************************************************************/

struct ddb_info ddb_none = {
	.type     = DDB_NONE,
	.name     = "unknown Digital Devices PCIe card, install newer driver",
	.regmap   = &octopus_map,
};

struct ddb_info ddb_octopus = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Octopus DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
};

struct ddb_info ddb_octopusv3 = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Octopus V3 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
};

struct ddb_info ddb_octopus_le = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Octopus LE DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 2,
	.i2c_mask = 0x03,
};

struct ddb_info ddb_octopus_oem = {
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

struct ddb_info ddb_octopus_mini = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Octopus Mini",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
};

struct ddb_info ddb_v6 = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Cine S2 V6 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 3,
	.i2c_mask = 0x07,
};

struct ddb_info ddb_v6_5 = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Cine S2 V6.5 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
};

struct ddb_info ddb_v7a = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Cine S2 V7 Advanced DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.board_control   = 2,
	.board_control_2 = 4,
	.ts_quirks = TS_QUIRK_REVERSED,
};

struct ddb_info ddb_v7 = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Cine S2 V7 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.board_control   = 2,
	.board_control_2 = 4,
	.ts_quirks = TS_QUIRK_REVERSED,
};

struct ddb_info ddb_ctv7 = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices Cine CT V7 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.board_control   = 3,
	.board_control_2 = 4,
};

struct ddb_info ddb_satixS2v3 = {
	.type     = DDB_OCTOPUS,
	.name     = "Mystique SaTiX-S2 V3 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 3,
	.i2c_mask = 0x07,
};

struct ddb_info ddb_ci = {
	.type     = DDB_OCTOPUS_CI,
	.name     = "Digital Devices Octopus CI",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x03,
};

struct ddb_info ddb_cis = {
	.type     = DDB_OCTOPUS_CI,
	.name     = "Digital Devices Octopus CI single",
	.regmap   = &octopus_map,
	.port_num = 3,
	.i2c_mask = 0x03,
};

struct ddb_info ddb_ci_s2_pro = {
	.type     = DDB_OCTOPUS_CI,
	.name     = "Digital Devices Octopus CI S2 Pro",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x01,
	.board_control   = 2,
	.board_control_2 = 4,
};

struct ddb_info ddb_ci_s2_pro_a = {
	.type     = DDB_OCTOPUS_CI,
	.name     = "Digital Devices Octopus CI S2 Pro Advanced",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x01,
	.board_control   = 2,
	.board_control_2 = 4,
};

struct ddb_info ddb_dvbct = {
	.type     = DDB_OCTOPUS,
	.name     = "Digital Devices DVBCT V6.1 DVB adapter",
	.regmap   = &octopus_map,
	.port_num = 3,
	.i2c_mask = 0x07,
};

/****************************************************************************/

struct ddb_info ddb_mod = {
	.type     = DDB_MOD,
	.name     = "Digital Devices DVB-C modulator",
	.regmap   = &octopus_mod_map,
	.port_num = 10,
	.temp_num = 1,
};

struct ddb_info ddb_mod_fsm_24 = {
	.type     = DDB_MOD,
	.version  = 2,
	.name     = "Digital Devices DVB-C modulator FSM-24",
	.regmap   = &octopus_mod_2_map,
	.port_num = 24,
	.temp_num = 1,
	.tempmon_irq = 8,
};

struct ddb_info ddb_mod_fsm_16 = {
	.type     = DDB_MOD,
	.version  = 2,
	.name     = "Digital Devices DVB-C modulator FSM-16",
	.regmap   = &octopus_mod_2_map,
	.port_num = 16,
	.temp_num = 1,
	.tempmon_irq = 8,
};

struct ddb_info ddb_mod_fsm_8 = {
	.type     = DDB_MOD,
	.name     = "Digital Devices DVB-C modulator FSM-8",
	.version  = 2,
	.regmap   = &octopus_mod_2_map,
	.port_num = 8,
	.temp_num = 1,
	.tempmon_irq = 8,
};

struct ddb_info ddb_sdr = {
	.type     = DDB_MOD,
	.name     = "Digital Devices SDR",
	.version  = 3,
	.regmap   = &octopus_sdr_map,
	.port_num = 16,
	.temp_num = 1,
	.tempmon_irq = 8,
};

struct ddb_info ddb_octopro_hdin = {
	.type     = DDB_OCTOPRO_HDIN,
	.name     = "Digital Devices OctopusNet Pro HDIN",
	.regmap   = &octopro_hdin_map,
	.port_num = 10,
	.i2c_mask = 0x3ff,
	.mdio_num = 1,
};

struct ddb_info ddb_octopro = {
	.type     = DDB_OCTOPRO,
	.name     = "Digital Devices OctopusNet Pro",
	.regmap   = &octopro_map,
	.port_num = 10,
	.i2c_mask = 0x3ff,
	.mdio_num = 1,
};

/****************************************************************************/

struct ddb_info ddb_s2_48 = {
	.type     = DDB_OCTOPUS_MAX,
	.name     = "Digital Devices MAX S8 4/8",
	.regmap   = &octopus_map,
	.port_num = 4,
	.i2c_mask = 0x01,
	.board_control = 1,
	.tempmon_irq = 24,
};

struct ddb_info ddb_ct2_8 = {
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

struct ddb_info ddb_c2t2_8 = {
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

struct ddb_info ddb_isdbt_8 = {
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

struct ddb_info ddb_c2t2i_v0_8 = {
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

struct ddb_info ddb_c2t2i_8 = {
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

struct ddb_info ddb_octonet = {
	.type     = DDB_OCTONET,
	.name     = "Digital Devices OctopusNet network DVB adapter",
	.regmap   = &octopus_net_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.ns_num   = 12,
	.mdio_num = 1,
};

struct ddb_info ddb_octonet_jse = {
	.type     = DDB_OCTONET,
	.name     = "Digital Devices OctopusNet network DVB adapter JSE",
	.regmap   = &octopus_net_map,
	.port_num = 4,
	.i2c_mask = 0x0f,
	.ns_num   = 15,
	.mdio_num = 1,
};

struct ddb_info ddb_octonet_gtl = {
	.type     = DDB_OCTONET,
	.name     = "Digital Devices OctopusNet GTL",
	.regmap   = &octopus_net_gtl,
	.port_num = 4,
	.i2c_mask = 0x05,
	.ns_num   = 12,
	.mdio_num = 1,
	.con_clock = 1,
};

struct ddb_info ddb_octonet_tbd = {
	.type     = DDB_OCTONET,
	.name     = "Digital Devices OctopusNet",
	.regmap   = &octopus_net_map,
};
