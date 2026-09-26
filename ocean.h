#ifndef OCEAN_H
#define OCEAN_H

#include <glad/gl.h>
#include <cglm/cglm.h>

/* the sea south of the ruins: one big grid that follows the camera, fine near you and
 * coarse out to the horizon, rippled by the same waves boats and swimmers bob on.
 * drawn with shaders/ocean.{vert,frag}; how deep it is comes from the terrain's
 * height texture (renderer_set_heightmap) */
typedef struct {
    GLuint vao, vbo, ebo, prog;
    GLsizei count;
    float spacing;          /* grid spacing near the middle, meters */
    vec4 whirl;             /* x, z, radius, strength: the undersea palace's whirlpool */
} Ocean;

void ocean_init(Ocean *o);
void ocean_free(Ocean *o);

/* storm: 0 = calm lagoon, 1 = whitecaps */
void ocean_draw(Ocean *o, vec3 eye, mat4 view_proj, float time, float storm);

/* the water's surface height at (x, z): the sea level plus the waves there.
 * `depth` is how deep the water is at that spot (waves die down in the shallows) */
float ocean_height(float x, float z, float time, float storm, float depth);

#endif
