#include "game.h"
#include "meshgen.h"

#include <string.h>

/* The coast's creatures (creatures.c runs their shared life: hits, deaths, walking):
 *   giant crabs   scuttle sideways, armoured in front: hit them from the side
 *   jellyfish     drift under the surface, glowing at night; swim into one and it stings
 *   sharks        circle in deep water, fin cutting the surface; they hunt swimmers
 *   magma imps    Old Ember's children, flinging coals; frost and water hurt them most
 *   the Drowned   barnacled skeletons of the Drowned Court
 *   wraiths       drift out of the ruins at night; half your blows pass through them
 *   angler eels   lurk in the Court's flooded channels, a lure glowing over their jaws
 *   bats          flutter in the crypt, and over the volcano after dark
 *   the Drowned King, twice a man's height, who keeps the Tide Bell */

/* ---------- models ---------- */

typedef struct {
    MeshBuilder mb[5];
    Material mat[5];
    int n;
} Parts;

static int part(Parts *p, float r, float g, float b, float rough, float metal, const float *glow, float alpha)
{
    mb_init(&p->mb[p->n]);
    material_color(&p->mat[p->n], r, g, b, rough, metal);
    if (glow)
        glm_vec3_copy((float *)glow, p->mat[p->n].emissive);
    if (alpha < 1.0f) {
        p->mat[p->n].base_color[3] = alpha;
        p->mat[p->n].alpha_mode = ALPHA_BLEND;
    }
    return p->n++;
}

static void blob(Parts *p, int m, int bone, vec3 c, vec3 r)
{
    mat4 xf;
    glm_translate_make(xf, c);
    glm_scale(xf, r);
    p->mb[m].joint = bone;
    mb_capsule(&p->mb[m], xf, 1.0f, 0.0f, 14);
}

static void slab(Parts *p, int m, int bone, vec3 c, vec3 half, float yaw, float pitch, float roll)
{
    mat4 xf;
    glm_translate_make(xf, c);
    glm_rotate_y(xf, yaw, xf);
    glm_rotate_x(xf, pitch, xf);
    glm_rotate_z(xf, roll, xf);
    p->mb[m].joint = bone;
    mb_box(&p->mb[m], xf, half, 1.0f);
}

static void finish(Model *m, Parts *p, int bones, const int *parents, const vec3 *pivots)
{
    if (bones <= 1) {
        model_from_builders(m, p->mb, p->mat, p->n);
    } else {
        mat4 rest[10];
        for (int b = 0; b < bones; b++) {
            vec3 rel;
            if (parents[b] >= 0)
                glm_vec3_sub((float *)pivots[b], (float *)pivots[parents[b]], rel);
            else
                glm_vec3_copy((float *)pivots[b], rel);
            glm_translate_make(rest[b], rel);
        }
        model_from_builders_skinned(m, p->mb, p->mat, p->n, bones, parents, rest);
    }
    for (int i = 0; i < p->n; i++)
        mb_free(&p->mb[i]);
}

/* a giant crab: bones 0 body, 1 left claw, 2 right claw */
static void build_crab(Model *m)
{
    Parts p = {0};
    int shell = part(&p, 0.62f, 0.14f, 0.06f, 0.35f, 0.0f, NULL, 1);
    int under = part(&p, 0.85f, 0.55f, 0.35f, 0.5f, 0.0f, NULL, 1);
    int dark = part(&p, 0.02f, 0.02f, 0.02f, 0.15f, 0.0f, NULL, 1);
    blob(&p, shell, 0, (vec3){0, 0.42f, 0}, (vec3){0.6f, 0.22f, 0.45f});
    blob(&p, under, 0, (vec3){0, 0.32f, 0}, (vec3){0.5f, 0.12f, 0.38f});
    for (int s = -1; s <= 1; s += 2) {
        for (int k = 0; k < 3; k++)
            slab(&p, under, 0, (vec3){s * 0.72f, 0.22f, 0.2f - k * 0.2f}, (vec3){0.3f, 0.035f, 0.035f}, s * (k - 1) * 0.3f, 0, s * -0.6f);
        blob(&p, dark, 0, (vec3){s * 0.12f, 0.66f, 0.38f}, (vec3){0.04f, 0.06f, 0.04f});
        blob(&p, shell, 1 + (s > 0), (vec3){s * 0.55f, 0.4f, 0.62f}, (vec3){0.2f, 0.14f, 0.26f});
        slab(&p, shell, 1 + (s > 0), (vec3){s * 0.52f, 0.42f, 0.88f}, (vec3){0.07f, 0.04f, 0.14f}, 0, 0, 0);
    }
    static const int parents[3] = { -1, 0, 0 };
    static const vec3 pivots[3] = { { 0, 0.4f, 0 }, { -0.4f, 0.4f, 0.45f }, { 0.4f, 0.4f, 0.45f } };
    finish(m, &p, 3, parents, pivots);
}

