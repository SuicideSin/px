//
// texture.h
//
struct texture {
	GLuint  id;
	int     w;
	int     h;
	uint8_t *data;
};

struct texture *textureGen(int, int, uint8_t*);
void            textureRefresh(unsigned int, int, int, uint8_t*);
void            textureDraw(struct texture *, float, float);
void            textureDrawRect(struct texture *, int, int, int, int, float, float);
GLuint          fbGen();
