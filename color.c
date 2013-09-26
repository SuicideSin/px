
#include <inttypes.h>
#include <math.h>

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#include "color.h"

static float hue(float h, float m1, float m2)
{
    h = h < 0 ? h + 1 : (h > 1 ? h - 1 : h);

    if      (h * 6 < 1) return m1 + (m2 - m1) * h * 6;
    else if (h * 2 < 1) return m2;
    else if (h * 3 < 2) return m1 + (m2 - m1) * (2.0/3.0 - h) * 6;
    else                return m1;
}

struct rgba hsla2rgba(struct hsla hsla)
{
	float h = (float)hsla.h,
	      s = (float)hsla.s,
	      l = (float)hsla.l,
	      a = (float)hsla.a;

	h = fmod(h, 1.0);

	float m2 = l <= 0.5 ? l * (s + 1) : l + s - l * s;
	float m1 = l * 2 - m2;

	return (struct rgba){
		hue(h + 1.0/3.0, m1, m2) * 255,
		hue(h,           m1, m2) * 255,
		hue(h - 1.0/3.0, m1, m2) * 255, a * 255
	};
}

struct hsla rgba2hsla(struct rgba rgba)
{
	float r = (float)rgba.r/255,
	      g = (float)rgba.g/255,
	      b = (float)rgba.b/255,
	      a = (float)rgba.a/255;

	float mx = max(max(r, g), b),
	      mn = min(min(r, g), b);

	float h, s,
	      l = (mx + mn) / 2.0,
	      d = mx - mn;

	if (mx == mn) {
		h = s = 0;
	} else {
		s = l > 0.5 ? d / (2.0 - mx - mn) : d / (mx + mn);

		if      (r == mx) h = (g - b) / d + (g < b ? 6.0 : 0);
		else if (g == mx) h = (b - r) / d + 2.0;
		else              h = (r - g) / d + 4.0;

		h /= 6;
	}
	return (struct hsla){h, s, l, a};
}