/* a jellyfish: a see-through bell and trailing tentacles, glowing faintly */
static void build_jelly(Model *m)
{
    Parts p = {0};
    int bell = part(&p, 0.8f, 0.55f, 0.9f, 0.1f, 0.0f, (float[3]){ 0.5f, 0.2f, 0.7f }, 0.55f);
    int core = part(&p, 0.9f, 0.6f, 0.95f, 0.2f, 0.0f, (float[3]){ 1.6f, 0.6f, 2.2f }, 1);
    blob(&p, core, 0, (vec3){0, 0.1f, 0}, (vec3){0.12f, 0.08f, 0.12f});
    for (int k = 0; k < 10; k++) {
        float a = k * 0.628f;
        mat4 xf;
        glm_translate_make(xf, (vec3){cosf(a) * 0.22f, 0.0f, sinf(a) * 0.22f});
        glm_rotate_x(xf, GLM_PIf, xf);
        p.mb[core].joint = 0;
        mb_cylinder(&p.mb[core], xf, 0.012f, 0.003f, 0.9f + (k % 3) * 0.3f, 5, 1.0f);
    }
    blob(&p, bell, 0, (vec3){0, 0.12f, 0}, (vec3){0.38f, 0.24f, 0.38f});
    finish(m, &p, 1, NULL, NULL);
}

/* a shark: bones 0 body, 1 tail */
static void build_shark(Model *m)
{
    Parts p = {0};
    int back = part(&p, 0.3f, 0.34f, 0.38f, 0.4f, 0.0f, NULL, 1);
    int belly = part(&p, 0.8f, 0.8f, 0.78f, 0.45f, 0.0f, NULL, 1);
    int dark = part(&p, 0.01f, 0.01f, 0.01f, 0.1f, 0.0f, NULL, 1);
    blob(&p, back, 0, (vec3){0, 0.05f, 0.4f}, (vec3){0.38f, 0.42f, 1.3f});
    blob(&p, belly, 0, (vec3){0, -0.12f, 0.5f}, (vec3){0.32f, 0.3f, 1.1f});
    blob(&p, back, 1, (vec3){0, 0.05f, -0.9f}, (vec3){0.2f, 0.24f, 0.8f});
    slab(&p, back, 0, (vec3){0, 0.58f, 0.2f}, (vec3){0.03f, 0.3f, 0.22f}, 0, -0.35f, 0);   /* the fin you see first */
    slab(&p, back, 1, (vec3){0, 0.3f, -1.75f}, (vec3){0.03f, 0.45f, 0.18f}, 0, -0.6f, 0);
    slab(&p, back, 1, (vec3){0, -0.2f, -1.7f}, (vec3){0.03f, 0.25f, 0.12f}, 0, 0.6f, 0);
    for (int s = -1; s <= 1; s += 2) {
        slab(&p, back, 0, (vec3){s * 0.5f, -0.2f, 0.7f}, (vec3){0.32f, 0.02f, 0.14f}, s * -0.4f, 0, s * -0.3f);
        blob(&p, dark, 0, (vec3){s * 0.26f, 0.12f, 1.35f}, (vec3){0.04f, 0.04f, 0.04f});
    }
    static const int parents[2] = { -1, 0 };
    static const vec3 pivots[2] = { { 0, 0, 0.3f }, { 0, 0, -0.5f } };
    finish(m, &p, 2, parents, pivots);
}

