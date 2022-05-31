/*
	MIT License

	Copyright (c) 2022 Julian Scheffers

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#include "pax_shaders.h"
#include <math.h>

// #define DO_BICUBIC

#ifdef DO_BICUBIC
static inline float pax_interp_value(float a) {
	// Cubic interpolation: y = -2x³ + 3x²
	return -2*a*a*a + 3*a*a;
}
#else
#define pax_interp_value(a) (a)
#endif

// Texture shader for multi-bpp bitmap fonts.
pax_col_t pax_shader_font_bmp_hi(pax_col_t base, int x, int y, float u, float v, void *args0) {
	pax_font_bmp_args_t *args = args0;
	
	int glyph_x = u;
	int glyph_y = v;
	if (glyph_x >= args->glyph_w) glyph_x = args->glyph_w - 1;
	if (glyph_y >= args->glyph_h) glyph_y = args->glyph_h - 1;
	size_t glyph_index =
		args->glyph_index
		+ glyph_x / args->ppb
		+ args->glyph_y_mul * glyph_y;
	
	uint16_t val =
		(args->range->bitmap_mono.glyphs[glyph_index]
			>> args->bpp * (glyph_x & (args->mask)))
		& args->mask;
	
	uint8_t   alpha = val * 255 / (args->mask);
	pax_col_t tint  = (alpha << 24) | 0x00ffffff;
	
	return pax_col_tint(base, tint);
}

// Texture shader for multi-bpp bitmap fonts with linear interpolation.
pax_col_t pax_shader_font_bmp_hi_aa(pax_col_t base, int x, int y, float u, float v, void *args0) {
	pax_font_bmp_args_t *args = args0;
	
	u -= 0.5;
	v -= 0.5;
	int glyph_x = (float) (u + 1);
	int glyph_y = (float) (v + 1);
	glyph_x --;
	glyph_y --;
	float dx = pax_interp_value(u - glyph_x);
	float dy = pax_interp_value(v - glyph_y);
	
	if (glyph_x >= args->glyph_w) glyph_x = args->glyph_w - 1;
	if (glyph_y >= args->glyph_h) glyph_y = args->glyph_h - 1;
	
	size_t glyph_index_0 = args->glyph_index + glyph_x / args->ppb + args->glyph_y_mul * glyph_y;
	glyph_y ++;
	size_t glyph_index_2 = args->glyph_index + glyph_x / args->ppb + args->glyph_y_mul * glyph_y;
	glyph_x ++;
	size_t glyph_index_3 = args->glyph_index + glyph_x / args->ppb + args->glyph_y_mul * glyph_y;
	glyph_y --;
	size_t glyph_index_1 = args->glyph_index + glyph_x / args->ppb + args->glyph_y_mul * glyph_y;
	glyph_x --;
	
	const uint8_t *arr = args->range->bitmap_mono.glyphs;
	uint8_t glyph_bit_0 = 0;
	uint8_t glyph_bit_1 = 0;
	uint8_t glyph_bit_2 = 0;
	uint8_t glyph_bit_3 = 0;
	
	if (glyph_x >= 0 && glyph_y >= 0) {
		glyph_bit_0 = (arr[glyph_index_0] >> args->bpp * (glyph_x & (args->mask))) & args->mask;
		// glyph_bit_0 = arr[glyph_index_0] & (1 << (glyph_x & 7));
	}
	
	if (glyph_x < args->glyph_w - 1 && glyph_y >= 0) {
		glyph_bit_1 = (arr[glyph_index_1] >> args->bpp * ((glyph_x + 1) & (args->mask))) & args->mask;
		// glyph_bit_1 = arr[glyph_index_1] & (1 << ((glyph_x+1) & 7));
	}
	
	if (glyph_x >= 0 && glyph_y < args->glyph_h - 1) {
		glyph_bit_2 = (arr[glyph_index_2] >> args->bpp * (glyph_x & (args->mask))) & args->mask;
		// glyph_bit_2 = arr[glyph_index_2] & (1 << (glyph_x & 7));
	}
	
	if (glyph_x < args->glyph_w - 1 && glyph_y < args->glyph_h - 1) {
		glyph_bit_3 = (arr[glyph_index_3] >> args->bpp * ((glyph_x + 1) & (args->mask))) & args->mask;
		// glyph_bit_3 = arr[glyph_index_3] & (1 << ((glyph_x+1) & 7));
	}
	
	float c0 = glyph_bit_0 / (float) args->mask;
	float c1 = glyph_bit_1 / (float) args->mask;
	float c2 = glyph_bit_2 / (float) args->mask;
	float c3 = glyph_bit_3 / (float) args->mask;
	
	float c4 = c0 + (c1 - c0) * dx;
	float c5 = c2 + (c3 - c2) * dx;
	
	float coeff = c4 + (c5 - c4) * dy;
	pax_col_t tint = (pax_col_t) (0xff000000 * coeff) | 0x00ffffff;
	return pax_col_tint(base, tint);
}

// Texture shader for 1bpp bitmap fonts.
pax_col_t pax_shader_font_bmp(pax_col_t tint, int x, int y, float u, float v, void *args0) {
	pax_font_bmp_args_t *args = args0;
	
	int glyph_x = u;
	int glyph_y = v;
	if (glyph_x >= args->glyph_w) glyph_x = args->glyph_w - 1;
	if (glyph_y >= args->glyph_h) glyph_y = args->glyph_h - 1;
	size_t glyph_index = args->glyph_index + glyph_x / 8 + args->glyph_y_mul * glyph_y;
	
	return args->range->bitmap_mono.glyphs[glyph_index] & (1 << (glyph_x & 7)) ? tint : 0;
}

// Texture shader for 1bpp bitmap fonts with linear interpolation.
pax_col_t pax_shader_font_bmp_aa(pax_col_t base, int x, int y, float u, float v, void *args0) {
	pax_font_bmp_args_t *args = args0;
	
	u -= 0.5;
	v -= 0.5;
	int glyph_x = (float) (u + 1);
	int glyph_y = (float) (v + 1);
	glyph_x --;
	glyph_y --;
	float dx = pax_interp_value(u - glyph_x);
	float dy = pax_interp_value(v - glyph_y);
	
	if (glyph_x >= args->glyph_w) glyph_x = args->glyph_w - 1;
	if (glyph_y >= args->glyph_h) glyph_y = args->glyph_h - 1;
	
	size_t glyph_index_0 = args->glyph_index + glyph_x / 8 + args->glyph_y_mul * glyph_y;
	glyph_y ++;
	size_t glyph_index_2 = args->glyph_index + glyph_x / 8 + args->glyph_y_mul * glyph_y;
	glyph_x ++;
	size_t glyph_index_3 = args->glyph_index + glyph_x / 8 + args->glyph_y_mul * glyph_y;
	glyph_y --;
	size_t glyph_index_1 = args->glyph_index + glyph_x / 8 + args->glyph_y_mul * glyph_y;
	glyph_x --;
	
	const uint8_t *arr = args->range->bitmap_mono.glyphs;
	bool glyph_bit_0 = 0;
	bool glyph_bit_1 = 0;
	bool glyph_bit_2 = 0;
	bool glyph_bit_3 = 0;
	
	if (glyph_x >= 0 && glyph_y >= 0) {
		glyph_bit_0 = arr[glyph_index_0] & (1 << (glyph_x & 7));
	}
	
	if (glyph_x < args->glyph_w - 1 && glyph_y >= 0) {
		glyph_bit_1 = arr[glyph_index_1] & (1 << ((glyph_x+1) & 7));
	}
	
	if (glyph_x >= 0 && glyph_y < args->glyph_h - 1) {
		glyph_bit_2 = arr[glyph_index_2] & (1 << (glyph_x & 7));
	}
	
	if (glyph_x < args->glyph_w - 1 && glyph_y < args->glyph_h - 1) {
		glyph_bit_3 = arr[glyph_index_3] & (1 << ((glyph_x+1) & 7));
	}
	
	float c0 = glyph_bit_0;
	float c1 = glyph_bit_1;
	float c2 = glyph_bit_2;
	float c3 = glyph_bit_3;
	
	float c4 = c0 + (c1 - c0) * dx;
	float c5 = c2 + (c3 - c2) * dx;
	
	float coeff = c4 + (c5 - c4) * dy;
	pax_col_t tint = (pax_col_t) (0xff000000 * coeff) | 0x00ffffff;
	return pax_col_tint(base, tint);
}

pax_col_t pax_shader_texture(pax_col_t tint, int x, int y, float u, float v, void *args) {
	if (!args) {
		return (u < 0.5) ^ (v >= 0.5) ? 0xffff00ff : 0xff1f1f1f;
	}
	pax_buf_t *image = (pax_buf_t *) args;
	pax_col_t  color = pax_get_pixel(image, u*image->width, v*image->height);
	return pax_col_tint(color, tint);
}
