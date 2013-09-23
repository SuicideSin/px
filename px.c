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
#include <errno.h>
#include <time.h>

#include "texture.h"
#include "px.h"
#include "tga.h"

#define PX_NAME "px"
#define PX_MAX_LOG_SIZE 128

#define rgba(r, g, b, a) ((struct rgba){r, g, b, a})
#define WHITE            rgba(255, 255, 255, 255)
#define GREY             rgba(128, 128, 128, 255)
#define DARKGREY         rgba(64, 64, 64, 255)
#define TRANSPARENT      rgba(0, 0, 0, 0)

static struct rgba *spriteReadPixels(struct sprite *s);
static void paletteAddColor(int x, int y, struct rgba color);
static void boundaryDraw(struct rgba color, int x, int y, int w, int h);
static void setupPalette();

struct session     *session;
struct palette     *palette;

static void debug(const char *str, ...)
{
	char msg[PX_MAX_LOG_SIZE];
	va_list ap;

	va_start(ap, str);
	vsnprintf(msg, sizeof(msg), str, ap);
	va_end(ap);

	fprintf(stderr, "%s: %s\n", PX_NAME, msg);
}

static void fatal(const char *err, ...)
{
	char msg[PX_MAX_LOG_SIZE];
	va_list ap;

	va_start(ap, err);
	vsnprintf(msg, sizeof(msg), err, ap);
	va_end(ap);

	fprintf(stderr, "%s: fatal: %s\n", PX_NAME, msg);

	exit(EXIT_FAILURE);
}

static void fbClear()
{
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0.0, 0.0, 0.0, 1.0);
}

