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

#include "color.h"
#include "texture.h"
#include "px.h"
#include "tga.h"

#define PX_NAME "px"
#define PX_MAX_LOG_SIZE 128
#define LENGTH(x) (sizeof(x) / sizeof(x[0]))

#define point(x, y)      ((struct point){(x), (y)})
#define rgba(r, g, b, a) ((struct rgba){r, g, b, a})
#define WHITE            rgba(255, 255, 255, 255)
#define GREY             rgba(128, 128, 128, 255)
#define DARKGREY         rgba(64, 64, 64, 255)
#define TRANSPARENT      rgba(0, 0, 0, 0)

static struct rgba *spriteReadPixels(struct sprite *s);
static void paletteAddColor(int x, int y, struct rgba color);
static void boundaryDraw(struct rgba color, int x, int y, int w, int h);
static void setupPalette();
static void createFrame(GLFWwindow *, const union arg *);
static void saveCopy(GLFWwindow *, const union arg *);
static void save(GLFWwindow *, const union arg *);
static void move(GLFWwindow *, const union arg *);
static void pan(GLFWwindow *, const union arg *);
static void zoom(GLFWwindow *, const union arg *);
static void undo(GLFWwindow *, const union arg *);
static void redo(GLFWwindow *, const union arg *);
static void pause(GLFWwindow *, const union arg *);
static void windowClose(GLFWwindow *, const union arg *);
static void brushSize(GLFWwindow *, const union arg *);
static void adjustFPS(GLFWwindow *, const union arg *);
static void brush(GLFWwindow *, const union arg *);
static void marquee(GLFWwindow *, const union arg *);

struct session     *session;
struct palette     *palette;

#include "config.h"

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

