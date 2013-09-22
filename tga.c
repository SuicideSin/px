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

struct header {
	char  idlen;
	char  colormaptype;
	char  imagetype;
	short colormapoff;
	short colormaplen;
	char  colormapdepth;
	short x;
	short y;
	short width;
	short height;
	char  depth;
	char  imagedesc;
};

struct pixel {
	unsigned char r, g, b, a;
};

struct tga *tgaDecode(const char *path)
{
	FILE *fp = fopen(path, "rb");

	if (!fp)
		return NULL;

	struct tga *t = malloc(sizeof(*t));
	struct header h;

	fread(&h.idlen, 1, 1, fp);
	fread(&h.colormaptype, 1, 1, fp);
	fread(&h.imagetype, 1, 1, fp);
	fread(&h.colormapoff, 2, 1, fp);
	fread(&h.colormaplen, 2, 1, fp);
	fread(&h.colormapdepth, 1, 1, fp);
	fread(&h.x, 2, 1, fp);
	fread(&h.y, 2, 1, fp);
	fread(&h.width, 2, 1, fp);
	fread(&h.height, 2, 1, fp);
	fread(&h.depth, 1, 1, fp);
	fread(&h.imagedesc, 1, 1, fp);

	t->data   = malloc(sizeof(struct pixel) * h.width * h.height);
	t->width  = h.width;
	t->height = h.height;
	t->depth  = h.depth;

	int bytes = h.depth / 8;
	char p[4];

	for (int i = 0; i < h.width * h.height; i++) {
		p[3] = 0xff; // Default to 100% opaque

		if (!fread(p, bytes, 1, fp)) {
			fprintf(stderr, "error: unexpected EOF at offset %d\n", i);
			exit(-1);
		}
		*(struct pixel *)&t->data[i] = (struct pixel){p[2], p[1], p[0], p[3]};
	}
	return t;
}


int tgaEncode(struct tga *t)
{
	return 1;
}

