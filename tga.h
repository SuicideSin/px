/*
 *
 * libtga
 * tga.h
 *
 */
struct tga {
	struct {
		char  idlen;
		char  colormaptype;
		char  imagetype;
		short colormapoff;
		short colormaplen;
		char  colormapdepth;
		short x;
		short y;
		char  imagedesc;
	} header;

	short    width;
	short    height;
	char     depth;
	uint32_t *data;
};

struct tga *tgaDecode(const char *path);
int         tgaEncode(uint32_t *data, short w, short h, char depth, const char *path);
