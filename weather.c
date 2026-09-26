#include "weather.h"
#include "shader.h"
#include "world.h"

#include <stdlib.h>
#include <string.h>

#define MAX_DROPS 5000
#define RAIN_RADIUS 20.0f
#define RAIN_HEIGHT 14.0f
#define DROP_SPEED 16.0f

typedef struct {
    float rain, cloud, fog, wind, hail;
} Target;

static const Target TARGETS[WEATHER_COUNT] = {
    [WEATHER_RAIN]      = { 0.7f, 0.85f, 0.25f, 0.15f, 0.0f },
    [WEATHER_STORM]     = { 1.0f, 1.0f, 0.35f, 0.45f, 0.0f },
    [WEATHER_FOG]       = { 0.0f, 0.7f, 1.0f, 0.0f, 0.0f },
    [WEATHER_CLEAR]     = { 0.0f, 0.15f, 0.0f, 0.1f, 0.0f },
    [WEATHER_WINDSTORM] = { 0.0f, 0.6f, 0.15f, 1.0f, 0.0f },
    [WEATHER_HAIL]      = { 0.0f, 0.95f, 0.2f, 0.35f, 1.0f },
    [WEATHER_TORNADO]   = { 0.35f, 1.0f, 0.2f, 0.8f, 0.0f },
};

const char *weather_name(WeatherType type)
{
    static const char *names[WEATHER_COUNT] = { "Rain", "Thunderstorm", "Fog", "Clear skies", "Wind storm",
                                                "Hail", "Tornado" };
    return names[type];
}

