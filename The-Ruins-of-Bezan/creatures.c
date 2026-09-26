#include "game.h"
#include "meshgen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Everything that roams the ruins and wants to bite you:
 *   rats      Poly Haven's street rat, scaled up and given a bendy spine in code
 *   foxes     the Khronos Fox with its own Survey / Walk / Run animations
 *   skeletons KayKit skeletons: sleep in the crypt, rise, fight, sometimes get back up
 *   goblins   KayKit's hooded rogue, shrunk, with its skin recolored green
 *   slimes    built in code: a wobbling, hopping blob that splits when it dies */

typedef struct {
    const char *name;
    float hp, walk, run, detect, reach, damage, windup, cycle, radius, height;
} Stats;

static Stats stats(const Creature *c)
{
    switch (c->type) {
    case CR_RAT:    return (Stats){ "Giant Rat", 30, 1.3f, 4.3f, 11, 1.15f, 5, 0.35f, 1.0f, 0.35f, 0.3f };
    case CR_FOX:    return (Stats){ "Ruin Fox", 55, 1.5f, 5.6f, 15, 1.55f, 9, 0.45f, 1.4f, 0.5f, 0.75f };
    case CR_GOBLIN: return (Stats){ "Goblin", 35, 1.6f, 5.2f, 13, 1.4f, 7, 0.35f, 1.1f, 0.4f, 1.2f };
    case CR_SKELETON: {
        static const Stats sk[SK_COUNT] = {
            { "Skeleton", 45, 1.3f, 3.6f, 14, 1.7f, 9, 0.5f, 1.5f, 0.45f, 1.7f },
            { "Skeleton Warrior", 75, 1.2f, 3.2f, 14, 1.9f, 13, 0.6f, 1.7f, 0.5f, 1.8f },
            { "Skeleton Rogue", 40, 1.6f, 4.8f, 15, 1.6f, 8, 0.35f, 1.1f, 0.45f, 1.8f },
            { "Skeleton Mage", 38, 1.2f, 3.0f, 18, 1.6f, 6, 0.5f, 1.5f, 0.45f, 1.9f },
        };
        return sk[c->variant];
    }
    case CR_SLIME: {
        static const Stats sl[3] = {
            { "Great Slime", 44, 0, 3.2f, 11, 1.2f, 8, 0, 1.2f, 0.75f, 1.0f },
            { "Slime", 22, 0, 3.6f, 11, 0.9f, 5, 0, 1.0f, 0.45f, 0.6f },
            { "Little Slime", 8, 0, 4.0f, 11, 0.6f, 2, 0, 0.8f, 0.28f, 0.38f },
        };
        return sl[c->variant];
    }
    case CR_CRAB:     return (Stats){ "Giant Crab", 60, 0.9f, 2.6f, 9, 1.6f, 11, 0.4f, 1.3f, 0.7f, 0.7f };
    case CR_JELLYFISH: return (Stats){ "Jellyfish", 12, 0, 0, 0, 1.3f, 8, 0, 1.5f, 0.4f, 0.4f };
    case CR_SHARK:    return (Stats){ "Shark", 90, 0, 6.2f, 24, 1.8f, 20, 0, 1.6f, 0.8f, 0.8f };
    case CR_BAT:      return (Stats){ "Bat", 10, 0, 7.0f, 10, 0.8f, 4, 0, 3.0f, 0.25f, 0.25f };
    case CR_IMP:      return (Stats){ "Magma Imp", 40, 1.4f, 4.2f, 18, 1.4f, 7, 0.5f, 1.6f, 0.35f, 1.0f };
    case CR_DROWNED:  return (Stats){ "Drowned Guard", 70, 1.2f, 3.4f, 14, 1.8f, 12, 0.55f, 1.6f, 0.45f, 1.7f };
    case CR_WRAITH:   return (Stats){ "Wraith", 50, 1.4f, 3.6f, 16, 1.7f, 9, 0.5f, 1.5f, 0.45f, 1.8f };
    case CR_EEL:      return (Stats){ "Angler Eel", 55, 0, 4.5f, 6, 2.4f, 14, 0, 1.4f, 0.4f, 0.5f };
    case CR_KING:     return (Stats){ "The Drowned King", 650, 1.0f, 2.6f, 30, 3.2f, 24, 0.8f, 2.2f, 1.0f, 3.9f };
    default: return (Stats){ "?", 10, 1, 1, 1, 1, 1, 1, 1, 0.5f, 1 };
    }
}

float creature_radius(const Creature *c) { return stats(c).radius; }
float creature_height(const Creature *c) { return stats(c).height; }
const char *creature_name(const Creature *c) { return stats(c).name; }

/* ---------- loading ---------- */

/* goblin skin: any warm, light skin tone in the texture turns sickly green */
static void goblin_recolor(unsigned char *px, int w, int h)
{
    for (int i = 0; i < w * h; i++) {
        unsigned char *p = &px[i * 4];
        float r = p[0] / 255.0f, g = p[1] / 255.0f, b = p[2] / 255.0f;
        float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b));
        float sat = mx > 0 ? (mx - mn) / mx : 0;
        /* skin: red is the brightest channel, fairly light, moderately saturated */
        if (r >= g && g >= b && mx > 0.55f && sat > 0.15f && sat < 0.6f && (r - g) < 0.3f) {
            float l = (r + g + b) / 3.0f;
            p[0] = (unsigned char)(fminf(1.0f, l * 0.55f) * 255);
            p[1] = (unsigned char)(fminf(1.0f, l * 1.05f) * 255);
            p[2] = (unsigned char)(fminf(1.0f, l * 0.35f) * 255);
        }
    }
}

static void find_rig(const Model *m, Rig *r, const char *attack)
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
    r->talk = model_find_anim(m, "Interact");
    r->sit = model_find_anim(m, "Sit_Chair_Idle");
    r->cheer = model_find_anim(m, "Cheer");
    r->hand_r = model_find_node(m, "handslot.r");
    r->hand_l = model_find_node(m, "handslot.l");
}

/* a slime is two models: the glowing core, eyes and bubbles inside (drawn like anything
 * else), and the jelly around them, drawn afterwards with shaders/slime: glossy, see-
 * through, rippling. The jelly counts as solid for shadows. */
