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


int tgaEncode(struct tga *t, const char *path)
{
	FILE *fp = fopen(path, "wb");

	if (!fp)
		return 1;

	fwrite(&t->header.idlen, 1, 1, fp);
	fwrite(&t->header.colormaptype, 1, 1, fp);
	fwrite(&t->header.imagetype, 1, 1, fp);
	fwrite(&t->header.colormapoff, 2, 1, fp);
	fwrite(&t->header.colormaplen, 2, 1, fp);
	fwrite(&t->header.colormapdepth, 1, 1, fp);
	fwrite(&t->header.x, 2, 1, fp);
	fwrite(&t->header.y, 2, 1, fp);
	fwrite(&t->width, 2, 1, fp);
	fwrite(&t->height, 2, 1, fp);
	fwrite(&t->depth, 1, 1, fp);
	fwrite(&t->header.imagedesc, 1, 1, fp);

	int bytes = t->depth / 8;
	char p[4];

	for (int i = 0; i < t->width * t->height; i++) {
		p[0] = ((struct pixel *)&t->data[i])->b;
		p[1] = ((struct pixel *)&t->data[i])->g;
		p[2] = ((struct pixel *)&t->data[i])->r;
		p[3] = ((struct pixel *)&t->data[i])->a;

		if (!fwrite(p, bytes, 1, fp)) {
			fprintf(stderr, "error: unexpected EOF at offset %d\n", i);
			exit(-1);
		}
	}
	return 0;
}

