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

static void paletteAddColor(int x, int y, struct rgba color);
static void setupPalette();

struct session     *session;
struct framebuffer *fb;
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

static void canvasResize(struct canvas *c, int w, int h)
{
	int stride = w * sizeof(struct rgba);
	c->pixels = realloc(c->pixels, h * stride);

	memset(c->pixels, 0, h * stride);

	if (c->texture != 0)
		glDeleteTextures(1, &c->texture);

	c->texture = textureGen(w, h, c->pixels);
}

static struct canvas canvas(int w, int h)
{
	struct canvas c = (struct canvas){
		.pixels       = NULL,
		.texture      = 0,
		.dirty        = false,
		.draw.prev.x  = -1,
		.draw.prev.y  = -1,
		.draw.curr.x  = -1,
		.draw.curr.y  = -1,
		.draw.drawing = false,
		.draw.color   = TRANSPARENT
	};

	canvasResize(&c, w, h);

	return c;
}

static struct sprite sprite(int w, int h)
{
	return (struct sprite){
		.fw      = w,
		.fh      = h,
		.nframes = 0,
		.frames  = NULL
	};
}

static void createFrame(int _pos)
{
	struct sprite *s = session->sprite;
	struct canvas c = canvas(s->fw, s->fh);

	s->frames = realloc(s->frames, (s->nframes + 1) * sizeof(c));
	s->frames[s->nframes] = c;
	session->canvas = &s->frames[s->nframes];
	s->nframes++;
}

static void createSprite(int w, int h)
{
	struct sprite s = sprite(w, h);

	session->sprites = realloc(session->sprites, (session->nsprites + 1) * sizeof(s));
	session->sprites[session->nsprites] = s;
	session->sprite = &session->sprites[session->nsprites];
	session->nsprites++;
}

static bool canvasWithinBoundary(struct canvas *c, int x, int y)
{
	return session->x <= x && x < (session->x + session->sprite->fw * session->zoom) &&
		session->y <= y && y < (session->y + session->sprite->fh * session->zoom);
}


static void drawCursor(GLFWwindow *win, int x, int y)
{
	if (!canvasWithinBoundary(session->canvas, x, y))
		return;

	int s = session->brush.size * session->zoom;

	int cx = x - (x % session->zoom) + 0.5;
	int cy = y - (y % session->zoom) + 0.5;

	glColor4ubv((GLubyte*)&session->fg);
	glRecti(cx, cy, cx + s, cy + s);
}

static void setPixel(uint8_t *data, int x, int y, int stride, struct rgba color)
{
	int off = pixelOffset(x, y, stride);
	*(struct rgba *)(&data[off]) = color;
}

static void canvasDraw(struct canvas *c, int x, int y)
{
	x -= session->x;
	y -= session->y;

	x /= session->zoom;
	y /= session->zoom;

	y += session->brush.size;

	y = fb->h - y;

	c->draw.prev = c->draw.curr;
	c->draw.curr.x = x;
	c->draw.curr.y = y;
	c->dirty = true;
}

static void canvasRender(struct canvas *c)
{
	if (!c->dirty)
		return;

	glColor4ubv((GLubyte*)&session->fg);

	int x = c->draw.curr.x,
		y = c->draw.curr.y,
		x1 = c->draw.prev.x,
		y1 = c->draw.prev.y,
		dx = abs(x1 - x),
		dy = abs(y1 - y),
		sx = x < x1 ? 1 : -1,
		sy = y < y1 ? 1 : -1;

	int err = dx - dy;
	int s = session->brush.size;
		
	if (c->draw.drawing > DRAW_STARTED) {
		for (;;) {
			glRecti(x, y, x + s, y + s);

			if (x == x1 && y == y1)
				break;

			int err2 = err * 2;

			if (err2 > -dy) {
				err -= dy;
				x += sx;
			}
			if (x == x1 && y == y1) {
				glRecti(x, y, x + s, y + s);
				break;
			}
			if (err2 < dx) {
				err += dx;
				y += sy;
			}
		}
	} else {
		glRecti(x, y, x + s, y + s);
	}
	c->dirty = false;
}

static void canvasStartDrawing(struct canvas *c, int x, int y)
{
	if (!canvasWithinBoundary(c, x, y))
		return;

	canvasDraw(c, x, y);
	c->draw.drawing = DRAW_STARTED;
}

static void canvasStopDrawing(struct canvas *c)
{
	c->draw.drawing = DRAW_ENDED;
}