static void build_slime(Model *core, Model *body)
{
    MeshBuilder mbs[3];
    Material mats[3];
    for (int i = 0; i < 3; i++)
        mb_init(&mbs[i]);
    material_color(&mats[0], 0.02f, 0.02f, 0.02f, 0.1f, 0.0f);         /* eyes, glossy black */
    material_color(&mats[1], 0.1f, 0.6f, 0.15f, 0.5f, 0.0f);
    glm_vec3_copy((vec3){0.25f, 1.4f, 0.25f}, mats[1].emissive);        /* the glowing core */
    material_color(&mats[2], 0.6f, 1.0f, 0.65f, 0.2f, 0.0f);
    glm_vec3_copy((vec3){0.1f, 0.4f, 0.12f}, mats[2].emissive);         /* bubbles floating in it */

    mat4 xf;
    glm_translate_make(xf, (vec3){0, 0.42f, 0});
    glm_scale(xf, (vec3){1.0f, 0.9f, 1.0f});
    mb_capsule(&mbs[1], xf, 0.19f, 0.0f, 16);
    for (int s = -1; s <= 1; s += 2) {
        glm_translate_make(xf, (vec3){s * 0.15f, 0.6f, 0.36f});
        mb_capsule(&mbs[0], xf, 0.06f, 0.0f, 12);
        glm_translate_make(xf, (vec3){s * 0.14f, 0.63f, 0.41f});
        mb_capsule(&mbs[2], xf, 0.018f, 0.0f, 6);      /* a highlight in each eye */
    }
    for (int k = 0; k < 7; k++) {
        float a = k * 2.4f, r = 0.12f + (k % 3) * 0.08f;
        glm_translate_make(xf, (vec3){cosf(a) * r, 0.2f + k * 0.06f, sinf(a) * r});
        mb_capsule(&mbs[2], xf, 0.025f + (k % 2) * 0.015f, 0.0f, 8);
    }
    model_from_builders(core, mbs, mats, 3);
    for (int i = 0; i < 3; i++)
        mb_free(&mbs[i]);

    /* the jelly: a smooth, finely divided blob, flattening where it sits */
    MeshBuilder jelly;
    mb_init(&jelly);
    glm_translate_make(xf, (vec3){0, 0.46f, 0});
    glm_scale(xf, (vec3){1.0f, 0.9f, 1.0f});
    mb_capsule(&jelly, xf, 0.5f, 0.0f, 40);
    Material jm;
    material_color(&jm, 0.2f, 0.8f, 0.3f, 0.05f, 0.0f);
    jm.base_color[3] = 1.0f;
    glm_vec3_copy((vec3){0.02f, 0.14f, 0.04f}, jm.emissive);
    model_from_builders(body, &jelly, &jm, 1);
    mb_free(&jelly);
}

/* the fix for one creature model: its size (and anything else, e.g. .yaw to turn
 * a model that faces the wrong way) */
static void resize(Model *m, float scale)
{
    Transform fix = { .scale = { scale, scale, scale } };
    model_adjust(m, &fix, false);
}

void creatures_load(Game *g)
{
    load_or_die(&g->fox_model, "assets/models/fox/Fox.gltf", NULL);
    resize(&g->fox_model, 0.009f);          /* authored in centimeters, and small */
    ModelOptions rig = { .autorig_bones = 8 };
    load_or_die(&g->rat_model, "assets/models/street_rat/street_rat.gltf", &rig);
    resize(&g->rat_model, 6.0f);            /* a street rat, blown up to giant size */
    g->fox_idle = model_find_anim(&g->fox_model, "Survey");
    g->fox_walk = model_find_anim(&g->fox_model, "Walk");
    g->fox_run = model_find_anim(&g->fox_model, "Run");
    for (int i = 0; i < 8; i++) {
        char name[16];
        snprintf(name, sizeof name, "bone%d", i);
        g->rat_bone[i] = model_find_node(&g->rat_model, name);
    }

    static const char *skel[SK_COUNT] = { "Skeleton_Minion", "Skeleton_Warrior", "Skeleton_Rogue", "Skeleton_Mage" };
    static const char *attacks[SK_COUNT] = { "1H_Melee_Attack_Chop", "1H_Melee_Attack_Chop",
                                             "Dualwield_Melee_Attack_Stab", "Spellcast_Shoot" };
    for (int i = 0; i < SK_COUNT; i++) {
        char path[128];
        snprintf(path, sizeof path, "assets/kaykit/%s.glb", skel[i]);
        load_or_die(&g->skel_models[i], path, NULL);
        resize(&g->skel_models[i], i == SK_WARRIOR ? 0.88f : 0.82f);
        find_rig(&g->skel_models[i], &g->skel_rigs[i], attacks[i]);
    }

    ModelOptions green = { .recolor = goblin_recolor };
    load_or_die(&g->goblin_model, "assets/kaykit/Rogue_Hooded.glb", &green);
    static const char *hide[] = { "Knife_Offhand", "1H_Crossbow", "2H_Crossbow", "Throwable" };
    for (size_t i = 0; i < sizeof hide / sizeof hide[0]; i++)
        model_hide_node(&g->goblin_model, hide[i]);
    resize(&g->goblin_model, 0.62f);        /* the hooded rogue, shrunk to goblin size */
    find_rig(&g->goblin_model, &g->goblin_rig, "1H_Melee_Attack_Stab");

    static const char *weapons[WPN_COUNT] = { "Skeleton_Blade", "Skeleton_Axe", "Skeleton_Staff", "Skeleton_Shield_Small_A" };
    for (int i = 0; i < WPN_COUNT; i++) {
        char path[128];
        snprintf(path, sizeof path, "assets/kaykit/%s.gltf", weapons[i]);
        load_or_die(&g->weapon_models[i], path, NULL);
    }

    build_slime(&g->slime_model, &g->slime_body_model);
}

void creatures_free(Game *g)
{
    for (int i = 0; i < MAX_CREATURES; i++)
        if (g->creatures[i].used)
            pose_free(&g->creatures[i].pose);
    model_free(&g->fox_model);
    model_free(&g->rat_model);
    model_free(&g->slime_model);
    model_free(&g->slime_body_model);
    model_free(&g->goblin_model);
    for (int i = 0; i < SK_COUNT; i++)
        model_free(&g->skel_models[i]);
    for (int i = 0; i < WPN_COUNT; i++)
        model_free(&g->weapon_models[i]);
}

