#include "game.h"
#include "ui.h"

#include <stdint.h>
#include <string.h>

/* Old Ember, the volcano to the north-east. Every half hour (ERUPTION_PERIOD) it wakes:
 *   calm       smoke rises from the crater; the vents near the top breathe poison
 *   stirring   the last two minutes: tremors, rumbling, the plume thickening, warnings
 *   erupting   a blast, lava thrown into the sky, and a burning cloud rolling out from
 *              the crater across the whole coast. Whatever it reaches out in the open
 *              dies: creatures, animals, you. People flee. Plants burn.
 *   ash        the sky goes dark and ash drifts down
 *   rebirth    the Covenant: light floods back, and everything returns as it was
 * Only the crypt, the palace's stone halls and the Drowned Court under the sea are safe. */

#define STIR_TIME   120.0f
#define FRONT_SPEED 55.0f       /* how fast the burning cloud spreads, m/s */
#define FRONT_REACH 720.0f
#define ASH_TIME    25.0f
#define REBIRTH_TIME 8.0f

static const vec3 CRATER = { VOLCANO_X, RIM_Y + 4.0f, VOLCANO_Z };

/* ---------- building the mountain's top ---------- */

static float rnd(int i, int salt)
{
    uint32_t h = (uint32_t)i * 2654435761u ^ (uint32_t)salt * 97531u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return (h & 0xffffff) / (float)0xffffff;
}

static void spike(WorldBuild *wb, vec3 base, float h, float r, float tilt, float yaw)
{
    mat4 xf;
    glm_translate_make(xf, base);
    glm_rotate_y(xf, yaw, xf);
    glm_rotate_z(xf, tilt, xf);
    wb_xcylinder(wb, WC_VOLCANO, WM_OBSIDIAN, xf, r, 0.0f, h, 6);
}