/* an angler eel: a chain of six segments, a lure glowing over its jaws */
static void build_eel(Model *m)
{
    Parts p = {0};
    int skin = part(&p, 0.12f, 0.2f, 0.08f, 0.3f, 0.0f, NULL, 1);
    int lure = part(&p, 0.9f, 1.0f, 0.6f, 0.2f, 0.0f, (float[3]){ 3.0f, 3.5f, 1.2f }, 1);
    int teeth = part(&p, 0.9f, 0.88f, 0.8f, 0.3f, 0.0f, NULL, 1);
    int parents[6];
    vec3 pivots[6];
    for (int b = 0; b < 6; b++) {
        parents[b] = b ? b - 1 : -1;
        glm_vec3_copy((vec3){0, 0, 1.5f - b * 0.6f}, pivots[b]);
        blob(&p, skin, b, (vec3){0, 0, 1.3f - b * 0.6f}, (vec3){0.22f - b * 0.025f, 0.26f - b * 0.03f, 0.38f});
    }
    blob(&p, skin, 0, (vec3){0, 0.1f, 1.75f}, (vec3){0.26f, 0.3f, 0.3f});
    for (int k = 0; k < 6; k++)
        slab(&p, teeth, 0, (vec3){(k - 2.5f) * 0.07f, -0.08f, 1.98f}, (vec3){0.012f, 0.05f, 0.012f}, 0, 0, 0);
    slab(&p, skin, 0, (vec3){0, 0.45f, 1.9f}, (vec3){0.012f, 0.2f, 0.012f}, 0, 0.6f, 0);
    blob(&p, lure, 0, (vec3){0, 0.62f, 2.1f}, (vec3){0.07f, 0.07f, 0.07f});
    finish(m, &p, 6, parents, pivots);
}

/* a bat: bones 0 body, 1 and 2 wings */
static void build_bat(Model *m)
{
    Parts p = {0};
    int fur = part(&p, 0.12f, 0.08f, 0.07f, 0.9f, 0.0f, NULL, 1);
    int wing = part(&p, 0.2f, 0.12f, 0.12f, 0.8f, 0.0f, NULL, 1);
    int eye = part(&p, 0.8f, 0.1f, 0.05f, 0.2f, 0.0f, (float[3]){ 2.0f, 0.2f, 0.05f }, 1);
    blob(&p, fur, 0, (vec3){0, 0, 0}, (vec3){0.08f, 0.08f, 0.11f});
    for (int s = -1; s <= 1; s += 2) {
        slab(&p, wing, 1 + (s > 0), (vec3){s * 0.22f, 0.02f, 0}, (vec3){0.2f, 0.008f, 0.1f}, 0, 0, 0);
        blob(&p, eye, 0, (vec3){s * 0.03f, 0.03f, 0.09f}, (vec3){0.012f, 0.012f, 0.012f});
        slab(&p, fur, 0, (vec3){s * 0.04f, 0.1f, 0.05f}, (vec3){0.015f, 0.04f, 0.01f}, 0, 0, s * 0.3f);
    }
    static const int parents[3] = { -1, 0, 0 };
    static const vec3 pivots[3] = { { 0, 0, 0 }, { -0.06f, 0.02f, 0 }, { 0.06f, 0.02f, 0 } };
    finish(m, &p, 3, parents, pivots);
}

/* dyes for the KayKit bodies */
static void ember_recolor(unsigned char *px, int w, int h)
{
    for (int i = 0; i < w * h; i++) {
        unsigned char *p = &px[i * 4];
        float l = (p[0] * 0.3f + p[1] * 0.59f + p[2] * 0.11f) / 255.0f;
        /* charred black with hot orange in the light parts */
        float hot = fmaxf(0.0f, l - 0.45f) * 1.8f;
        p[0] = (unsigned char)(fminf(1.0f, 0.08f + hot * 1.0f) * 255);
        p[1] = (unsigned char)(fminf(1.0f, 0.05f + hot * 0.35f) * 255);
        p[2] = (unsigned char)(fminf(1.0f, 0.04f + hot * 0.05f) * 255);
    }
}

static void drowned_recolor(unsigned char *px, int w, int h)
{
    for (int i = 0; i < w * h; i++) {
        unsigned char *p = &px[i * 4];
        float l = (p[0] * 0.3f + p[1] * 0.59f + p[2] * 0.11f) / 255.0f;
        /* bleached bone gone green and blue with barnacles and weed */
        float weed = (float)((i * 2654435761u) >> 28) / 16.0f;
        p[0] = (unsigned char)(fminf(1.0f, l * 0.55f + weed * 0.05f) * 255);
        p[1] = (unsigned char)(fminf(1.0f, l * 0.85f + 0.05f) * 255);
        p[2] = (unsigned char)(fminf(1.0f, l * 0.75f + 0.06f) * 255);
    }
}