static const Model *creature_model(Game *g, const Creature *c)
{
    const Model *b = beast_model(g, c);
    if (b)
        return b;
    switch (c->type) {
    case CR_RAT: return &g->rat_model;
    case CR_FOX: return &g->fox_model;
    case CR_SKELETON: return &g->skel_models[c->variant];
    case CR_GOBLIN: return &g->goblin_model;
    default: return &g->slime_model;
    }
}

static const Rig *creature_rig(Game *g, const Creature *c)
{
    const Rig *b = beast_rig(g, c);
    if (b)
        return b;
    return c->type == CR_SKELETON ? &g->skel_rigs[c->variant] : &g->goblin_rig;
}

Creature *creature_spawn(Game *g, CreatureType type, int variant, vec3 pos, float yaw)
{
    for (int i = 0; i < MAX_CREATURES; i++) {
        Creature *c = &g->creatures[i];
        if (c->used)
            continue;
        memset(c, 0, sizeof *c);
        c->used = true;
        c->type = type;
        c->variant = variant;
        c->xf = TRANSFORM_AT(pos, yaw);
        if (type == CR_SLIME)
            glm_vec3_fill(c->scale, variant == 0 ? 1.5f : variant == 1 ? 0.9f : 0.55f);
        glm_vec3_copy(pos, c->home);
        c->hp = c->max_hp = stats(c).hp;
        c->state = type == CR_SKELETON ? ST_DORMANT : ST_IDLE;
        c->spawn = -1;
        c->phase = frand() * 10.0f;
        c->hover = type == CR_WRAITH ? 0.5f : 0.0f;
        c->state_t = frand() * 2.0f;
        c->anim_t = frand() * 10.0f;
        pose_init(&c->pose, creature_model(g, c));
        g->creatures_left++;
        return c;
    }
    return NULL;
}

/* ---------- damage ---------- */

static void kill(Game *g, Creature *c)
{
    c->state = ST_DEAD;
    c->death_t = 0.0f;
    c->hp = 0.0f;
    g->creatures_left--;
    static const Sfx death_sfx[CR_TYPE_COUNT] = { SFX_RAT_DIE, SFX_FOX_DIE, SFX_SKELETON_DIE, SFX_GOBLIN_DIE, SFX_SLIME_SPLIT,
                                                  SFX_CRAB, SFX_SLIME_SPLIT, SFX_SHARK_BITE, SFX_BAT, SFX_IMP,
                                                  SFX_SKELETON_DIE, SFX_WRAITH, SFX_EEL, SFX_KING_ROAR };
    audio_play_at(death_sfx[c->type], c->pos, 1.0f);
    quest_creature_killed(g, c);

    if (c->type == CR_SLIME && c->variant < 2) {
        /* it splits in two smaller slimes that bounce apart */
        message(g, COL_TEXT, "The %s splits in two!", creature_name(c));
        for (int k = 0; k < 2; k++) {
            vec3 p = { c->pos[0] + (k ? 0.5f : -0.5f), c->pos[1], c->pos[2] };
            Creature *n = creature_spawn(g, CR_SLIME, c->variant + 1, p, c->yaw + (k ? 1.0f : -1.0f));
            if (n) {
                n->aggro = true;
                n->state = ST_CHASE;
                n->vy = 3.0f;
                glm_vec3_copy((vec3){k ? 3.0f : -3.0f, 0, frand() - 0.5f}, n->knock);
            }
        }
        c->used = false;
        pose_free(&c->pose);
        return;
    }
    message(g, COL_TEXT, "The %s is %s.", creature_name(c),
            c->type == CR_SKELETON || c->type == CR_DROWNED ? "a heap of bones" : c->type == CR_WRAITH ? "gone, like smoke" :
            c->type == CR_IMP ? "a cooling cinder" : "dead");
    /* sometimes they were carrying something */
    float r = frand();
    ItemId drop = ITEM_NONE;
    if (c->type == CR_SKELETON && r < 0.35f)
        drop = ITEM_MANA_POTION;
    else if (c->type == CR_GOBLIN && r < 0.5f)
        drop = r < 0.2f ? ITEM_GRENADE : ITEM_CHEESE;
    else if ((c->type == CR_RAT || c->type == CR_FOX) && r < 0.4f)
        drop = r < 0.25f ? ITEM_SWEET_POTATO : ITEM_CHEESE;
    else if (c->type == CR_CRAB && r < 0.5f)
        drop = r < 0.15f ? ITEM_LIME : ITEM_FISH_SARDINE;
    else if ((c->type == CR_DROWNED || c->type == CR_WRAITH) && r < 0.4f)
        drop = ITEM_MANA_POTION;
    else if (c->type == CR_SHARK)
        drop = ITEM_FISH_SNAPPER;
    else if (c->type == CR_IMP && r < 0.3f)
        drop = ITEM_LIME;
    if (c->type == CR_CRAB || c->type == CR_SHARK || c->type == CR_EEL)
        add_shells(g, frand() < 0.2f ? SHELL_BLUE : SHELL_PINK, 1);
    if (drop != ITEM_NONE) {
        for (int i = 0; i < MAX_PICKUPS; i++) {
            Pickup *p = &g->pickups[i];
            if (!p->used) {
                *p = (Pickup){ .used = true, .item = drop, .count = 1 };
                p->xf = TRANSFORM_AT(c->pos, frand() * 6.0f);
                p->pos[1] += 0.05f;
                break;
            }
        }
    }
    if (g->creatures_left == 0)
        message(g, COL_GOLD, "The ruins fall silent. You have cleared them all.");
}

