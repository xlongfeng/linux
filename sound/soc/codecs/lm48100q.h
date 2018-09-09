/*
 * LM48100Q ALSA SoC Audio driver
 *
 * Copyright (c) 2017, longfeng.xiao <xlongfeng@126.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _LM48100Q_H
#define _LM48100Q_H

/*
 * Registers addresses
 */
#define LM48100Q_MODE_CONTROL			0x00
#define LM48100Q_DIAGNOSTIC_CONTROL		0x01
#define LM48100Q_FAULT_DETECTION_CONTROL	0x02
#define LM48100Q_VOLUME_1_CONTROL		0x03
#define LM48100Q_VOLUME_2_CONTROL		0x04
#define LM48100Q_MAX_REG_OFFSET			0x04


/*
 * Field Definitions.
 */

/*
 * LM48100Q_MODE_CONTROL
 */
#define LM48100Q_POWER_ON_MASK			0x10
#define LM48100Q_POWER_ON			0x10
#define LM48100Q_INPUT_1_MASK			0x04
#define LM48100Q_INPUT_1			0x04
#define LM48100Q_INPUT_2_MASK			0x08
#define LM48100Q_INPUT_2			0x08

#endif
