#include "particles.h"
#include "shader.h"

#include <stdlib.h>
#include <string.h>

/* per-particle data sent to the GPU: position + size, color */
typedef struct {
    float pos[3], size;
    float color[4];
} Instance;

float frand(void) { return rand() / (float)RAND_MAX; }
static float srand1(void) { return frand() * 2.0f - 1.0f; }

void particles_init(Particles *ps)
{
    memset(ps, 0, sizeof *ps);
    ps->cap = 8192;
    ps->p = malloc(ps->cap * sizeof *ps->p);
    ps->prog = shader_load("shaders/particle.vert", "shaders/particle.frag");

    glCreateBuffers(1, &ps->vbo);
    glNamedBufferStorage(ps->vbo, ps->cap * sizeof(Instance), NULL, GL_DYNAMIC_STORAGE_BIT);
    glCreateVertexArrays(1, &ps->vao);
    glVertexArrayVertexBuffer(ps->vao, 0, ps->vbo, 0, sizeof(Instance));
    glVertexArrayBindingDivisor(ps->vao, 0, 1);     /* one Instance per quad */
    glEnableVertexArrayAttrib(ps->vao, 0);
    glEnableVertexArrayAttrib(ps->vao, 1);
    glVertexArrayAttribFormat(ps->vao, 0, 4, GL_FLOAT, GL_FALSE, offsetof(Instance, pos));
    glVertexArrayAttribFormat(ps->vao, 1, 4, GL_FLOAT, GL_FALSE, offsetof(Instance, color));
    glVertexArrayAttribBinding(ps->vao, 0, 0);
    glVertexArrayAttribBinding(ps->vao, 1, 0);
}

void particles_free(Particles *ps)
{
    free(ps->p);
    glDeleteBuffers(1, &ps->vbo);
    glDeleteVertexArrays(1, &ps->vao);
    glDeleteProgram(ps->prog);
}

void particles_emit(Particles *ps, const Particle *p)
{
    if (ps->count < ps->cap)
        ps->p[ps->count++] = *p;
}

void particles_update(Particles *ps, float dt)
{
    for (int i = 0; i < ps->count; ) {
        Particle *p = &ps->p[i];
        p->life -= dt;
        if (p->life <= 0.0f) {
            ps->p[i] = ps->p[--ps->count];     /* swap-remove */
            continue;
        }
        p->vel[1] -= p->gravity * dt;
        glm_vec3_scale(p->vel, fmaxf(0.0f, 1.0f - p->drag * dt), p->vel);
        glm_vec3_muladds(p->vel, dt, p->pos);
        i++;
    }
}

static int build(Particles *ps, Instance *out, bool additive)
{
    int n = 0;
    for (int i = 0; i < ps->count; i++) {
        const Particle *p = &ps->p[i];
        if (p->additive != additive)
            continue;
        float t = 1.0f - p->life / p->max_life;
        Instance *in = &out[n++];
        memcpy(in->pos, p->pos, sizeof in->pos);
        in->size = glm_lerp(p->size0, p->size1, t);
        for (int k = 0; k < 4; k++)
            in->color[k] = glm_lerp(p->color0[k], p->color1[k], t);
    }
    return n;
}