void damage_creature(Game *g, Creature *c, float dmg, vec3 push)
{
    if (c->state == ST_DEAD)
        return;
    /* a skeleton warrior catches blows from the front on its shield */
    if (c->type == CR_SKELETON && c->variant == SK_WARRIOR && c->state != ST_DORMANT) {
        vec3 to_player;
        glm_vec3_sub(g->pos, c->pos, to_player);
        float facing = sinf(c->yaw) * to_player[0] + cosf(c->yaw) * to_player[2];
        if (facing > 0.0f && frand() < 0.4f) {
            dmg *= 0.3f;
            vec3 at = { c->pos[0], c->pos[1] + 1.0f, c->pos[2] };
            fx_sparks(&g->ps, at, 10);
            audio_play_at(SFX_BLOCK, c->pos, 0.8f);
        }
    }
    /* a giant crab's claws and shell take blows from the front */
    if (c->type == CR_CRAB) {
        vec3 to_player;
        glm_vec3_sub(g->pos, c->pos, to_player);
        float facing = (sinf(c->yaw) * to_player[0] + cosf(c->yaw) * to_player[2]) / fmaxf(glm_vec3_norm(to_player), 1e-3f);
        if (facing > 0.5f) {
            dmg *= 0.35f;
            fx_sparks(&g->ps, (vec3){c->pos[0], c->pos[1] + 0.5f, c->pos[2]}, 6);
            audio_play_at(SFX_BLOCK, c->pos, 0.6f);
        }
    }
    /* half of what hits a wraith goes straight through */
    if (c->type == CR_WRAITH && frand() < 0.5f && glm_vec3_norm(push) > 0.5f) {
        fx_magic(&g->ps, (vec3){c->pos[0], c->pos[1] + 1.2f, c->pos[2]}, (vec3){0.5f, 1.0f, 2.0f}, 0.4f, 10);
        return;
    }
    if (c->state == ST_DORMANT) {
        c->state = ST_AWAKEN;       /* rudely woken */
        c->state_t = 0.0f;
    }
    c->hp -= dmg;
    c->hit_flash = 0.18f;
    c->bar_t = 3.0f;
    c->aggro = true;
    glm_vec3_add(c->knock, push, c->knock);
    vec3 at = { c->pos[0], c->pos[1] + creature_height(c) * 0.6f, c->pos[2] };
    vec3 dir;
    glm_vec3_normalize_to(push, dir);
    if (c->type == CR_SKELETON || c->type == CR_DROWNED || c->type == CR_WRAITH)
        fx_dust(&g->ps, at, 6);         /* bone dust, not blood */
    else if (c->type == CR_IMP)
        fx_sparks(&g->ps, at, 14);
    else if (c->type == CR_SLIME || c->type == CR_JELLYFISH)
        fx_slime(&g->ps, at, dir, 14);
    else
        fx_blood(&g->ps, at, dir, 14);
    if (c->type == CR_SKELETON)
        audio_play_at(SFX_BONES, c->pos, 0.8f);
    if (c->hp <= 0.0f)
        kill(g, c);
    else if (c->type == CR_GOBLIN && c->hp < c->max_hp * 0.3f && frand() < 0.5f)
        c->flee_t = 4.0f;               /* goblins lose their nerve */
}

/* ---------- behaviour ---------- */

static void turn_toward(float *yaw, float target, float rate)
{
    *yaw += glm_clamp(wrap_angle(target - *yaw), -rate, rate);
}

static float anim_length(Game *g, const Creature *c, int anim)
{
    const Model *m = creature_model(g, c);
    return anim >= 0 && anim < m->anim_count ? m->anims[anim].duration : 1.0f;
}

static Sfx voice(const Creature *c)
{
    static const Sfx v[CR_TYPE_COUNT] = { SFX_RAT_SQUEAK, SFX_FOX_BARK, SFX_BONES, SFX_GOBLIN, SFX_SLIME,
                                          SFX_CRAB, SFX_SLIME, SFX_SHARK_BITE, SFX_BAT, SFX_IMP, SFX_DROWNED,
                                          SFX_WRAITH, SFX_EEL, SFX_KING_ROAR };
    return v[c->type];
}

