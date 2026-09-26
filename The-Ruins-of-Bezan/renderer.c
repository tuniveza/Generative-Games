#include "renderer.h"
#include "shader.h"

#include <string.h>

static GLuint make_texture(GLenum format, int w, int h, GLenum filter)
{
    GLuint tex;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, 1, format, w, h);
    glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, filter);
    glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, filter);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

static void free_targets(Renderer *r)
{
    glDeleteFramebuffers(1, &r->hdr_fbo);
    glDeleteFramebuffers(1, &r->ldr_fbo);
    glDeleteFramebuffers(1, &r->bloom_fbo);
    glDeleteTextures(1, &r->hdr_color);
    glDeleteTextures(1, &r->hdr_depth);
    glDeleteTextures(1, &r->ldr_color);
    glDeleteTextures(BLOOM_LEVELS, r->bloom_tex);
}

static void make_targets(Renderer *r)
{
    int w = r->width, h = r->height;

    r->hdr_color = make_texture(GL_RGBA16F, w, h, GL_LINEAR);
    r->hdr_depth = make_texture(GL_DEPTH_COMPONENT32F, w, h, GL_NEAREST);
    glCreateFramebuffers(1, &r->hdr_fbo);
    glNamedFramebufferTexture(r->hdr_fbo, GL_COLOR_ATTACHMENT0, r->hdr_color, 0);
    glNamedFramebufferTexture(r->hdr_fbo, GL_DEPTH_ATTACHMENT, r->hdr_depth, 0);

    r->ldr_color = make_texture(GL_RGBA8, w, h, GL_LINEAR);
    glCreateFramebuffers(1, &r->ldr_fbo);
    glNamedFramebufferTexture(r->ldr_fbo, GL_COLOR_ATTACHMENT0, r->ldr_color, 0);

    /* each bloom level is half the size of the one before */
    for (int i = 0; i < BLOOM_LEVELS; i++) {
        w = w / 2 > 1 ? w / 2 : 1;
        h = h / 2 > 1 ? h / 2 : 1;
        r->bloom_w[i] = w;
        r->bloom_h[i] = h;
        r->bloom_tex[i] = make_texture(GL_R11F_G11F_B10F, w, h, GL_LINEAR);
    }
    glCreateFramebuffers(1, &r->bloom_fbo);
}

