#include "terrain.h"
#include "meshgen.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ---------- noise ---------- */

/* repeatable pseudo-random value in 0..1 for a grid point */
static float hash(int x, int z, unsigned seed)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)z * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xffffff) / (float)0xffffff;
}

/* value noise: random heights on an integer grid, smoothly blended in between */
static float value_noise(float x, float z, unsigned seed)
{
    int xi = (int)floorf(x), zi = (int)floorf(z);
    float fx = x - xi, fz = z - zi;
    /* smoothstep so the slopes don't have visible creases at grid lines */
    float u = fx * fx * (3.0f - 2.0f * fx);
    float v = fz * fz * (3.0f - 2.0f * fz);

    float a = hash(xi,     zi,     seed);
    float b = hash(xi + 1, zi,     seed);
    float c = hash(xi,     zi + 1, seed);
    float d = hash(xi + 1, zi + 1, seed);
    return glm_lerp(glm_lerp(a, b, u), glm_lerp(c, d, u), v) * 2.0f - 1.0f;   /* -1..1 */
}

/* fractal noise: several layers of noise, each finer and fainter than the last */
static float fbm(float x, float z, unsigned seed)
{
    float sum = 0.0f, amp = 0.5f, freq = 1.0f;
    for (int i = 0; i < 6; i++) {
        sum += amp * value_noise(x * freq, z * freq, seed + i);
        freq *= 2.0f;
        amp *= 0.5f;
    }
    return sum;
}

/* ---------- mesh ---------- */

static float grid_height(const Terrain *t, int x, int z)
{
    x = x < 0 ? 0 : x >= t->res ? t->res - 1 : x;
    z = z < 0 ? 0 : z >= t->res ? t->res - 1 : z;
    return t->heights[z * t->res + x];
}

void terrain_create(Terrain *t, int res, float size, float height, float base_y,
                    float flat_radius, unsigned seed)
{
    const char *sets[3] = { "assets/textures/leafy_grass", "assets/textures/rock_face",
                            "assets/textures/forest_ground_04" };
    for (int i = 0; i < 3; i++) {
        Material m;
        material_from_dir(&m, sets[i]);
        t->tex[i * 3 + 0] = m.base_tex;
        t->tex[i * 3 + 1] = m.normal_tex;
        t->tex[i * 3 + 2] = m.mr_tex;
    }

    t->res = res;
    t->size = size;
    t->heights = malloc(res * res * sizeof *t->heights);

    float step = size / (res - 1);
    float half = size * 0.5f;

    for (int z = 0; z < res; z++) {
        for (int x = 0; x < res; x++) {
            float wx = x * step - half;
            float wz = z * step - half;

            float h = fbm(wx * 0.02f, wz * 0.02f, seed) * height;

            /* flatten a clearing around the origin, blending into the hills */
            float dist = sqrtf(wx * wx + wz * wz);
            float k = glm_clamp((dist - flat_radius) / (flat_radius * 0.5f), 0.0f, 1.0f);
            k = k * k * (3.0f - 2.0f * k);

            t->heights[z * res + x] = base_y + h * k;
        }
    }

    /* vertices: position + normal, interleaved */
    float *verts = malloc(res * res * 6 * sizeof *verts);
    for (int z = 0; z < res; z++) {
        for (int x = 0; x < res; x++) {
            float *v = &verts[(z * res + x) * 6];
            v[0] = x * step - half;
            v[1] = grid_height(t, x, z);
            v[2] = z * step - half;

            /* normal from the slope between neighbors (central differences) */
            vec3 n = {
                grid_height(t, x - 1, z) - grid_height(t, x + 1, z),
                2.0f * step,
                grid_height(t, x, z - 1) - grid_height(t, x, z + 1),
            };
            glm_vec3_normalize(n);
            glm_vec3_copy(n, &v[3]);
        }
    }

    /* two triangles per grid cell, counter-clockwise seen from above */
    t->index_count = (res - 1) * (res - 1) * 6;
    GLuint *idx = malloc(t->index_count * sizeof *idx);
    GLuint *p = idx;
    for (int z = 0; z < res - 1; z++) {
        for (int x = 0; x < res - 1; x++) {
            GLuint i = z * res + x;
            *p++ = i;     *p++ = i + res;     *p++ = i + 1;
            *p++ = i + 1; *p++ = i + res;     *p++ = i + res + 1;
        }
    }

    glCreateBuffers(1, &t->vbo);
    glNamedBufferStorage(t->vbo, res * res * 6 * sizeof *verts, verts, 0);
    glCreateBuffers(1, &t->ebo);
    glNamedBufferStorage(t->ebo, t->index_count * sizeof *idx, idx, 0);

    glCreateVertexArrays(1, &t->vao);
    glVertexArrayVertexBuffer(t->vao, 0, t->vbo, 0, 6 * sizeof(float));
    glVertexArrayElementBuffer(t->vao, t->ebo);
    glEnableVertexArrayAttrib(t->vao, 0);
    glEnableVertexArrayAttrib(t->vao, 1);
    glVertexArrayAttribFormat(t->vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribFormat(t->vao, 1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    glVertexArrayAttribBinding(t->vao, 0, 0);
    glVertexArrayAttribBinding(t->vao, 1, 0);

    free(verts);
    free(idx);
}

void terrain_free(Terrain *t)
{
    glDeleteVertexArrays(1, &t->vao);
    glDeleteBuffers(1, &t->vbo);
    glDeleteBuffers(1, &t->ebo);
    free(t->heights);
    *t = (Terrain){0};
}

float terrain_height(const Terrain *t, float x, float z)
{
    /* world -> grid coordinates, then blend the 4 surrounding points */
    float step = t->size / (t->res - 1);
    float gx = (x + t->size * 0.5f) / step;
    float gz = (z + t->size * 0.5f) / step;
    int xi = (int)floorf(gx), zi = (int)floorf(gz);
    float fx = gx - xi, fz = gz - zi;

    float a = grid_height(t, xi,     zi);
    float b = grid_height(t, xi + 1, zi);
    float c = grid_height(t, xi,     zi + 1);
    float d = grid_height(t, xi + 1, zi + 1);
    return glm_lerp(glm_lerp(a, b, fx), glm_lerp(c, d, fx), fz);
}

void terrain_draw(const Terrain *t, GLuint prog, mat4 view_proj)
{
    glUseProgram(prog);
    glProgramUniformMatrix4fv(prog, 0, 1, GL_FALSE, (const float *)view_proj);
    /* units 0..7 and 9: grass, rock, forest floor (albedo, normal, arm each).
     * unit 8 is the sun's shadow map, so the last texture skips over it */
    glBindTextures(0, 8, t->tex);
    glBindTextureUnit(9, t->tex[8]);
    glProgramUniform4fv(prog, 20, t->hole_count, (const float *)t->holes);
    glProgramUniform1i(prog, 28, t->hole_count);
    glBindVertexArray(t->vao);
    glDrawElements(GL_TRIANGLES, t->index_count, GL_UNSIGNED_INT, NULL);
}

void terrain_add_hole(Terrain *t, float x0, float z0, float x1, float z1)
{
    if (t->hole_count >= MAX_HOLES) {
        fprintf(stderr, "terrain: more than %d holes, ignoring one\n", MAX_HOLES);
        return;
    }
    float *h = t->holes[t->hole_count++];
    h[0] = x0;
    h[1] = z0;
    h[2] = x1;
    h[3] = z1;
}
