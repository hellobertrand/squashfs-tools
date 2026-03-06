#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zxc.h>

#include "squashfs_fs.h"
#include "zxc_wrapper.h"
#include "compressor.h"
#include "print_pager.h"

static int compression_level = ZXC_DEFAULT_COMPRESSION_LEVEL;

static int zxc_options(char *argv[], int argc)
{
	if (strcmp(argv[0], "-Xcompression-level") == 0) {
		if (argc < 2) {
			fprintf(stderr, "zxc: -Xcompression-level missing "
				"compression level\n");
			fprintf(stderr, "zxc: -Xcompression-level it should "
				"be 1 <= n <= 5\n");
			goto failed;
		}

		compression_level = atoi(argv[1]);
		if (compression_level < 1 || compression_level > 5) {
			fprintf(stderr, "zxc: -Xcompression-level invalid, it "
				"should be 1 <= n <= 5\n");
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

	if (compression_level == ZXC_DEFAULT_COMPRESSION_LEVEL)
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
		compression_level = ZXC_DEFAULT_COMPRESSION_LEVEL;
		return 0;
	}

	if (size < sizeof(*comp_opts))
		goto failed;

	SQUASHFS_INSWAP_COMP_OPTS(comp_opts);

	if (comp_opts->compression_level < 1 ||
	    comp_opts->compression_level > 5) {
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

	if (comp_opts->compression_level < 1 ||
	    comp_opts->compression_level > 5) {
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
	/* ZXC buffer API does not require persistent context across blocks */
	*strm = NULL;
	return 0;
}

static int zxc_squashfs_compress(void *strm, void *dest, void *src, int size,
			 int block_size, int *error)
{
	int64_t res = zxc_compress(src, size, dest, block_size,
				   compression_level, 1);

	if (res < 0) {
		/* Return 0 to indicate failure to compress (e.g. incompressible) */
		return 0;
	}

	return (int)res;
}

static int zxc_squashfs_uncompress(void *dest, void *src, int size, int outsize,
			   int *error)
{
	int64_t res = zxc_decompress(src, size, dest, outsize, 1);

	if (res < 0) {
		fprintf(stderr, "\t%d %d zxc error %d\n", outsize, size, (int)res);
		*error = (int)res;
		return -1;
	}

	return (int)res;
}

static void zxc_usage(FILE *stream, int cols)
{
	autowrap_print(stream, "\t  -Xcompression-level <compression-level>\n", cols);
	autowrap_printf(stream, cols, "\t\t<compression-level> should be 1 .. 5 (default %d).\n", ZXC_DEFAULT_COMPRESSION_LEVEL);
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
