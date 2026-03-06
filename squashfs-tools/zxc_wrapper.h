#ifndef ZXC_WRAPPER_H
#define ZXC_WRAPPER_H
/*
 * Squashfs
 *
 * Copyright (c) 2025-2026
 * Bertrand Lebonnois
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * zxc_wrapper.h
 *
 */
#include "endian_compat.h"

#if __BYTE_ORDER == __BIG_ENDIAN
extern unsigned int inswap_le16(unsigned short);
extern unsigned int inswap_le32(unsigned int);

#define SQUASHFS_INSWAP_COMP_OPTS(s) { \
	(s)->compression_level = inswap_le32((s)->compression_level); \
}
#else
#define SQUASHFS_INSWAP_COMP_OPTS(s)
#endif

struct zxc_comp_opts {
	int compression_level;
};
#endif
