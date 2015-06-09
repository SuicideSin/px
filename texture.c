//
// texture.c
// texture manipulation
//
#define GL_GLEXT_PROTOTYPES

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <stdlib.h>

#include "texture.h"

void textureDraw(struct texture *t, float sx, float sy)
{
	textureDrawRect(t, 0, 0, t->w, t->h, sx, sy);
}

void textureDrawRect(
	struct texture *t,  // Texture pointer
	int   x,  int   y,  // Position in the image to draw
	int   w,  int   h,  // Width & height of the image to draw
	float sx, float sy) // Screen position to draw at
{
	float rx = (float)x / (float)t->w,
	      ry = (float)y / (float)t->h;

	float rw = (float)w / (float)t->w,
	      rh = (float)h / (float)t->h;

	glBindTexture(GL_TEXTURE_2D, t->id);

	glBegin(GL_QUADS);
	glTexCoord2f(rx,      ry);      glVertex2f(sx,     sy);
	glTexCoord2f(rx + rw, ry);      glVertex2f(sx + w, sy);
	glTexCoord2f(rx + rw, ry + rh); glVertex2f(sx + w, sy + h);
	glTexCoord2f(rx,      ry + rh); glVertex2f(sx,     sy + h);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, 0);
}

struct texture *textureGen(int w, int h, uint8_t *data)
{
	struct texture *t = malloc(sizeof(*t));

	t->w    = w;
	t->h    = h;
	t->data = data;

	glGenTextures(1, &t->id);
	glBindTexture(GL_TEXTURE_2D, t->id);

	// Don't interpolate when resizing
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Repeat texture
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Allow blending
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glTexImage2D(GL_TEXTURE_2D,
				 0,
				 GL_RGBA,
				 (GLsizei)w, (GLsizei)h,
				 0,
				 GL_RGBA,
				 GL_UNSIGNED_BYTE,
				 data);

	glBindTexture(GL_TEXTURE_2D, 0);

	return t;
}

GLuint fbGen()
{
	GLuint id;
	glGenFramebuffers(1, &id);

	return id;
}

void textureRefresh(GLuint id, int w, int h, uint8_t *data)
{
	glBindTexture(GL_TEXTURE_2D, id);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)w, (GLsizei)h, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, 0);
}