static void wraith_recolor(unsigned char *px, int w, int h)
{
    for (int i = 0; i < w * h; i++) {
        unsigned char *p = &px[i * 4];
        float l = (p[0] * 0.3f + p[1] * 0.59f + p[2] * 0.11f) / 255.0f;
        p[0] = (unsigned char)(fminf(1.0f, l * 0.35f + 0.05f) * 255);
        p[1] = (unsigned char)(fminf(1.0f, l * 0.45f + 0.08f) * 255);
        p[2] = (unsigned char)(fminf(1.0f, l * 0.7f + 0.14f) * 255);
    }
}

static void humanoid_rig(const Model *m, Rig *r, const char *attack)
{
    r->idle = model_find_anim(m, "Idle");
    r->walk = model_find_anim(m, "Walking_A");
    r->run = model_find_anim(m, "Running_A");
    r->attack = model_find_anim(m, attack);
    r->hit = model_find_anim(m, "Hit_A");
    r->death = model_find_anim(m, "Death_C_Skeletons");
    if (r->death < 0)
        r->death = model_find_anim(m, "Death_A");
    r->awaken = model_find_anim(m, "Skeletons_Awaken_Floor");
    r->dormant = model_find_anim(m, "Skeletons_Inactive_Floor_Pose");
    r->cast = model_find_anim(m, "Spellcast_Shoot");
    r->cheer = model_find_anim(m, "Cheer");
    r->hand_r = model_find_node(m, "handslot.r");
    r->hand_l = model_find_node(m, "handslot.l");
}

static Model king_model;
static Rig king_rig;

void beasts_load(Game *g)
{
    build_crab(&g->crab_model);
    build_jelly(&g->jelly_model);
    build_shark(&g->shark_model);
    build_eel(&g->eel_model);
    build_bat(&g->bat_model);

    ModelOptions ember = { .recolor = ember_recolor };
    load_or_die(&g->imp_model, "assets/kaykit/Rogue_Hooded.glb", &ember);
    static const char *hide[] = { "Knife_Offhand", "1H_Crossbow", "2H_Crossbow", "Throwable", "Knife" };
    for (size_t i = 0; i < sizeof hide / sizeof hide[0]; i++)
        model_hide_node(&g->imp_model, hide[i]);
    Transform small = { .scale = { 0.55f, 0.55f, 0.55f } };
    model_adjust(&g->imp_model, &small, false);
    humanoid_rig(&g->imp_model, &g->imp_rig, "Throw");

    ModelOptions weed = { .recolor = drowned_recolor };
    load_or_die(&g->drowned_model, "assets/kaykit/Skeleton_Minion.glb", &weed);
    Transform size = { .scale = { 0.85f, 0.85f, 0.85f } };
    model_adjust(&g->drowned_model, &size, false);
    humanoid_rig(&g->drowned_model, &g->drowned_rig, "1H_Melee_Attack_Chop");

    ModelOptions ghost = { .recolor = wraith_recolor };
    load_or_die(&g->wraith_model, "assets/kaykit/Skeleton_Mage.glb", &ghost);
    model_adjust(&g->wraith_model, &size, false);
    humanoid_rig(&g->wraith_model, &g->wraith_rig, "Spellcast_Shoot");

    load_or_die(&king_model, "assets/kaykit/Skeleton_Warrior.glb", &weed);
    Transform huge = { .scale = { 1.9f, 1.9f, 1.9f } };
    model_adjust(&king_model, &huge, false);
    humanoid_rig(&king_model, &king_rig, "2H_Melee_Attack_Chop");
    if (king_rig.attack < 0)
        king_rig.attack = model_find_anim(&king_model, "1H_Melee_Attack_Chop");
}

void beasts_free(Game *g)
{
    model_free(&g->crab_model);
    model_free(&g->jelly_model);
    model_free(&g->shark_model);
    model_free(&g->eel_model);
    model_free(&g->bat_model);
    model_free(&g->imp_model);
    model_free(&g->drowned_model);
    model_free(&g->wraith_model);
    model_free(&king_model);
}

const Model *beast_model(Game *g, const Creature *c)
{
    switch (c->type) {
    case CR_CRAB: return &g->crab_model;
    case CR_JELLYFISH: return &g->jelly_model;
    case CR_SHARK: return &g->shark_model;
    case CR_EEL: return &g->eel_model;
    case CR_BAT: return &g->bat_model;
    case CR_IMP: return &g->imp_model;
    case CR_DROWNED: return &g->drowned_model;
    case CR_WRAITH: return &g->wraith_model;
    case CR_KING: return &king_model;
    default: return NULL;
    }
}

