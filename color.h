
struct rgba {
	uint8_t r, g, b, a;
};

struct hsla {
	float h, s, l, a;
};

struct rgba hsla2rgba(struct hsla hsla);
struct hsla rgba2hsla(struct rgba rgba);