void volcano_build(WorldBuild *wb)
{
    const Terrain *t = wb->terrain;
    /* black glass spikes around the crater's rim */
    for (int i = 0; i < 28; i++) {
        float a = i * 6.2832f / 28.0f + rnd(i, 1) * 0.2f;
        float r = CRATER_R + 2.0f + rnd(i, 2) * 6.0f;
        float x = VOLCANO_X + cosf(a) * r, z = VOLCANO_Z + sinf(a) * r;
        spike(wb, (vec3){x, terrain_height(t, x, z) - 0.5f, z}, 2.0f + rnd(i, 3) * 5.0f, 0.4f + rnd(i, 4) * 0.7f,
              (rnd(i, 5) - 0.5f) * 0.6f, a);
    }
    /* burnt trees on the lower slopes: dead trunks and bare branches */
    for (int i = 0; i < 40; i++) {
        float a = rnd(i, 10) * 6.2832f, r = VOLCANO_R * (0.45f + rnd(i, 11) * 0.5f);
        float x = VOLCANO_X + cosf(a) * r, z = VOLCANO_Z + sinf(a) * r;
        float y = terrain_height(t, x, z) - 0.3f, h = 3.0f + rnd(i, 12) * 4.0f;
        mat4 xf;
        glm_translate_make(xf, (vec3){x, y, z});
        glm_rotate_z(xf, (rnd(i, 13) - 0.5f) * 0.3f, xf);
        wb_xcylinder(wb, WC_VOLCANO, WM_OBSIDIAN, xf, 0.22f, 0.08f, h, 7);
        for (int b = 0; b < 3; b++) {
            mat4 br;
            glm_mat4_copy(xf, br);
            glm_translate(br, (vec3){0, h * (0.45f + b * 0.15f), 0});
            glm_rotate_y(br, rnd(i, 20 + b) * 6.28f, br);
            glm_rotate_z(br, 0.9f + rnd(i, 30 + b) * 0.4f, br);
            wb_xcylinder(wb, WC_VOLCANO, WM_OBSIDIAN, br, 0.07f, 0.02f, 1.2f + rnd(i, 40 + b), 5);
        }
    }
    /* vents on the upper slopes and down in the crater, ringed with yellow crust */
    for (int i = 0; i < 9; i++) {
        float a = i * 6.2832f / 9.0f + 0.3f, r = i < 6 ? CRATER_R + 14.0f + rnd(i, 50) * 16.0f : CRATER_R * 0.6f;
        float x = VOLCANO_X + cosf(a) * r, z = VOLCANO_Z + sinf(a) * r;
        float y = terrain_height(t, x, z);
        for (int k = 0; k < 6; k++) {
            float b = k * 1.047f;
            wb_ball(wb, WC_VOLCANO, WM_DARK_ROCK, (vec3){x + cosf(b) * 1.4f, y, z + sinf(b) * 1.4f}, (vec3){0.7f, 0.45f, 0.7f}, 8);
        }
        wb_cylinder(wb, WC_VOLCANO, WM_CLOTH_YELLOW, (vec3){x, y - 0.1f, z}, 1.3f, 1.1f, 0.2f, 12);
        wb_cylinder(wb, WC_VOLCANO, WM_EMBER, (vec3){x, y - 0.05f, z}, 0.5f, 0.4f, 0.2f, 10);
        level_add_spawn(wb->lv, SPAWN_VENT, (vec3){x, y + 0.3f, z}, 0.0f);
        if (i < 6)
            level_add_spawn(wb->lv, SPAWN_LAMP, (vec3){x, y + 1.0f, z}, 0.0f)->variant = LIGHT_EMBER;
    }
    /* the crater's glow lights the smoke from below */
    level_add_spawn(wb->lv, SPAWN_LAMP, (vec3){VOLCANO_X, LAVA_Y + 3.0f, VOLCANO_Z}, 0.0f)->variant = LIGHT_EMBER;

    /* the Ember Heart's altar, on the rim facing the coast */
    float ax = VOLCANO_X - CRATER_R * 0.75f, az = VOLCANO_Z + CRATER_R * 0.75f;
    float ay = terrain_height(t, ax, az);
    wb_block(wb, WC_VOLCANO, WM_DARK_ROCK, (vec3){ax - 2.0f, ay - 1.5f, az - 2.0f}, (vec3){ax + 2.0f, ay + 0.3f, az + 2.0f});
    wb_block(wb, WC_VOLCANO, WM_OBSIDIAN, (vec3){ax - 0.5f, ay + 0.3f, az - 0.5f}, (vec3){ax + 0.5f, ay + 1.2f, az + 0.5f});
    for (int k = 0; k < 4; k++) {
        float b = k * GLM_PI_2f + GLM_PI_4f;
        spike(wb, (vec3){ax + cosf(b) * 1.6f, ay + 0.3f, az + sinf(b) * 1.6f}, 3.0f, 0.3f, 0.15f, b);
    }
    Spawn *s = level_add_spawn(wb->lv, SPAWN_THING, (vec3){ax, ay + 1.3f, az}, 0.0f);
    s->variant = THING_EMBER_HEART;

    /* the Covenant's circle of standing stones, halfway up the south-west slope */
    float cx = VOLCANO_X - 62.0f, cz = VOLCANO_Z + 52.0f, cy = terrain_height(t, cx, cz);
    for (int k = 0; k < 7; k++) {
        float b = k * 6.2832f / 7.0f;
        float x = cx + cosf(b) * 6.0f, z = cz + sinf(b) * 6.0f, y = terrain_height(t, x, z);
        wb_block(wb, WC_VOLCANO, WM_DARK_ROCK, (vec3){x - 0.5f, y - 1.0f, z - 0.4f}, (vec3){x + 0.5f, y + 2.6f + (k % 2) * 0.8f, z + 0.4f});
    }
    wb_block(wb, WC_VOLCANO, WM_DARK_ROCK, (vec3){cx - 1.0f, cy - 1.0f, cz - 0.6f}, (vec3){cx + 1.0f, cy + 0.9f, cz + 0.6f});
    s = level_add_spawn(wb->lv, SPAWN_THING, (vec3){cx, cy + 1.4f, cz - 0.7f}, 0.0f);
    s->variant = THING_LORE;
    s->index = 8;
    s = level_add_spawn(wb->lv, SPAWN_THING, (vec3){ax + 3.0f, ay + 1.3f, az + 1.5f}, 0.0f);
    s->variant = THING_LORE;
    s->index = 9;
    s = level_add_spawn(wb->lv, SPAWN_THING, (vec3){cx + 0.3f, cy + 0.95f, cz}, 0.0f);
    s->variant = THING_RELIC;
    s->index = 8;
    level_add_spawn(wb->lv, SPAWN_PROP, (vec3){cx + 2.0f, cy, cz - 2.0f}, 0.0f)->variant = PM_PUMICE;

    /* the mountain's children: imps on the slopes, bats in the crater */
    for (int i = 0; i < 7; i++) {
        float a = rnd(i, 60) * 6.2832f, r = VOLCANO_R * (0.3f + rnd(i, 61) * 0.4f);
        float x = VOLCANO_X + cosf(a) * r, z = VOLCANO_Z + sinf(a) * r;
        level_add_spawn(wb->lv, SPAWN_CREATURE, (vec3){x, terrain_height(t, x, z), z}, a)->variant = CR_IMP;
    }
    for (int i = 0; i < 4; i++) {
        float a = i * 1.57f;
        level_add_spawn(wb->lv, SPAWN_CREATURE, (vec3){VOLCANO_X + cosf(a) * 20.0f, RIM_Y - 4.0f, VOLCANO_Z + sinf(a) * 20.0f}, 0)->variant = CR_BAT;
    }
    /* a lava river down the east flank, glowing */
    for (int k = 0; k < 40; k++) {
        float r = CRATER_R + k * 2.6f;
        float a = -0.2f + sinf(k * 0.3f) * 0.12f;
        float x = VOLCANO_X + cosf(a) * r, z = VOLCANO_Z + sinf(a) * r;
        float y = terrain_height(t, x, z);
        mat4 xf;
        glm_translate_make(xf, (vec3){x, y + 0.05f, z});
        glm_rotate_y(xf, -a, xf);
        wb_xbox(wb, WC_VOLCANO, WM_EMBER, xf, (vec3){1.5f, 0.12f, 1.3f - (k % 3) * 0.2f});
    }
}