static struct rgba sample(int x, int y)
{
	struct rgba pixel;	
	glReadPixels(x, fb->h - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
	return pixel;
}

static void setFgColor(struct rgba color)
{
	session->fg = color;
	paletteAddColor(0, 0, session->fg);
}

static void pickColor(int x, int y)
{
	setFgColor(sample(x, y));
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

static void center()
{
	int cx, cy;

	cx  = fb->w/2;
	cy  = fb->h/2;

	// TODO(cloudhead): use total width of all frames in sprite.
	cx -= session->sprite->fw/2 * session->zoom;
	cy -= session->sprite->fh/2 * session->zoom;

	session->x = cx + session->offx;
	session->y = cy + session->offy;

	session->x -= session->x % session->zoom;
	session->y -= session->y % session->zoom;
}

static void reset()
{
	session->offx = 0;
	session->offy = 0;

	center();
}

static void move(int x, int y)
{
	session->offx += x;
	session->offy += y;

	center();
}

static void fbSizeCallback(GLFWwindow *win, int w, int h)
{
	fb->w = w;
	fb->h = h;

	fbClear();
	setupPalette();
	center();
}

static void mouseButtonCallback(GLFWwindow *win, int button, int action, int mods)
{
	if (action == GLFW_PRESS) {
		double x, y;

		glfwGetCursorPos(win, &x, &y);

		if (mods & GLFW_MOD_CONTROL) {
			pickColor(round(x), round(y));
		} else {
			canvasStartDrawing(session->canvas, floor(x), floor(y));
		}
	} else if (action == GLFW_RELEASE) {
		canvasStopDrawing(session->canvas);
	}
}

static void cursorPosCallback(GLFWwindow *win, double fx, double fy)
{
	int x = floor(fx),
		y = floor(fy);

	struct canvas *c = session->canvas;

	if (c->draw.drawing == DRAW_STARTED || c->draw.drawing == DRAW_DRAWING) {
		canvasDraw(c, x, y);
		c->draw.drawing = DRAW_DRAWING;
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

static void brushSize(int n)
{
	session->brush.size += n;

	if (session->brush.size < 1)
		session->brush.size = 1;
}

static void zoom(int n)
{
	session->zoom += n;

	if (session->zoom < 1)
		session->zoom = 1;

	center();
	fbClear();
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
		case ',':             zoom(-1);         return;
		case '.':             zoom(+1);         return;
		case '[':             brushSize(-1);    return;
		case ']':             brushSize(+1);    return;
		case GLFW_KEY_LEFT:   move(-50, 0);     return;
		case GLFW_KEY_RIGHT:  move(+50, 0);     return;
		case GLFW_KEY_DOWN:   move(0, +50);     return;
		case GLFW_KEY_UP:     move(0, -50);     return;
		case GLFW_KEY_ESCAPE: windowClose(win); return;
	}
	if (mods & GLFW_MOD_CONTROL && key == 'f') {
		createFrame(-1);
	} else if (mods & GLFW_MOD_CONTROL && key == 'd') {
		// TODO: Remove frame
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

static void drawBoundaries()
{
	struct sprite *s = session->sprite;

	for (int i = 0; i < s->nframes; i++) {
		boundaryDraw(
			session->x + i * s->fw * session->zoom,
			session->y,
			s->fw * session->zoom,
			s->fh * session->zoom
		);
	}
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

	session             = malloc(sizeof(*session));
	session->sprite     = NULL;
	session->sprites    = NULL;
	session->nsprites   = 0;
	session->x          = 0;
	session->y          = 0;
	session->offx       = 0;
	session->offy       = 0;
	session->zoom       = 1;
	session->canvas     = NULL;
	session->brush.size = 1;
	session->fg         = WHITE;
	session->bg         = WHITE;
	
	createSprite(64, 64);
	createFrame(-1);

	reset();

	// Color palette
	palette = malloc(sizeof(*palette));
	palette->pixels = NULL;
	palette->size = 20;

	glGenFramebuffers(1, &fb->texture);
	glBindFramebuffer(GL_FRAMEBUFFER, fb->texture);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, session->canvas->texture, 0);

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
			struct canvas *c = session->canvas;
			struct sprite *s = session->sprite;

			glBindFramebuffer(GL_FRAMEBUFFER, fb->texture);
			canvasRender(session->canvas);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			glClear(GL_COLOR_BUFFER_BIT);
			glClearColor(0.0, 0.0, 0.0, 1.0);

			drawBoundaries();

			glTranslatef(session->x, session->y, 0.0f);
			glScalef(session->zoom, session->zoom, 1.0f);

			textureDraw(c->texture, s->fw, s->fh, 0, 0, s->fw, s->fh, 0, 0);
		}
		glPopMatrix();

		textureRefresh(palette->texture, fb->w, palette->h, palette->pixels);
		textureDraw(palette->texture, fb->w, palette->h, 0, 0, fb->w, palette->h, 0, 0);

		drawCursor(window, floor(mx), floor(my));

		glFlush();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	glDeleteFramebuffers(1, &fb->texture);
	glfwDestroyWindow(window);
	glfwTerminate();

	free(palette->pixels);
	free(session);
	free(fb);
	free(palette);

	exit(0);
}