static void creature_update(Game *g, Creature *c, float real_dt)
{
    float dt = real_dt * (g->slow_t > 0.0f ? 0.25f : 1.0f);
    Stats st = stats(c);
    float detect = st.detect * (g->mask_on ? 0.45f : 1.0f) * (g->crouching ? 0.6f : 1.0f);
    bool humanoid = c->type == CR_SKELETON || c->type == CR_GOBLIN;

    c->hit_flash = fmaxf(0.0f, c->hit_flash - real_dt);
    c->bar_t -= real_dt;
    c->frozen_t -= real_dt;
    if (c->burn_t > 0.0f && c->state != ST_DEAD) {
        c->burn_t -= real_dt;
        c->hp -= (c->type == CR_IMP || creature_aquatic(c) ? 0.0f : 6.0f) * real_dt;
        if (frand() < real_dt * 20.0f)
            fx_flame(&g->ps, (vec3){c->pos[0] + frand() - 0.5f, c->pos[1] + creature_height(c) * frand(), c->pos[2] + frand() - 0.5f}, 0.4f, 0.05f);
        if (c->hp <= 0.0f) {
            kill(g, c);
            if (!c->used)
                return;
        }
    }
    if (c->frozen_t > 0.0f && c->state != ST_DEAD)
        return;                 /* frozen solid: nothing moves, not even the clock */

    c->anim_t += dt;
    c->state_t += dt;
    c->flee_t -= dt;
    c->dance_t -= dt;

    if (c->state == ST_DEAD) {
        c->death_t += real_dt;
        /* skeletons sometimes pull themselves back together */
        if (c->type == CR_SKELETON && !c->resurrected && c->death_t > 7.0f && frand() < 0.35f) {
            c->resurrected = true;
            c->state = ST_AWAKEN;
            c->state_t = 0.0f;
            c->hp = c->max_hp * 0.6f;
            g->creatures_left++;
            audio_play_at(SFX_BONES, c->pos, 1.0f);
            message(g, COL_RED, "The bones rattle... and reassemble themselves!");
            return;
        }
        if (c->death_t > 7.0f)
            c->resurrected = true;      /* only one chance */
        if (c->death_t > 9.0f)
            c->pos[1] -= real_dt * 0.25f;     /* sink away */
        if (c->death_t > 12.0f) {
            pose_free(&c->pose);
            c->used = false;
        }
        return;
    }

    vec3 to_player;
    glm_vec3_sub(g->pos, c->pos, to_player);
    to_player[1] = 0.0f;
    float dist = glm_vec3_norm(to_player);
    vec3 want = { 0, 0, 0 };
    float speed = 0.0f;

    /* the swimmers and flyers have their own ways */
    if (creature_aquatic(c) || c->type == CR_BAT) {
        beast_update(g, c, dt, dist, to_player);
        glm_vec3_muladds(c->knock, dt, c->pos);
        glm_vec3_scale(c->knock, expf(-dt * 4.0f), c->knock);
        return;
    }
    /* unseen (Tome of Shadows), or staring at a garden gnome: they lose interest in you */
    bool distracted = g->gnome_t > 0.0f && glm_vec3_distance(g->gnome_pos, c->pos) < 20.0f;
    if ((g->shadow_t > 0.0f || distracted) && (c->state == ST_CHASE || c->state == ST_ATTACK) && c->type != CR_KING) {
        c->state = ST_IDLE;
        c->aggro = false;
    }
    if (distracted && c->state != ST_DEAD)
        c->yaw += wrap_angle(atan2f(g->gnome_pos[0] - c->pos[0], g->gnome_pos[2] - c->pos[2]) - c->yaw) * (1.0f - expf(-dt * 3.0f));

    /* sleeping skeletons wake when you come close */
    if (c->state == ST_DORMANT) {
        if (!g->dead && dist < 5.0f)
            c->state = ST_AWAKEN, c->state_t = 0.0f;
        return;
    }
    if (c->state == ST_AWAKEN) {
        if (c->state_t < 0.05f)
            audio_play_at(SFX_BONES, c->pos, 1.0f);
        const Rig *rig = creature_rig(g, c);
        if (c->state_t > anim_length(g, c, rig->awaken) * 0.9f) {
            c->state = ST_CHASE;
            c->aggro = true;
        }
        return;
    }

    if (dist < 22.0f && frand() < real_dt * 0.07f)
        audio_play_at(voice(c), c->pos, 0.5f);

    if (c->dance_t > 0.0f)
        c->state = ST_DANCE;
    else if (c->flee_t > 0.0f)
        c->state = ST_FLEE;
    else if (c->state == ST_DANCE || c->state == ST_FLEE)
        c->state = ST_WANDER;

    switch (c->state) {
    case ST_DANCE:
        c->yaw += dt * (humanoid ? 1.5f : 5.0f);
        break;
    case ST_FLEE:
        if (dist > 1e-3f)
            glm_vec3_scale(to_player, -1.0f / dist, want);
        speed = st.run;
        break;
    default: {
        vec3 ce = { c->pos[0], c->pos[1] + st.height * 0.8f, c->pos[2] };
        bool hidden = g->shadow_t > 0.0f || distracted;
        bool sees = !g->dead && !hidden && dist < detect && level_line_clear(&g->level, ce, g->head);
        c->lost_t = sees ? 0.0f : c->lost_t + dt;
        bool hunting = c->state == ST_CHASE || c->state == ST_ATTACK;
        if (!hunting && !g->dead && (sees || (c->aggro && dist < detect * 2.0f))) {
            c->state = ST_CHASE;
            c->state_t = 0.0f;
            audio_play_at(voice(c), c->pos, 1.0f);   /* spotted you */
        }
        if (hunting && (g->dead || (c->lost_t > 4.0f && !c->aggro) || dist > detect * 3.0f)) {
            c->state = ST_WANDER;
            c->state_t = 0.0f;
            c->aggro = false;
            glm_vec3_copy(c->home, c->target);
        }

        bool mage = (c->type == CR_SKELETON && c->variant == SK_MAGE) || c->type == CR_IMP;
        if (c->state == ST_CHASE) {
            if (dist > 1e-3f)
                glm_vec3_scale(to_player, 1.0f / dist, want);
            speed = st.run;
            if (mage) {
                /* keeps its distance and throws bolts */
                if (dist < 6.0f)
                    glm_vec3_negate(want);
                else if (dist < 14.0f)
                    speed = 0.0f;
                if (dist < 16.0f && sees && c->state_t > 2.2f) {
                    c->state = ST_ATTACK;
                    c->attack_t = 0.0f;
                    c->attack_hit = false;
                }
            } else if (dist < st.reach) {
                c->state = ST_ATTACK;
                c->attack_t = 0.0f;
                c->attack_hit = false;
            }
            /* the King calls his guards when he's hurt */
            if (c->type == CR_KING && c->hp < c->max_hp * 0.5f && !c->resurrected) {
                c->resurrected = true;
                for (int k = 0; k < 3; k++) {
                    float a = k * 2.1f;
                    Creature *d = creature_spawn(g, CR_DROWNED, 0, (vec3){c->pos[0] + cosf(a) * 4.0f, c->pos[1], c->pos[2] + sinf(a) * 4.0f}, a);
                    if (d) {
                        d->aggro = true;
                        d->state = ST_CHASE;
                    }
                }
                audio_play_at(SFX_KING_ROAR, c->pos, 1.2f);
                message(g, COL_RED, "The Drowned King roars, and the floor gives up its dead!");
            }
        } else if (c->state == ST_ATTACK) {
            c->attack_t += dt;
            turn_toward(&c->yaw, atan2f(to_player[0], to_player[2]), dt * 10.0f);
            if (!c->attack_hit && c->attack_t >= st.windup) {
                c->attack_hit = true;
                audio_play_at(voice(c), c->pos, 0.8f);
                if (mage) {
                    vec3 from = { c->pos[0] + sinf(c->yaw) * 0.6f, c->pos[1] + (c->type == CR_IMP ? 0.8f : 1.4f), c->pos[2] + cosf(c->yaw) * 0.6f };
                    vec3 aim;
                    glm_vec3_sub(g->head, from, aim);
                    aim[1] -= 0.3f;
                    glm_vec3_normalize(aim);
                    glm_vec3_scale(aim, c->type == CR_IMP ? 14.0f : 11.0f, aim);
                    if (c->type == CR_IMP)
                        aim[1] += 2.0f;         /* lobbed */
                    bolt_fire(g, c->type == CR_IMP ? BOLT_EMBER : BOLT_MAGE, from, aim, true);
                } else if (c->type == CR_KING && c->spawn >= 0 && frand() < 0.35f) {
                    /* the King's slam: a shockwave through the floor */
                    explode(g, (vec3){c->pos[0] + sinf(c->yaw) * 2.0f, c->pos[1], c->pos[2] + cosf(c->yaw) * 2.0f}, 0.0f, 0.1f, false);
                    if (dist < 7.0f)
                        hurt_player(g, c->pos, st.damage * 0.8f);
                    audio_play_at(SFX_KING_ROAR, c->pos, 1.0f);
                } else if (dist < st.reach * 1.35f && c->type != CR_SLIME) {
                    hurt_player(g, c->pos, st.damage);
                    if (c->type == CR_WRAITH)
                        g->mana = fmaxf(0.0f, g->mana - 15.0f);     /* its touch drains magic */
                }
            }
            if (c->attack_t >= st.cycle) {
                c->state = ST_CHASE;
                c->state_t = 0.0f;
            }
        } else if (c->state == ST_IDLE) {
            if (c->state_t > 1.5f + (c->anim_t - floorf(c->anim_t)) * 2.5f) {
                float a = frand() * 6.28f, r = 1.5f + frand() * 4.0f;
                glm_vec3_copy((vec3){c->home[0] + cosf(a) * r, c->home[1], c->home[2] + sinf(a) * r}, c->target);
                c->state = ST_WANDER;
                c->state_t = 0.0f;
            }
        } else if (c->state == ST_WANDER) {
            vec3 d;
            glm_vec3_sub(c->target, c->pos, d);
            d[1] = 0.0f;
            float dd = glm_vec3_norm(d);
            if (dd < 0.5f || c->state_t > 8.0f) {
                c->state = ST_IDLE;
                c->state_t = 0.0f;
            } else {
                glm_vec3_scale(d, 1.0f / dd, want);
                speed = st.walk > 0 ? st.walk : st.run * 0.5f;
            }
        }
        break;
    }
    }

    if (c->type == CR_SLIME) {
        /* slimes hop: a squash, a leap toward where they want to go, a splat */
        c->hop_t += dt;
        bool airborne = c->vy != 0.0f || c->pos[1] > ground_at(g, c->pos[0], c->pos[2], c->pos[1]) + 0.02f;
        if (!airborne && c->hop_t > (c->state == ST_CHASE ? 0.55f : 1.3f) && (speed > 0.0f || c->state == ST_ATTACK || c->state == ST_DANCE)) {
            c->hop_t = 0.0f;
            c->vy = 3.2f + (2 - c->variant) * 0.4f;
            if (glm_vec3_norm2(want) > 0.0f)
                c->yaw = atan2f(want[0], want[2]);
            audio_play_at(SFX_SLIME, c->pos, 0.5f);
        }
        if (airborne || c->vy > 0.0f) {
            vec3 fwd = { sinf(c->yaw), 0, cosf(c->yaw) };
            glm_vec3_muladds(fwd, st.run * dt, c->pos);
            c->pos[1] += c->vy * dt;
            c->vy -= 14.0f * dt;
            float gy = ground_at(g, c->pos[0], c->pos[2], c->pos[1] + 0.3f);
            if (c->pos[1] <= gy && c->vy < 0.0f) {
                c->pos[1] = gy;
                c->vy = 0.0f;
                c->hop_t = 0.0f;
                /* landing on you hurts */
                if (!g->dead && dist < st.radius + 0.6f && c->state != ST_DANCE) {
                    hurt_player(g, c->pos, st.damage);
                    c->state = ST_CHASE;
                }
            }
        }
        glm_vec3_muladds(c->knock, dt, c->pos);
        glm_vec3_scale(c->knock, expf(-dt * 5.0f), c->knock);
        level_collide(&g->level, c->pos, st.radius * 0.8f, st.height);
        if (c->state == ST_ATTACK)
            c->state = ST_CHASE;
        if (c->vy == 0.0f)
            c->pos[1] = ground_at(g, c->pos[0], c->pos[2], c->pos[1]);
        return;
    }

    /* walk, steering around whatever blocks the way (crabs face you and go sideways) */
    if (speed > 0.0f) {
        turn_toward(&c->yaw, atan2f(want[0], want[2]) + c->detour, dt * 7.0f);
        vec3 before, fwd = { sinf(c->yaw), 0.0f, cosf(c->yaw) };
        glm_vec3_copy(c->pos, before);
        glm_vec3_muladds(fwd, speed * dt, c->pos);
        if (c->type == CR_CRAB)
            glm_vec3_muladds((vec3){cosf(c->yaw), 0, -sinf(c->yaw)}, sinf(c->anim_t * 1.3f) * speed * 0.6f * dt, c->pos);
        level_collide(&g->level, c->pos, st.radius * 0.8f, st.height);
        /* land creatures won't wade in over their heads */
        float surface, depth;
        if (sea_at(g, c->pos[0], c->pos[2], &surface, &depth) && depth > st.height * 0.7f)
            glm_vec3_copy(before, c->pos);
        float moved = glm_vec3_distance(before, c->pos);
        if (moved < speed * dt * 0.35f)
            c->detour = c->detour != 0.0f ? c->detour : (frand() < 0.5f ? 1.2f : -1.2f);
        else
            c->detour *= expf(-dt * 1.2f);
        if (fabsf(c->detour) < 0.05f)
            c->detour = 0.0f;
    }

    /* knockback and the ground */
    glm_vec3_muladds(c->knock, dt, c->pos);
    glm_vec3_scale(c->knock, expf(-dt * 6.0f), c->knock);
    level_collide(&g->level, c->pos, st.radius * 0.8f, st.height);
    c->pos[1] = ground_at(g, c->pos[0], c->pos[2], c->pos[1] - c->hover) + c->hover + (c->hover > 0.0f ? sinf(c->phase + c->anim_t * 1.5f) * 0.15f : 0.0f);
    /* a tornado throws them about too */
    vec3 twist;
    weather_twister_force(&g->weather, c->pos, twist);
    glm_vec3_muladds(twist, dt * 0.5f, c->knock);

    /* don't stand inside the player */
    if (dist < 0.35f + st.radius && dist > 1e-3f)
        glm_vec3_muladds(to_player, -(0.35f + st.radius - dist) / dist, c->pos);
}

