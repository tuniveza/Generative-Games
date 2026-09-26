#include "ui.h"
#include "shader.h"

#include <SDL3/SDL.h>
#include <cglm/cglm.h>
#include <stb_truetype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ATLAS 1024
#define MAX_VERTS 32768

enum { MODE_COLOR = 0, MODE_FONT = 1, MODE_IMAGE = 2, MODE_IMAGE_SRGB = 3 };

typedef struct {
    float x, y, u, v;
    float r, g, b, a;
    float mode;
} Vertex;

static const float SIZES[FONT_COUNT] = { 19.0f, 26.0f, 58.0f };

static struct {
    GLuint prog, vao, vbo, atlas, white;
    stbtt_packedchar chars[FONT_COUNT][96];
    float ascent[FONT_COUNT];
    Vertex verts[MAX_VERTS];
    int count;
    GLuint tex;             /* texture of the current batch */
    int width, height;
} ui;

static unsigned char *load_font(const char *path)
{
    unsigned char *ttf = SDL_LoadFile(path, NULL);
    if (!ttf) {
        fprintf(stderr, "can't read font %s: %s\n", path, SDL_GetError());
        exit(1);
    }
    return ttf;
}

void ui_init(const char *display_font, const char *text_font)
{
    unsigned char *ttf[2] = { load_font(text_font), load_font(display_font) };

    /* all sizes of printable ASCII packed into one atlas, 2x2 oversampled for smoothness */
    unsigned char *pixels = calloc(ATLAS, ATLAS);
    stbtt_pack_context pc;
    stbtt_PackBegin(&pc, pixels, ATLAS, ATLAS, 0, 1, NULL);
    stbtt_PackSetOversampling(&pc, 2, 2);
    for (int f = 0; f < FONT_COUNT; f++) {
        unsigned char *data = ttf[f == FONT_LARGE];
        stbtt_fontinfo info;
        stbtt_InitFont(&info, data, stbtt_GetFontOffsetForIndex(data, 0));
        int asc, desc, gap;
        stbtt_GetFontVMetrics(&info, &asc, &desc, &gap);
        stbtt_PackFontRange(&pc, data, 0, SIZES[f], 32, 96, ui.chars[f]);
        ui.ascent[f] = asc * stbtt_ScaleForPixelHeight(&info, SIZES[f]);
    }
    stbtt_PackEnd(&pc);
    SDL_free(ttf[0]);
    SDL_free(ttf[1]);

    glCreateTextures(GL_TEXTURE_2D, 1, &ui.atlas);
    glTextureStorage2D(ui.atlas, 1, GL_R8, ATLAS, ATLAS);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage2D(ui.atlas, 0, 0, 0, ATLAS, ATLAS, GL_RED, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTextureParameteri(ui.atlas, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(ui.atlas, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    free(pixels);

    unsigned char white[4] = { 255, 255, 255, 255 };
    glCreateTextures(GL_TEXTURE_2D, 1, &ui.white);
    glTextureStorage2D(ui.white, 1, GL_RGBA8, 1, 1);
    glTextureSubImage2D(ui.white, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, white);

    ui.prog = shader_load("shaders/ui.vert", "shaders/ui.frag");
    glCreateBuffers(1, &ui.vbo);
    glNamedBufferStorage(ui.vbo, sizeof ui.verts, NULL, GL_DYNAMIC_STORAGE_BIT);
    glCreateVertexArrays(1, &ui.vao);
    glVertexArrayVertexBuffer(ui.vao, 0, ui.vbo, 0, sizeof(Vertex));
    for (int a = 0; a < 3; a++) {
        glEnableVertexArrayAttrib(ui.vao, a);
        glVertexArrayAttribBinding(ui.vao, a, 0);
    }
    glVertexArrayAttribFormat(ui.vao, 0, 4, GL_FLOAT, GL_FALSE, offsetof(Vertex, x));
    glVertexArrayAttribFormat(ui.vao, 1, 4, GL_FLOAT, GL_FALSE, offsetof(Vertex, r));
    glVertexArrayAttribFormat(ui.vao, 2, 1, GL_FLOAT, GL_FALSE, offsetof(Vertex, mode));
}

void ui_free(void)
{
    glDeleteProgram(ui.prog);
    glDeleteBuffers(1, &ui.vbo);
    glDeleteVertexArrays(1, &ui.vao);
    glDeleteTextures(1, &ui.atlas);
    glDeleteTextures(1, &ui.white);
}

static void flush(void)
{
    if (!ui.count)
        return;
    glNamedBufferSubData(ui.vbo, 0, ui.count * sizeof(Vertex), ui.verts);
    glBindTextureUnit(0, ui.tex);
    glBindTextureUnit(1, ui.atlas);
    glDrawArrays(GL_TRIANGLES, 0, ui.count);
    ui.count = 0;
}

void ui_begin(int width, int height)
{
    ui.width = width;
    ui.height = height;
    ui.count = 0;
    ui.tex = ui.white;
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(ui.prog);
    glProgramUniform2f(ui.prog, 0, (float)width, (float)height);
    glBindVertexArray(ui.vao);
}

void ui_end(void)
{
    flush();
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

static void quad(GLuint tex, float x0, float y0, float x1, float y1,
                 float u0, float v0, float u1, float v1, const float c[4], int mode)
{
    if (tex != ui.tex) {
        flush();
        ui.tex = tex;
    }
    if (ui.count + 6 > MAX_VERTS)
        flush();
    Vertex v[4] = {
        { x0, y0, u0, v0, c[0], c[1], c[2], c[3], (float)mode },
        { x1, y0, u1, v0, c[0], c[1], c[2], c[3], (float)mode },
        { x1, y1, u1, v1, c[0], c[1], c[2], c[3], (float)mode },
        { x0, y1, u0, v1, c[0], c[1], c[2], c[3], (float)mode },
    };
    static const int order[6] = { 0, 1, 2, 0, 2, 3 };
    for (int i = 0; i < 6; i++)
        ui.verts[ui.count++] = v[order[i]];
}

void ui_rect(float x, float y, float w, float h, const float color[4])
{
    /* solid rects use the font texture slot's neighbor: sampling is skipped in the shader */
    quad(ui.tex, x, y, x + w, y + h, 0, 0, 1, 1, color, MODE_COLOR);
}

void ui_frame(float x, float y, float w, float h, float t, const float color[4])
{
    ui_rect(x, y, w, t, color);
    ui_rect(x, y + h - t, w, t, color);
    ui_rect(x, y + t, t, h - 2 * t, color);
    ui_rect(x + w - t, y + t, t, h - 2 * t, color);
}

void ui_image(GLuint tex, float x, float y, float w, float h, const float tint[4], bool srgb)
{
    static const float white[4] = { 1, 1, 1, 1 };
    quad(tex, x, y, x + w, y + h, 0, 1, 1, 0, tint ? tint : white, srgb ? MODE_IMAGE_SRGB : MODE_IMAGE);
}

float ui_text(int font, float x, float y, const float color[4], const char *text)
{
    float cx = x, cy = y + ui.ascent[font];
    for (const char *s = text; *s; s++) {
        int ch = (unsigned char)*s;
        if (ch == '\n') {
            cx = x;
            cy += ui_line_height(font);
            continue;
        }
        if (ch < 32 || ch >= 128)
            ch = '?';
        stbtt_aligned_quad q;
        stbtt_GetPackedQuad(ui.chars[font], ATLAS, ATLAS, ch - 32, &cx, &cy, &q, 0);
        quad(ui.tex, q.x0, q.y0, q.x1, q.y1, q.s0, q.t0, q.s1, q.t1, color, MODE_FONT);
    }
    return cx - x;
}

float ui_text_width(int font, const char *text)
{
    float cx = 0, cy = 0, best = 0;
    for (const char *s = text; *s; s++) {
        int ch = (unsigned char)*s;
        if (ch == '\n') {
            best = fmaxf(best, cx);
            cx = 0;
            continue;
        }
        if (ch < 32 || ch >= 128)
            ch = '?';
        stbtt_aligned_quad q;
        stbtt_GetPackedQuad(ui.chars[font], ATLAS, ATLAS, ch - 32, &cx, &cy, &q, 0);
    }
    return fmaxf(best, cx);
}

float ui_line_height(int font)
{
    return SIZES[font] * 1.15f;
}

float ui_text_shadow(int font, float x, float y, const float color[4], const char *text)
{
    float shadow[4] = { 0, 0, 0, color[3] * 0.7f };
    ui_text(font, x + 2, y + 2, shadow, text);
    return ui_text(font, x, y, color, text);
}