static void fillRect(int x1, int y1, int x2, int y2, struct rgba color)
{
	glColor4ubv((GLubyte*)&color);
	glRecti(x1, y1, x2, y2);
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

static void spriteSnapshot(struct sprite *s)
{
	struct snapshot snap = (struct snapshot){
		.pixels = spriteReadPixels(s),
		.x      = 0,
		.y      = 0,
		.w      = s->fw * s->nframes,
		.h      = s->fh
	};
	if (s->snapshot < s->nsnapshots - 1) {
		for (int i = s->snapshot; i < s->nsnapshots; i++) {
			free(s->snapshots[i].pixels);
		}
		s->nsnapshots = s->snapshot + 1;
	}
	s->snapshots = realloc(s->snapshots, (s->nsnapshots + 1) * sizeof(*s->snapshots));
	s->snapshots[s->nsnapshots] = snap;
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

	struct snapshot snap = s->snapshots[snapshot];
	
	glBindFramebuffer(GL_FRAMEBUFFER, s->fb);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glDrawPixels(snap.w, snap.h, GL_RGBA, GL_UNSIGNED_BYTE, snap.pixels);
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

static void brush(GLFWwindow *_w, const union arg *_a)
{
	session->tool.curr            = TOOL_BRUSH;
	session->tool.u.brush.size    = +1;
	session->tool.u.brush.prev.x  = -1;
	session->tool.u.brush.prev.y  = -1;
	session->tool.u.brush.curr.x  = -1;
	session->tool.u.brush.curr.y  = -1;
	session->tool.u.brush.drawing = false;
	session->tool.u.brush.color   = TRANSPARENT;
}

static struct sprite sprite(int fw, int fh, uint8_t *pixels, int start, int end)
{
	struct sprite s = (struct sprite){
		.pixels       = pixels,
		.texture      = 0,
		.fb           = fbGen(),
		.dirty        = false,
		.fw           = fw,
		.fh           = fh,
		.nframes      = 0,
		.snapshot     = -1,
		.snapshots    = NULL,
		.nsnapshots   = 0
	};

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

static void createFrame()
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

static struct point nearest(struct point p)
{
	return (struct point){
		.x = p.x - (p.x % session->zoom) + 0.5,
		.y = p.y - (p.y % session->zoom) + 0.5
	};
}

static void drawCursor(GLFWwindow *win, int x, int y, enum tool t)
{
	int s = session->tool.u.brush.size * session->zoom;

	struct point n = nearest(point(x, y));
	struct sprite *sp = session->sprite;

	switch (t) {
	case TOOL_BRUSH:
		if (spriteWithinBoundary(session->sprite, x, y)) {
			fillRect(n.x, n.y, n.x + s, n.y + s, session->fg);
		} else {
			boundaryDraw(GREY, n.x, n.y, n.x + s, n.y + s);
		}
		break;
	case TOOL_SAMPLER:
		boundaryDraw(WHITE, n.x, n.y, n.x + s, n.y + s);
		break;
	case TOOL_MULTI: {
			int frame = (n.x - session->x) / sp->fw;
			for (int i = 0; i < sp->nframes - frame; i++) {
				fillRect(n.x + i * sp->fw * session->zoom,
						 n.y,
						 n.x + i * sp->fw * session->zoom + s,
						 n.y + s, session->fg);
			}
		}
		break;
	case TOOL_MARQUEE: {
			struct marquee *m = &session->tool.u.marquee;
			if (m->state > MARQUEE_NONE && m->max.x != -1 && m->max.y != -1) { // Draw marquee
				glLogicOp(GL_INVERT);
				boundaryDraw(GREY, m->min.x, m->min.y, m->max.x, m->max.y);
				glLogicOp(GL_COPY);
			}
			if (m->state == MARQUEE_ENDED) {
				boundaryDraw(WHITE, n.x, n.y, n.x + 1, n.y + 1);
			}
			break;
		}
	}
}

static void spriteDraw(struct sprite *s, int x, int y)
{
	x -= session->x;
	y -= session->y;

	x /= session->zoom;
	y /= session->zoom;

	y += session->tool.u.brush.size;

	y = session->h - y;

	// TODO(cloudhead): put this in a function
	session->tool.u.brush.prev = session->tool.u.brush.curr;
	session->tool.u.brush.curr.x = x;
	session->tool.u.brush.curr.y = y;

	s->dirty = true;
}

static void spritePaint(struct sprite *s, int x, int y, int x1, int y1)
{
	int size = session->tool.u.brush.size;

	if (session->tool.u.brush.drawing > DRAW_STARTED) {
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

	int x  = session->tool.u.brush.curr.x;
	int y  = session->tool.u.brush.curr.y;
	int x1 = session->tool.u.brush.prev.x;
	int y1 = session->tool.u.brush.prev.y;

	switch (session->tool.curr) {
	case TOOL_BRUSH:
		spritePaint(s, x, y, x1, y1);
		break;
	case TOOL_MULTI:
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
	session->tool.u.brush.drawing = DRAW_STARTED;
}

static void spriteStopDrawing(struct sprite *s)
{
	session->tool.u.brush.drawing = DRAW_ENDED;
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

static void paletteAddColor(int x, int y, struct rgba color)
{
	fillRect(x, y, x + palette->size, y + palette->size, color);
}

static void marquee(GLFWwindow *_, const union arg *arg)
{
	session->tool.curr            = TOOL_MARQUEE;
	session->tool.u.marquee.state = MARQUEE_NONE;
	session->tool.u.marquee.min.x = -1;
	session->tool.u.marquee.min.y = -1;
	session->tool.u.marquee.max.x = -1;
	session->tool.u.marquee.max.y = -1;
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

static void move(GLFWwindow *_, const union arg *arg)
{
	session->offx += arg->p.x;
	session->offy += arg->p.y;

	center();
}

static struct point* pan_offset;
static void pan(GLFWwindow *win, const union arg *arg)
{
	if (arg->b) {
		double x, y;
		glfwGetCursorPos(win, &x, &y);
		pan_offset    = malloc(sizeof *pan_offset);
		pan_offset->x = (int)x;
		pan_offset->y = (int)y;
	} else {
		free(pan_offset);
		pan_offset = NULL;
	}
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
	double x, y;

	glfwGetCursorPos(win, &x, &y);

	switch (session->tool.curr) {
	case TOOL_BRUSH: {
			if (action == GLFW_PRESS) {
				spriteStartDrawing(session->sprite, floor(x), floor(y));
			} else if (action == GLFW_RELEASE) {
				spriteStopDrawing(session->sprite);
			}
		}
		break;
	case TOOL_MARQUEE: {
			struct marquee *m = &session->tool.u.marquee;

			if (m->state == MARQUEE_STARTED && action == GLFW_RELEASE) {
				m->state = MARQUEE_ENDED;
			} else {
				m->min.x = floor(x);
				m->min.y = floor(y);
				m->state = MARQUEE_STARTED;
			}
		}
		break;
	case TOOL_SAMPLER: {
			pickColor(round(x), round(y));
		}
		break;
	case TOOL_MULTI:
		break;
	}
}

static void cursorPosCallback(GLFWwindow *win, double fx, double fy)
{
	int x = floor(fx),
	    y = floor(fy);

	if (pan_offset) {
		move(win, &(union arg){ .p = { x - pan_offset->x, y - pan_offset->y } });
		pan_offset->x = x;
		pan_offset->y = y;
		return;
	}

	struct sprite *s = session->sprite;

	switch (session->tool.curr) {
	case TOOL_BRUSH:
		if (session->tool.u.brush.drawing == DRAW_STARTED || session->tool.u.brush.drawing == DRAW_DRAWING) {
			spriteDraw(s, x, y);
			session->tool.u.brush.drawing = DRAW_DRAWING;
		}
		break;
	case TOOL_MARQUEE:
		if (session->tool.u.marquee.state == MARQUEE_STARTED) {
			session->tool.u.marquee.max = nearest(point(x, y));
		}
		break;
	default:
		break;
	}
}

static void setupPalette()
{
	int x = 0, y = 0;
	int ncolors = 32;
	int s = palette->size = floor((float)session->h / (float)ncolors);
	int stride = s * sizeof(struct rgba);

	palette->h = session->h;
	palette->pixels = realloc(palette->pixels, palette->h * stride);
	memset(palette->pixels, 0, palette->h * stride);

	if (palette->texture)
		glDeleteTextures(1, &palette->texture);

	palette->texture = textureGen(s, palette->h, palette->pixels);

	fbAttach(palette->fb, palette->texture);
	glBindFramebuffer(GL_FRAMEBUFFER, palette->fb);

	fbClear();

	for (int i = 0; i < ncolors; i++) {
		paletteAddColor(x, y,
			hsla2rgba((struct hsla){i * 1.0f/(float)ncolors, 0.5, 0.5, 1.0})
		);
		y += s;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void brushSize(GLFWwindow *_, const union arg *arg)
{
	session->tool.u.brush.size += arg->i;

	if (session->tool.u.brush.size < 1)
		session->tool.u.brush.size = 1;
}

static void zoom(GLFWwindow *_, const union arg *arg)
{
	session->zoom += arg->i;

	if (session->zoom < 1)
		session->zoom = 1;

	center();
	fbClear();
}

static void windowClose(GLFWwindow *win, const union arg *_)
{
	glfwSetWindowShouldClose(win, GL_TRUE);
}

static void adjustFPS(GLFWwindow *_, const union arg *arg)
{
	session->fps += arg->i;

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

static void save()
{
	saveTo(session->filepath);
}

static void keyCallback(GLFWwindow *win, int key, int scancode, int action, int mods)
{
	for (int i = 0; i < LENGTH(bindings); i++) {
		if (bindings[i].key == key
			&& bindings[i].mods == mods
			&& bindings[i].action == action
			&& bindings[i].callback) {
			bindings[i].callback(win, &(bindings[i].arg));
			return;
		}
	}
	if (key == GLFW_KEY_LEFT_CONTROL) {
		session->tool.curr = (action == GLFW_PRESS) ? TOOL_SAMPLER : TOOL_BRUSH;
	}
	if (key == GLFW_KEY_LEFT_SHIFT) {
		session->tool.curr = (action == GLFW_PRESS) ? TOOL_MULTI : TOOL_BRUSH;
	}
}

static void boundaryDraw(struct rgba color, int x1, int y1, int x2, int y2)
{
	glColor4ubv((GLubyte*)&color);
	glBegin(GL_LINE_LOOP);
	glVertex3f(x1 - 0.5,     y1 - 0.5, 0);
	glVertex3f(x2 + 0.5, y1 - 0.5, 0);
	glVertex3f(x2 + 0.5, y2 + 0.5, 0);
	glVertex3f(x1 - 0.5,     y2 + 0.5, 0);
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
			session->x + s->fw * session->zoom,
			session->y + s->fh * session->zoom
		);
	}
	boundaryDraw(
		GREY,
		session->x,
		session->y,
		session->x + s->fw * s->nframes * session->zoom,
		session->y + s->fh * session->zoom
	);
}

static void createBlank()
{
	addSprite(sprite(64, 64, NULL, 0, 64));
	createFrame(NULL, NULL);
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
	palette->texture = 0;
	palette->fb = fbGen();

	fbClear();
	setupPalette();
	setFgColor(WHITE);
	brush(window, NULL);

	while (!glfwWindowShouldClose(window)) {
		double mx, my;
		int    w, h;

		glfwGetFramebufferSize(window, &w, &h);
		glfwGetCursorPos(window, &mx, &my);

		glViewport(0, 0, w, h);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0.0, w, h, 0.0, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);

		glLoadIdentity();
		glTranslatef(0, 0, 0);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_COLOR_LOGIC_OP);

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

		textureDraw(palette->texture, palette->size, palette->h, 0, 0, palette->size, palette->h, 0, 0);
		drawCursor(window, floor(mx), floor(my), session->tool.curr);

		glDisable(GL_TEXTURE_2D);
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

