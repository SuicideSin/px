/*
 *
 * libtga
 * tga.h
 *
 */
struct tga {
	int      width;
	int      height;
	char     depth;
	uint32_t *data;
};

struct tga *tgaDecode(const char *path);
int         tgaEncode(struct tga *t);
