#include "ocean.h"
#include "shader.h"
#include "world.h"

#include <stdlib.h>

#define GRID 256                /* vertices per side */
#define NEAR_REACH 300.0f       /* the grid's spacing grows from ~2.3 m at the middle... */
#define FAR_REACH 1100.0f       /* ...to this much extra at the edge (past it all is fog) */

/* the swell: direction (x, z), wavelength, height. must match shaders/ocean.vert */
static const float WAVES[4][4] = {
    {  0.20f, -1.00f, 31.0f, 0.20f },
    {  0.25f, -0.97f, 18.0f, 0.14f },
    { -0.50f, -0.86f,  9.5f, 0.08f },
    {  0.80f, -0.60f,  5.3f, 0.045f },
};

/* grid coordinate -1..1 -> meters from the camera */
static float reach(float u)
{
    float a = fabsf(u);
    return copysignf(a * NEAR_REACH + a * a * a * a * a * FAR_REACH, u);
}

void ocean_init(Ocean *o)
{
    *o = (Ocean){0};
    o->spacing = NEAR_REACH * 2.0f / (GRID - 1);
    o->prog = shader_load("shaders/ocean.vert", "shaders/ocean.frag");

    float *verts = malloc(GRID * GRID * 2 * sizeof *verts);
    for (int z = 0; z < GRID; z++)
        for (int x = 0; x < GRID; x++) {
            verts[(z * GRID + x) * 2 + 0] = reach(x / (GRID - 1.0f) * 2.0f - 1.0f);
            verts[(z * GRID + x) * 2 + 1] = reach(z / (GRID - 1.0f) * 2.0f - 1.0f);
        }
    o->count = (GRID - 1) * (GRID - 1) * 6;
    GLuint *idx = malloc(o->count * sizeof *idx), *p = idx;
    for (int z = 0; z < GRID - 1; z++)
        for (int x = 0; x < GRID - 1; x++) {
            GLuint i = z * GRID + x;
            *p++ = i;     *p++ = i + GRID; *p++ = i + 1;
            *p++ = i + 1; *p++ = i + GRID; *p++ = i + GRID + 1;
        }

    glCreateBuffers(1, &o->vbo);
    glNamedBufferStorage(o->vbo, GRID * GRID * 2 * sizeof *verts, verts, 0);
    glCreateBuffers(1, &o->ebo);
    glNamedBufferStorage(o->ebo, o->count * sizeof *idx, idx, 0);
    glCreateVertexArrays(1, &o->vao);
    glVertexArrayVertexBuffer(o->vao, 0, o->vbo, 0, 2 * sizeof(float));
    glVertexArrayElementBuffer(o->vao, o->ebo);
    glEnableVertexArrayAttrib(o->vao, 0);
    glVertexArrayAttribFormat(o->vao, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(o->vao, 0, 0);
    free(verts);
    free(idx);
}

void ocean_free(Ocean *o)
{
    glDeleteVertexArrays(1, &o->vao);
    glDeleteBuffers(1, &o->vbo);
    glDeleteBuffers(1, &o->ebo);
    glDeleteProgram(o->prog);
}

float ocean_height(float x, float z, float time, float storm, float depth)
{
    float h = 0.0f;
    float scale = (0.7f + storm * 2.6f) * glm_clamp(depth / 3.0f, 0.15f, 1.0f);
    for (int i = 0; i < 4; i++) {
        float k = 2.0f * GLM_PIf / WAVES[i][2];
        float c = sqrtf(9.81f / k);
        float phase = k * (WAVES[i][0] * x + WAVES[i][1] * z - c * time);
        h += WAVES[i][3] * sinf(phase);
    }
    return SEA_Y + h * scale;
}

void ocean_draw(Ocean *o, vec3 eye, mat4 view_proj, float time, float storm)
{
    /* follow the camera in whole grid steps, so the water near you doesn't swim */
    float cx = floorf(eye[0] / o->spacing) * o->spacing;
    float cz = floorf(eye[2] / o->spacing) * o->spacing;

    glUseProgram(o->prog);
    glProgramUniformMatrix4fv(o->prog, 0, 1, GL_FALSE, (const float *)view_proj);
    glProgramUniform4f(o->prog, 1, cx, cz, storm, time);
    glProgramUniform4fv(o->prog, 2, 1, o->whirl);
    glBindVertexArray(o->vao);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDrawElements(GL_TRIANGLES, o->count, GL_UNSIGNED_INT, NULL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
