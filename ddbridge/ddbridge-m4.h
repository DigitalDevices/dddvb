// SPDX-License-Identifier: GPL-2.0
/*
 * ddbridge-m4.c: Digital Devices MAX M4 driver
 *
 * Copyright (C) 2018 Digital Devices GmbH
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

#ifndef _DDBRIDGE_M4_H_
#define _DDBRIDGE_M4_H_

struct dvb_frontend *ddb_mx_attach(struct ddb_input *input, int nr, int tuner, int type);

#endif