/* ---------- animation ---------- */

static void pose_humanoid(Game *g, Creature *c)
{
    const Model *m = creature_model(g, c);
    const Rig *r = creature_rig(g, c);
    pose_reset(&c->pose, m);
    int anim = r->idle;
    float t = c->anim_t;
    bool loop = true;
    switch (c->state) {
    case ST_DORMANT: anim = r->dormant; t = 0; loop = false; break;
    case ST_AWAKEN: anim = r->awaken; t = c->state_t; loop = false; break;
    case ST_WANDER: anim = r->walk; break;
    case ST_CHASE: case ST_FLEE: anim = r->run; break;
    case ST_ATTACK: anim = r->attack; t = c->attack_t * 1.2f; loop = false; break;
    case ST_DANCE: anim = r->cheer; break;
    case ST_DEAD: anim = r->death; t = c->death_t; loop = false; break;
    default: break;
    }
    if (c->type == CR_SKELETON && c->variant == SK_MAGE && c->state == ST_CHASE &&
        glm_vec3_distance(g->pos, c->pos) < 14.0f && glm_vec3_distance(g->pos, c->pos) > 6.0f)
        anim = r->idle;
    anim_apply(m, anim, t, loop, 1.0f, &c->pose);
    if (c->hit_flash > 0.0f && c->state != ST_DEAD)
        anim_apply(m, r->hit, 0.2f - c->hit_flash, false, 0.6f, &c->pose);
    pose_update(m, &c->pose);
}

