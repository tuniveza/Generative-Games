#include "weather.h"
#include "shader.h"

#include <stdlib.h>
#include <string.h>

#define MAX_DROPS 5000
#define RAIN_RADIUS 20.0f
#define RAIN_HEIGHT 14.0f
#define DROP_SPEED 16.0f

typedef struct {
    float rain, cloud, fog;
} Target;

static const Target TARGETS[WEATHER_COUNT] = {
    [WEATHER_RAIN]  = { 0.7f, 0.85f, 0.25f },
    [WEATHER_STORM] = { 1.0f, 1.0f, 0.35f },
    [WEATHER_FOG]   = { 0.0f, 0.7f, 1.0f },
    [WEATHER_CLEAR] = { 0.0f, 0.15f, 0.0f },
};

const char *weather_name(WeatherType type)
{
    static const char *names[WEATHER_COUNT] = { "Rain", "Thunderstorm", "Fog", "Clear skies" };
    return names[type];
}

void weather_init(Weather *w)
{
    memset(w, 0, sizeof *w);
    w->type = WEATHER_RAIN;
    w->rain = TARGETS[WEATHER_RAIN].rain;
    w->cloud = TARGETS[WEATHER_RAIN].cloud;
    w->fog = TARGETS[WEATHER_RAIN].fog;
    w->wet = 1.0f;
    w->next_bolt = 6.0f;
    w->auto_change = true;
    w->change_t = 240.0f;
    glm_vec3_copy((vec3){0.12f, -1.0f, 0.05f}, w->fall);
    glm_vec3_normalize(w->fall);

    w->drops = malloc(MAX_DROPS * sizeof *w->drops);
    w->prog = shader_load("shaders/rain.vert", "shaders/rain.frag");
    glCreateBuffers(1, &w->vbo);
    glNamedBufferStorage(w->vbo, MAX_DROPS * sizeof(Drop), NULL, GL_DYNAMIC_STORAGE_BIT);
    glCreateVertexArrays(1, &w->vao);
    glVertexArrayVertexBuffer(w->vao, 0, w->vbo, 0, sizeof(Drop));
    glVertexArrayBindingDivisor(w->vao, 0, 1);
    glEnableVertexArrayAttrib(w->vao, 0);
    glVertexArrayAttribFormat(w->vao, 0, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(w->vao, 0, 0);
}

void weather_free(Weather *w)
{
    free(w->drops);
    glDeleteBuffers(1, &w->vbo);
    glDeleteVertexArrays(1, &w->vao);
    glDeleteProgram(w->prog);
}

void weather_set(Weather *w, WeatherType type)
{
    w->type = type;
    w->change_t = 180.0f + frand() * 180.0f;
}

/* where a drop at (x, z) stops: a roof, a floor, the ground */
static float landing(const Level *lv, const Terrain *t, float x, float z)
{
    float g = level_hole(lv, x, z) ? CRYPT_Y : terrain_height(t, x, z);
    return fmaxf(g, level_cover(lv, x, z));
}

static void spawn_drop(Drop *d, vec3 cam, bool anywhere)
{
    float a = frand() * 6.2832f, r = sqrtf(frand()) * RAIN_RADIUS;
    d->x = cam[0] + cosf(a) * r;
    d->z = cam[2] + sinf(a) * r;
    d->y = cam[1] + (anywhere ? frand() * RAIN_HEIGHT * 1.4f - RAIN_HEIGHT * 0.4f : RAIN_HEIGHT);
    d->len = 0.35f + frand() * 0.25f;
}

void weather_update(Weather *w, float dt, vec3 cam, const Level *lv, const Terrain *t, Particles *ps)
{
    if (w->auto_change && (w->change_t -= dt) <= 0.0f) {
        /* rain is the ruins' usual weather; now and then something else */
        static const WeatherType cycle[] = { WEATHER_RAIN, WEATHER_STORM, WEATHER_RAIN, WEATHER_FOG,
                                             WEATHER_RAIN, WEATHER_CLEAR };
        static int next = 1;
        weather_set(w, cycle[next]);
        next = (next + 1) % (int)(sizeof cycle / sizeof cycle[0]);
    }

    const Target *tg = &TARGETS[w->type];
    float k = 1.0f - expf(-dt * 0.35f);
    w->rain += (tg->rain - w->rain) * k;
    w->cloud += (tg->cloud - w->cloud) * k;
    w->fog += (tg->fog - w->fog) * k;
    /* ground gets wet quickly and dries slowly */
    float wet_target = w->rain > 0.05f ? 1.0f : 0.0f;
    w->wet += (wet_target - w->wet) * (1.0f - expf(-dt * (wet_target > w->wet ? 0.3f : 0.03f)));

    /* lightning: a double flicker, and thunder a moment later depending on distance */
    w->thunder_now = false;
    w->flash = fmaxf(0.0f, w->flash - dt * 5.0f);
    if (w->type == WEATHER_STORM && (w->next_bolt -= dt) <= 0.0f) {
        w->next_bolt = 7.0f + frand() * 14.0f;
        w->flash = 1.0f;
        float dist = 0.3f + frand() * 3.0f;         /* "seconds away" */
        w->thunder_delay = dist;
        w->thunder_volume = 1.2f - dist * 0.25f;
        glm_vec3_copy((vec3){frand() - 0.5f, 0.8f, frand() - 0.5f}, w->bolt_dir);
        glm_vec3_normalize(w->bolt_dir);
    }
    if (w->flash > 0.5f && w->flash < 0.7f && frand() < 0.3f)
        w->flash = 0.9f;                            /* flicker */
    if (w->thunder_delay > 0.0f && (w->thunder_delay -= dt) <= 0.0f)
        w->thunder_now = true;

    /* raindrops: a cloud of streaks around the camera, respawned at the top */
    int want = (int)(MAX_DROPS * w->rain);
    while (w->drop_count < want)
        spawn_drop(&w->drops[w->drop_count++], cam, true);
    if (w->drop_count > want)
        w->drop_count = want;

    vec3 step;
    glm_vec3_scale(w->fall, DROP_SPEED * dt, step);
    for (int i = 0; i < w->drop_count; i++) {
        Drop *d = &w->drops[i];
        d->x += step[0];
        d->y += step[1];
        d->z += step[2];
        float dx = d->x - cam[0], dz = d->z - cam[2];
        if (dx * dx + dz * dz > RAIN_RADIUS * RAIN_RADIUS * 1.2f) {
            spawn_drop(d, cam, true);
            continue;
        }
        float floor_y = landing(lv, t, d->x, d->z);
        if (d->y < floor_y) {
            /* a splash, now and then, on whatever it hit */
            if (frand() < 0.12f) {
                Particle p = {0};
                glm_vec3_copy((vec3){d->x, floor_y + 0.02f, d->z}, p.pos);
                glm_vec3_copy((vec3){(frand() - 0.5f) * 0.8f, 0.8f + frand() * 0.6f, (frand() - 0.5f) * 0.8f}, p.vel);
                glm_vec4_copy((vec4){0.6f, 0.65f, 0.7f, 0.35f}, p.color0);
                glm_vec4_copy((vec4){0.6f, 0.65f, 0.7f, 0.0f}, p.color1);
                p.size0 = 0.03f;
                p.size1 = 0.015f;
                p.max_life = p.life = 0.25f;
                p.gravity = 9.0f;
                particles_emit(ps, &p);
            }
            spawn_drop(d, cam, false);
        }
    }
}

void weather_apply(const Weather *w, const Environment *base, Environment *out)
{
    *out = *base;
    /* under cloud: dimmer, flatter sun; greyer sky; more haze */
    float c = w->cloud;
    glm_vec3_scale(out->sun_color, 1.0f - c * 0.75f, out->sun_color);
    out->shadow = 1.0f - c * 0.7f;
    vec3 grey;
    float lum = (base->fog_color[0] + base->fog_color[1] + base->fog_color[2]) / 3.0f;
    glm_vec3_copy((vec3){lum * 0.85f, lum * 0.9f, lum * 1.0f}, grey);
    glm_vec3_lerp(out->fog_color, grey, c * 0.8f, out->fog_color);
    glm_vec3_lerp(out->zenith, grey, c * 0.75f, out->zenith);
    vec3 amb_grey = { 0, 0, 0 };
    float amb = (base->sky_ambient[0] + base->sky_ambient[1] + base->sky_ambient[2]) / 3.0f;
    glm_vec3_copy((vec3){amb, amb * 1.02f, amb * 1.08f}, amb_grey);
    glm_vec3_lerp(out->sky_ambient, amb_grey, c * 0.6f, out->sky_ambient);
    out->fog_density = base->fog_density * (1.0f + w->rain * 1.8f + w->fog * 6.0f);
    out->wetness = w->wet;
    out->cloud = c;
    out->rain = w->rain;
    out->flash = w->flash;
    /* lightning briefly lights the world from above */
    if (w->flash > 0.0f) {
        vec3 bolt;
        glm_vec3_scale((vec3){6.0f, 6.5f, 8.0f}, w->flash, bolt);
        glm_vec3_add(out->sun_color, bolt, out->sun_color);
        glm_vec3_lerp(out->sun_dir, (float *)w->bolt_dir, w->flash, out->sun_dir);
        out->shadow = fmaxf(out->shadow, w->flash);
    }
}

void weather_draw(Weather *w, vec3 camera)
{
    if (!w->drop_count || camera[1] < FLOOR_Y - 2.0f)
        return;
    glNamedBufferSubData(w->vbo, 0, w->drop_count * sizeof(Drop), w->drops);
    glUseProgram(w->prog);
    glProgramUniform3fv(w->prog, 0, 1, w->fall);
    glBindVertexArray(w->vao);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, w->drop_count);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
