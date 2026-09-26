#include "shader.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMMON_PATH "shaders/common.glsl"

static char *read_file(const char *path)
{
    char *data = SDL_LoadFile(path, NULL);
    if (!data) {
        fprintf(stderr, "can't read %s: %s\n", path, SDL_GetError());
        exit(1);
    }
    return data;
}

static GLuint compile(GLenum type, const char *path, const char *common)
{
    char *src = read_file(path);

    /* split after the first line (#version) and slip the common code in between */
    char *body = strchr(src, '\n');
    body = body ? body + 1 : src + strlen(src);
    int version_len = (int)(body - src);

    const char *parts[4] = { src, "#line 1 1\n", common, "\n#line 2 0\n" };
    const char *all[5] = { parts[0], parts[1], parts[2], parts[3], body };
    GLint lens[5] = { version_len, -1, -1, -1, -1 };

    GLuint s = glCreateShader(type);
    glShaderSource(s, 5, all, lens);
    glCompileShader(s);
    SDL_free(src);

    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        /* line numbers: "0:N" is in the shader file, "1:N" in common.glsl */
        fprintf(stderr, "%s: compile error (source 1 = %s):\n%s\n", path, COMMON_PATH, log);
        exit(1);
    }
    return s;
}

GLuint shader_load(const char *vs_path, const char *fs_path)
{
    char *common = read_file(COMMON_PATH);
    GLuint v = compile(GL_VERTEX_SHADER, vs_path, common);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs_path, common);
    SDL_free(common);

    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);

    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof log, NULL, log);
        fprintf(stderr, "%s + %s: link error:\n%s\n", vs_path, fs_path, log);
        exit(1);
    }
    return p;
}
