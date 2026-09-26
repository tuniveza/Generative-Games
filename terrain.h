#ifndef TERRAIN_H
#define TERRAIN_H

#include <glad/gl.h>
#include <cglm/cglm.h>

#define MAX_HOLES 8             /* must match u_holes in shaders/terrain.frag */

/* a square heightmap grid centered on the origin, generated from noise */
#define TERRAIN_SETS 5      /* grass, rock, forest floor, sand, burnt ground */

typedef struct {
    GLuint vao, vbo, ebo;
    GLuint tex[TERRAIN_SETS * 3];   /* albedo, normal, arm for each set */
    GLuint height_tex;      /* the heights as a texture (R32F), for water depth in shaders */
    GLsizei index_count;
    int res;                /* vertices per side */
    float size;             /* world units per side */
    float *heights;         /* res * res, row-major (z rows, x columns) */
    float holes[MAX_HOLES][4];  /* x0, z0, x1, z1: cut away (stairwells) */
    int hole_count;
} Terrain;

/* `height` = how tall the hills get; `base_y` = height of the flat clearing at the center,
 * which is `flat_radius` wide and blends into the hills over the next half of that.
 * `shape`, if given, gets each point's height and returns the final one (the coast,
 * the volcano...; see world.c) */
void terrain_create(Terrain *t, int res, float size, float height, float base_y,
                    float flat_radius, unsigned seed, float (*shape)(float x, float z, float h));
void terrain_free(Terrain *t);

/* ground height under a world (x, z), smoothly interpolated between grid points */
float terrain_height(const Terrain *t, float x, float z);

/* draws with shaders/terrain.{vert,frag}; camera and lights come from the frame uniforms.
 * view_proj is passed separately so the shadow pass can reuse it */
void terrain_draw(const Terrain *t, GLuint prog, mat4 view_proj);
void terrain_add_hole(Terrain *t, float x0, float z0, float x1, float z1);

#endif
