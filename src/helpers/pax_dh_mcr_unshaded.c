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

#ifndef PAX_GFX_C
#ifndef ARDUINO
#pragma message "This file should not be compiled on it's own."
#endif
#else

#include "pax_internal.h"

/* ======= UNSHADED DRAWING ====== */

// Multi-core method for unshaded triangles.
// Assumes points are sorted by Y.
// If odd_scanline is true, the odd (counted from 0) lines are drawn, otherwise the even lines are drawn.
static void paxmcr_tri_unshaded(bool odd_scanline, pax_buf_t *buf, pax_col_t color,
		float x0, float y0, float x1, float y1, float x2, float y2) {
	
	if (color < 0x01000000) {
		PAX_SUCCESS();
		return;
	}
	pax_setter_t setter = color >= 0xff000000 ? pax_set_pixel : pax_merge_pixel;
	
	// Find the appropriate Y for y0, y1 and y2 inside the triangle.
	float y_post_0 = (int) (y0 + 0.5) + 0.5;
	float y_post_1 = (int) (y1 + 0.5) + 0.5;
	float y_pre_2  = (int) (y2 - 0.5) + 0.5;
	
	// And the coefficients for x0->x1, x1->x2 and x0->x2.
	float x0_x1_dx = (x1 - x0) / (y1 - y0);
	float x1_x2_dx = (x2 - x1) / (y2 - y1);
	float x0_x2_dx = (x2 - x0) / (y2 - y0);
	
	// Clip: Y axis.
	if (y_post_0 > buf->clip.y + buf->clip.h) {
		y_post_0 = (int) (buf->clip.y + buf->clip.h - 0.5) + 0.5;
	}
	if (y_post_1 > buf->clip.y + buf->clip.h) {
		y_post_1 = (int) (buf->clip.y + buf->clip.h - 0.5) + 0.5;
	}
	if (y_pre_2  > buf->clip.y + buf->clip.h) {
		y_pre_2  = (int) (buf->clip.y + buf->clip.h - 0.5) + 0.5;
	}
	
	if (y_pre_2  < buf->clip.y) {
		y_pre_2  = (int) (buf->clip.y + 0.5) + 0.5;
	}
	if (y_post_1 < buf->clip.y) {
		y_post_1 = (int) (buf->clip.y + 0.5) + 0.5;
	}
	if (y_post_0 < buf->clip.y) {
		y_post_0 = (int) (buf->clip.y + 0.5) + 0.5;
	}
	
	// Draw top half.
	// This condition is false if no one point is inside the triangle and above y1.
	if (y_post_0 < y_post_1 && y_post_0 >= y0) {
		// Find the X counterparts to the other points we found.
		float x_a = x0 + x0_x1_dx * (y_post_0 - y0);
		float x_b = x0 + x0_x2_dx * (y_post_0 - y0);
		// Snap y to the correct line.
		int y = y_post_0;
		if ((y & 1) != odd_scanline) {
			y ++;
			x_a += x0_x1_dx;
			x_b += x0_x2_dx;
		}
		for (; y < (int) y_post_1; y += 2) {
			// Plot the horizontal line.
			float x_left, x_right;
			if (x_a < x_b) {
				x_left  = x_a;
				x_right = x_b;
			} else {
				x_left  = x_b;
				x_right = x_a;
			}
			// Clip: X axis.
			if (x_right > buf->clip.x + buf->clip.w) {
				x_right = buf->clip.x + buf->clip.w;
			}
			if (x_left < buf->clip.x) {
				x_left = buf->clip.x;
			}
			for (int x = x_left + 0.5; x < x_right; x ++) {
				// And simply merge colors accordingly.
				setter(buf, color, x, y);
			}
			// Move X.
			x_a += 2*x0_x1_dx;
			x_b += 2*x0_x2_dx;
		}
	}
	// Draw bottom half.
	// This condition might be confusing, but it's false if no point at all is inside the triangle.
	if (y_post_0 <= y_pre_2 && y_post_1 >= y1 && y_pre_2 <= y2) {
		// Find the X counterparts to the other points we found.
		float x_a = x1 + x1_x2_dx * (y_post_1 - y1);
		float x_b = x0 + x0_x2_dx * (y_post_1 - y0);
		// Snap y to the correct line.
		int y = y_post_1;
		if ((y & 1) != odd_scanline) {
			y ++;
			x_a += x1_x2_dx;
			x_b += x0_x2_dx;
		}
		for (; y <= (int) y_pre_2; y += 2) {
			// Plot the horizontal line.
			float x_left, x_right;
			if (x_a < x_b) {
				x_left  = x_a;
				x_right = x_b;
			} else {
				x_left  = x_b;
				x_right = x_a;
			}
			// Clip: X axis.
			if (x_right > buf->clip.x + buf->clip.w) {
				x_right = buf->clip.x + buf->clip.w;
			}
			if (x_left < buf->clip.x) {
				x_left = buf->clip.x;
			}
			for (int x = x_left + 0.5; x < x_right; x ++) {
				// And simply merge colors accordingly.
				setter(buf, color, x, y);
			}
			// Move X.
			x_a += 2*x1_x2_dx;
			x_b += 2*x0_x2_dx;
		}
	}
}

// Multi-core method for rectangle drawing.
// If odd_scanline is true, the odd (counted from 0) lines are drawn, otherwise the even lines are drawn.
static void paxmcr_rect_unshaded(bool odd_scanline, pax_buf_t *buf, pax_col_t color,
		float x, float y, float width, float height) {
	
	if (color < 0x01000000) {
		PAX_SUCCESS();
		return;
	}
	pax_setter_t setter = color >= 0xff000000 ? pax_set_pixel : pax_merge_pixel;
	
	// Snap c_y to the correct line.
	int c_y = y + 0.5;
	if ((c_y & 1) != odd_scanline) {
		c_y ++;
	}
	
	// Pixel time.
	for (; c_y <= y + height - 0.5; c_y += 2) {
		for (int c_x = x + 0.5; c_x <= x + width - 0.5; c_x ++) {
			setter(buf, color, c_x, c_y);
		}
	}
}

#endif
