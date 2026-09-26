#ifndef SHADER_H
#define SHADER_H

#include <glad/gl.h>

/* compiles and links a vertex + fragment shader pair. shaders/common.glsl is inserted
 * right after the #version line of both, so every shader shares the frame uniforms
 * and lighting code. exits with the compiler log on error. */
GLuint shader_load(const char *vs_path, const char *fs_path);

#endif