const Rig *beast_rig(Game *g, const Creature *c)
{
    switch (c->type) {
    case CR_IMP: return &g->imp_rig;
    case CR_DROWNED: return &g->drowned_rig;
    case CR_WRAITH: return &g->wraith_rig;
    case CR_KING: return &king_rig;
    default: return NULL;
    }
}

/* ---------- behaviour of the swimmers and flyers (the walkers use creatures.c's) ---------- */

bool creature_aquatic(const Creature *c)
{
    return c->type == CR_JELLYFISH || c->type == CR_SHARK || c->type == CR_EEL;
}

static bool sea_safe(const Game *g)
{
    return g->first_tide;       /* the Eye of the First Tide: the sea's creatures leave you be */
}

void beast_update(Game *g, Creature *c, float dt, float dist, vec3 to_player)
{
    c->phase += dt;
    switch (c->type) {
    case CR_JELLYFISH: {
        /* drifts on a slow circle, pulsing; stings on contact */
        float a = c->phase * 0.08f + c->home[0];
        vec3 want = { c->home[0] + cosf(a) * 4.0f, c->home[1] + sinf(c->phase * 0.5f) * 0.4f, c->home[2] + sinf(a) * 4.0f };
        glm_vec3_lerp(c->pos, want, 1.0f - expf(-dt * 0.4f), c->pos);
        vec3 body = { g->pos[0], g->pos[1] + 1.0f, g->pos[2] };
        float touch = glm_vec3_distance(body, c->pos);
        c->attack_t -= dt;
        if (touch < 1.3f && c->attack_t <= 0.0f && !g->dead && !sea_safe(g) &&
            !(g->equip[EQ_FEET] == ITEM_RUBBER_BOOTS && !g->swimming)) {
            c->attack_t = 1.5f;
            hurt_player(g, c->pos, 8.0f);
            g->sting_t = 4.0f;
            audio_play_at(SFX_JELLY_STING, c->pos, 1.0f);
            fx_bolt(&g->ps, c->pos, body, (vec3){3.0f, 1.0f, 4.0f});
            message(g, COL_RED, "A jellyfish stings you! It burns like nettles. (a lime would help)");
        }
        break;
    }
    case CR_SHARK: {
        float surface, depth;
        bool sea = sea_at(g, g->pos[0], g->pos[2], &surface, &depth);
        bool prey = g->swimming && sea && dist < 24.0f && !g->dead && !sea_safe(g) && g->shadow_t <= 0.0f;
        vec3 want;
        float speed;
        if (prey) {
            glm_vec3_copy(g->pos, want);
            want[1] = g->pos[1] + 0.6f;
            speed = 6.2f;
            c->state = ST_CHASE;
        } else {
            /* circling, near the surface so its fin shows */
            float a = c->phase * 0.18f + c->home[2];
            glm_vec3_copy((vec3){c->home[0] + cosf(a) * 14.0f, SEA_Y - 0.55f, c->home[2] + sinf(a) * 14.0f}, want);
            if (g->boat_in >= 0 && glm_vec3_distance(g->boats[g->boat_in].pos, c->pos) < 25.0f) {
                /* curious about boats: it circles yours */
                vec3 b;
                glm_vec3_copy(g->boats[g->boat_in].pos, b);
                glm_vec3_copy((vec3){b[0] + cosf(c->phase * 0.6f) * 5.0f, SEA_Y - 0.6f, b[2] + sinf(c->phase * 0.6f) * 5.0f}, want);
            }
            speed = 3.2f;
            c->state = ST_WANDER;
        }
        vec3 d;
        glm_vec3_sub(want, c->pos, d);
        float len = glm_vec3_norm(d);
        if (len > 0.01f) {
            c->yaw += glm_clamp(wrap_angle(atan2f(d[0], d[2]) - c->yaw), -dt * 2.0f, dt * 2.0f);
            vec3 fwd = { sinf(c->yaw), d[1] / len, cosf(c->yaw) };
            glm_vec3_muladds(fwd, speed * dt, c->pos);
        }
        /* stay in deep water */
        float s2, d2;
        if (!sea_at(g, c->pos[0], c->pos[2], &s2, &d2) || d2 < 2.5f)
            glm_vec3_lerp(c->pos, c->home, dt * 0.5f, c->pos);
        else
            c->pos[1] = glm_clamp(c->pos[1], s2 - d2 + 0.5f, s2 - 0.4f);
        c->attack_t -= dt;
        if (prey && dist < 1.8f && c->attack_t <= 0.0f) {
            c->attack_t = 1.6f;
            hurt_player(g, c->pos, 20.0f);
            audio_play_at(SFX_SHARK_BITE, c->pos, 1.0f);
            fx_blood(&g->ps, (vec3){g->pos[0], g->pos[1] + 1.0f, g->pos[2]}, (vec3){0, 0, 0}, 20);
            message(g, COL_RED, "Teeth! Something huge and grey bites and turns away. Get out of the water!");
        }
        break;
    }
    case CR_EEL: {
        /* lurks in its channel; lunges at anyone who comes near the water's edge */
        vec3 want;
        if (dist < 5.0f && !g->dead) {
            glm_vec3_copy((vec3){g->pos[0], c->home[1], g->pos[2]}, want);
            c->state = ST_CHASE;
        } else {
            float a = c->phase * 0.3f;
            glm_vec3_copy((vec3){c->home[0] + cosf(a) * 3.0f, c->home[1], c->home[2] + sinf(a) * 6.0f}, want);
            c->state = ST_WANDER;
        }
        vec3 d;
        glm_vec3_sub(want, c->pos, d);
        float len = glm_vec3_norm(d);
        if (len > 0.05f) {
            c->yaw += glm_clamp(wrap_angle(atan2f(d[0], d[2]) - c->yaw), -dt * 3.0f, dt * 3.0f);
            glm_vec3_muladds(d, fminf(len, (c->state == ST_CHASE ? 4.5f : 1.5f) * dt) / len, c->pos);
        }
        if (glm_vec3_distance(c->pos, c->home) > 9.0f)
            glm_vec3_lerp(c->pos, c->home, dt, c->pos);
        c->attack_t -= dt;
        if (dist < 2.4f && c->attack_t <= 0.0f && !g->dead) {
            c->attack_t = 1.4f;
            hurt_player(g, c->pos, 14.0f);
            audio_play_at(SFX_EEL, c->pos, 1.0f);
        }
        break;
    }
    case CR_BAT: {
        /* flutters about, and every so often dives at you */
        vec3 want;
        c->attack_t -= dt;
        bool dive = dist < 10.0f && c->attack_t < 0.8f && !g->dead && g->shadow_t <= 0.0f;
        if (dive)
            glm_vec3_copy((vec3){g->pos[0], g->pos[1] + 1.5f, g->pos[2]}, want);
        else
            glm_vec3_copy((vec3){c->home[0] + sinf(c->phase * 1.3f) * 4.0f, c->home[1] + 2.5f + sinf(c->phase * 2.1f),
                                 c->home[2] + cosf(c->phase * 0.9f) * 4.0f}, want);
        vec3 d;
        glm_vec3_sub(want, c->pos, d);
        float len = glm_vec3_norm(d);
        if (len > 0.05f) {
            glm_vec3_muladds(d, fminf(len, (dive ? 7.0f : 3.0f) * dt) / len, c->pos);
            c->yaw = atan2f(d[0], d[2]);
        }
        if (dive && glm_vec3_distance(c->pos, (vec3){g->pos[0], g->pos[1] + 1.5f, g->pos[2]}) < 0.8f) {
            hurt_player(g, c->pos, 4.0f);
            audio_play_at(SFX_BAT, c->pos, 1.0f);
            c->attack_t = 3.0f + frand() * 2.0f;
        }
        if (c->attack_t < -1.0f)
            c->attack_t = 3.0f;
        if (frand() < dt * 0.3f && dist < 25.0f)
            audio_play_at(SFX_BAT, c->pos, 0.5f);
        break;
    }
    default:
        break;
    }
    (void)to_player;
}

