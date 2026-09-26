#ifndef RENDERER_H
#define RENDERER_H

#include <glad/gl.h>
#include <cglm/cglm.h>

#define MAX_LIGHTS 32           /* must match shaders/common.glsl */
#define BLOOM_LEVELS 6
#define SHADOW_SIZE 4096

/* mirrors the Frame uniform block in shaders/common.glsl (std140) */
typedef struct {
    mat4 view_proj;
    mat4 inv_view_proj;
    mat4 shadow_matrix;
    vec4 camera_pos;
    vec4 sun_dir;
    vec4 sun_color;
    vec4 sky_color;
    vec4 ground_color;
    vec4 fog;
    vec4 misc;
    vec4 zenith;
    vec4 weather;           /* wetness, cloud cover, lightning flash, rain */
    vec4 cover_grid;        /* x0, z0, 1 / cell size, 1 = present */
    vec4 sea;               /* sea level, camera underwater 0..1, volcanic ash 0..1, in the undersea palace 0..1 */
    vec4 height_grid;       /* terrain heights: x0, z0, 1 / spacing, samples per side */
    vec4 light_pos[MAX_LIGHTS];
    vec4 light_color[MAX_LIGHTS];
} FrameUniforms;

/* lighting mood of the scene */
typedef struct {
    vec3 sun_dir;           /* towards the sun */
    vec3 sun_color;         /* includes intensity */
    vec3 sky_ambient;
    vec3 ground_ambient;
    vec3 fog_color;
    float fog_density;
    vec3 zenith;            /* sky color straight up */
    float exposure;
    float shadow;           /* 0..1 how strong sun shadows are (weak under cloud) */
    float wetness, cloud, flash, rain;
} Environment;

typedef struct {
    float damage;           /* 0..1 */
    float slowmo;
    float dead;
    float underwater;       /* 0..1 the camera is under the sea */
    float ash;              /* 0..1 the volcano's ash cloud */
    float toxic;            /* 0..1 choking on volcanic fumes */
} PostEffects;

typedef struct {
    int width, height;

    GLuint hdr_fbo, hdr_color, hdr_depth;       /* scene is lit into floating point */
    GLuint ldr_fbo, ldr_color;                  /* final 8-bit image, UI goes on top */
    GLuint shadow_fbo, shadow_tex;
    GLuint bloom_fbo, bloom_tex[BLOOM_LEVELS];
    int bloom_w[BLOOM_LEVELS], bloom_h[BLOOM_LEVELS];

    GLuint ubo, empty_vao;
    GLuint cover_tex, height_tex;
    GLuint sky_prog, down_prog, up_prog, post_prog;

    FrameUniforms frame;
    Environment env;
    mat4 light_view_proj;   /* for drawing the shadow pass */
} Renderer;

void renderer_init(Renderer *r, int width, int height);
void renderer_resize(Renderer *r, int width, int height);
void renderer_free(Renderer *r);

void renderer_set_camera(Renderer *r, mat4 view, mat4 proj, vec3 eye);
void renderer_set_environment(Renderer *r, const Environment *env, float time);
void renderer_clear_lights(Renderer *r);
/* the level's "what's overhead" grid (for rain, wetness and indoor light) */
void renderer_set_cover(Renderer *r, GLuint tex, float x0, float z0, float cell);
void renderer_add_light(Renderer *r, vec3 pos, vec3 color, float radius);
/* how open the sky is above the camera (0..1): thins the fog indoors */
void renderer_set_camera_sky(Renderer *r, float open);
/* the terrain's height texture, so water knows how deep it is */
void renderer_set_heightmap(Renderer *r, GLuint tex, float x0, float z0, float spacing, int samples);
void renderer_set_sea(Renderer *r, float sea_y, float underwater, float ash, float palace);

/* shadow pass covering a circle of `radius` around `center`; draw casters with
 * light_view_proj and the shadow program, then call renderer_begin_scene */
void renderer_begin_shadow(Renderer *r, vec3 center, float radius);

/* uploads the frame uniforms, binds the HDR target, clears, draws the sky */
void renderer_begin_scene(Renderer *r);

/* clears depth so first-person hands never clip into walls */
void renderer_begin_viewmodel(Renderer *r);

/* bloom + tone mapping into the final image; leaves it bound so UI can draw on it */
void renderer_finish(Renderer *r, const PostEffects *fx);

/* copies the final image to the window */
void renderer_present(Renderer *r);

/* reads the final image (RGBA8, bottom row first) into `pixels` (width*height*4) */
void renderer_read_pixels(Renderer *r, unsigned char *pixels);

void renderer_fullscreen(Renderer *r);   /* draws a screen-covering triangle */

#endif