/* the funnel: rings from a narrow foot up to a wide top, drawn with shaders/tornado */
static void build_twister(Weather *w)
{
    enum { RINGS = 40, SEGS = 36 };
    float *v = malloc((RINGS + 1) * (SEGS + 1) * 5 * sizeof *v);
    int n = 0;
    for (int r = 0; r <= RINGS; r++) {
        float t = (float)r / RINGS;
        float radius = 1.2f + 26.0f * powf(t, 2.2f);
        float y = t * 140.0f;
        for (int s = 0; s <= SEGS; s++) {
            float a = (float)s / SEGS * 6.2832f;
            v[n++] = cosf(a) * radius;
            v[n++] = y;
            v[n++] = sinf(a) * radius;
            v[n++] = (float)s / SEGS;
            v[n++] = t;
        }
    }
    w->twister_count = RINGS * SEGS * 6;
    GLuint *idx = malloc(w->twister_count * sizeof *idx), *p = idx;
    for (int r = 0; r < RINGS; r++)
        for (int s = 0; s < SEGS; s++) {
            GLuint i = r * (SEGS + 1) + s;
            *p++ = i; *p++ = i + SEGS + 1; *p++ = i + 1;
            *p++ = i + 1; *p++ = i + SEGS + 1; *p++ = i + SEGS + 2;
        }
    glCreateBuffers(1, &w->twister_vbo);
    glNamedBufferStorage(w->twister_vbo, n * sizeof *v, v, 0);
    glCreateBuffers(1, &w->twister_ebo);
    glNamedBufferStorage(w->twister_ebo, w->twister_count * sizeof *idx, idx, 0);
    glCreateVertexArrays(1, &w->twister_vao);
    glVertexArrayVertexBuffer(w->twister_vao, 0, w->twister_vbo, 0, 5 * sizeof(float));
    glVertexArrayElementBuffer(w->twister_vao, w->twister_ebo);
    glEnableVertexArrayAttrib(w->twister_vao, 0);
    glEnableVertexArrayAttrib(w->twister_vao, 1);
    glVertexArrayAttribFormat(w->twister_vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribFormat(w->twister_vao, 1, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    glVertexArrayAttribBinding(w->twister_vao, 0, 0);
    glVertexArrayAttribBinding(w->twister_vao, 1, 0);
    free(v);
    free(idx);
    w->twister_prog = shader_load("shaders/tornado.vert", "shaders/tornado.frag");
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
    glm_vec3_copy((vec3){0.8f, 0.0f, 0.6f}, w->wind_dir);
    w->wind = TARGETS[WEATHER_RAIN].wind;
    build_twister(w);

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
    glDeleteBuffers(1, &w->twister_vbo);
    glDeleteBuffers(1, &w->twister_ebo);
    glDeleteVertexArrays(1, &w->twister_vao);
    glDeleteProgram(w->twister_prog);
    free(w->drops);
    glDeleteBuffers(1, &w->vbo);
    glDeleteVertexArrays(1, &w->vao);
    glDeleteProgram(w->prog);
}

void weather_set(Weather *w, WeatherType type)
{
    w->type = type;
    w->change_t = 180.0f + frand() * 180.0f;
    if (type == WEATHER_TORNADO || type == WEATHER_WINDSTORM) {
        /* the wind swings round to a new quarter */
        float a = frand() * 6.2832f;
        glm_vec3_copy((vec3){cosf(a), 0.0f, sinf(a)}, w->wind_dir);
    }
    if (type == WEATHER_TORNADO)
        w->change_t = 150.0f + frand() * 90.0f;
}

void weather_snap(Weather *w)
{
    const Target *t = &TARGETS[w->type];
    w->rain = t->rain;
    w->cloud = t->cloud;
    w->fog = t->fog;
    w->wind = t->wind;
    w->hail = t->hail;
    w->wet = t->rain > 0.05f ? 1.0f : 0.0f;
    if (w->type == WEATHER_TORNADO) {
        w->twister_power = 1.0f;
    }
}

void weather_twister_force(const Weather *w, vec3 p, vec3 out)
{
    glm_vec3_zero(out);
    if (!w->twister || w->twister_power <= 0.01f)
        return;
    vec3 d = { w->twister_pos[0] - p[0], 0.0f, w->twister_pos[2] - p[2] };
    float r = glm_vec3_norm(d);
    if (r > 40.0f || r < 1e-3f)
        return;
    glm_vec3_scale(d, 1.0f / r, d);
    float pull = w->twister_power * (1.0f - r / 40.0f);
    /* inward and around, and up near the core */
    vec3 around = { -d[2], 0.0f, d[0] };
    glm_vec3_scale(d, pull * 14.0f, out);
    glm_vec3_muladds(around, pull * 22.0f, out);
    if (r < 10.0f)
        out[1] = pull * (1.0f - r / 10.0f) * 40.0f;
}

/* where a drop at (x, z) stops: a roof, a floor, the ground */
static float landing(const Level *lv, const Terrain *t, float x, float z)
{
    float g = level_hole(lv, x, z) ? CRYPT_Y : fmaxf(terrain_height(t, x, z), SEA_Y);
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
                                             WEATHER_RAIN, WEATHER_CLEAR, WEATHER_WINDSTORM, WEATHER_RAIN,
                                             WEATHER_HAIL, WEATHER_CLEAR, WEATHER_RAIN, WEATHER_TORNADO,
                                             WEATHER_CLEAR };
        static int next = 1;
        weather_set(w, cycle[next]);
        next = (next + 1) % (int)(sizeof cycle / sizeof cycle[0]);
    }

    const Target *tg = &TARGETS[w->type];
    float k = 1.0f - expf(-dt * 0.35f);
    w->rain += (tg->rain - w->rain) * k;
    w->cloud += (tg->cloud - w->cloud) * k;
    w->fog += (tg->fog - w->fog) * k;
    w->wind += (tg->wind - w->wind) * k;
    w->hail += (tg->hail - w->hail) * k;
    w->gust_phase += dt;
    w->gust = w->wind * (0.55f + 0.45f * sinf(w->gust_phase * 0.9f) * sinf(w->gust_phase * 0.37f + 1.0f));
    /* the rain slants with the wind */
    glm_vec3_copy((vec3){w->wind_dir[0] * (0.12f + w->gust * 0.9f), -1.0f, w->wind_dir[2] * (0.12f + w->gust * 0.9f)}, w->fall);
    glm_vec3_normalize(w->fall);

    /* the tornado forms somewhere upwind, wanders across the land and blows itself out */
    if (w->type == WEATHER_TORNADO && !w->twister) {
        w->twister = true;
        w->twister_power = 0.0f;
        w->twister_t = 0.0f;
        glm_vec3_copy((vec3){cam[0] - w->wind_dir[0] * 140.0f, 0.0f, cam[2] - w->wind_dir[2] * 140.0f}, w->twister_pos);
        glm_vec3_scale(w->wind_dir, 6.0f, w->twister_vel);
    }
    if (w->twister) {
        w->twister_t += dt;
        float want = w->type == WEATHER_TORNADO ? 1.0f : 0.0f;
        w->twister_power += glm_clamp(want - w->twister_power, -dt * 0.1f, dt * 0.08f);
        /* it meanders, drifting back toward you if it strays too far */
        float wander = sinf(w->twister_t * 0.21f) * 0.8f + sinf(w->twister_t * 0.07f + 2.0f) * 0.6f;
        vec3 to_cam = { cam[0] - w->twister_pos[0], 0.0f, cam[2] - w->twister_pos[2] };
        float far = glm_vec3_norm(to_cam);
        vec3 steer = { w->twister_vel[2] * wander * 0.3f, 0.0f, -w->twister_vel[0] * wander * 0.3f };
        if (far > 160.0f)
            glm_vec3_muladds(to_cam, 0.02f / far * 6.0f, steer);
        glm_vec3_muladds(steer, dt, w->twister_vel);
        float sp = glm_vec3_norm(w->twister_vel);
        if (sp > 7.0f)
            glm_vec3_scale(w->twister_vel, 7.0f / sp, w->twister_vel);
        glm_vec3_muladds(w->twister_vel, dt, w->twister_pos);
        w->twister_pos[1] = fmaxf(terrain_height(t, w->twister_pos[0], w->twister_pos[2]), SEA_Y);
        /* debris whirling up around its foot */
        if (w->twister_power > 0.05f && glm_vec3_distance(w->twister_pos, cam) < 250.0f) {
            for (int i = 0; i < (int)(dt * 120.0f * w->twister_power) + 1; i++) {
                Particle p = {0};
                float a = frand() * 6.2832f, r = 2.0f + frand() * 10.0f;
                glm_vec3_copy((vec3){w->twister_pos[0] + cosf(a) * r, w->twister_pos[1] + frand() * 3.0f, w->twister_pos[2] + sinf(a) * r}, p.pos);
                glm_vec3_copy((vec3){-sinf(a) * 16.0f, 6.0f + frand() * 14.0f, cosf(a) * 16.0f}, p.vel);
                bool dirt = frand() < 0.6f;
                glm_vec4_copy(dirt ? (vec4){0.16f, 0.13f, 0.1f, 0.7f} : (vec4){0.1f, 0.2f, 0.06f, 0.9f}, p.color0);
                glm_vec4_copy(dirt ? (vec4){0.3f, 0.27f, 0.24f, 0.0f} : (vec4){0.1f, 0.2f, 0.06f, 0.0f}, p.color1);
                p.size0 = dirt ? 0.6f + frand() : 0.08f;
                p.size1 = dirt ? 2.5f : 0.08f;
                p.max_life = p.life = 1.5f + frand() * 2.0f;
                p.gravity = dirt ? -1.0f : 2.0f;
                p.drag = 0.4f;
                particles_emit(ps, &p);
            }
        }
        if (w->twister_power <= 0.0f && want == 0.0f)
            w->twister = false;
    }

    /* a gale: dust, sand and leaves streaming past */
    if (w->wind > 0.4f && cam[1] > FLOOR_Y - 1.5f) {
        float rate = (w->wind - 0.4f) * 90.0f * dt;
        for (int i = 0; i < (int)rate + (frand() < rate - (int)rate); i++) {
            Particle p = {0};
            float a = frand() * 6.2832f, r = 4.0f + frand() * 18.0f;
            glm_vec3_copy((vec3){cam[0] + cosf(a) * r, cam[1] - 1.0f + frand() * 4.0f, cam[2] + sinf(a) * r}, p.pos);
            float sp = 10.0f + w->gust * 16.0f;
            glm_vec3_copy((vec3){w->wind_dir[0] * sp, (frand() - 0.3f) * 1.5f, w->wind_dir[2] * sp}, p.vel);
            bool leaf = frand() < 0.35f;
            glm_vec4_copy(leaf ? (vec4){0.25f, 0.3f, 0.08f, 0.9f} : (vec4){0.55f, 0.5f, 0.4f, 0.22f}, p.color0);
            glm_vec4_copy(leaf ? (vec4){0.3f, 0.25f, 0.08f, 0.0f} : (vec4){0.55f, 0.5f, 0.4f, 0.0f}, p.color1);
            p.size0 = leaf ? 0.05f : 0.3f;
            p.size1 = leaf ? 0.05f : 1.2f;
            p.max_life = p.life = 1.2f + frand();
            particles_emit(ps, &p);
        }
    }

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

    /* raindrops (or hailstones): a cloud of streaks around the camera, respawned at the top */
    int want = (int)(MAX_DROPS * fmaxf(w->rain, w->hail * 0.6f));
    while (w->drop_count < want)
        spawn_drop(&w->drops[w->drop_count++], cam, true);
    if (w->drop_count > want)
        w->drop_count = want;

    vec3 step;
    glm_vec3_scale(w->fall, DROP_SPEED * (1.0f + w->hail * 0.6f) * dt, step);
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
            /* hail bounces, rain splashes */
            if (w->hail > 0.3f && frand() < 0.2f) {
                Particle p = {0};
                glm_vec3_copy((vec3){d->x, floor_y + 0.03f, d->z}, p.pos);
                glm_vec3_copy((vec3){(frand() - 0.5f) * 1.5f, 1.5f + frand() * 1.5f, (frand() - 0.5f) * 1.5f}, p.vel);
                glm_vec4_copy((vec4){0.85f, 0.9f, 0.95f, 0.9f}, p.color0);
                glm_vec4_copy((vec4){0.85f, 0.9f, 0.95f, 0.0f}, p.color1);
                p.size0 = p.size1 = 0.03f;
                p.max_life = p.life = 0.8f;
                p.gravity = 9.0f;
                particles_emit(ps, &p);
            } else if (frand() < 0.12f) {
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
    out->fog_density = base->fog_density * (1.0f + w->rain * 1.8f + w->fog * 6.0f + w->wind * 1.2f + w->hail * 1.5f);
    /* a gale's dust browns the air; a tornado's sky turns green-grey */
    glm_vec3_lerp(out->fog_color, (vec3){out->fog_color[0] * 1.1f, out->fog_color[1] * 0.95f, out->fog_color[2] * 0.75f}, w->wind * 0.4f, out->fog_color);
    if (w->twister)
        glm_vec3_lerp(out->fog_color, (vec3){out->fog_color[0] * 0.7f, out->fog_color[1] * 0.85f, out->fog_color[2] * 0.7f}, w->twister_power * 0.7f, out->fog_color);
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

void weather_draw_twister(Weather *w, mat4 view_proj)
{
    if (!w->twister || w->twister_power <= 0.01f)
        return;
    glUseProgram(w->twister_prog);
    glProgramUniformMatrix4fv(w->twister_prog, 0, 1, GL_FALSE, (const float *)view_proj);
    glProgramUniform4f(w->twister_prog, 1, w->twister_pos[0], w->twister_pos[1], w->twister_pos[2], w->twister_power);
    glBindVertexArray(w->twister_vao);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDrawElements(GL_TRIANGLES, w->twister_count, GL_UNSIGNED_INT, NULL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void weather_draw(Weather *w, vec3 camera)
{
    if (!w->drop_count || camera[1] < FLOOR_Y - 2.0f)
        return;
    glNamedBufferSubData(w->vbo, 0, w->drop_count * sizeof(Drop), w->drops);
    glUseProgram(w->prog);
    glProgramUniform3fv(w->prog, 0, 1, w->fall);
    glProgramUniform1f(w->prog, 1, w->hail > 0.3f ? 0.018f : 0.006f);
    glProgramUniform1f(w->prog, 2, w->hail > 0.3f ? 1.0f : 0.0f);
    glBindVertexArray(w->vao);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, w->drop_count);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