void particles_draw(Particles *ps)
{
    if (!ps->count)
        return;
    static Instance buf[8192];

    glUseProgram(ps->prog);
    glBindVertexArray(ps->vao);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    /* covering particles first (smoke, blood), then glowing ones on top */
    for (int pass = 0; pass < 2; pass++) {
        int n = build(ps, buf, pass == 1);
        if (!n)
            continue;
        glNamedBufferSubData(ps->vbo, 0, n * sizeof(Instance), buf);
        if (pass == 0)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        else
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, n);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

/* ---------- effects ---------- */

void fx_flame(Particles *ps, vec3 pos, float size, float dt)
{
    /* a steady stream: bright yellow core fading to red, then a little smoke.
     * 90 per second at size 1; the fraction left over is a dice roll, so each
     * flame gets its own share instead of one counter spilling between fires */
    float want = dt * 90.0f * size;
    int n = (int)want + (frand() < want - (int)want);
    for (int i = 0; i < n; i++) {
        Particle p = {0};
        glm_vec3_copy(pos, p.pos);
        p.pos[0] += srand1() * 0.06f * size;
        p.pos[2] += srand1() * 0.06f * size;
        glm_vec3_copy((vec3){srand1() * 0.15f, 0.6f + frand() * 0.5f, srand1() * 0.15f}, p.vel);
        glm_vec3_scale(p.vel, size, p.vel);
        glm_vec4_copy((vec4){6.0f, 3.2f, 0.9f, 0.9f}, p.color0);
        glm_vec4_copy((vec4){2.5f, 0.4f, 0.05f, 0.0f}, p.color1);
        p.size0 = 0.16f * size;
        p.size1 = 0.03f * size;
        p.max_life = p.life = 0.35f + frand() * 0.35f;
        p.gravity = -0.8f;
        p.drag = 1.0f;
        p.additive = true;
        particles_emit(ps, &p);
    }
    if (frand() < dt * 8.0f * size) {
        Particle p = {0};
        glm_vec3_copy(pos, p.pos);
        p.pos[1] += 0.25f * size;
        glm_vec3_copy((vec3){srand1() * 0.1f, 0.5f, srand1() * 0.1f}, p.vel);
        glm_vec4_copy((vec4){0.1f, 0.09f, 0.08f, 0.35f}, p.color0);
        glm_vec4_copy((vec4){0.2f, 0.2f, 0.2f, 0.0f}, p.color1);
        p.size0 = 0.1f * size;
        p.size1 = 0.6f * size;
        p.max_life = p.life = 1.8f;
        p.gravity = -0.2f;
        p.drag = 0.5f;
        particles_emit(ps, &p);
    }
    if (frand() < dt * 3.0f * size) {       /* the odd ember */
        Particle p = {0};
        glm_vec3_copy(pos, p.pos);
        glm_vec3_copy((vec3){srand1() * 0.5f, 1.0f + frand(), srand1() * 0.5f}, p.vel);
        glm_vec4_copy((vec4){8.0f, 3.0f, 0.5f, 1.0f}, p.color0);
        glm_vec4_copy((vec4){4.0f, 0.6f, 0.1f, 0.0f}, p.color1);
        p.size0 = p.size1 = 0.025f;
        p.max_life = p.life = 1.2f;
        p.gravity = -0.3f;
        p.drag = 0.3f;
        p.additive = true;
        particles_emit(ps, &p);
    }
}

void fx_blood(Particles *ps, vec3 pos, vec3 dir, int count)
{
    for (int i = 0; i < count; i++) {
        Particle p = {0};
        glm_vec3_copy(pos, p.pos);
        glm_vec3_copy((vec3){dir[0] * 2.0f + srand1() * 1.5f, 1.0f + frand() * 2.0f,
                             dir[2] * 2.0f + srand1() * 1.5f}, p.vel);
        glm_vec4_copy((vec4){0.35f, 0.01f, 0.01f, 1.0f}, p.color0);
        glm_vec4_copy((vec4){0.2f, 0.0f, 0.0f, 0.0f}, p.color1);
        p.size0 = 0.06f + frand() * 0.05f;
        p.size1 = 0.02f;
        p.max_life = p.life = 0.5f + frand() * 0.4f;
        p.gravity = 9.0f;
        p.drag = 0.5f;
        particles_emit(ps, &p);
    }
}

void fx_dust(Particles *ps, vec3 pos, int count)
{
    for (int i = 0; i < count; i++) {
        Particle p = {0};
        glm_vec3_copy(pos, p.pos);
        glm_vec3_copy((vec3){srand1() * 1.2f, frand() * 0.8f, srand1() * 1.2f}, p.vel);
        glm_vec4_copy((vec4){0.45f, 0.4f, 0.33f, 0.5f}, p.color0);
        glm_vec4_copy((vec4){0.45f, 0.4f, 0.33f, 0.0f}, p.color1);
        p.size0 = 0.15f;
        p.size1 = 0.7f;
        p.max_life = p.life = 0.8f + frand() * 0.6f;
        p.gravity = -0.1f;
        p.drag = 2.0f;
        particles_emit(ps, &p);
    }
}

void fx_sparks(Particles *ps, vec3 pos, int count)
{
    for (int i = 0; i < count; i++) {
        Particle p = {0};
        glm_vec3_copy(pos, p.pos);
        glm_vec3_copy((vec3){srand1() * 3.0f, frand() * 3.0f, srand1() * 3.0f}, p.vel);
        glm_vec4_copy((vec4){10.0f, 6.0f, 2.0f, 1.0f}, p.color0);
        glm_vec4_copy((vec4){5.0f, 1.0f, 0.1f, 0.0f}, p.color1);
        p.size0 = 0.03f;
        p.size1 = 0.01f;
        p.max_life = p.life = 0.3f + frand() * 0.3f;
        p.gravity = 9.0f;
        p.additive = true;
        particles_emit(ps, &p);
    }
}

void fx_explosion(Particles *ps, vec3 pos)
{
    for (int i = 0; i < 90; i++) {
        Particle p = {0};
        glm_vec3_copy(pos, p.pos);
        vec3 d = { srand1(), frand() * 1.2f, srand1() };
        glm_vec3_normalize(d);
        glm_vec3_scale(d, 3.0f + frand() * 7.0f, p.vel);
        glm_vec4_copy((vec4){20.0f, 9.0f, 2.5f, 1.0f}, p.color0);
        glm_vec4_copy((vec4){3.0f, 0.4f, 0.05f, 0.0f}, p.color1);
        p.size0 = 0.4f + frand() * 0.4f;
        p.size1 = 1.2f;
        p.max_life = p.life = 0.35f + frand() * 0.4f;
        p.drag = 4.0f;
        p.gravity = -1.0f;
        p.additive = true;
        particles_emit(ps, &p);
    }
    for (int i = 0; i < 40; i++) {
        Particle p = {0};
        glm_vec3_copy(pos, p.pos);
        glm_vec3_copy((vec3){srand1() * 3.0f, 1.0f + frand() * 3.0f, srand1() * 3.0f}, p.vel);
        glm_vec4_copy((vec4){0.08f, 0.07f, 0.06f, 0.8f}, p.color0);
        glm_vec4_copy((vec4){0.25f, 0.24f, 0.22f, 0.0f}, p.color1);
        p.size0 = 0.6f;
        p.size1 = 3.0f;
        p.max_life = p.life = 2.0f + frand() * 1.5f;
        p.drag = 1.5f;
        p.gravity = -0.4f;
        particles_emit(ps, &p);
    }
    fx_sparks(ps, pos, 40);
}

void fx_sparkle(Particles *ps, vec3 pos)
{
    Particle p = {0};
    glm_vec3_copy(pos, p.pos);
    p.pos[0] += srand1() * 0.25f;
    p.pos[1] += srand1() * 0.2f;
    p.pos[2] += srand1() * 0.25f;
    glm_vec3_copy((vec3){0.0f, 0.2f, 0.0f}, p.vel);
    glm_vec4_copy((vec4){4.0f, 3.5f, 1.5f, 1.0f}, p.color0);
    glm_vec4_copy((vec4){2.0f, 1.5f, 0.5f, 0.0f}, p.color1);
    p.size0 = 0.05f;
    p.size1 = 0.0f;
    p.max_life = p.life = 0.8f;
    p.additive = true;
    particles_emit(ps, &p);
}

void fx_notes(Particles *ps, vec3 pos)
{
    /* colorful rising sparks, a stand-in for music notes */
    Particle p = {0};
    glm_vec3_copy(pos, p.pos);
    glm_vec3_copy((vec3){srand1() * 0.6f, 1.2f, srand1() * 0.6f}, p.vel);
    float h = frand();
    glm_vec4_copy((vec4){2.0f + 3.0f * h, 3.0f * (1.0f - h), 4.0f * h, 1.0f}, p.color0);
    glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 0.0f}, p.color1);
    p.size0 = 0.09f;
    p.size1 = 0.04f;
    p.max_life = p.life = 1.6f;
    p.additive = true;
    particles_emit(ps, &p);
}

