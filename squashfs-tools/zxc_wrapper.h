#ifndef ZXC_WRAPPER_H
#define ZXC_WRAPPER_H

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

/* Default compression level */
#define ZXC_DEFAULT_COMPRESSION_LEVEL 3

struct zxc_comp_opts {
	int compression_level;
};
#endif
