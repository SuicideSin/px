//
// px.h
//
struct palette {
	GLuint  texture;
	GLuint  fb;
	int     h;
	int     size;
	uint8_t *pixels;
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

enum mstate {
	MARQUEE_NONE,
	MARQUEE_STARTED,
	MARQUEE_ENDED
};

struct marquee {
	struct point min, max;
	enum mstate  state;
};

struct brush {
	int          size;
	enum dstate  drawing;
	struct rgba  color;
	struct point curr;
	struct point prev;
};

struct snapshot {
	struct rgba *pixels;
	int x, y;
	int w, h;
};

struct sprite {
	GLuint          texture;
	GLuint          fb;
	uint8_t         *pixels;
	bool            dirty;
	int             fw;
	int             fh;
	int             nframes;
	void            *image;
	int             snapshot;
	struct snapshot *snapshots;
	int             nsnapshots;
};

enum tool {
	TOOL_BRUSH,
	TOOL_SAMPLER,
	TOOL_MARQUEE,
	TOOL_MULTI
};

struct session {
	int           w;
	int           h;
	int           x;
	int           y;
	int           offx;
	int           offy;
	int           zoom;
	int           nsprites;
	int           fps;
	bool          paused;
	double        started;
	char          *filepath;
	struct sprite *sprites;
	struct sprite *sprite;
	struct rgba   fg;
	struct rgba   bg;

	struct {
		enum tool curr;
		union {
			struct brush   brush;
			struct marquee marquee;
		} u;
	} tool;
};

union arg {
	bool         b;
	int          i;
	unsigned int ui;
	float        f;
	const void   *v;
	struct point p;
};

struct binding {
	int             mods;
	int             key;
	int             action;
	void            (*callback)(GLFWwindow *win, const union arg *arg);
	const union arg arg;
};