/* ---------- the cycle ---------- */

void volcano_init(Game *g)
{
    memset(&g->volcano, 0, sizeof g->volcano);
}

void volcano_force(Game *g)
{
    Volcano *v = &g->volcano;
    if (v->phase == VOLC_CALM || v->phase == VOLC_STIRRING) {
        v->t = ERUPTION_PERIOD - 45.0f;
        message(g, COL_RED, "(debug) Old Ember will erupt in 45 seconds.");
    }
}

float vent_fumes(const Game *g, vec3 p)
{
    float f = 0.0f;
    for (int i = 0; i < g->vent_count; i++) {
        float d = glm_vec3_distance(p, (float *)g->vents[i]);
        if (d < 9.0f)
            f = fmaxf(f, 1.0f - d / 9.0f);
    }
    /* the air of the crater itself */
    float dx = p[0] - VOLCANO_X, dz = p[2] - VOLCANO_Z;
    float r = sqrtf(dx * dx + dz * dz);
    if (r < CRATER_R + 12.0f && p[1] > RIM_Y - 30.0f)
        f = fmaxf(f, 0.55f * (1.0f - fmaxf(0.0f, r - CRATER_R) / 12.0f) + 0.2f);
    /* thick ash in the air burns the lungs a little too */
    if (g->volcano.phase == VOLC_ASH && level_sky_exposure(&g->level, p) > 0.5f)
        f = fmaxf(f, 0.3f);
    return f;
}

static bool sheltered(Game *g)
{
    Area a = level_area(&g->level, g->pos);
    return a == AREA_CRYPT || a == AREA_UNDERSEA || a == AREA_PALACE || g->underwater;
}

static void plume(Game *g, float dt, float strength)
{
    float rate = strength * 14.0f * dt;
    for (int i = 0; i < (int)rate + (frand() < rate - (int)rate); i++) {
        Particle p = {0};
        glm_vec3_copy((vec3){CRATER[0] + (frand() - 0.5f) * 14.0f, CRATER[1] - 4.0f, CRATER[2] + (frand() - 0.5f) * 14.0f}, p.pos);
        glm_vec3_copy((vec3){(frand() - 0.5f) * 2.0f + g->weather.wind_dir[0] * 3.0f * g->weather.wind, 8.0f + frand() * 6.0f * strength,
                             (frand() - 0.5f) * 2.0f + g->weather.wind_dir[2] * 3.0f * g->weather.wind}, p.vel);
        float grey = 0.12f + frand() * 0.1f;
        glm_vec4_copy((vec4){grey, grey * 0.95f, grey * 0.9f, 0.55f}, p.color0);
        glm_vec4_copy((vec4){grey * 1.6f, grey * 1.6f, grey * 1.6f, 0.0f}, p.color1);
        p.size0 = 6.0f + frand() * 4.0f;
        p.size1 = 26.0f + strength * 20.0f;
        p.max_life = p.life = 10.0f + frand() * 6.0f;
        p.gravity = -0.3f;
        p.drag = 0.15f;
        particles_emit(&g->ps, &p);
    }
}

