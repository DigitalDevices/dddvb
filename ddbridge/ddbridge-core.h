// SPDX-License-Identifier: GPL-2.0
/*
 * ddbridge-core.c: Digital Devices bridge core functions
 *
 * Copyright (C) 2010-2017 Digital Devices GmbH
 *                         Marcus Metzler <mocm@metzlerbros.de>
 *                         Ralph Metzler <rjkm@metzlerbros.de>
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _DDBRIDGE_SX8_H_
#define _DDBRIDGE_SX8_H_

int ddb_dvb_usercopy(struct file *file,
		     unsigned int cmd, unsigned long arg,
		     int (*func)(struct file *file,
				 unsigned int cmd, void *arg));
#endif
