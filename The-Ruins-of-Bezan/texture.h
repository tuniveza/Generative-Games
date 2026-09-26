#ifndef TEXTURE_H
#define TEXTURE_H

#include <glad/gl.h>
#include <stdbool.h>

/* uploads RGBA8 pixels as a mipmapped, anisotropically filtered, repeating texture */
GLuint texture_from_pixels(const unsigned char *pixels, int w, int h, bool srgb);

/* loads an image file; color textures (albedo, emissive) should be srgb = true */
GLuint texture_load(const char *path, bool srgb);

/* 1x1 texture of a single color, handy as a stand-in */
GLuint texture_solid(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

#endif