static void throw_bomb(Volcano *v)
{
    for (int i = 0; i < 12; i++) {
        if (v->bomb_on[i])
            continue;
        v->bomb_on[i] = true;
        glm_vec3_copy((vec3){CRATER[0], CRATER[1], CRATER[2]}, v->bombs[i]);
        float a = frand() * 6.2832f, out = 15.0f + frand() * 35.0f;
        glm_vec3_copy((vec3){cosf(a) * out, 35.0f + frand() * 30.0f, sinf(a) * out}, v->bomb_vel[i]);
        return;
    }
}

void volcano_update(Game *g, float dt)
{
    Volcano *v = &g->volcano;
    v->t += dt;
    v->phase_t += dt;
    float dist = sqrtf((g->pos[0] - VOLCANO_X) * (g->pos[0] - VOLCANO_X) + (g->pos[2] - VOLCANO_Z) * (g->pos[2] - VOLCANO_Z));

    /* the ever-present plume, and embers over the crater */
    float near = dist < 600.0f ? 1.0f : 0.0f;
    plume(g, dt * near, v->phase == VOLC_STIRRING ? 2.0f : v->phase == VOLC_ERUPTING ? 4.0f : 0.8f);
    if (dist < 120.0f && frand() < dt * 20.0f)
        fx_flame(&g->ps, (vec3){VOLCANO_X + (frand() - 0.5f) * 30.0f, LAVA_Y + 0.3f, VOLCANO_Z + (frand() - 0.5f) * 30.0f}, 3.0f, 0.05f);
    /* poison gas curling from the vents */
    for (int i = 0; i < g->vent_count; i++) {
        if (glm_vec3_distance(g->vents[i], g->pos) > 90.0f || frand() > dt * 10.0f)
            continue;
        Particle p = {0};
        glm_vec3_copy(g->vents[i], p.pos);
        glm_vec3_copy((vec3){(frand() - 0.5f) * 0.8f, 1.5f + frand(), (frand() - 0.5f) * 0.8f}, p.vel);
        glm_vec4_copy((vec4){0.55f, 0.6f, 0.2f, 0.45f}, p.color0);
        glm_vec4_copy((vec4){0.65f, 0.65f, 0.4f, 0.0f}, p.color1);
        p.size0 = 0.8f;
        p.size1 = 4.5f;
        p.max_life = p.life = 3.5f;
        p.gravity = -0.2f;
        p.drag = 0.5f;
        particles_emit(&g->ps, &p);
    }

    switch (v->phase) {
    case VOLC_CALM:
        v->ash = fmaxf(0.0f, v->ash - dt * 0.05f);
        if (v->t > ERUPTION_PERIOD - STIR_TIME) {
            v->phase = VOLC_STIRRING;
            v->phase_t = 0.0f;
            audio_play(SFX_RUMBLE, 1.0f);
            message(g, COL_RED, "The ground trembles. Far off, Old Ember groans awake. Two minutes to find shelter: underground, or under the sea!");
        }
        break;
    case VOLC_STIRRING: {
        float left = ERUPTION_PERIOD - v->t;
        g->shake_t = fmaxf(g->shake_t, 0.05f + (1.0f - left / STIR_TIME) * 0.1f);
        if (frand() < dt * 0.25f)
            audio_play(SFX_RUMBLE, 0.6f + (1.0f - left / STIR_TIME) * 0.4f);
        static int last_warn;
        int secs = (int)left;
        if ((secs == 60 || secs == 30 || secs == 10) && secs != last_warn) {
            last_warn = secs;
            message(g, COL_RED, "Old Ember will erupt in %d seconds!", secs);
        }
        if (left <= 0.0f) {
            v->phase = VOLC_ERUPTING;
            v->phase_t = 0.0f;
            v->front = CRATER_R;
            v->caught_you = false;
            audio_play(SFX_ERUPTION, 1.0f);
            g->shake_t = 1.5f;
            g->flash_t = 0.4f;
            glm_vec3_copy((vec3){CRATER[0], CRATER[1], CRATER[2]}, g->flash_pos);
            npcs_flee_eruption(g);
            fishing_cancel(g);
            message(g, COL_RED, "OLD EMBER ERUPTS! A wall of fire and ash rolls down the mountain toward the sea!");
        }
        break;
    }
    case VOLC_ERUPTING: {
        v->front += FRONT_SPEED * dt;
        v->ash = fminf(1.0f, v->ash + dt * 0.2f);
        if (frand() < dt * 3.0f)
            throw_bomb(v);
        /* lava fountains out of the crater */
        if (dist < 700.0f)
            for (int k = 0; k < 4; k++) {
                Particle p = {0};
                glm_vec3_copy((vec3){CRATER[0] + (frand() - 0.5f) * 10.0f, CRATER[1] - 2.0f, CRATER[2] + (frand() - 0.5f) * 10.0f}, p.pos);
                glm_vec3_copy((vec3){(frand() - 0.5f) * 20.0f, 30.0f + frand() * 25.0f, (frand() - 0.5f) * 20.0f}, p.vel);
                glm_vec4_copy((vec4){30.0f, 9.0f, 1.5f, 1.0f}, p.color0);
                glm_vec4_copy((vec4){6.0f, 1.0f, 0.1f, 0.0f}, p.color1);
                p.size0 = 2.0f;
                p.size1 = 1.0f;
                p.max_life = p.life = 3.5f;
                p.gravity = 12.0f;
                p.additive = true;
                particles_emit(&g->ps, &p);
            }
        /* the burning cloud: a ring of dark, lit billows where the front is */
        if (fabsf(v->front - dist) < 150.0f) {
            for (int k = 0; k < 10; k++) {
                float a = atan2f(g->pos[2] - VOLCANO_Z, g->pos[0] - VOLCANO_X) + (frand() - 0.5f) * 1.2f;
                Particle p = {0};
                float x = VOLCANO_X + cosf(a) * v->front, z = VOLCANO_Z + sinf(a) * v->front;
                glm_vec3_copy((vec3){x, terrain_height(&g->terrain, x, z) + frand() * 12.0f, z}, p.pos);
                glm_vec3_copy((vec3){cosf(a) * FRONT_SPEED * 0.8f, 3.0f + frand() * 6.0f, sinf(a) * FRONT_SPEED * 0.8f}, p.vel);
                bool glow = frand() < 0.3f;
                glm_vec4_copy(glow ? (vec4){3.0f, 0.8f, 0.15f, 0.7f} : (vec4){0.09f, 0.07f, 0.06f, 0.85f}, p.color0);
                glm_vec4_copy(glow ? (vec4){0.3f, 0.1f, 0.05f, 0.0f} : (vec4){0.2f, 0.18f, 0.16f, 0.0f}, p.color1);
                p.size0 = 8.0f + frand() * 6.0f;
                p.size1 = 22.0f;
                p.max_life = p.life = 3.0f;
                p.drag = 0.6f;
                p.additive = glow;
                particles_emit(&g->ps, &p);
            }
            audio_play(SFX_RUMBLE, 0.05f);
        }
        creatures_burn(g, (vec3){VOLCANO_X, 0, VOLCANO_Z}, v->front);
        for (int i = 0; i < MAX_ANIMALS; i++) {
            Animal *a = &g->animals[i];
            if (a->used && a->gone_t <= 0.0f && glm_vec3_distance((vec3){a->pos[0], 0, a->pos[2]}, (vec3){VOLCANO_X, 0, VOLCANO_Z}) < v->front &&
                a->pos[1] > SEA_Y - 3.0f)
                a->gone_t = 1.0f;
        }
        for (int i = 0; i < g->prop_count; i++) {
            Prop *p = &g->props[i];
            if (p->burns && p->growth > 0.0f &&
                glm_vec3_distance((vec3){p->pos[0], 0, p->pos[2]}, (vec3){VOLCANO_X, 0, VOLCANO_Z}) < v->front) {
                p->growth = 0.0f;
                fx_flame(&g->ps, p->pos, 1.2f, 0.1f);
            }
        }
        for (int i = 0; i < g->thing_count; i++)
            if (g->things[i].kind == THING_SHELL)
                g->things[i].done = true;
        /* and you, if you're out in it */
        if (!v->caught_you && dist < v->front && !sheltered(g) && !g->dead) {
            v->caught_you = true;
            message(g, COL_RED, "The burning cloud rolls over you...");
            hurt_player(g, (vec3){VOLCANO_X, 0, VOLCANO_Z}, 10000.0f);
        }
        if (v->front > FRONT_REACH) {
            v->phase = VOLC_ASH;
            v->phase_t = 0.0f;
        }
        break;
    }
    case VOLC_ASH:
        v->ash = fminf(1.0f, v->ash + dt * 0.1f);
        if (v->phase_t < 10.0f && frand() < dt * 1.5f)
            throw_bomb(v);
        if (v->phase_t > ASH_TIME) {
            v->phase = VOLC_REBIRTH;
            v->phase_t = 0.0f;
            audio_play(SFX_REBIRTH, 1.0f);
            message(g, COL_GOLD, "Light floods back over the coast, green and gold. Everything the fire took returns. The Covenant holds.");
            creatures_regenerate(g, false);
            animals_regenerate(g);
            quest_regenerate(g);
            for (int i = 0; i < g->npc_count; i++)
                g->npcs[i].gone_t = 0.0f;
            if (g->dead && v->caught_you) {
                respawn_player_at(g, g->spawn, g->spawn_yaw);
                message(g, COL_GOLD, "You wake by the shrine's altar, whole, smelling faintly of smoke.");
            }
        }
        break;
    case VOLC_REBIRTH:
        v->ash = fmaxf(0.0f, v->ash - dt * 0.15f);
        if (g->pos[1] > SEA_Y && frand() < dt * 40.0f) {
            float a = frand() * 6.2832f, r = frand() * 30.0f;
            fx_magic(&g->ps, (vec3){g->pos[0] + cosf(a) * r, g->pos[1] + frand() * 3.0f, g->pos[2] + sinf(a) * r},
                     (vec3){1.5f, 4.0f, 1.0f}, 0.3f, 1);
        }
        if (v->phase_t > REBIRTH_TIME) {
            v->phase = VOLC_CALM;
            v->phase_t = 0.0f;
            v->t = 0.0f;
        }
        break;
    }

    /* lava bombs: arcing, trailing fire, bursting where they land */
    for (int i = 0; i < 12; i++) {
        if (!v->bomb_on[i])
            continue;
        v->bomb_vel[i][1] -= 14.0f * dt;
        glm_vec3_muladds(v->bomb_vel[i], dt, v->bombs[i]);
        if (glm_vec3_distance(v->bombs[i], g->pos) < 400.0f)
            fx_flame(&g->ps, v->bombs[i], 2.5f, dt);
        float ground = terrain_height(&g->terrain, v->bombs[i][0], v->bombs[i][2]);
        if (v->bombs[i][1] < ground) {
            v->bomb_on[i] = false;
            if (glm_vec3_distance(v->bombs[i], g->pos) < 200.0f) {
                explode(g, v->bombs[i], 40.0f, 7.0f, !sheltered(g));
                audio_play_at(SFX_LAVA_BOMB, v->bombs[i], 1.0f);
            }
        }
    }

    /* ash drifting down around you */
    if (v->ash > 0.05f && level_sky_exposure(&g->level, g->head) > 0.3f && !g->underwater) {
        float rate = v->ash * 80.0f * dt;
        for (int i = 0; i < (int)rate + 1; i++) {
            Particle p = {0};
            float a = frand() * 6.2832f, r = frand() * 20.0f;
            glm_vec3_copy((vec3){g->head[0] + cosf(a) * r, g->head[1] + 6.0f + frand() * 4.0f, g->head[2] + sinf(a) * r}, p.pos);
            glm_vec3_copy((vec3){g->weather.wind_dir[0] * 1.5f, -1.2f - frand(), g->weather.wind_dir[2] * 1.5f}, p.vel);
            bool ember = frand() < 0.08f && v->phase == VOLC_ERUPTING;
            glm_vec4_copy(ember ? (vec4){6.0f, 1.5f, 0.2f, 1.0f} : (vec4){0.3f, 0.28f, 0.26f, 0.8f}, p.color0);
            glm_vec4_copy(ember ? (vec4){1.0f, 0.2f, 0.0f, 0.0f} : (vec4){0.3f, 0.28f, 0.26f, 0.0f}, p.color1);
            p.size0 = p.size1 = ember ? 0.05f : 0.04f;
            p.max_life = p.life = 6.0f;
            p.additive = ember;
            particles_emit(&g->ps, &p);
        }
    }
}

