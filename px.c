#define GL_GLEXT_PROTOTYPES

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "texture.h"
#include "px.h"

#define rgba(r, g, b, a) ((struct rgba){r, g, b, a})
#define WHITE            rgba(255, 255, 255, 255)
#define TRANSPARENT      rgba(0, 0, 0, 0)

static void canvasSetPixel(int x, int y, struct rgba color);
static void paletteAddColor(int x, int y, struct rgba color);
static void setupPalette();

struct framebuffer *fb;
struct canvas      *c;
struct palette     *palette;

static void fbClear()
{
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0.0, 0.0, 0.0, 1.0);
}

static void errorCallback(int error, const char* description)
{
	fputs(description, stderr);
}

static int pixelOffset(int x, int y, int stride)
{
	return (y - 0) * stride + (x - 0) * sizeof(struct rgba);
}

static bool canvasWithinBoundary(int x, int y)
{
	return c->x <= x && x < (c->x + c->w * c->zoom) &&
		c->y <= y && y < (c->y + c->h * c->zoom);
}

static void canvasDrawCursor(GLFWwindow *win, int x, int y)
{
	if (!canvasWithinBoundary(x, y))
		return;

	int s = c->brush.size * c->zoom;

	int cx = x - (x % c->zoom) + 0.5;
	int cy = y - (y % c->zoom) + 0.5;

	glColor4ubv((GLubyte*)&c->fg);
	glRecti(cx, cy, cx + s, cy + s);
}

static void setPixel(uint8_t *data, int x, int y, int stride, struct rgba color)
{
	int off = pixelOffset(x, y, stride);
	*(struct rgba *)(&data[off]) = color;
}

static void canvasDraw(int x, int y)
{
	if (canvasWithinBoundary(x, y)) {
		canvasSetPixel(x, y, c->fg);
	}
}

static struct rgba sample(int x, int y)
{
	struct rgba pixel;	
	glReadPixels(x, fb->h - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
	return pixel;
}

static void setFgColor(struct rgba color)
{
	c->fg = color;
	paletteAddColor(0, 0, c->fg);
}

static void canvasPickColor(int x, int y)
{
	setFgColor(sample(x, y));
}

static void canvasSetPixel(int x, int y, struct rgba color)
{
	c->draw.x = x;
	c->draw.y = y;
	c->draw.color = color;
	c->dirty = true;
}

static void drawPixel(int x, int y, struct rgba color)
{
	if (!c->dirty)
		return;

	x -= c->x;
	y -= c->y;

	x /= c->zoom;
	y /= c->zoom;

	y += c->brush.size;

	y = fb->h - y;

	glColor4ubv((GLubyte*)&color);
	glRecti(x, y, x + c->brush.size, y + c->brush.size);

	c->dirty = false;
}

static void paletteSetPixel(int x, int y, struct rgba color)
{
	setPixel(palette->pixels, x, y, fb->w * sizeof(color), color);
}

static void paletteAddColor(int x, int y, struct rgba color)
{
	for (int i = x; i < x + palette->size; i++) {
		for (int j = y; j < y + palette->size; j++) {
			paletteSetPixel(i, j, color);
		}
	}
}

static void canvasCenter(struct canvas *c)
{
	int cx, cy;

	cx  = fb->w/2;
	cy  = fb->h/2;
	cx -= c->w/2 * c->zoom;
	cy -= c->h/2 * c->zoom;

	c->x = cx + c->offx;
	c->y = cy + c->offy;

	c->x -= c->x % c->zoom;
	c->y -= c->y % c->zoom;
}

static void canvasReset(struct canvas *c)
{
	c->offx = 0;
	c->offy = 0;

	canvasCenter(c);
}

static void canvasMove(struct canvas *c, int x, int y)
{
	c->offx += x;
	c->offy += y;

	canvasCenter(c);
}

static void fbSizeCallback(GLFWwindow *win, int w, int h)
{
	fb->w = w;
	fb->h = h;

	fbClear();
	setupPalette();
	canvasCenter(c);
}

static void mouseButtonCallback(GLFWwindow *win, int button, int action, int mods)
{
	if (action == GLFW_PRESS) {
		double x, y;

		glfwGetCursorPos(win, &x, &y);

		if (mods & GLFW_MOD_CONTROL) {
			canvasPickColor(round(x), round(y));
		} else {
			canvasDraw(floor(x), floor(y));
		}
	}
}

static void cursorPosCallback(GLFWwindow *win, double fx, double fy)
{
	int action = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_1);

	int x = floor(fx),
		y = floor(fy);

	if (action == GLFW_PRESS) {
		canvasDraw(x, y);
	}
}

static void setupPalette()
{
	int x = 0, y = 0;
	int s = palette->size;

	palette->h = (255 / (fb->w / s)) * s + s;
	palette->pixels = realloc(palette->pixels, fb->w * palette->h * sizeof(struct rgba));
	memset(palette->pixels, 0, fb->w * palette->h * sizeof(struct rgba));

	palette->texture = textureGen(fb->w, palette->h, palette->pixels);
	glBindTexture(GL_TEXTURE_2D, palette->texture);

	for (int r = 0; r <= 255; r += 51) {
		for (int g = 0; g <= 255; g += 51) {
			for (int b = 0; b <= 255; b += 51) {
				paletteAddColor(x, y, rgba(r, g, b, 255));

				if (x + s > fb->w) {
					y += s;
					x = 0;
				} else {
					x += s;
				}
			}
		}
	}
	glBindTexture(GL_TEXTURE_2D, 0);
}