static void pose_rat(Game *g, Creature *c)
{
    /* bend the generated spine. bone 0 = tail tip ... bone 7 = head */
    const Model *m = &g->rat_model;
    pose_reset(&c->pose, m);
    bool moving = c->state == ST_CHASE || c->state == ST_FLEE || c->state == ST_WANDER;
    float freq = moving ? (c->state == ST_WANDER ? 7.0f : 14.0f) : 3.0f;
    for (int i = 0; i < 8; i++) {
        float tail = (7 - i) / 7.0f;
        float amp = c->state == ST_DEAD ? 0.0f : (0.05f + tail * tail * (moving ? 0.35f : 0.2f));
        versor q;
        glm_quatv(q, sinf(c->anim_t * freq - i * 0.8f) * amp, (vec3){0, 1, 0});
        if (i == 7 && !moving && c->state != ST_DEAD) {
            versor nod;
            glm_quatv(nod, sinf(c->anim_t * 9.0f) * 0.12f, (vec3){1, 0, 0});
            glm_quat_mul(q, nod, q);
        }
        if (c->state == ST_ATTACK && i >= 5) {
            float t = glm_clamp(c->attack_t / 0.35f, 0, 1);
            versor up;
            glm_quatv(up, -sinf(t * GLM_PIf) * 0.35f, (vec3){1, 0, 0});
            glm_quat_mul(q, up, q);
        }
        int b = g->rat_bone[i];
        glm_quat_mul(c->pose.r[b], q, c->pose.r[b]);
    }
    pose_update(m, &c->pose);
}

static void pose_fox(Game *g, Creature *c)
{
    const Model *m = &g->fox_model;
    pose_reset(&c->pose, m);
    int anim = g->fox_idle;
    float rate = 1.0f;
    if (c->state == ST_CHASE || c->state == ST_FLEE)
        anim = g->fox_run;
    else if (c->state == ST_ATTACK)
        anim = g->fox_run, rate = 1.6f;
    else if (c->state == ST_WANDER)
        anim = g->fox_walk;
    else if (c->state == ST_DANCE)
        rate = 3.0f;
    anim_apply(m, anim, c->state == ST_DEAD ? 0.0f : c->anim_t * rate, c->state != ST_DEAD, 1.0f, &c->pose);
    pose_update(m, &c->pose);
}

void creatures_update(Game *g, float dt)
{
    for (int i = 0; i < MAX_CREATURES; i++) {
        Creature *c = &g->creatures[i];
        if (!c->used)
            continue;
        /* far-off creatures doze to save time */
        float d = glm_vec3_distance(c->pos, g->pos);
        if (d > 80.0f && c->state != ST_DEAD)
            continue;
        /* wraiths and bats only come out at night (the crypt is always night) */
        if ((c->type == CR_WRAITH || c->type == CR_BAT) && !g->night && level_area(&g->level, c->pos) != AREA_CRYPT)
            continue;
        creature_update(g, c, dt);
        if (!c->used)
            continue;
        if (d > 60.0f || (c->frozen_t > 0.0f && c->state != ST_DEAD))
            continue;       /* keep the last pose */
        if (c->type == CR_RAT)
            pose_rat(g, c);
        else if (c->type == CR_FOX)
            pose_fox(g, c);
        else if (c->type >= CR_CRAB)
            beast_pose(g, c);
        else if (c->type != CR_SLIME)
            pose_humanoid(g, c);
    }
}

/* ---------- drawing ---------- */

/* where the creature is drawn: its placement, plus the bounce, lunge, fall or
 * squash of what it's doing right now */
static void creature_matrix(const Creature *c, mat4 out)
{
    Transform t = c->xf;
    if (c->type == CR_RAT && (c->state == ST_CHASE || c->state == ST_FLEE))
        t.pos[1] += fabsf(sinf(c->anim_t * 14.0f)) * 0.04f;   /* scurrying bounce */
    if (c->state == ST_DANCE && c->type != CR_SKELETON && c->type != CR_GOBLIN && c->type < CR_IMP)
        t.pos[1] += fabsf(sinf(c->anim_t * 8.0f)) * 0.25f;
    if (c->state == ST_ATTACK && c->type == CR_FOX) {
        float f = glm_clamp((c->attack_t - 0.25f) / 0.3f, 0, 1);
        float lunge = sinf(f * GLM_PIf) * 0.5f;
        t.pos[0] += sinf(c->yaw) * lunge;
        t.pos[2] += cosf(c->yaw) * lunge;
    }
    if (c->state == ST_DEAD && (c->type == CR_RAT || c->type == CR_FOX)) {
        float f = glm_clamp(c->death_t / 0.5f, 0, 1);
        t.roll += f * f * GLM_PI_2f * 1.05f;        /* keel over */
    }
    if (c->type == CR_SLIME) {
        /* squash when about to hop and on landing, stretch while flying */
        float squash = c->vy != 0.0f ? 1.0f + fminf(fabsf(c->vy) * 0.06f, 0.25f)
                                     : 1.0f - 0.25f * expf(-c->hop_t * 8.0f) - 0.05f * sinf(c->anim_t * 4.0f);
        if (c->state == ST_DEAD)
            squash = fmaxf(0.1f, 1.0f - c->death_t * 2.0f);
        glm_vec3_mul(t.scale, (vec3){1.0f / sqrtf(squash), squash, 1.0f / sqrtf(squash)}, t.scale);
    }
    transform_matrix(&t, out);
}

/* `body` is the creature model's full matrix (model_matrix), so the weapon follows its hand */
static void draw_weapon(Game *g, const Creature *c, mat4 body, int bone, int weapon, GLuint prog, mat4 vp, const DrawParams *dp)
{
    if (bone < 0)
        return;
    mat4 w;
    glm_mat4_mul(body, (vec4 *)c->pose.world[bone], w);
    model_draw(&g->weapon_models[weapon], NULL, prog, vp, w, dp);
}