void volcano_environment(const Game *g, Environment *env)
{
    const Volcano *v = &g->volcano;
    float ash = v->ash;
    if (v->phase == VOLC_STIRRING) {
        float k = 1.0f - (ERUPTION_PERIOD - v->t) / STIR_TIME;
        glm_vec3_lerp(env->fog_color, (vec3){env->fog_color[0] * 1.15f, env->fog_color[1] * 0.85f, env->fog_color[2] * 0.7f}, k * 0.6f, env->fog_color);
    }
    if (ash > 0.0f) {
        /* the sky goes brown-black, the sun a dull red coin; a glow from the mountain */
        glm_vec3_scale(env->sun_color, 1.0f - ash * 0.8f, env->sun_color);
        glm_vec3_lerp(env->sun_color, (vec3){1.5f, 0.35f, 0.1f}, ash * 0.4f, env->sun_color);
        glm_vec3_lerp(env->fog_color, (vec3){0.11f, 0.07f, 0.05f}, ash * 0.85f, env->fog_color);
        glm_vec3_lerp(env->zenith, (vec3){0.05f, 0.03f, 0.03f}, ash * 0.85f, env->zenith);
        glm_vec3_scale(env->sky_ambient, 1.0f - ash * 0.6f, env->sky_ambient);
        glm_vec3_add(env->sky_ambient, (vec3){0.06f * ash, 0.015f * ash, 0.0f}, env->sky_ambient);
        env->fog_density *= 1.0f + ash * 2.5f;
    }
    if (v->phase == VOLC_REBIRTH) {
        float k = sinf(fminf(v->phase_t / REBIRTH_TIME, 1.0f) * GLM_PIf);
        glm_vec3_add(env->sky_ambient, (vec3){0.12f * k, 0.2f * k, 0.06f * k}, env->sky_ambient);
    }
}