/* ---------- poses ---------- */

void beast_pose(Game *g, Creature *c)
{
    const Model *m = beast_model(g, c);
    if (!m)
        return;
    const Rig *r = beast_rig(g, c);
    if (r) {
        /* the KayKit ones walk, run and swing like the skeletons */
        pose_reset(&c->pose, m);
        int anim = r->idle;
        float t = c->anim_t;
        bool loop = true;
        switch (c->state) {
        case ST_WANDER: anim = r->walk; break;
        case ST_CHASE: case ST_FLEE: anim = r->run; break;
        case ST_ATTACK: anim = r->attack; t = c->attack_t * 1.2f; loop = false; break;
        case ST_DANCE: anim = r->cheer; break;
        case ST_DEAD: anim = r->death; t = c->death_t; loop = false; break;
        default: break;
        }
        if (c->type == CR_WRAITH && c->state != ST_ATTACK && c->state != ST_DEAD)
            anim = r->idle;         /* wraiths glide, they don't walk */
        anim_apply(m, anim, t, loop, 1.0f, &c->pose);
        if (c->hit_flash > 0.0f && c->state != ST_DEAD)
            anim_apply(m, r->hit, 0.2f - c->hit_flash, false, 0.6f, &c->pose);
        pose_update(m, &c->pose);
        return;
    }
    pose_reset(&c->pose, m);
    versor q;
    switch (c->type) {
    case CR_CRAB:
        for (int s = 0; s < 2; s++) {
            float snap = c->state == ST_ATTACK ? sinf(glm_clamp(c->attack_t / 0.4f, 0, 1) * GLM_PIf) * 0.8f : sinf(c->anim_t * 3.0f + s) * 0.1f;
            glm_quatv(q, -snap, (vec3){1, 0, 0});
            glm_quat_mul(c->pose.r[1 + s], q, c->pose.r[1 + s]);
        }
        break;
    case CR_SHARK:
        glm_quatv(q, sinf(c->anim_t * (c->state == ST_CHASE ? 9.0f : 4.0f)) * 0.3f, (vec3){0, 1, 0});
        glm_quat_mul(c->pose.r[1], q, c->pose.r[1]);
        break;
    case CR_EEL:
        for (int b = 1; b < 6; b++) {
            glm_quatv(q, sinf(c->anim_t * 5.0f - b * 0.9f) * 0.35f, (vec3){0, 1, 0});
            glm_quat_mul(c->pose.r[b], q, c->pose.r[b]);
        }
        break;
    case CR_BAT: {
        float flap = sinf(c->anim_t * 22.0f) * 0.9f;
        glm_quatv(q, -flap, (vec3){0, 0, 1});
        glm_quat_mul(c->pose.r[1], q, c->pose.r[1]);
        glm_quatv(q, flap, (vec3){0, 0, 1});
        glm_quat_mul(c->pose.r[2], q, c->pose.r[2]);
        break;
    }
    default:
        break;
    }
    pose_update(m, &c->pose);
}

