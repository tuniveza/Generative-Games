#ifndef PARTICLES_H
#define PARTICLES_H

#include <glad/gl.h>
#include <cglm/cglm.h>
#include <stdbool.h>

typedef struct {
    vec3 pos, vel;
    vec4 color0, color1;    /* start and end color (rgb can exceed 1 to glow) */
    float size0, size1;
    float life, max_life;   /* seconds */
    float gravity;          /* + pulls down, - rises */
    float drag;             /* velocity lost per second, 0..1+ */
    bool additive;          /* fire and sparks add light; smoke and blood cover */
} Particle;

typedef struct {
    Particle *p;
    int count, cap;
    GLuint vao, vbo, prog;
} Particles;

/* random number in 0..1 (shared by everything that jitters or rolls dice) */
float frand(void);

void particles_init(Particles *ps);
void particles_free(Particles *ps);
void particles_emit(Particles *ps, const Particle *p);
void particles_update(Particles *ps, float dt);
void particles_draw(Particles *ps);     /* uses the frame's camera */

/* ready-made effects */
void fx_flame(Particles *ps, vec3 pos, float size, float dt);   /* call every frame */
void fx_blood(Particles *ps, vec3 pos, vec3 dir, int count);
void fx_dust(Particles *ps, vec3 pos, int count);
void fx_sparks(Particles *ps, vec3 pos, int count);
void fx_explosion(Particles *ps, vec3 pos);
void fx_sparkle(Particles *ps, vec3 pos);                       /* item glint */
void fx_notes(Particles *ps, vec3 pos);                         /* boombox */
void fx_slime(Particles *ps, vec3 pos, vec3 dir, int count);    /* green goo */
void fx_magic(Particles *ps, vec3 pos, vec3 color, float spread, int count);  /* sparkling motes */
void fx_mote(Particles *ps, vec3 pos, vec3 color);              /* one tiny spark (in the hand) */
void fx_frost_ring(Particles *ps, vec3 center, float radius);   /* ice burst */
void fx_bolt(Particles *ps, vec3 a, vec3 b, vec3 color);        /* a jagged lightning line */

#endif
