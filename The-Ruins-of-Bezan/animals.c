#include "game.h"
#include "shapes.h"

#include <string.h>

/* The coast's wildlife, all built in code:
 *   walruses  lounge on the west rocks and whistle to each other across the water; one
 *             whistling sets the others off. Feed them fish and they'll whistle for you.
 *   gulls     wheel over the beach and the marina, and settle on posts and sand
 *   crabs     little hermit crabs scuttling sideways about the sand
 *   turtles   paddle slowly through the shallows
 *   fish      shoals that turn and flash in the clear water
 *   dolphins  loop out past the harbour mouth, leaping now and then
 * None of them will hurt you. (The jellyfish and the sharks are in beasts.c.) */

enum { A_REST, A_MOVE, A_SWIM, A_FLY, A_LEAP };

/* ---------- models ---------- */

typedef struct {
    MeshBuilder mb[5];
    Material mat[5];
    int n;
} Parts;

static int part(Parts *p, float r, float g, float b, float rough, const float *glow)
{
    mb_init(&p->mb[p->n]);
    material_color(&p->mat[p->n], r, g, b, rough, 0.0f);
    if (glow)
        glm_vec3_copy((float *)glow, p->mat[p->n].emissive);
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

static void slab(Parts *p, int m, int bone, vec3 c, vec3 half, float yaw, float roll)
{
    mat4 xf;
    glm_translate_make(xf, c);
    glm_rotate_y(xf, yaw, xf);
    glm_rotate_z(xf, roll, xf);
    p->mb[m].joint = bone;
    mb_box(&p->mb[m], xf, half, 1.0f);
}

static void tusk(Parts *p, int m, int bone, vec3 at)
{
    mat4 xf;
    glm_translate_make(xf, at);
    glm_rotate_x(xf, GLM_PIf - 0.25f, xf);
    p->mb[m].joint = bone;
    mb_cylinder(&p->mb[m], xf, 0.035f, 0.012f, 0.42f, 8, 1.0f);
}

/* bones as translations from their parent (pivots), for model_from_builders_skinned */
static void skinned(Model *m, Parts *p, int bones, const int *parents, const vec3 *pivots)
{
    mat4 rest[8];
    for (int b = 0; b < bones; b++) {
        vec3 rel;
        if (parents[b] >= 0)
            glm_vec3_sub((float *)pivots[b], (float *)pivots[parents[b]], rel);
        else
            glm_vec3_copy((float *)pivots[b], rel);
        glm_translate_make(rest[b], rel);
    }
    model_from_builders_skinned(m, p->mb, p->mat, p->n, bones, parents, rest);
    for (int i = 0; i < p->n; i++)
        mb_free(&p->mb[i]);
}

/* bones: 0 hips, 1 chest, 2 head, 3/4 front flippers, 5 hind flippers */
static void build_walrus(Model *m)
{
    Parts p = {0};
    int hide = part(&p, 0.52f, 0.33f, 0.26f, 0.65f, NULL);
    int pale = part(&p, 0.72f, 0.56f, 0.47f, 0.7f, NULL);
    int ivory = part(&p, 0.9f, 0.86f, 0.72f, 0.35f, NULL);
    int dark = part(&p, 0.02f, 0.02f, 0.02f, 0.2f, NULL);
    blob(&p, hide, 0, (vec3){0, 0.5f, -0.35f}, (vec3){0.72f, 0.55f, 1.0f});
    blob(&p, hide, 1, (vec3){0, 0.62f, 0.4f}, (vec3){0.68f, 0.62f, 0.7f});
    blob(&p, hide, 2, (vec3){0, 0.95f, 1.0f}, (vec3){0.36f, 0.33f, 0.38f});
    blob(&p, pale, 2, (vec3){0, 0.84f, 1.28f}, (vec3){0.28f, 0.2f, 0.18f});     /* the whiskery muzzle */
    for (int s = -1; s <= 1; s += 2) {
        tusk(&p, ivory, 2, (vec3){s * 0.1f, 0.74f, 1.36f});
        blob(&p, dark, 2, (vec3){s * 0.2f, 1.06f, 1.2f}, (vec3){0.035f, 0.035f, 0.035f});
        for (int w = 0; w < 3; w++)
            slab(&p, pale, 2, (vec3){s * 0.24f, 0.82f + w * 0.03f, 1.38f}, (vec3){0.09f, 0.004f, 0.004f}, s * 0.3f, s * (w - 1) * 0.2f);
        slab(&p, hide, 3 + (s > 0), (vec3){s * 0.72f, 0.12f, 0.55f}, (vec3){0.28f, 0.05f, 0.2f}, s * 0.5f, s * 0.3f);
    }
    slab(&p, hide, 5, (vec3){0, 0.18f, -1.35f}, (vec3){0.45f, 0.06f, 0.22f}, 0, 0);
    static const int parents[6] = { -1, 0, 1, 1, 1, 0 };
    static const vec3 pivots[6] = { { 0, 0.5f, -0.2f }, { 0, 0.6f, 0.3f }, { 0, 0.85f, 0.8f },
                                    { -0.55f, 0.25f, 0.5f }, { 0.55f, 0.25f, 0.5f }, { 0, 0.3f, -1.1f } };
    skinned(m, &p, 6, parents, pivots);
}

/* bones: 0 body, 1 left wing, 2 right wing */
static void build_gull(Model *m)
{
    Parts p = {0};
    int white = part(&p, 0.88f, 0.88f, 0.86f, 0.7f, NULL);
    int grey = part(&p, 0.45f, 0.48f, 0.52f, 0.7f, NULL);
    int beak = part(&p, 0.95f, 0.75f, 0.1f, 0.5f, NULL);
    int dark = part(&p, 0.03f, 0.03f, 0.03f, 0.2f, NULL);
    blob(&p, white, 0, (vec3){0, 0, 0}, (vec3){0.1f, 0.1f, 0.24f});
    blob(&p, white, 0, (vec3){0, 0.08f, 0.2f}, (vec3){0.07f, 0.07f, 0.08f});
    blob(&p, beak, 0, (vec3){0, 0.07f, 0.3f}, (vec3){0.018f, 0.018f, 0.05f});
    for (int s = -1; s <= 1; s += 2) {
        blob(&p, dark, 0, (vec3){s * 0.05f, 0.11f, 0.24f}, (vec3){0.012f, 0.012f, 0.012f});
        slab(&p, grey, 1 + (s > 0), (vec3){s * 0.34f, 0.03f, 0.0f}, (vec3){0.3f, 0.01f, 0.1f}, 0, 0);
        slab(&p, dark, 1 + (s > 0), (vec3){s * 0.62f, 0.03f, -0.02f}, (vec3){0.05f, 0.011f, 0.08f}, 0, 0);
    }
    slab(&p, grey, 0, (vec3){0, 0.0f, -0.26f}, (vec3){0.07f, 0.01f, 0.07f}, 0, 0);
    static const int parents[3] = { -1, 0, 0 };
    static const vec3 pivots[3] = { { 0, 0, 0 }, { -0.08f, 0.03f, 0 }, { 0.08f, 0.03f, 0 } };
    skinned(m, &p, 3, parents, pivots);
}

/* bones: 0 body, 1-4 flippers, 5 head */
static void build_turtle(Model *m)
{
    Parts p = {0};
    int shell = part(&p, 0.22f, 0.3f, 0.12f, 0.45f, NULL);
    int skin = part(&p, 0.45f, 0.5f, 0.35f, 0.7f, NULL);
    blob(&p, shell, 0, (vec3){0, 0.1f, 0}, (vec3){0.42f, 0.16f, 0.52f});
    blob(&p, skin, 5, (vec3){0, 0.08f, 0.6f}, (vec3){0.11f, 0.09f, 0.14f});
    for (int k = 0; k < 4; k++) {
        float sx = k % 2 ? 1.0f : -1.0f, front = k < 2 ? 1.0f : -1.0f;
        slab(&p, skin, 1 + k, (vec3){sx * 0.5f, 0.05f, front * 0.3f}, (vec3){front > 0 ? 0.26f : 0.14f, 0.02f, 0.08f}, sx * front * 0.4f, 0);
    }
    static const int parents[6] = { -1, 0, 0, 0, 0, 0 };
    static const vec3 pivots[6] = { { 0, 0.05f, 0 }, { -0.35f, 0.05f, 0.3f }, { 0.35f, 0.05f, 0.3f },
                                    { -0.35f, 0.05f, -0.3f }, { 0.35f, 0.05f, -0.3f }, { 0, 0.08f, 0.5f } };
    skinned(m, &p, 6, parents, pivots);
}

/* bones: 0 body, 1 tail, 2 flukes */
static void build_dolphin(Model *m)
{
    Parts p = {0};
    int back = part(&p, 0.35f, 0.4f, 0.46f, 0.25f, NULL);
    int belly = part(&p, 0.78f, 0.8f, 0.82f, 0.3f, NULL);
    int dark = part(&p, 0.02f, 0.02f, 0.02f, 0.2f, NULL);
    blob(&p, back, 0, (vec3){0, 0.04f, 0.2f}, (vec3){0.3f, 0.32f, 0.85f});
    blob(&p, belly, 0, (vec3){0, -0.06f, 0.25f}, (vec3){0.26f, 0.24f, 0.75f});
    blob(&p, back, 0, (vec3){0, 0.0f, 1.12f}, (vec3){0.07f, 0.06f, 0.22f});        /* the beak */
    blob(&p, back, 1, (vec3){0, 0.02f, -0.7f}, (vec3){0.16f, 0.17f, 0.5f});
    slab(&p, back, 2, (vec3){0, 0.0f, -1.25f}, (vec3){0.36f, 0.02f, 0.1f}, 0, 0);
    slab(&p, back, 0, (vec3){0, 0.38f, 0.0f}, (vec3){0.02f, 0.16f, 0.14f}, 0, 0);   /* dorsal fin */
    for (int s = -1; s <= 1; s += 2) {
        blob(&p, dark, 0, (vec3){s * 0.16f, 0.08f, 0.82f}, (vec3){0.025f, 0.025f, 0.025f});
        slab(&p, back, 0, (vec3){s * 0.34f, -0.14f, 0.4f}, (vec3){0.16f, 0.015f, 0.07f}, s * 0.4f, s * -0.3f);
    }
    static const int parents[3] = { -1, 0, 1 };
    static const vec3 pivots[3] = { { 0, 0, 0.1f }, { 0, 0, -0.4f }, { 0, 0, -1.05f } };
    skinned(m, &p, 3, parents, pivots);
}

/* a little hermit crab in a borrowed spiral shell (rigid: it scuttles as a whole) */
static void build_crab(Model *m)
{
    Parts p = {0};
    int shell = part(&p, 0.8f, 0.62f, 0.45f, 0.4f, NULL);
    int body = part(&p, 0.8f, 0.3f, 0.15f, 0.5f, NULL);
    int dark = part(&p, 0.02f, 0.02f, 0.02f, 0.2f, NULL);
    blob(&p, shell, 0, (vec3){0, 0.08f, -0.03f}, (vec3){0.07f, 0.07f, 0.09f});
    blob(&p, shell, 0, (vec3){0, 0.14f, -0.08f}, (vec3){0.04f, 0.04f, 0.05f});
    blob(&p, body, 0, (vec3){0, 0.04f, 0.06f}, (vec3){0.05f, 0.03f, 0.04f});
    for (int s = -1; s <= 1; s += 2) {
        blob(&p, body, 0, (vec3){s * 0.06f, 0.04f, 0.11f}, (vec3){0.025f, 0.018f, 0.03f});   /* claws */
        blob(&p, dark, 0, (vec3){s * 0.02f, 0.08f, 0.1f}, (vec3){0.008f, 0.008f, 0.008f});
        for (int k = 0; k < 3; k++)
            slab(&p, body, 0, (vec3){s * 0.06f, 0.02f, 0.03f - k * 0.025f}, (vec3){0.035f, 0.004f, 0.004f}, 0, s * -0.4f);
    }
    model_from_builders(m, p.mb, p.mat, p.n);
    for (int i = 0; i < p.n; i++)
        mb_free(&p.mb[i]);
}

void animals_load(Game *g)
{
    build_walrus(&g->animal_models[SPECIES_WALRUS]);
    build_gull(&g->animal_models[SPECIES_GULL]);
    build_turtle(&g->animal_models[SPECIES_TURTLE]);
    build_dolphin(&g->animal_models[SPECIES_DOLPHIN]);
    build_crab(&g->animal_models[SPECIES_CRAB]);
    shape_fish(&g->animal_models[SPECIES_FISH], (vec3){0.25f, 0.5f, 0.6f}, (vec3){0.85f, 0.9f, 0.9f}, 0.26f, 0.28f, NULL);
}

void animals_free(Game *g)
{
    for (int i = 0; i < MAX_ANIMALS; i++)
        if (g->animals[i].used)
            pose_free(&g->animals[i].pose);
    for (int s = 0; s < SPECIES_COUNT; s++)
        model_free(&g->animal_models[s]);
}

void animal_spawn(Game *g, Species sp, vec3 pos, float yaw)
{
    for (int i = 0; i < MAX_ANIMALS; i++) {
        Animal *a = &g->animals[i];
        if (a->used)
            continue;
        memset(a, 0, sizeof *a);
        a->used = true;
        a->species = sp;
        a->xf = TRANSFORM_AT(pos, yaw);
        glm_vec3_copy(pos, a->home);
        glm_vec3_copy(pos, a->target);
        a->phase = frand() * 10.0f;
        a->call_t = 4.0f + frand() * 14.0f;
        a->state = sp == SPECIES_GULL ? A_FLY : (sp == SPECIES_FISH || sp == SPECIES_TURTLE || sp == SPECIES_DOLPHIN) ? A_SWIM : A_REST;
        pose_init(&a->pose, &g->animal_models[sp]);
        return;
    }
}

Animal *animal_near(Game *g, Species sp, float reach)
{
    Animal *best = NULL;
    float bd = reach;
    for (int i = 0; i < MAX_ANIMALS; i++) {
        Animal *a = &g->animals[i];
        if (!a->used || a->species != sp || a->gone_t > 0.0f)
            continue;
        float d = glm_vec3_distance(a->pos, g->pos);
        if (d < bd) {
            bd = d;
            best = a;
        }
    }
    return best;
}

static void whistle(Game *g, Animal *a, float len, float pitch)
{
    a->whistle_t = len;
    audio_play_at_pitch(SFX_WALRUS_WHISTLE, a->pos, 1.0f, pitch);
    /* the others answer, one after another */
    for (int i = 0; i < MAX_ANIMALS; i++) {
        Animal *o = &g->animals[i];
        if (o != a && o->used && o->species == SPECIES_WALRUS && o->whistle_t <= 0.0f &&
            glm_vec3_distance(o->pos, a->pos) < 25.0f && frand() < 0.6f)
            o->call_t = fminf(o->call_t, 0.6f + frand() * 1.6f);
    }
}

void animal_feed(Game *g, Animal *a)
{
    a->fed++;
    g->quest.walrus_fed++;
    audio_play(SFX_EAT, 0.8f);
    whistle(g, a, 3.2f, 1.1f);
    add_shells(g, SHELL_PINK, 1);
    message(g, COL_FUN, "The walrus gulps the fish down and whistles a happy little tune. It nudges a pink shell toward you.");
    if (g->quest.walrus_fed >= 5 && g->quest.stage[Q_WALRUS] < 2) {
        g->quest.stage[Q_WALRUS] = 2;
        game_give(g, g->gill_pearl ? ITEM_DOLPHIN_CHARM : ITEM_GILL_PEARL, 1);
        audio_play(SFX_QUEST_DONE, 1.0f);
        message(g, COL_GOLD, "The whole colony whistles a chorus for you, and the biggest walrus spits up something shiny: a gift!");
        for (int i = 0; i < MAX_ANIMALS; i++)
            if (g->animals[i].used && g->animals[i].species == SPECIES_WALRUS)
                g->animals[i].call_t = 0.3f + frand() * 2.0f;
    } else if (g->quest.stage[Q_WALRUS] == 0) {
        g->quest.stage[Q_WALRUS] = 1;
    }
}

void animals_serenade(Game *g)
{
    int k = 0;
    for (int i = 0; i < MAX_ANIMALS; i++) {
        Animal *a = &g->animals[i];
        if (a->used && a->species == SPECIES_WALRUS && glm_vec3_distance(a->pos, g->pos) < 35.0f)
            a->call_t = 0.5f + (k++) * 0.8f;        /* one after another, in time with you */
    }
    if (k)
        message(g, COL_FUN, "You strum the ukulele. One by one, the walruses start whistling along.");
    else
        message(g, COL_FUN, "You strum a jaunty little tune on the ukulele.");
}

void animals_regenerate(Game *g)
{
    for (int i = 0; i < MAX_ANIMALS; i++) {
        Animal *a = &g->animals[i];
        if (!a->used)
            continue;
        a->gone_t = 0.0f;
        glm_vec3_copy(a->home, a->pos);
    }
}

/* ---------- behaviour ---------- */

static void wander_target(Animal *a, float radius, float y)
{
    float ang = frand() * 6.2832f, r = radius * (0.3f + frand() * 0.7f);
    glm_vec3_copy((vec3){a->home[0] + cosf(ang) * r, y, a->home[2] + sinf(ang) * r}, a->target);
}

static void steer(Animal *a, float speed, float turn, float dt)
{
    vec3 d;
    glm_vec3_sub(a->target, a->pos, d);
    float flat = sqrtf(d[0] * d[0] + d[2] * d[2]);
    if (flat > 1e-3f)
        a->yaw += glm_clamp(wrap_angle(atan2f(d[0], d[2]) - a->yaw), -turn * dt, turn * dt);
    a->pos[0] += sinf(a->yaw) * speed * dt;
    a->pos[2] += cosf(a->yaw) * speed * dt;
}

static void update_walrus(Game *g, Animal *a, float dt, float dist)
{
    float surface, depth;
    bool sea = sea_at(g, a->pos[0], a->pos[2], &surface, &depth) && depth > 0.8f;
    a->call_t -= dt;
    if (a->call_t <= 0.0f) {
        a->call_t = 8.0f + frand() * 16.0f;
        if (dist < 45.0f) {
            if (frand() < 0.8f)
                whistle(g, a, 2.2f + frand() * 1.2f, 0.9f + frand() * 0.25f);
            else
                audio_play_at(SFX_WALRUS_GRUNT, a->pos, 0.9f);
        }
    }
    a->whistle_t = fmaxf(0.0f, a->whistle_t - dt);
    if (sea) {
        a->state = A_SWIM;
        a->pos[1] = surface - 0.55f;
        if (glm_vec3_distance(a->pos, a->target) < 1.5f || a->state_t > 12.0f) {
            wander_target(a, 12.0f, a->pos[1]);
            a->state_t = 0.0f;
        }
        steer(a, 1.2f, 1.0f, dt);
        vec3 test = { a->pos[0], a->pos[1], a->pos[2] };
        if (!sea_at(g, test[0], test[2], &surface, &depth))
            glm_vec3_copy(a->home, a->target);
    } else {
        a->state_t -= dt;
        if (a->state == A_MOVE) {
            steer(a, 0.6f, 1.2f, dt);
            if (glm_vec3_distance((vec3){a->pos[0], a->target[1], a->pos[2]}, a->target) < 0.6f)
                a->state = A_REST, a->state_t = 6.0f + frand() * 10.0f;
        } else if (a->state_t <= 0.0f && a->whistle_t <= 0.0f) {
            wander_target(a, 5.0f, a->home[1]);
            a->state = A_MOVE;
        }
        level_collide(&g->level, a->pos, 0.7f, 1.0f);
        a->pos[1] = ground_at(g, a->pos[0], a->pos[2], a->pos[1] + 0.6f);
    }
    /* pose: breathing, the galumph, and the whistle's pucker and raised head */
    const Model *m = &g->animal_models[SPECIES_WALRUS];
    pose_reset(&a->pose, m);
    float breathe = 1.0f + sinf(a->anim_t * 1.6f) * 0.03f;
    glm_vec3_copy((vec3){breathe, breathe, 1.0f}, a->pose.s[1]);
    versor q;
    float lift = a->whistle_t > 0.0f ? -0.45f * fminf(1.0f, a->whistle_t * 2.0f) : sinf(a->anim_t * 0.4f + a->phase) * 0.08f;
    glm_quatv(q, lift, (vec3){1, 0, 0});
    glm_quat_mul(a->pose.r[2], q, a->pose.r[2]);
    if (a->whistle_t > 0.0f) {
        float pucker = 1.0f + 0.12f * sinf(a->anim_t * 18.0f);
        glm_vec3_copy((vec3){0.95f, 1.0f, pucker}, a->pose.s[2]);
    }
    bool moving = a->state == A_MOVE || a->state == A_SWIM;
    for (int s = 0; s < 2; s++) {
        glm_quatv(q, (moving ? sinf(a->anim_t * 5.0f + s * 3.14f) * 0.6f : 0.1f), (vec3){0, 0, 1});
        glm_quat_mul(a->pose.r[3 + s], q, a->pose.r[3 + s]);
    }
    glm_quatv(q, sinf(a->anim_t * (moving ? 5.0f : 1.2f)) * 0.3f, (vec3){1, 0, 0});
    glm_quat_mul(a->pose.r[5], q, a->pose.r[5]);
    pose_update(m, &a->pose);
}

static void update_gull(Game *g, Animal *a, float dt, float dist)
{
    a->call_t -= dt;
    if (a->call_t <= 0.0f) {
        a->call_t = 5.0f + frand() * 12.0f;
        if (dist < 60.0f)
            audio_play_at_pitch(SFX_GULL, a->pos, 0.8f, 0.9f + frand() * 0.25f);
    }
    if (a->state == A_FLY) {
        /* wheeling in wide circles over home */
        float ang = a->anim_t * 0.35f + a->phase;
        glm_vec3_copy((vec3){a->home[0] + cosf(ang) * 14.0f, a->home[1] + 4.0f + sinf(a->anim_t * 0.5f) * 2.0f,
                             a->home[2] + sinf(ang) * 14.0f}, a->target);
        vec3 d;
        glm_vec3_sub(a->target, a->pos, d);
        glm_vec3_lerp(a->pos, a->target, 1.0f - expf(-dt * 1.2f), a->pos);
        if (glm_vec3_norm(d) > 0.05f)
            a->yaw = atan2f(d[0], d[2]);
        a->roll = -0.35f;
        if ((a->state_t -= dt) <= 0.0f && frand() < 0.3f) {
            a->state = A_MOVE;       /* coming in to land */
            float gy = ground_at(g, a->home[0], a->home[2], a->home[1] + 20.0f);
            glm_vec3_copy((vec3){a->home[0] + (frand() - 0.5f) * 6.0f, gy, a->home[2] + (frand() - 0.5f) * 6.0f}, a->target);
            a->target[1] = ground_at(g, a->target[0], a->target[2], a->home[1] + 20.0f);
        } else if (a->state_t <= 0.0f)
            a->state_t = 10.0f + frand() * 10.0f;
    } else if (a->state == A_MOVE) {
        glm_vec3_lerp(a->pos, a->target, 1.0f - expf(-dt * 1.5f), a->pos);
        a->roll = 0.0f;
        if (glm_vec3_distance(a->pos, a->target) < 0.2f)
            a->state = A_REST, a->state_t = 8.0f + frand() * 15.0f;
    } else {
        a->roll = 0.0f;
        if ((a->state_t -= dt) <= 0.0f || dist < 4.0f) {
            a->state = A_FLY;
            a->state_t = 12.0f + frand() * 10.0f;
            if (dist < 4.0f)
                audio_play_at(SFX_GULL, a->pos, 1.0f);
        }
    }
    const Model *m = &g->animal_models[SPECIES_GULL];
    pose_reset(&a->pose, m);
    float flap = a->state == A_REST ? -0.1f : sinf(a->anim_t * (a->state == A_MOVE ? 14.0f : 6.0f)) * 0.7f;
    versor q;
    glm_quatv(q, flap, (vec3){0, 0, 1});
    glm_quat_mul(a->pose.r[2], q, a->pose.r[2]);
    glm_quatv(q, -flap, (vec3){0, 0, 1});
    glm_quat_mul(a->pose.r[1], q, a->pose.r[1]);
    if (a->state == A_REST) {       /* wings folded */
        glm_vec3_copy((vec3){0.25f, 1, 1}, a->pose.s[1]);
        glm_vec3_copy((vec3){0.25f, 1, 1}, a->pose.s[2]);
    }
    pose_update(m, &a->pose);
}

static void update_swimmer(Game *g, Animal *a, float dt, float dist, float speed, float radius)
{
    float surface, depth;
    if (!sea_at(g, a->pos[0], a->pos[2], &surface, &depth)) {
        glm_vec3_copy(a->home, a->target);
        surface = SEA_Y;
        depth = 3.0f;
    }
    if (glm_vec3_distance((vec3){a->pos[0], 0, a->pos[2]}, (vec3){a->target[0], 0, a->target[2]}) < 1.5f || (a->state_t -= dt) <= 0.0f) {
        wander_target(a, radius, a->home[1]);
        a->state_t = 8.0f + frand() * 8.0f;
    }
    /* scatter from a swimmer */
    bool scared = g->swimming && dist < 5.0f;
    if (scared) {
        vec3 away;
        glm_vec3_sub(a->pos, g->pos, away);
        glm_vec3_add(a->pos, away, a->target);
    }
    steer(a, speed * (scared ? 2.5f : 1.0f), 1.5f, dt);
    float bottom = surface - depth;
    a->pos[1] = glm_clamp(a->home[1] + sinf(a->anim_t * 0.3f + a->phase) * 0.5f, bottom + 0.3f, surface - 0.4f);
}

static void update_dolphin(Game *g, Animal *a, float dt, float dist)
{
    float surface, depth;
    if (!sea_at(g, a->pos[0], a->pos[2], &surface, &depth))
        surface = SEA_Y, depth = 5.0f;
    float ang = a->anim_t * 0.12f + a->phase;
    glm_vec3_copy((vec3){a->home[0] + cosf(ang) * 40.0f, surface, a->home[2] + sinf(ang) * 25.0f}, a->target);
    steer(a, 5.0f, 1.2f, dt);
    if (a->state == A_LEAP) {
        a->state_t += dt;
        float t = a->state_t / 1.2f;
        a->pos[1] = surface - 0.4f + sinf(t * GLM_PIf) * 2.4f;
        a->pitch = -cosf(t * GLM_PIf) * 0.9f;
        if (t >= 1.0f) {
            a->state = A_SWIM;
            a->pitch = 0.0f;
            audio_play_at(SFX_SPLASH, a->pos, 0.8f);
            for (int i = 0; i < 20; i++) {
                Particle p = {0};
                glm_vec3_copy((vec3){a->pos[0] + (frand() - 0.5f), surface, a->pos[2] + (frand() - 0.5f)}, p.pos);
                glm_vec3_copy((vec3){(frand() - 0.5f) * 3, 2.0f + frand() * 3, (frand() - 0.5f) * 3}, p.vel);
                glm_vec4_copy((vec4){0.8f, 0.95f, 0.95f, 0.7f}, p.color0);
                glm_vec4_copy((vec4){0.8f, 0.95f, 0.95f, 0.0f}, p.color1);
                p.size0 = 0.1f;
                p.size1 = 0.05f;
                p.max_life = p.life = 0.8f;
                p.gravity = 9.0f;
                particles_emit(&g->ps, &p);
            }
        }
    } else {
        a->pos[1] = surface - 0.6f + sinf(a->anim_t * 1.3f) * 0.2f;
        if ((a->call_t -= dt) <= 0.0f) {
            a->call_t = 6.0f + frand() * 10.0f;
            a->state = A_LEAP;
            a->state_t = 0.0f;
            if (dist < 70.0f)
                audio_play_at(SFX_DOLPHIN, a->pos, 1.0f);
        }
    }
    const Model *m = &g->animal_models[SPECIES_DOLPHIN];
    pose_reset(&a->pose, m);
    versor q;
    for (int b = 1; b < 3; b++) {
        glm_quatv(q, sinf(a->anim_t * 4.0f - b) * 0.25f, (vec3){1, 0, 0});
        glm_quat_mul(a->pose.r[b], q, a->pose.r[b]);
    }
    pose_update(m, &a->pose);
}

void animals_update(Game *g, float dt)
{
    for (int i = 0; i < MAX_ANIMALS; i++) {
        Animal *a = &g->animals[i];
        if (!a->used || a->gone_t > 0.0f)
            continue;
        float dist = glm_vec3_distance(a->pos, g->pos);
        if (dist > 160.0f)
            continue;
        a->anim_t += dt;
        switch (a->species) {
        case SPECIES_WALRUS: update_walrus(g, a, dt, dist); break;
        case SPECIES_GULL: update_gull(g, a, dt, dist); break;
        case SPECIES_DOLPHIN: update_dolphin(g, a, dt, dist); break;
        case SPECIES_FISH: update_swimmer(g, a, dt, dist, 1.6f, 7.0f); break;
        case SPECIES_TURTLE: {
            update_swimmer(g, a, dt, dist, 0.7f, 10.0f);
            const Model *m = &g->animal_models[SPECIES_TURTLE];
            pose_reset(&a->pose, m);
            versor q;
            for (int k = 0; k < 4; k++) {
                glm_quatv(q, sinf(a->anim_t * 2.0f + (k % 2) * 3.14f) * 0.5f, (vec3){0, 1, 0});
                glm_quat_mul(a->pose.r[1 + k], q, a->pose.r[1 + k]);
            }
            pose_update(m, &a->pose);
            break;
        }
        case SPECIES_CRAB: {
            /* sideways about the sand; into its shell (well, away) if you come close */
            if (dist < 3.0f) {
                vec3 away;
                glm_vec3_sub(a->pos, g->pos, away);
                glm_vec3_add(a->pos, away, a->target);
                a->state = A_MOVE;
            }
            if (a->state == A_MOVE) {
                vec3 d;
                glm_vec3_sub(a->target, a->pos, d);
                d[1] = 0;
                float len = glm_vec3_norm(d);
                if (len < 0.2f)
                    a->state = A_REST, a->state_t = 2.0f + frand() * 4.0f;
                else
                    glm_vec3_muladds(d, (dist < 3.0f ? 1.6f : 0.5f) * dt / len, a->pos);
                a->yaw = atan2f(d[0], d[2]) + GLM_PI_2f;     /* crabs go sideways */
            } else if ((a->state_t -= dt) <= 0.0f) {
                wander_target(a, 4.0f, a->home[1]);
                a->state = A_MOVE;
            }
            a->pos[1] = ground_at(g, a->pos[0], a->pos[2], a->pos[1] + 0.5f);
            break;
        }
        default:
            break;
        }
    }
}

void animals_draw(Game *g, GLuint prog, mat4 vp, bool depth)
{
    DrawParams dp = { .depth_only = depth };
    Frustum fr;
    frustum_from(vp, &fr);
    for (int i = 0; i < MAX_ANIMALS; i++) {
        Animal *a = &g->animals[i];
        if (!a->used || a->gone_t > 0.0f)
            continue;
        float d = glm_vec3_distance(a->pos, g->eye);
        if (d > (depth ? 40.0f : 110.0f) || !frustum_sphere(&fr, a->pos, 3.0f))
            continue;
        const Model *m = &g->animal_models[a->species];
        if (a->species == SPECIES_FISH) {
            /* a shoal: the same fish several times, each on its own little orbit */
            for (int k = 0; k < 8; k++) {
                float o = k * 0.8f + a->phase;
                Transform t = a->xf;
                t.pos[0] += cosf(o * 2.3f + a->anim_t * 0.9f) * 0.9f;
                t.pos[1] += sinf(o * 1.7f + a->anim_t) * 0.35f;
                t.pos[2] += sinf(o * 3.1f + a->anim_t * 0.8f) * 0.9f;
                t.yaw += sinf(a->anim_t * 9.0f + k) * 0.25f;
                mat4 xf;
                transform_matrix(&t, xf);
                model_draw(m, NULL, prog, vp, xf, &dp);
            }
            continue;
        }
        Transform t = a->xf;
        if (a->species == SPECIES_CRAB)
            t.pos[1] += fabsf(sinf(a->anim_t * 18.0f)) * 0.01f * (a->state == A_MOVE);
        mat4 xf;
        transform_matrix(&t, xf);
        model_draw(m, a->species == SPECIES_CRAB ? NULL : &a->pose, prog, vp, xf, &dp);
    }
}