void beast_draw(Game *g, Creature *c, GLuint prog, mat4 vp, mat4 xf, const DrawParams *dp)
{
    const Model *m = beast_model(g, c);
    if (!m)
        return;
    DrawParams bp = *dp;
    if (!dp->depth_only) {
        if (c->type == CR_IMP)
            glm_vec4_add(bp.tint, (vec4){0.35f + 0.1f * sinf(g->time * 7.0f + c->phase), 0.08f, 0.0f, 0.0f}, bp.tint);
        if (c->type == CR_WRAITH)
            glm_vec4_add(bp.tint, (vec4){0.02f, 0.08f, 0.16f, 0.0f}, bp.tint);
        if (c->type == CR_KING)
            glm_vec4_add(bp.tint, (vec4){0.0f, 0.05f, 0.06f, 0.0f}, bp.tint);
        if (c->type == CR_JELLYFISH && g->night)
            glm_vec4_add(bp.tint, (vec4){0.3f, 0.1f, 0.4f, 0.0f}, bp.tint);
    }
    mat4 t;
    glm_mat4_copy(xf, t);
    if (c->type == CR_JELLYFISH) {
        float pulse = 1.0f + sinf(c->phase * 3.0f) * 0.12f;
        glm_scale(t, (vec3){pulse, 1.0f / pulse, pulse});
    }
    model_draw(m, beast_rig(g, c) || c->type != CR_JELLYFISH ? &c->pose : NULL, prog, vp, t, &bp);
    /* the Drowned carry rusted blades; the King a great axe and a crown */
    const Rig *r = beast_rig(g, c);
    if (r && (c->type == CR_DROWNED || c->type == CR_KING) && r->hand_r >= 0) {
        mat4 body, w;
        model_matrix(m, t, body);
        glm_mat4_mul(body, c->pose.world[r->hand_r], w);
        model_draw(&g->weapon_models[c->type == CR_KING ? WPN_AXE : WPN_BLADE], NULL, prog, vp, w, &bp);
    }
}
