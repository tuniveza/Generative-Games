#include "texture.h"

#include <stb_image.h>
#include <stdio.h>

GLuint texture_from_pixels(const unsigned char *pixels, int w, int h, bool srgb)
{
    int levels = 1;
    for (int size = w > h ? w : h; size > 1; size >>= 1)
        levels++;

    GLuint tex;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    /* color is stored in sRGB and converted to linear when sampled; data stays linear */
    glTextureStorage2D(tex, levels, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, w, h);
    glTextureSubImage2D(tex, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateTextureMipmap(tex);

    glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
    /* keeps textures sharp on floors and walls seen at a steep angle */
    if (GLAD_GL_ARB_texture_filter_anisotropic || GLAD_GL_EXT_texture_filter_anisotropic)
        glTextureParameterf(tex, GL_TEXTURE_MAX_ANISOTROPY, 8.0f);
    return tex;
}

GLuint texture_load(const char *path, bool srgb)
{
    int w, h, n;
    unsigned char *pixels = stbi_load(path, &w, &h, &n, 4);
    if (!pixels) {
        fprintf(stderr, "can't load texture %s: %s\n", path, stbi_failure_reason());
        return texture_solid(255, 0, 255, 255);     /* loud magenta so it's obvious */
    }
    GLuint tex = texture_from_pixels(pixels, w, h, srgb);
    stbi_image_free(pixels);
    return tex;
}

GLuint texture_solid(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    unsigned char px[4] = { r, g, b, a };
    return texture_from_pixels(px, 1, 1, false);
}