static void fbAttach(GLuint fb, GLuint tex)
{
	glBindFramebuffer(GL_FRAMEBUFFER, fb);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

	GLenum status;

	if ((status = glCheckFramebufferStatus(GL_FRAMEBUFFER)) != GL_FRAMEBUFFER_COMPLETE) {
		fatal("glCheckFramebufferStatus: error %u", status);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void errorCallback(int error, const char* description)
{
	fputs(description, stderr);
}

static int pixelOffset(int x, int y, int stride)
{
	return y * stride + x * sizeof(struct rgba);
}

static void spriteSnapshot(struct sprite *s)
{
	struct rgba *pixels = spriteReadPixels(s);

	if (s->snapshot < s->nsnapshots - 1) {
		for (int i = s->snapshot; i < s->nsnapshots; i++) {
			free(s->snapshots[i]);
		}
		s->nsnapshots = s->snapshot + 1;
	}
	s->snapshots = realloc(s->snapshots, (s->nsnapshots + 1) * sizeof(*s->snapshots));
	s->snapshots[s->nsnapshots] = pixels;
	s->nsnapshots++;
	s->snapshot++;
}

static void spriteFlash(struct sprite *s)
{
	glBindFramebuffer(GL_FRAMEBUFFER, s->fb);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0.75, 0.0, 0.0, 1.0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void spriteRestoreSnapshot(struct sprite *s, int snapshot)
{
	s->snapshot = snapshot;
	
	glBindFramebuffer(GL_FRAMEBUFFER, s->fb);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glDrawPixels(s->fw * s->nframes, s->fh, GL_RGBA, GL_UNSIGNED_BYTE, s->snapshots[snapshot]);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void spriteRedo(struct sprite *s)
{
	if (s->snapshot == s->nsnapshots - 1) { // Max redos reached
		spriteFlash(s);
		return;
	}
	spriteRestoreSnapshot(s, s->snapshot + 1);
}

static void spriteUndo(struct sprite *s)
{
	if (s->snapshot == 0) { // Max undos reached
		spriteFlash(s);
		return;
	}
	spriteRestoreSnapshot(s, s->snapshot - 1);
}

static void undo()
{
	spriteUndo(session->sprite);
}

static void redo()
{
	spriteRedo(session->sprite);
}

static void spriteResizeFrame(struct sprite *s, int w, int h)
{
	w *= s->nframes;

	int stride = w * sizeof(struct rgba);
	s->pixels = realloc(s->pixels, h * stride);

	memset(s->pixels, 0, h * stride);

	if (s->texture != 0)
		glDeleteTextures(1, &s->texture);

	s->texture = textureGen(w, h, s->pixels);
}

static struct sprite sprite(int fw, int fh, uint8_t *pixels, int start, int end)
{
	struct sprite s = (struct sprite){
		.pixels       = pixels,
		.texture      = 0,
		.fb           = 0,
		.dirty        = false,
		.draw.prev.x  = -1,
		.draw.prev.y  = -1,
		.draw.curr.x  = -1,
		.draw.curr.y  = -1,
		.draw.drawing = false,
		.draw.color   = TRANSPARENT,
		.fw           = fw,
		.fh           = fh,
		.nframes      = 0,
		.snapshot     = -1,
		.snapshots    = NULL,
		.nsnapshots   = 0
	};
	glGenFramebuffers(1, &s.fb);

	if (!pixels) {
		spriteResizeFrame(&s, end - start, fh);
	} else {
		s.nframes = (end - start) / fw;
		s.texture = textureGen(end - start, fh, s.pixels);
		fbAttach(s.fb, s.texture);
	}

	return s;
}

static struct rgba *spriteReadPixels(struct sprite *s)
{
	struct rgba *tmp = malloc(sizeof(struct rgba) * s->fw * s->nframes * s->fh);

	glBindFramebuffer(GL_FRAMEBUFFER, s->fb);
	glReadPixels(0, 0, s->fw * s->nframes, s->fh, GL_RGBA, GL_UNSIGNED_BYTE, tmp);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return tmp;
}

static void createFrame(int _pos)
{
	struct sprite *s = session->sprite;
	struct rgba *tmp = spriteReadPixels(s); // Create copy of framebuffer pixels

	int w = s->fw * (s->nframes + 1);
	int stride = w * sizeof(struct rgba);

	if ((s->pixels = realloc(s->pixels, s->fh * stride)) == NULL) {
		fatal("couldn't allocate memory");
	}
	memset(s->pixels, 0, s->fh * stride);

	if (s->texture)
		glDeleteTextures(1, &s->texture);

	s->texture = textureGen(w, s->fh, s->pixels);
	fbAttach(s->fb, s->texture);

	glBindFramebuffer(GL_FRAMEBUFFER, s->fb);
	glDrawPixels(s->fw * s->nframes, s->fh, GL_RGBA, GL_UNSIGNED_BYTE, tmp);
	glBlitFramebuffer(
		s->fw * (s->nframes - 1), 0,       // Source x0, y0
		s->fw * s->nframes,       s->fh,   // Source x1, y1
		s->fw * s->nframes,       0,       // Destination x0, y0
		s->fw * (s->nframes + 1), s->fh,   // Destination x1, y1
		GL_COLOR_BUFFER_BIT, GL_NEAREST
	);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	s->nframes++;

	free(tmp);
}

static void addSprite(struct sprite s)
{
	session->sprites = realloc(session->sprites, (session->nsprites + 1) * sizeof(s));
	session->sprites[session->nsprites] = s;
	session->sprite = &session->sprites[session->nsprites];
	session->nsprites++;
}

static bool loadSprites(char *path)
{
	struct tga *t;
	struct sprite s;

	session->filepath = path;

	if ((t = tgaDecode(path)) == NULL) {
		if (errno == ENOENT) {
			return false;
		} else {
			fatal(" couldn't load image '%s'", path);
		}
	}
	s = sprite(t->height, t->height, (uint8_t *)t->data, 0, t->width);
	s.image = t;

	debug("loading image '%s' (%dx%dx%d)\n", path, t->width, t->height, t->depth);

	addSprite(s);

	return true;
}

static bool spriteWithinBoundary(struct sprite *s, int x, int y)
{
	return session->x <= x && x < (session->x + s->fw * s->nframes * session->zoom) &&
		session->y <= y && y < (session->y + s->fh * session->zoom);
}

static void drawCursor(GLFWwindow *win, int x, int y, enum cursor c)
{
	if (!spriteWithinBoundary(session->sprite, x, y))
		return;

	int s = session->brush.size * session->zoom;

	int cx = x - (x % session->zoom) + 0.5;
	int cy = y - (y % session->zoom) + 0.5;

	struct sprite *sp = session->sprite;

	switch (c) {
	case CURSOR_DEFAULT:
		glColor4ubv((GLubyte*)&session->fg);
		glRecti(cx, cy, cx + s, cy + s);
		break;
	case CURSOR_SAMPLER:
		boundaryDraw(WHITE, cx, cy, s, s);
		break;
	case CURSOR_MULTI:
		for (int i = 0; i < sp->nframes; i++) {
			glColor4ubv((GLubyte*)&session->fg);
			glRecti(cx + i * sp->fw * session->zoom,
					cy,
					cx + i * sp->fw * session->zoom + s,
					cy + s);
		}
		break;
	}
}

static void setPixel(uint8_t *data, int x, int y, int stride, struct rgba color)
{
	int off = pixelOffset(x, y, stride);
	*(struct rgba *)(&data[off]) = color;
}

static void spriteDraw(struct sprite *s, int x, int y)
{
	x -= session->x;
	y -= session->y;

	x /= session->zoom;
	y /= session->zoom;

	y += session->brush.size;

	y = session->h - y;

	s->draw.prev = s->draw.curr;
	s->draw.curr.x = x;
	s->draw.curr.y = y;
	s->dirty = true;
}

static void spritePaint(struct sprite *s, int x, int y, int x1, int y1)
{
	int size = session->brush.size;

	if (s->draw.drawing > DRAW_STARTED) {
		int dx = abs(x1 - x);
		int dy = abs(y1 - y);
		int sx = x < x1 ? 1 : -1;
		int sy = y < y1 ? 1 : -1;
		int err = dx - dy;

		for (;;) {
			glRecti(x, y, x + size, y + size);

			if (x == x1 && y == y1)
				break;

			int err2 = err * 2;

			if (err2 > -dy) {
				err -= dy;
				x += sx;
			}
			if (x == x1 && y == y1) {
				glRecti(x, y, x + size, y + size);
				break;
			}
			if (err2 < dx) {
				err += dx;
				y += sy;
			}
		}
	} else {
		glRecti(x, y, x + size, y + size);
	}
}

static void spriteRender(struct sprite *s)
{
	if (!s->dirty)
		return;

	glColor4ubv((GLubyte*)&session->fg);

	int x  = s->draw.curr.x;
	int y  = s->draw.curr.y;
	int x1 = s->draw.prev.x;
	int y1 = s->draw.prev.y;

	switch (session->cursor) {
	case CURSOR_DEFAULT:
		spritePaint(s, x, y, x1, y1);
		break;
	case CURSOR_MULTI:
		for (int i = 0; i < s->nframes; i++) {
			spritePaint(s, x + i * s->fw, y, x1 + i * s->fw, y1);
		}
		break;
	default:
		break;
	}
	s->dirty = false;
}

static void spriteRenderCurrentFrame(struct sprite *s)
{
	double elapsed = glfwGetTime() - session->started;
	double frac = session->fps * elapsed;
	int frame = (int)floor(frac) % s->nframes;

	textureDraw(s->texture, s->fw * s->nframes, s->fh, frame * s->fw, 0, s->fw, s->fh, 0, 0);
}

static void spriteStartDrawing(struct sprite *s, int x, int y)
{
	if (!spriteWithinBoundary(s, x, y))
		return;

	spriteDraw(s, x, y);
	s->draw.drawing = DRAW_STARTED;
}

static void spriteStopDrawing(struct sprite *s)
{
	s->draw.drawing = DRAW_ENDED;
	spriteSnapshot(s);
}

static struct rgba sample(int x, int y)
{
	struct rgba pixel;
	glReadPixels(x, session->h - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
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
	setPixel(palette->pixels, x, y, session->w * sizeof(color), color);
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

	cx  = session->w/2;
	cy  = session->h/2;

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
	session->w = w;
	session->h = h;

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
			spriteStartDrawing(session->sprite, floor(x), floor(y));
		}
	} else if (action == GLFW_RELEASE) {
		spriteStopDrawing(session->sprite);
	}
}

static void cursorPosCallback(GLFWwindow *win, double fx, double fy)
{
	int x = floor(fx),
		y = floor(fy);

	struct sprite *s = session->sprite;

	if (s->draw.drawing == DRAW_STARTED || s->draw.drawing == DRAW_DRAWING) {
		spriteDraw(s, x, y);
		s->draw.drawing = DRAW_DRAWING;
	}
}

static void setupPalette()
{
	int x = 0, y = 0;
	int s = palette->size;

	palette->h = (255 / (session->w / s)) * s + s;
	palette->pixels = realloc(palette->pixels, session->w * palette->h * sizeof(struct rgba));
	memset(palette->pixels, 0, session->w * palette->h * sizeof(struct rgba));

	palette->texture = textureGen(session->w, palette->h, palette->pixels);
	glBindTexture(GL_TEXTURE_2D, palette->texture);

	for (int r = 0; r <= 255; r += 51) {
		for (int g = 0; g <= 255; g += 51) {
			for (int b = 0; b <= 255; b += 51) {
				paletteAddColor(x, y, rgba(r, g, b, 255));

				if (x + s > session->w) {
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

static void adjustFPS(int n)
{
	session->fps += n;

	if (session->fps < 1)
		session->fps = 1;
}

static void pause()
{
	session->paused = !session->paused;
}

static void saveTo(const char *filename)
{
	struct sprite *s = session->sprite;
	struct tga *t = (struct tga *)s->image;
	struct rgba *tmp = spriteReadPixels(session->sprite);

	short w = s->fw * s->nframes;
	short h = s->fh;

	char depth = t ? t->depth : 32;

	if (tgaEncode((uint32_t *)tmp, w, h, depth, filename) != 0) {
		debug("error: unable to save copy to '%s'", filename);
	}
	free(tmp);
}

static void saveCopy()
{
	size_t len = strlen(session->filepath);
	char filename[len + 1 + 3]; // <filename>.001

	sprintf(filename, "%s.%.3d", session->filepath, 1);
	saveTo(filename);
}

static void keyCallback(GLFWwindow *win, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_LEFT_CONTROL) {
		session->cursor = (action == GLFW_PRESS) ? CURSOR_SAMPLER : CURSOR_DEFAULT;
	}
	if (key == GLFW_KEY_LEFT_SHIFT) {
		session->cursor = (action == GLFW_PRESS) ? CURSOR_MULTI : CURSOR_DEFAULT;
	}

	if (action != GLFW_PRESS)
		return;

	switch (key) {
		case ',':             zoom(-1);         return;
		case '.':             zoom(+1);         return;
		case '[':             brushSize(-1);    return;
		case ']':             brushSize(+1);    return;
		case GLFW_KEY_U:      undo();           return;
		case GLFW_KEY_R:      redo();           return;
		case GLFW_KEY_SPACE:  pause();          return;
		case GLFW_KEY_LEFT:   move(-50, 0);     return;
		case GLFW_KEY_RIGHT:  move(+50, 0);     return;
		case GLFW_KEY_DOWN:   move(0, +50);     return;
		case GLFW_KEY_UP:     move(0, -50);     return;
		case GLFW_KEY_ESCAPE: windowClose(win); return;
	}
	if (mods & GLFW_MOD_SHIFT && key == GLFW_KEY_EQUAL) {
		adjustFPS(+1);
	} else if (key == GLFW_KEY_MINUS) {
		adjustFPS(-1);
	} else if (mods & GLFW_MOD_CONTROL && key == GLFW_KEY_F) {
		createFrame(-1);
	} else if (mods & GLFW_MOD_CONTROL && key == 'd') {
		// TODO: Remove frame
	} else if (mods & GLFW_MOD_CONTROL && key == GLFW_KEY_W) {
		saveCopy();
	} else if (mods & GLFW_MOD_CONTROL && key == GLFW_KEY_S) {
		saveTo(session->filepath);
	}
}

static void boundaryDraw(struct rgba color, int x, int y, int w, int h)
{
	glColor4ubv((GLubyte*)&color);
	glBegin(GL_LINE_LOOP);
	glVertex3f(x - 0.5,     y - 0.5, 0);
	glVertex3f(x + w + 0.5, y - 0.5, 0);
	glVertex3f(x + w + 0.5, y + h + 0.5, 0);
	glVertex3f(x - 0.5,     y + h + 0.5, 0);
	glEnd();
}

static void drawBoundaries()
{
	struct sprite *s = session->sprite;

	for (int i = 0; i < s->nframes; i++) {
		boundaryDraw(
			DARKGREY,
			session->x + i * s->fw * session->zoom,
			session->y,
			s->fw * session->zoom,
			s->fh * session->zoom
		);
	}
	boundaryDraw(
		GREY,
		session->x,
		session->y,
		s->fw * s->nframes * session->zoom,
		s->fh * session->zoom
	);
}

static void createBlank()
{
	addSprite(sprite(64, 64, NULL, 0, 64));
	createFrame(-1);
}

static void createFilename(char **filename)
{
	char timestr[128];
	time_t t = time(NULL);
	struct tm *tmp = localtime(&t);

	*filename = malloc(sizeof(timestr));

	strftime(timestr, sizeof(timestr), "%Y-%m-%d-%H%M%S", tmp);
	sprintf(*filename, "%s.tga", timestr);
}

int main(int argc, char *argv[])
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

	session             = malloc(sizeof(*session));
	session->sprite     = NULL;
	session->sprites    = NULL;
	session->nsprites   = 0;
	session->w          = 640;
	session->h          = 480;
	session->x          = 0;
	session->y          = 0;
	session->offx       = 0;
	session->offy       = 0;
	session->zoom       = 1;
	session->brush.size = 1;
	session->paused     = true;
	session->fg         = WHITE;
	session->bg         = WHITE;
	session->started    = glfwGetTime();
	session->fps        = 6;
	session->filepath   = NULL;

	if (argc > 1) {
		glfwSetWindowTitle(window, argv[1]);

		if (!loadSprites(argv[1])) {
			createBlank();
		}
	} else {
		createFilename(&session->filepath);
		createBlank();
	}
	reset();

	// Create first snapshot for undos.
	spriteSnapshot(session->sprite);

	// Color palette
	palette = malloc(sizeof(*palette));
	palette->pixels = NULL;
	palette->size = 20;

	fbClear();
	setupPalette();
	setFgColor(WHITE);

	while (!glfwWindowShouldClose(window)) {
		double mx, my;

		glfwGetCursorPos(window, &mx, &my);

		glViewport(0, 0, session->w, session->h);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0.0, session->w, session->h, 0.0, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);

		glLoadIdentity();
		glTranslatef(0, 0, 0);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_TEXTURE_2D);

		glPushMatrix(); {
			struct sprite *s = session->sprite;

			glBindFramebuffer(GL_FRAMEBUFFER, s->fb);
			spriteRender(s);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			glClear(GL_COLOR_BUFFER_BIT);
			glClearColor(0.0, 0.0, 0.0, 1.0);

			drawBoundaries();

			glTranslatef(session->x, session->y, 0.0f);
			glScalef(session->zoom, session->zoom, 1.0f);

			textureDraw(s->texture, s->fw * s->nframes, s->fh, 0, 0, s->fw * s->nframes, s->fh, 0, 0);

			if (s->nframes > 1 && !session->paused) {
				glTranslatef(-s->fw - 0.5, 0, 0.0f);
				spriteRenderCurrentFrame(s);
			}
		}
		glPopMatrix();

		textureRefresh(palette->texture, session->w, palette->h, palette->pixels);
		textureDraw(palette->texture, session->w, palette->h, 0, 0, session->w, palette->h, 0, 0);

		drawCursor(window, floor(mx), floor(my), session->cursor);

		glFlush();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	glDeleteFramebuffers(1, &session->sprite->fb);
	glfwDestroyWindow(window);
	glfwTerminate();

	free(palette->pixels);
	free(session);
	free(palette);

	exit(0);
}