static void canvasBrushSize(int n)
{
	c->brush.size += n;

	if (c->brush.size < 1)
		c->brush.size = 1;
}

static void canvasZoom(struct canvas *c, int n)
{
	c->zoom += n;

	if (c->zoom < 1)
		c->zoom = 1;

	canvasCenter(c);
	fbClear();
}

static void canvasResize(struct canvas *c, int w, int h)
{
	c->w = w;
	c->h = h;
	c->stride = c->w * sizeof(struct rgba);
	c->pixels = realloc(c->pixels, c->h * c->stride);

	memset(c->pixels, 0, c->h * c->stride);

	if (c->texture != 0)
		glDeleteTextures(1, &c->texture);

	c->texture = textureGen(c->w, c->h, c->pixels);
}

static void windowClose(GLFWwindow *win)
{
	glfwSetWindowShouldClose(win, GL_TRUE);
}

static void keyCallback(GLFWwindow *win, int key, int scancode, int action, int mods)
{
	if (action != GLFW_PRESS)
		return;

	switch (key) {
		case ',':             canvasZoom(c, -1);        break;
		case '.':             canvasZoom(c, +1);        break;
		case '[':             canvasBrushSize(-1);      break;
		case ']':             canvasBrushSize(+1);      break;
		case GLFW_KEY_LEFT:   canvasMove(c, -50,   0);  break;
		case GLFW_KEY_RIGHT:  canvasMove(c, +50,   0);  break;
		case GLFW_KEY_DOWN:   canvasMove(c,   0, +50);  break;
		case GLFW_KEY_UP:     canvasMove(c,   0, -50);  break;
		case GLFW_KEY_ESCAPE: windowClose(win);         break;
	}
}

static void boundaryDraw(int x, int y, int w, int h)
{
	int offx = x, offy = y;

	glColor3f(1.0, 1.0, 1.0);
	glBegin(GL_LINE_LOOP);
	glVertex3f(offx,     offy, 0);
	glVertex3f(offx + w, offy, 0);
	glVertex3f(offx + w, offy + h, 0);
	glVertex3f(offx,     offy + h, 0);
	glEnd();
}

static struct canvas *canvas(int w, int h)
{
	struct canvas *c = malloc(sizeof(*c));

	c->pixels     = NULL;
	c->texture    = 0;
	c->zoom       = 1;
	c->x          = 0;
	c->y          = 0;
	c->w          = w;
	c->h          = h;
	c->brush.size = 1;
	c->dirty      = false;
	c->draw.x     = -1;
	c->draw.y     = -1;
	c->draw.color = TRANSPARENT;

	canvasReset(c);
	canvasResize(c, w, h);

	return c;
}

int main(void)
{
	GLFWwindow* window;
	glfwSetErrorCallback(errorCallback);

	if (!glfwInit())
		exit(1);

	window = glfwCreateWindow(640, 480, "px", NULL, NULL);
	if (!window) {
		glfwTerminate();
		exit(1);
	}
	glfwMakeContextCurrent(window);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetCursorPosCallback(window, cursorPosCallback);
	glfwSetFramebufferSizeCallback(window, fbSizeCallback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

	// Framebuffer
	fb = malloc(sizeof(*fb));
	fb->w = 640;
	fb->h = 480;

	// Color palette
	palette = malloc(sizeof(*palette));
	palette->pixels = NULL;
	palette->size = 20;

	// Canvas
	c = canvas(256, 256);

	glGenFramebuffers(1, &fb->texture);
	glBindFramebuffer(GL_FRAMEBUFFER, fb->texture);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, c->texture, 0);

	GLenum status;

	if ((status = glCheckFramebufferStatus(GL_FRAMEBUFFER)) != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "glCheckFramebufferStatus: error %u", status);
		exit(1);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	fbClear();
	setupPalette();
	setFgColor(WHITE);

	while (!glfwWindowShouldClose(window)) {
		double mx, my; 

		glfwGetCursorPos(window, &mx, &my);

		glViewport(0, 0, fb->w, fb->h);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0.0, fb->w, fb->h, 0.0, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);

		glLoadIdentity();
		glTranslatef(0, 0, 0);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_TEXTURE_2D);

		glPushMatrix(); {
			glBindFramebuffer(GL_FRAMEBUFFER, fb->texture);
			drawPixel(c->draw.x, c->draw.y, c->draw.color);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			glClear(GL_COLOR_BUFFER_BIT);
			glClearColor(0.0, 0.0, 0.0, 1.0);

			boundaryDraw(c->x, c->y, c->w * c->zoom, c->h * c->zoom);

			glTranslatef(c->x, c->y, 0.0f);
			glScalef(c->zoom, c->zoom, 1.0f);

			textureDraw(c->texture, c->w, c->h, 0, 0, c->w, c->h, 0, 0);
		}
		glPopMatrix();

		textureRefresh(palette->texture, fb->w, palette->h, palette->pixels);
		textureDraw(palette->texture, fb->w, palette->h, 0, 0, fb->w, palette->h, 0, 0);

		canvasDrawCursor(window, floor(mx), floor(my));

		glFlush();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	glDeleteFramebuffers(1, &fb->texture);
	glfwDestroyWindow(window);
	glfwTerminate();

	free(c->pixels);
	free(palette->pixels);
	free(c);
	free(fb);
	free(palette);

	exit(0);
}