void creatures_draw(Game *g, GLuint prog, mat4 vp, bool depth)
{
    DrawParams dp = { .depth_only = depth };
    Frustum fr;
    frustum_from(vp, &fr);
    for (int i = 0; i < MAX_CREATURES; i++) {
        Creature *c = &g->creatures[i];
        if (!c->used || glm_vec3_distance(c->pos, g->eye) > (depth ? 45.0f : 70.0f))
            continue;
        if ((c->type == CR_WRAITH || c->type == CR_BAT) && !g->night && level_area(&g->level, c->pos) != AREA_CRYPT)
            continue;
        vec3 mid = { c->pos[0], c->pos[1] + creature_height(c) * 0.5f, c->pos[2] };
        if (!frustum_sphere(&fr, mid, creature_height(c) + 0.5f))
            continue;
        mat4 xf;
        creature_matrix(c, xf);
        DrawParams cp = dp;
        if (!depth) {
            if (c->hit_flash > 0.0f)
                glm_vec4_copy((vec4){c->hit_flash * 8.0f, c->hit_flash * 1.0f, 0.0f, 0.0f}, cp.tint);
            if (c->frozen_t > 0.0f)
                glm_vec4_copy((vec4){0.05f, 0.25f, 0.6f, 0.0f}, cp.tint);
            if (c->type == CR_SKELETON)     /* a faint, cold glow in the eyes and bones */
                glm_vec4_add(cp.tint, (vec4){0.0f, 0.01f, 0.03f, 0.0f}, cp.tint);
        }
        if (c->type >= CR_CRAB) {
            beast_draw(g, c, prog, vp, xf, &cp);
            continue;
        }
        const Model *m = creature_model(g, c);
        model_draw(m, c->type == CR_SLIME ? NULL : &c->pose, prog, vp, xf, &cp);
        if (c->type == CR_SLIME && depth)
            model_draw(&g->slime_body_model, NULL, prog, vp, xf, &cp);    /* jelly casts a shadow */

        if (c->type == CR_SKELETON) {
            const Rig *r = &g->skel_rigs[c->variant];
            static const int right[SK_COUNT] = { WPN_BLADE, WPN_AXE, WPN_BLADE, WPN_STAFF };
            mat4 body;
            model_matrix(m, xf, body);
            draw_weapon(g, c, body, r->hand_r, right[c->variant], prog, vp, &cp);
            if (c->variant == SK_WARRIOR)
                draw_weapon(g, c, body, r->hand_l, WPN_SHIELD, prog, vp, &cp);
            if (c->variant == SK_ROGUE)
                draw_weapon(g, c, body, r->hand_l, WPN_BLADE, prog, vp, &cp);
        }
    }
}

/* the slimes' jelly: glossy, see-through and rippling, over everything solid */
void slimes_draw(Game *g, mat4 vp)
{
    GLuint prog = g->slime_prog;
    const Material *mat = &g->slime_body_model.prims[0].mat;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (int i = 0; i < MAX_CREATURES; i++) {
        Creature *c = &g->creatures[i];
        if (!c->used || c->type != CR_SLIME || glm_vec3_distance(c->pos, g->eye) > 70.0f)
            continue;
        mat4 xf;
        creature_matrix(c, xf);
        vec4 tint = { 0, 0, 0, 0 };
        if (c->hit_flash > 0.0f)
            glm_vec4_copy((vec4){c->hit_flash * 6.0f, c->hit_flash, 0.0f, 0.0f}, tint);
        if (c->frozen_t > 0.0f)
            glm_vec4_copy((vec4){0.05f, 0.25f, 0.6f, 0.0f}, tint);
        glProgramUniform4fv(prog, 2, 1, mat->base_color);
        glProgramUniform3fv(prog, 7, 1, mat->emissive);
        glProgramUniform4fv(prog, 11, 1, tint);
        float slosh = c->vy != 0.0f ? 1.0f : 0.3f + 0.7f * expf(-c->hop_t * 4.0f);
        glProgramUniform4f(prog, 13, c->phase * 3.0f, slosh, 0.0f, 0.0f);
        DrawParams dp = { .no_material = true };
        model_draw(&g->slime_body_model, NULL, prog, vp, xf, &dp);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

/* the eruption's burning cloud: everything out in the open within `radius` of the crater */
void creatures_burn(Game *g, vec3 center, float radius)
{
    for (int i = 0; i < MAX_CREATURES; i++) {
        Creature *c = &g->creatures[i];
        if (!c->used || c->state == ST_DEAD)
            continue;
        if (c->spawn >= 0 && g->cspawns[c->spawn].sheltered)
            continue;
        if (level_area(&g->level, c->pos) == AREA_CRYPT || c->pos[1] < SEA_Y - 8.0f)
            continue;
        float dx = c->pos[0] - center[0], dz = c->pos[2] - center[2];
        if (dx * dx + dz * dz < radius * radius) {
            fx_flame(&g->ps, (vec3){c->pos[0], c->pos[1] + 0.5f, c->pos[2]}, 1.5f, 0.1f);
            c->hp = 0.0f;
            kill(g, c);
        }
    }
}

/* the Covenant: every creature that ever lived here, back where it began */
void creatures_regenerate(Game *g, bool sheltered_too)
{
    for (int k = 0; k < g->cspawn_count; k++) {
        CreatureSpawn *cs = &g->cspawns[k];
        if (cs->sheltered && !sheltered_too)
            continue;
        bool alive = false;
        for (int i = 0; i < MAX_CREATURES; i++)
            if (g->creatures[i].used && g->creatures[i].spawn == k && g->creatures[i].state != ST_DEAD)
                alive = true;
        if (alive)
            continue;
        for (int i = 0; i < MAX_CREATURES; i++)
            if (g->creatures[i].used && g->creatures[i].spawn == k) {
                pose_free(&g->creatures[i].pose);
                g->creatures[i].used = false;
                g->creatures_left -= g->creatures[i].state != ST_DEAD;
            }
        Creature *c = creature_spawn(g, cs->type, cs->variant, cs->pos, cs->yaw);
        if (c) {
            c->spawn = k;
            fx_magic(&g->ps, (vec3){cs->pos[0], cs->pos[1] + 0.8f, cs->pos[2]}, (vec3){1.5f, 4.0f, 1.0f}, 0.6f, 20);
        }
    }
}
