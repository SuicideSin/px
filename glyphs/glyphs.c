#include <stdio.h>
#include <inttypes.h>

#include "tga.h"

int main(void)
{
	struct tga *t = tgaDecode("glyphs.tga");
	if (!t) {
		perror("fatal: couldn't decode glyphs.tga");
		return 1;
	}

	int n = t->width * t->height;

	printf("int glyphsWidth = %d;", t->width);
	printf("uint32_t glyphsData[] = {\n");

	for (int i = 0; i < n; i++) {
		printf("0x%x", t->data[i]);
		if (i < n - 1) {
			if (i % 8 == 7) {
				printf(",\n");
			} else {
				printf(", ");
			}
		}
	}
	printf(" };\n");

	return 0;
}
