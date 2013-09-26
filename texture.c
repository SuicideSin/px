//
// texture.c
// texture manipulation
//
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

void textureDraw(id, tw, th, x, y, w, h, sx, sy)
	GLuint id;           // Texture id
	int    tw; int   th; // Width & height of the texture
	int    x;  int   y;  // Position in the image to draw
	int    w;  int   h;  // Width & height of the image to draw
	float  sx; float sy; // Screen position to draw at
{
	float rx = (float)x / (float)tw,
		  ry = (float)y / (float)th;

	float rw = (float)w / (float)tw,
		  rh = (float)h / (float)th;

	glBindTexture(GL_TEXTURE_2D, id);

	glBegin(GL_QUADS);
	glTexCoord2f(rx,      ry);      glVertex2f(sx,     sy);
	glTexCoord2f(rx + rw, ry);      glVertex2f(sx + w, sy);
	glTexCoord2f(rx + rw, ry + rh); glVertex2f(sx + w, sy + h);
	glTexCoord2f(rx,      ry + rh); glVertex2f(sx,     sy + h);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint textureGen(int w, int h, uint8_t *data)
{
	GLuint id;

	glGenTextures(1, &id);

	glBindTexture(GL_TEXTURE_2D, id);

	// Don't interpolate when resizing
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Repeat texture
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Replace what was previously on screen
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glTexImage2D(GL_TEXTURE_2D,
				 0,
				 GL_RGBA,
				 (GLsizei)w, (GLsizei)h,
				 0,
				 GL_RGBA,
				 GL_UNSIGNED_BYTE,
				 data);

	glBindTexture(GL_TEXTURE_2D, 0);

	return id;
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

