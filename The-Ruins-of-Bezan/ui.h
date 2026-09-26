#ifndef UI_H
#define UI_H

#include <glad/gl.h>
#include <stdbool.h>

enum { FONT_SMALL, FONT_MEDIUM, FONT_LARGE, FONT_COUNT };

/* 2D overlay drawn in pixels, (0, 0) at the top-left of the window.
 * the display font is used for FONT_LARGE (titles), the text font for the rest */
void ui_init(const char *display_font, const char *text_font);
void ui_free(void);

void ui_begin(int width, int height);
void ui_end(void);

void ui_rect(float x, float y, float w, float h, const float color[4]);
void ui_frame(float x, float y, float w, float h, float thickness, const float color[4]);

/* image drawn as-is; srgb = true for textures stored in sRGB (item icons) */
void ui_image(GLuint tex, float x, float y, float w, float h, const float tint[4], bool srgb);

/* returns the width drawn; y is the top of the line */
float ui_text(int font, float x, float y, const float color[4], const char *text);
float ui_text_width(int font, const char *text);
float ui_line_height(int font);
/* text with a soft dark shadow, easier to read over the 3D scene */
float ui_text_shadow(int font, float x, float y, const float color[4], const char *text);

#endif