void volcano_hud(Game *g)
{
    Volcano *v = &g->volcano;
    float w = (float)g->width;
    char text[160];
    if (v->phase == VOLC_STIRRING) {
        int left = (int)(ERUPTION_PERIOD - v->t);
        snprintf(text, sizeof text, "OLD EMBER ERUPTS IN %d:%02d  --  get underground, or under the sea!", left / 60, left % 60);
        float pulse = 0.7f + 0.3f * sinf(g->time * 6.0f);
        float col[4] = { 1.0f, 0.35f * pulse, 0.2f, 1.0f };
        ui_text_shadow(FONT_MEDIUM, (w - ui_text_width(FONT_MEDIUM, text)) * 0.5f, 100, col, text);
    } else if (v->phase == VOLC_ERUPTING || v->phase == VOLC_ASH) {
        snprintf(text, sizeof text, "Old Ember is erupting");
        ui_text_shadow(FONT_MEDIUM, (w - ui_text_width(FONT_MEDIUM, text)) * 0.5f, 100, COL_RED, text);
    } else if (v->phase == VOLC_CALM) {
        int left = (int)(ERUPTION_PERIOD - v->t);
        snprintf(text, sizeof text, "Old Ember sleeps  %d:%02d", left / 60, left % 60);
        float dim[4] = { 1.0f, 0.6f, 0.4f, 0.55f };
        ui_text_shadow(FONT_SMALL, w - 28 - ui_text_width(FONT_SMALL, text), 46, dim, text);
    }
    if (g->dead && v->caught_you) {
        const char *t = "The Covenant will bring you back...";
        ui_text_shadow(FONT_MEDIUM, (w - ui_text_width(FONT_MEDIUM, t)) * 0.5f, g->height * 0.38f + 120, COL_GOLD, t);
    }
}
