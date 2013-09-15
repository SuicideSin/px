//
// px.h
//
struct rgba {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

struct palette {
	GLuint  texture;
	int     h;
	int     size;
	uint8_t *pixels;
};

struct brush {
	int size;
};

struct point {
	int x;
	int y;
};

enum dstate {
	DRAW_STARTED = 1,
	DRAW_DRAWING = 2,
	DRAW_ENDED = 3
};

struct draw {
	enum dstate  drawing;
	struct rgba  color;
	struct point curr;
	struct point prev;
};

struct canvas {
	GLuint       texture;
	uint8_t      *pixels;
	int          zoom;
	int          x;
	int          y;
	int          offx;
	int          offy;
	int          w;
	int          h;
	int          stride;
	bool         dirty;
	struct brush brush;
	struct draw  draw;
	struct rgba  fg;
	struct rgba  bg;
};

struct framebuffer {
	int    w;
	int    h;
	GLuint texture;
};