void renderer_init(Renderer *r, int width, int height)
{
    memset(r, 0, sizeof *r);
    r->width = width > 0 ? width : 1;
    r->height = height > 0 ? height : 1;
    make_targets(r);

    /* sun shadow map: depth compared in hardware for smooth filtering */
    r->shadow_tex = make_texture(GL_DEPTH_COMPONENT32F, SHADOW_SIZE, SHADOW_SIZE, GL_LINEAR);
    glTextureParameteri(r->shadow_tex, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTextureParameteri(r->shadow_tex, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glCreateFramebuffers(1, &r->shadow_fbo);
    glNamedFramebufferTexture(r->shadow_fbo, GL_DEPTH_ATTACHMENT, r->shadow_tex, 0);
    glNamedFramebufferDrawBuffer(r->shadow_fbo, GL_NONE);

    glCreateBuffers(1, &r->ubo);
    glNamedBufferStorage(r->ubo, sizeof(FrameUniforms), NULL, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, r->ubo);
    glCreateVertexArrays(1, &r->empty_vao);

    r->sky_prog  = shader_load("shaders/fullscreen.vert", "shaders/sky.frag");
    r->down_prog = shader_load("shaders/fullscreen.vert", "shaders/bloom_down.frag");
    r->up_prog   = shader_load("shaders/fullscreen.vert", "shaders/bloom_up.frag");
    r->post_prog = shader_load("shaders/fullscreen.vert", "shaders/post.frag");
}

void renderer_resize(Renderer *r, int width, int height)
{
    if (width <= 0 || height <= 0 || (width == r->width && height == r->height))
        return;
    free_targets(r);
    r->width = width;
    r->height = height;
    make_targets(r);
}

void renderer_free(Renderer *r)
{
    free_targets(r);
    glDeleteFramebuffers(1, &r->shadow_fbo);
    glDeleteTextures(1, &r->shadow_tex);
    glDeleteBuffers(1, &r->ubo);
    glDeleteVertexArrays(1, &r->empty_vao);
    glDeleteProgram(r->sky_prog);
    glDeleteProgram(r->down_prog);
    glDeleteProgram(r->up_prog);
    glDeleteProgram(r->post_prog);
}

void renderer_set_camera(Renderer *r, mat4 view, mat4 proj, vec3 eye)
{
    glm_mat4_mul(proj, view, r->frame.view_proj);
    glm_mat4_inv(r->frame.view_proj, r->frame.inv_view_proj);
    glm_vec4(eye, 1.0f, r->frame.camera_pos);
}

void renderer_set_environment(Renderer *r, const Environment *env, float time)
{
    r->env = *env;
    FrameUniforms *f = &r->frame;
    glm_vec4(r->env.sun_dir, f->sun_dir[3], f->sun_dir);
    glm_vec3_normalize(f->sun_dir);
    glm_vec4(r->env.sun_color, 0.0f, f->sun_color);
    glm_vec4(r->env.sky_ambient, 0.0f, f->sky_color);
    glm_vec4(r->env.ground_ambient, 0.0f, f->ground_color);
    glm_vec4(r->env.fog_color, r->env.fog_density, f->fog);
    glm_vec4(r->env.zenith, 0.0f, f->zenith);
    glm_vec4_copy((vec4){env->wetness, env->cloud, env->flash, env->rain}, f->weather);
    f->misc[0] = time;
}

void renderer_set_cover(Renderer *r, GLuint tex, float x0, float z0, float cell)
{
    r->cover_tex = tex;
    glm_vec4_copy((vec4){x0, z0, 1.0f / cell, tex ? 1.0f : 0.0f}, r->frame.cover_grid);
}

void renderer_set_camera_sky(Renderer *r, float open)
{
    r->frame.misc[2] = open;
}

void renderer_set_heightmap(Renderer *r, GLuint tex, float x0, float z0, float spacing, int samples)
{
    r->height_tex = tex;
    glm_vec4_copy((vec4){x0, z0, 1.0f / spacing, (float)samples}, r->frame.height_grid);
}

void renderer_set_sea(Renderer *r, float sea_y, float underwater, float ash, float palace)
{
    glm_vec4_copy((vec4){sea_y, underwater, ash, palace}, r->frame.sea);
}

void renderer_clear_lights(Renderer *r)
{
    r->frame.misc[1] = 0.0f;
}

void renderer_add_light(Renderer *r, vec3 pos, vec3 color, float radius)
{
    int i = (int)r->frame.misc[1];
    if (i >= MAX_LIGHTS)
        return;
    glm_vec4(pos, radius, r->frame.light_pos[i]);
    glm_vec4(color, 0.0f, r->frame.light_color[i]);
    r->frame.misc[1] = (float)(i + 1);
}

void renderer_begin_shadow(Renderer *r, vec3 center, float radius)
{
    /* orthographic box looking along the sun direction, covering the area around
     * `center`. it follows the player, but only in whole shadow-map texels, so
     * shadow edges don't crawl as you walk */
    vec3 dir, eye, origin = { 0, 0, 0 };
    glm_vec3_normalize_to(r->env.sun_dir, dir);
    glm_vec3_scale(dir, 200.0f, eye);

    mat4 view, proj;
    glm_lookat(eye, origin, fabsf(dir[1]) > 0.99f ? (vec3){0, 0, 1} : (vec3){0, 1, 0}, view);
    vec3 c;
    glm_mat4_mulv3(view, center, 1.0f, c);
    float texel = radius * 2.0f / SHADOW_SIZE;
    c[0] = floorf(c[0] / texel) * texel;
    c[1] = floorf(c[1] / texel) * texel;
    glm_ortho(c[0] - radius, c[0] + radius, c[1] - radius, c[1] + radius, 1.0f, 400.0f, proj);
    glm_mat4_mul(proj, view, r->light_view_proj);

    /* shaders want 0..1 texture coordinates, not -1..1 */
    mat4 bias = {
        {0.5f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.5f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.5f, 0.0f},
        {0.5f, 0.5f, 0.5f, 1.0f},
    };
    glm_mat4_mul(bias, r->light_view_proj, r->frame.shadow_matrix);
    r->frame.sun_dir[3] = r->env.sun_dir[1] > 0.02f ? r->env.shadow : 0.0f;   /* none when the sun is down */

    glBindFramebuffer(GL_FRAMEBUFFER, r->shadow_fbo);
    glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);
}

void renderer_fullscreen(Renderer *r)
{
    glBindVertexArray(r->empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void renderer_begin_scene(Renderer *r)
{
    glDisable(GL_POLYGON_OFFSET_FILL);
    glNamedBufferSubData(r->ubo, 0, sizeof r->frame, &r->frame);
    glBindTextureUnit(8, r->shadow_tex);
    glBindTextureUnit(10, r->cover_tex);
    glBindTextureUnit(11, r->height_tex);

    glBindFramebuffer(GL_FRAMEBUFFER, r->hdr_fbo);
    glViewport(0, 0, r->width, r->height);
    glClear(GL_DEPTH_BUFFER_BIT);

    /* sky fills every pixel, so no color clear needed */
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgram(r->sky_prog);
    renderer_fullscreen(r);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void renderer_begin_viewmodel(Renderer *r)
{
    (void)r;
    glClear(GL_DEPTH_BUFFER_BIT);
}

void renderer_finish(Renderer *r, const PostEffects *fx)
{
    glDisable(GL_DEPTH_TEST);

    /* bloom: shrink the bright parts down the chain... */
    glBindFramebuffer(GL_FRAMEBUFFER, r->bloom_fbo);
    glUseProgram(r->down_prog);
    GLuint src = r->hdr_color;
    int sw = r->width, sh = r->height;
    for (int i = 0; i < BLOOM_LEVELS; i++) {
        glNamedFramebufferTexture(r->bloom_fbo, GL_COLOR_ATTACHMENT0, r->bloom_tex[i], 0);
        glViewport(0, 0, r->bloom_w[i], r->bloom_h[i]);
        glBindTextureUnit(0, src);
        glProgramUniform2f(r->down_prog, 0, 1.0f / sw, 1.0f / sh);
        glProgramUniform1i(r->down_prog, 1, i == 0);
        renderer_fullscreen(r);
        src = r->bloom_tex[i];
        sw = r->bloom_w[i];
        sh = r->bloom_h[i];
    }

    /* ...then blur each level back up, adding onto the next bigger one */
    glUseProgram(r->up_prog);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (int i = BLOOM_LEVELS - 1; i > 0; i--) {
        glNamedFramebufferTexture(r->bloom_fbo, GL_COLOR_ATTACHMENT0, r->bloom_tex[i - 1], 0);
        glViewport(0, 0, r->bloom_w[i - 1], r->bloom_h[i - 1]);
        glBindTextureUnit(0, r->bloom_tex[i]);
        glProgramUniform2f(r->up_prog, 0, 1.0f / r->bloom_w[i], 1.0f / r->bloom_h[i]);
        renderer_fullscreen(r);
    }
    glDisable(GL_BLEND);

    /* combine, tone map, screen effects */
    glBindFramebuffer(GL_FRAMEBUFFER, r->ldr_fbo);
    glViewport(0, 0, r->width, r->height);
    glUseProgram(r->post_prog);
    glBindTextureUnit(0, r->hdr_color);
    glBindTextureUnit(1, r->bloom_tex[0]);
    glProgramUniform1f(r->post_prog, 0, r->env.exposure);
    glProgramUniform1f(r->post_prog, 1, 0.06f);
    glProgramUniform1f(r->post_prog, 2, fx ? fx->damage : 0.0f);
    glProgramUniform1f(r->post_prog, 3, fx ? fx->slowmo : 0.0f);
    glProgramUniform1f(r->post_prog, 4, fx ? fx->dead : 0.0f);
    glProgramUniform1f(r->post_prog, 5, fx ? fx->underwater : 0.0f);
    glProgramUniform1f(r->post_prog, 6, fx ? fx->ash : 0.0f);
    glProgramUniform1f(r->post_prog, 7, fx ? fx->toxic : 0.0f);
    glProgramUniform1f(r->post_prog, 8, r->frame.misc[0]);
    renderer_fullscreen(r);
}

void renderer_present(Renderer *r)
{
    /* bind + glBlitFramebuffer rather than glBlitNamedFramebuffer(..., 0, ...):
     * Mesa silently drops the named blit into the window (default framebuffer) */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, r->ldr_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, r->width, r->height, 0, 0, r->width, r->height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void renderer_read_pixels(Renderer *r, unsigned char *pixels)
{
    glGetTextureImage(r->ldr_color, 0, GL_RGBA, GL_UNSIGNED_BYTE, r->width * r->height * 4, pixels);
}
