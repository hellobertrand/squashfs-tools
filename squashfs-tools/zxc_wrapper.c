/*
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
 * zxc_wrapper.c
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zxc.h>
#include <pthread.h>

#include "squashfs_fs.h"
#include "zxc_wrapper.h"
#include "compressor.h"
#include "print_pager.h"

static int compression_level = ZXC_LEVEL_DEFAULT;

/* ========================================================================= */
/*  Compressor option callbacks                                              */
/* ========================================================================= */

static int zxc_options(char *argv[], int argc)
{
	if (strcmp(argv[0], "-Xcompression-level") == 0) {
		if (argc < 2) {
			fprintf(stderr, "zxc: -Xcompression-level missing "
				"compression level\n");
			fprintf(stderr, "zxc: -Xcompression-level it should "
				"be %d <= n <= %d\n",
				zxc_min_level(), zxc_max_level());
			goto failed;
		}

		compression_level = atoi(argv[1]);
		if (compression_level < zxc_min_level() ||
		    compression_level > zxc_max_level()) {
			fprintf(stderr, "zxc: -Xcompression-level invalid, it "
				"should be %d <= n <= %d\n",
				zxc_min_level(), zxc_max_level());
			goto failed;
		}
		return 1;
	}

	return -1;
failed:
	return -2;
}

static void *zxc_dump_options(int block_size, int *size)
{
	static struct zxc_comp_opts comp_opts;

	if (compression_level == ZXC_LEVEL_DEFAULT)
		return NULL;

	comp_opts.compression_level = compression_level;
	SQUASHFS_INSWAP_COMP_OPTS(&comp_opts);

	*size = sizeof(comp_opts);
	return &comp_opts;
}

static int zxc_extract_options(int block_size, void *buffer, int size)
{
	struct zxc_comp_opts *comp_opts = buffer;

	if (size == 0) {
		compression_level = ZXC_LEVEL_DEFAULT;
		return 0;
	}

	if (size < sizeof(*comp_opts))
		goto failed;

	SQUASHFS_INSWAP_COMP_OPTS(comp_opts);

	if (comp_opts->compression_level < zxc_min_level() ||
	    comp_opts->compression_level > zxc_max_level()) {
		fprintf(stderr, "zxc: bad compression level in compression "
			"options structure\n");
		goto failed;
	}

	compression_level = comp_opts->compression_level;

	return 0;

failed:
	fprintf(stderr, "zxc: error reading stored compressor options from "
		"filesystem!\n");

	return -1;
}

static void zxc_display_options(void *buffer, int size)
{
	struct zxc_comp_opts *comp_opts = buffer;

	if (size < sizeof(*comp_opts))
		goto failed;

	SQUASHFS_INSWAP_COMP_OPTS(comp_opts);

	if (comp_opts->compression_level < zxc_min_level() ||
	    comp_opts->compression_level > zxc_max_level()) {
		fprintf(stderr, "zxc: bad compression level in compression "
			"options structure\n");
		goto failed;
	}

	printf("\tcompression-level %d\n", comp_opts->compression_level);

	return;

failed:
	fprintf(stderr, "zxc: error reading stored compressor options from "
		"filesystem!\n");
}

static int zxc_squashfs_init(void **strm, int block_size, int datablock)
{
	*strm = NULL;
	return 0;
}

/* ========================================================================= */
/*  Thread-local reusable contexts                                           */
/* ========================================================================= */

static pthread_key_t zxc_tls_comp_key;
static pthread_key_t zxc_tls_decomp_key;
static pthread_once_t zxc_tls_once = PTHREAD_ONCE_INIT;

static void zxc_comp_tls_destructor(void *data)
{
	if (data)
		zxc_free_cctx(data);
}

static void zxc_decomp_tls_destructor(void *data)
{
	if (data)
		zxc_free_dctx(data);
}

static void zxc_tls_init(void)
{
	pthread_key_create(&zxc_tls_comp_key, zxc_comp_tls_destructor);
	pthread_key_create(&zxc_tls_decomp_key, zxc_decomp_tls_destructor);
}

/* ========================================================================= */
/*  Compression                                                              */
/* ========================================================================= */

static int zxc_squashfs_compress(void *strm, void *dest, void *src, int size,
			 int block_size, int *error)
{
	zxc_cctx *ctx;
	zxc_compress_opts_t opts;
	int64_t res;

	pthread_once(&zxc_tls_once, zxc_tls_init);

	memset(&opts, 0, sizeof(opts));
	opts.level = compression_level;
	opts.block_size = block_size;
	opts.checksum_enabled = 0;

	ctx = pthread_getspecific(zxc_tls_comp_key);
	if (!ctx) {
		ctx = zxc_create_cctx(&opts);
		if (!ctx)
			return 0;
		pthread_setspecific(zxc_tls_comp_key, ctx);
	}

	res = zxc_compress_block(ctx, src, size, dest, block_size, &opts);
	if (res <= 0)
		return 0;

	return (int)res;
}

/* ========================================================================= */
/*  Decompression                                                            */
/* ========================================================================= */

static int zxc_squashfs_uncompress(void *dest, void *src, int size, int outsize,
			   int *error)
{
	zxc_dctx *ctx;
	int64_t res;

	pthread_once(&zxc_tls_once, zxc_tls_init);

	ctx = pthread_getspecific(zxc_tls_decomp_key);
	if (!ctx) {
		ctx = zxc_create_dctx();
		if (!ctx) {
			*error = -1;
			return -1;
		}
		pthread_setspecific(zxc_tls_decomp_key, ctx);
	}

	res = zxc_decompress_block_safe(ctx, src, size, dest, outsize, NULL);
	if (res < 0) {
		fprintf(stderr, "\t%d %d zxc error %lld\n", outsize, size,
			(long long)res);
		*error = (int)res;
		return -1;
	}

	return (int)res;
}

/* ========================================================================= */
/*  Usage / help                                                             */
/* ========================================================================= */

static void zxc_usage(FILE *stream, int cols)
{
	autowrap_print(stream, "\t  -Xcompression-level <compression-level>\n", cols);
	autowrap_printf(stream, cols, "\t\t<compression-level> should be %d .. %d (default %d).\n",
		zxc_min_level(), zxc_max_level(), zxc_default_level());
}

static int option_args(char *option)
{
	if(strcmp(option, "-Xcompression-level") == 0)
		return 1;

	return 0;
}

struct compressor zxc_comp_ops = {
	.init = zxc_squashfs_init,
	.compress = zxc_squashfs_compress,
	.uncompress = zxc_squashfs_uncompress,
	.options = zxc_options,
	.dump_options = zxc_dump_options,
	.extract_options = zxc_extract_options,
	.display_options = zxc_display_options,
	.usage = zxc_usage,
	.option_args = option_args,
	.id = ZXC_COMPRESSION,
	.name = "zxc",
	.supported = 1
};