void fx_slime(Particles *ps, vec3 pos, vec3 dir, int count)
{
    for (int i = 0; i < count; i++) {
        Particle p = {0};
        glm_vec3_copy(pos, p.pos);
        glm_vec3_copy((vec3){dir[0] * 2.0f + srand1() * 1.8f, 1.5f + frand() * 2.0f, dir[2] * 2.0f + srand1() * 1.8f}, p.vel);
        glm_vec4_copy((vec4){0.2f, 0.8f, 0.25f, 0.9f}, p.color0);
        glm_vec4_copy((vec4){0.1f, 0.5f, 0.1f, 0.0f}, p.color1);
        p.size0 = 0.07f + frand() * 0.06f;
        p.size1 = 0.03f;
        p.max_life = p.life = 0.6f + frand() * 0.4f;
        p.gravity = 9.0f;
        p.drag = 0.4f;
        particles_emit(ps, &p);
    }
}

void fx_magic(Particles *ps, vec3 pos, vec3 color, float spread, int count)
{
    for (int i = 0; i < count; i++) {
        Particle p = {0};
        glm_vec3_copy(pos, p.pos);
        p.pos[0] += srand1() * spread;
        p.pos[1] += srand1() * spread;
        p.pos[2] += srand1() * spread;
        glm_vec3_copy((vec3){srand1() * 0.4f, 0.3f + frand() * 0.5f, srand1() * 0.4f}, p.vel);
        glm_vec4_copy((vec4){color[0], color[1], color[2], 1.0f}, p.color0);
        glm_vec4_copy((vec4){color[0] * 0.3f, color[1] * 0.3f, color[2] * 0.3f, 0.0f}, p.color1);
        p.size0 = 0.05f + frand() * 0.05f;
        p.size1 = 0.0f;
        p.max_life = p.life = 0.5f + frand() * 0.6f;
        p.drag = 1.0f;
        p.additive = true;
        particles_emit(ps, &p);
    }
}

