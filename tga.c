/*
 *
 * libtga
 * tga.c
 *
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "tga.h"

struct pixel {
	unsigned char r, g, b, a;
};

struct tga *tgaDecode(const char *path)
{
	FILE *fp = fopen(path, "rb");

	if (!fp)
		return NULL;

	struct tga *t = malloc(sizeof(*t));

	fread(&t->header.idlen, 1, 1, fp);
	fread(&t->header.colormaptype, 1, 1, fp);
	fread(&t->header.imagetype, 1, 1, fp);
	fread(&t->header.colormapoff, 2, 1, fp);
	fread(&t->header.colormaplen, 2, 1, fp);
	fread(&t->header.colormapdepth, 1, 1, fp);
	fread(&t->header.x, 2, 1, fp);
	fread(&t->header.y, 2, 1, fp);
	fread(&t->width, 2, 1, fp);
	fread(&t->height, 2, 1, fp);
	fread(&t->depth, 1, 1, fp);
	fread(&t->header.imagedesc, 1, 1, fp);

	t->data = malloc(sizeof(struct pixel) * t->width * t->height);

	int bytes = t->depth / 8;
	char p[4];

	for (int i = 0; i < t->width * t->height; i++) {
		p[3] = 0xff; // Default to 100% opaque

		if (!fread(p, bytes, 1, fp)) {
			fprintf(stderr, "error: unexpected EOF at offset %d\n", i);
			exit(-1);
		}
		*(struct pixel *)&t->data[i] = (struct pixel){p[2], p[1], p[0], p[3]};
	}
	return t;
}


int tgaEncode(uint32_t *pixels, short w, short h, char depth, const char *path)
{
	FILE *fp = fopen(path, "wb");

	if (!fp)
		return 1;

	short null = 0x0;

	fputc(0, fp); // ID length
	fputc(0, fp); // No color map
	fputc(2, fp); // Uncompressed 32-bit RGBA

	fwrite(&null, 2, 1, fp);  // Color map offset
	fwrite(&null, 2, 1, fp);  // Color map length
	fwrite(&null, 1, 1, fp);  // Color map entry size
	fwrite(&null, 2, 1, fp);  // X
	fwrite(&null, 2, 1, fp);  // Y
	fwrite(&w, 2, 1, fp);     // Width
	fwrite(&h, 2, 1, fp);     // Height
	fwrite(&depth, 1, 1, fp); // Depth
	fwrite(&null, 1, 1, fp);  // Image descriptor

	int bytes = depth / 8;
	char p[4];

	for (int i = 0; i < w * h; i++) {
		p[0] = ((struct pixel *)&pixels[i])->b;
		p[1] = ((struct pixel *)&pixels[i])->g;
		p[2] = ((struct pixel *)&pixels[i])->r;
		p[3] = ((struct pixel *)&pixels[i])->a;

		if (!fwrite(p, bytes, 1, fp)) {
			fprintf(stderr, "error: unexpected EOF at offset %d\n", i);
			exit(-1);
		}
	}
	return 0;
}