void fx_frost_ring(Particles *ps, vec3 center, float radius)
{
    for (int i = 0; i < 160; i++) {
        float a = frand() * 6.2832f;
        Particle p = {0};
        glm_vec3_copy((vec3){center[0] + cosf(a) * 0.5f, center[1] + 0.2f + frand() * 0.5f, center[2] + sinf(a) * 0.5f}, p.pos);
        float v = radius * (1.6f + frand() * 0.8f);
        glm_vec3_copy((vec3){cosf(a) * v, frand() * 0.8f, sinf(a) * v}, p.vel);
        glm_vec4_copy((vec4){1.5f, 3.0f, 5.0f, 1.0f}, p.color0);
        glm_vec4_copy((vec4){0.6f, 0.9f, 1.4f, 0.0f}, p.color1);
        p.size0 = 0.15f + frand() * 0.15f;
        p.size1 = 0.05f;
        p.max_life = p.life = 0.5f + frand() * 0.3f;
        p.drag = 2.5f;
        p.additive = true;
        particles_emit(ps, &p);
    }
}

void fx_bolt(Particles *ps, vec3 a, vec3 b, vec3 color)
{
    /* a jagged path of bright points between a and b */
    vec3 d;
    glm_vec3_sub(b, a, d);
    float len = glm_vec3_norm(d);
    int n = (int)(len * 6.0f) + 4;
    vec3 jitter = { 0, 0, 0 };
    for (int i = 0; i <= n; i++) {
        float t = (float)i / n;
        float fade = sinf(t * 3.14159f);
        jitter[0] += srand1() * 0.12f;
        jitter[1] += srand1() * 0.12f;
        jitter[2] += srand1() * 0.12f;
        glm_vec3_scale(jitter, 0.85f, jitter);
        Particle p = {0};
        glm_vec3_copy(a, p.pos);
        glm_vec3_muladds(d, t, p.pos);
        glm_vec3_muladds(jitter, fade, p.pos);
        glm_vec4_copy((vec4){color[0], color[1], color[2], 1.0f}, p.color0);
        glm_vec4_copy((vec4){color[0] * 0.5f, color[1] * 0.5f, color[2], 0.0f}, p.color1);
        p.size0 = 0.09f;
        p.size1 = 0.02f;
        p.max_life = p.life = 0.18f + frand() * 0.1f;
        p.additive = true;
        particles_emit(ps, &p);
    }
}

void fx_mote(Particles *ps, vec3 pos, vec3 color)
{
    Particle p = {0};
    glm_vec3_copy(pos, p.pos);
    p.pos[0] += srand1() * 0.03f;
    p.pos[1] += srand1() * 0.02f;
    p.pos[2] += srand1() * 0.03f;
    glm_vec3_copy((vec3){srand1() * 0.05f, 0.12f + frand() * 0.1f, srand1() * 0.05f}, p.vel);
    glm_vec4_copy((vec4){color[0], color[1], color[2], 1.0f}, p.color0);
    glm_vec4_copy((vec4){color[0] * 0.3f, color[1] * 0.3f, color[2] * 0.3f, 0.0f}, p.color1);
    p.size0 = 0.012f;
    p.size1 = 0.0f;
    p.max_life = p.life = 0.5f + frand() * 0.4f;
    p.additive = true;
    particles_emit(ps, &p);
}
