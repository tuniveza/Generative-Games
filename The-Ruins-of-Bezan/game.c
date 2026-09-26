#include "game.h"
#include "meshgen.h"
#include "shader.h"
#include "ui.h"

#include <stb_image_write.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


const float COL_TEXT[4]  = { 0.95f, 0.9f, 0.8f, 1.0f };
const float COL_GOLD[4]  = { 1.0f, 0.82f, 0.4f, 1.0f };
const float COL_RED[4]   = { 1.0f, 0.38f, 0.32f, 1.0f };
const float COL_FUN[4]   = { 0.6f, 0.9f, 1.0f, 1.0f };
const float COL_MAGIC[4] = { 0.7f, 0.75f, 1.0f, 1.0f };
const float COL_EDGE[4]  = { 0.55f, 0.43f, 0.24f, 0.9f };
static const float COL_PANEL[4] = { 0.05f, 0.04f, 0.03f, 0.82f };

/* ---------- messages ---------- */

void message(Game *g, const float color[4], const char *fmt, ...)
{
    memmove(&g->messages[1], &g->messages[0], (MAX_MESSAGES - 1) * sizeof g->messages[0]);
    Message *m = &g->messages[0];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(m->text, sizeof m->text, fmt, ap);
    va_end(ap);
    m->t = 0.0f;
    memcpy(m->color, color, sizeof m->color);
}

/* ---------- inventory ---------- */

static Pickup *new_pickup(Game *g, ItemId id, int count, vec3 pos)
{
    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *p = &g->pickups[i];
        if (!p->used) {
            *p = (Pickup){ .used = true, .item = id, .count = count };
            p->xf = TRANSFORM_AT(pos, frand() * 6.0f);
            return p;
        }
    }
    return NULL;
}

Pickup *spawn_pickup(Game *g, ItemId id, int count, vec3 pos)
{
    return new_pickup(g, id, count, pos);
}

void game_give(Game *g, ItemId id, int count)
{
    if (ITEMS[id].stacks) {
        for (int i = 0; i < INV_SLOTS; i++) {
            if (g->slots[i].id == id) {
                g->slots[i].count += count;
                return;
            }
        }
    }
    for (int i = 0; i < INV_SLOTS; i++) {
        if (g->slots[i].id == ITEM_NONE) {
            g->slots[i].id = id;
            g->slots[i].count = count;
            return;
        }
    }
    vec3 at = { g->pos[0], g->pos[1] + 0.3f, g->pos[2] };
    new_pickup(g, id, count, at);
    message(g, COL_TEXT, "Your satchel is full; the %s falls to the ground.", ITEMS[id].name);
}

bool has_item(const Game *g, ItemId id)
{
    for (int i = 0; i < INV_SLOTS; i++)
        if (g->slots[i].id == id)
            return true;
    return false;
}

static ItemId held_item(const Game *g)
{
    return g->slots[g->selected].id;
}

void game_give_quiet(Game *g, ItemId id, int count)
{
    if (ITEMS[id].stacks)
        for (int i = 0; i < INV_SLOTS; i++)
            if (g->slots[i].id == id) {
                g->slots[i].count += count;
                return;
            }
    for (int i = HOTBAR; i < INV_SLOTS + HOTBAR; i++) {
        int k = i % INV_SLOTS;     /* the satchel first, then the hotbar */
        if (g->slots[k].id == ITEM_NONE) {
            g->slots[k] = (Slot){ id, count };
            return;
        }
    }
    game_give(g, id, count);
}

int count_item(const Game *g, ItemId id)
{
    int n = 0;
    for (int i = 0; i < INV_SLOTS; i++)
        if (g->slots[i].id == id)
            n += g->slots[i].count;
    return n;
}

void take_items(Game *g, ItemId id, int count)
{
    for (int i = 0; i < INV_SLOTS && count > 0; i++)
        while (g->slots[i].id == id && count > 0) {
            count--;
            if (--g->slots[i].count <= 0)
                g->slots[i] = (Slot){0};
        }
}

/* ---------- shells: the coast's money ---------- */

const int SHELL_VALUE[SHELL_KINDS] = { 1, 5, 10, 25 };
const char *SHELL_NAME[SHELL_KINDS] = { "white", "pink", "blue", "golden" };

void add_shells(Game *g, int color, int count)
{
    g->shells[color] += count;
}

int shell_total(const Game *g)
{
    int v = 0;
    for (int i = 0; i < SHELL_KINDS; i++)
        v += g->shells[i] * SHELL_VALUE[i];
    return v;
}

bool pay_shells(Game *g, int price)
{
    if (shell_total(g) < price)
        return false;
    /* pay with the smallest shells first, then break a bigger one and take change */
    for (int i = 0; i < SHELL_KINDS && price > 0; i++) {
        int n = price / SHELL_VALUE[i];
        if (n > g->shells[i])
            n = g->shells[i];
        g->shells[i] -= n;
        price -= n * SHELL_VALUE[i];
    }
    for (int i = 0; i < SHELL_KINDS && price > 0; i++) {
        if (g->shells[i] > 0 && SHELL_VALUE[i] >= price) {
            g->shells[i]--;
            int change = SHELL_VALUE[i] - price;
            price = 0;
            for (int k = i - 1; k >= 0; k--) {
                g->shells[k] += change / SHELL_VALUE[k];
                change %= SHELL_VALUE[k];
            }
        }
    }
    return true;
}

bool is_day(const Game *g)
{
    return !g->night;
}

float storm_level(const Game *g)
{
    const Weather *w = &g->weather;
    float s = w->type == WEATHER_STORM ? 0.7f : w->rain * 0.3f;
    return glm_clamp(fmaxf(s, w->wind * 0.8f), 0.0f, 1.0f);
}

bool sea_at(const Game *g, float x, float z, float *surface, float *depth)
{
    if (terrain_height(&g->terrain, x, z) > SEA_Y + 0.3f)
        return false;
    vec3 p = { x, SEA_Y, z };
    if (world_area(p) == AREA_UNDERSEA)
        return false;
    float bottom = ground_at(g, x, z, SEA_Y + 0.5f);
    float d = SEA_Y - bottom;
    *surface = ocean_height(x, z, g->time, storm_level(g), d);
    *depth = *surface - bottom;
    return true;
}

static void use_up(Game *g, int slot)
{
    if (--g->slots[slot].count <= 0)
        g->slots[slot] = (Slot){0};
}

static void take_one(Game *g, ItemId id)
{
    for (int i = 0; i < INV_SLOTS; i++)
        if (g->slots[i].id == id) {
            use_up(g, i);
            return;
        }
}

void game_select(Game *g, int slot)
{
    if (slot < 0 || slot >= HOTBAR || g->action == ACT_SWING || g->action == ACT_THROW || g->action == ACT_CAST)
        return;
    if (g->selected != slot)
        audio_play(SFX_CLICK, 1.0f);
    g->selected = slot;
    g->action = ACT_IDLE;
}

/* ---------- world queries ---------- */

float ground_at(const Game *g, float x, float z, float feet)
{
    /* the terrain counts only where it isn't cut away, and only if it's not overhead
     * (it lies above the crypt) */
    float best = level_ground(&g->level, x, z, feet, STEP);
    if (!level_hole(&g->level, x, z)) {
        float t = terrain_height(&g->terrain, x, z);
        if (t <= feet + STEP && t > best)
            best = t;
    }
    return best;
}

float wrap_angle(float a)
{
    while (a > GLM_PIf) a -= 2.0f * GLM_PIf;
    while (a < -GLM_PIf) a += 2.0f * GLM_PIf;
    return a;
}

void look_dir(const Game *g, vec3 out)
{
    out[0] = -cosf(g->cam.fp_pitch) * sinf(g->cam.fp_yaw);
    out[1] = sinf(g->cam.fp_pitch);
    out[2] = -cosf(g->cam.fp_pitch) * cosf(g->cam.fp_yaw);
}

void flat_forward(const Game *g, vec3 out)
{
    out[0] = -sinf(g->cam.fp_yaw);
    out[1] = 0.0f;
    out[2] = -cosf(g->cam.fp_yaw);
}

/* ---------- environment ---------- */

static Environment base_env(bool night)
{
    Environment day = {
        .sun_dir = { 0.55f, 0.32f, -0.55f }, .sun_color = { 4.5f, 3.3f, 2.2f },
        .sky_ambient = { 0.34f, 0.4f, 0.52f }, .ground_ambient = { 0.13f, 0.11f, 0.09f },
        .fog_color = { 0.62f, 0.58f, 0.54f }, .fog_density = 0.0045f,
        .zenith = { 0.18f, 0.33f, 0.62f }, .exposure = 1.0f, .shadow = 1.0f,
    };
    Environment dark = {
        .sun_dir = { -0.35f, 0.65f, 0.4f }, .sun_color = { 0.1f, 0.14f, 0.24f },
        .sky_ambient = { 0.025f, 0.035f, 0.07f }, .ground_ambient = { 0.01f, 0.01f, 0.015f },
        .fog_color = { 0.012f, 0.018f, 0.035f }, .fog_density = 0.01f,
        .zenith = { 0.002f, 0.006f, 0.02f }, .exposure = 1.5f, .shadow = 1.0f,
    };
    return night ? dark : day;
}

void game_set_night(Game *g, bool night)
{
    g->night = night;
}

/* ---------- setup ---------- */

static const char *PROP_FILES[PM_COUNT] = {
    "gothic_statue", "Barrel_01", "wooden_crate_01", "antique_ceramic_vase_01", "boulder_01",
    "wooden_lantern_01", "stone_fire_pit", "shrub_02", "shrub_03", "fern_02", "flower_empodium",
    "shrub_sorrel_01", "tree_stump_01", "painted_wooden_bench", "wooden_bookshelf_worn",
    "book_encyclopedia_set_01", "wooden_candlestick", "Chandelier_02", "potted_plant_02",
    "GothicCabinet_01", "GothicCommode_01", "ornate_mirror_01", "horse_statue_01", "WoodenTable_02",
    "ArmChair_01", "vintage_oil_lamp",
    "bronze_whale_statue", "bronze_shark_statue", "bronze_ray_statue", "marble_bust_01", "lion_head",
    "cannon_01", "dutch_ship_medium", "dutch_ship_large_02", "ocean_buoy", "lateral_sea_marker", "lifebuoy",
    "wooden_barrels_01", "wooden_bucket_02", "lambis_shell", "CashRegister_01", "garden_gnome", "Ukulele_01",
    "bananas", "wicker_basket_01", "carved_wooden_elephant", "painted_wooden_shelves",
    "brass_diya_lantern", "Lantern_01", "lantern_chandelier_01", "wine_barrel_01", "wooden_picnic_table",
    "painted_wooden_chair_01", "painted_wooden_table", "spinning_wheel_01", "planter_box_01", "tea_set_01",
    "chess_set",
    "moon_rock_01", "moon_rock_03",
};

void load_or_die(Model *m, const char *path, const ModelOptions *opts)
{
    if (!model_load_ex(m, path, opts)) {
        fprintf(stderr, "missing asset %s\n", path);
        exit(1);
    }
}

/* ---------- view culling ---------- */

void frustum_from(mat4 m, Frustum *f)
{
    /* Gribb-Hartmann: each plane is the last row of the matrix plus or minus another */
    for (int i = 0; i < 6; i++) {
        int row = i / 2;
        float sign = (i & 1) ? -1.0f : 1.0f;
        for (int k = 0; k < 4; k++)
            f->planes[i][k] = m[k][3] + sign * m[k][row];
        float len = sqrtf(f->planes[i][0] * f->planes[i][0] + f->planes[i][1] * f->planes[i][1] +
                          f->planes[i][2] * f->planes[i][2]);
        for (int k = 0; k < 4; k++)
            f->planes[i][k] /= len;
    }
}

bool frustum_sphere(const Frustum *f, vec3 c, float r)
{
    for (int i = 0; i < 6; i++)
        if (f->planes[i][0] * c[0] + f->planes[i][1] * c[1] + f->planes[i][2] * c[2] + f->planes[i][3] < -r)
            return false;
    return true;
}

/* after changing a prop's xf (or its matrix): where it is for culling */
static void prop_bounds(Game *g, Prop *p)
{
    const Model *m = &g->prop_models[p->model];
    vec3 c, ext;
    glm_vec3_center((float *)m->min, (float *)m->max, c);
    glm_vec3_sub((float *)m->max, (float *)m->min, ext);
    glm_mat4_mulv3(p->matrix, c, 1.0f, p->center);
    float scale = fmaxf(glm_vec3_norm(p->matrix[0]), fmaxf(glm_vec3_norm(p->matrix[1]), glm_vec3_norm(p->matrix[2])));
    p->radius = glm_vec3_norm(ext) * 0.5f * scale;
}

/* re-place a prop after changing its pos / yaw / pitch / roll / scale */
static void prop_place(Game *g, Prop *p)
{
    transform_matrix(&p->xf, p->matrix);
    prop_bounds(g, p);
}

/* a prop standing on the floor at pos (the prop models are adjusted to stand on y = 0),
 * `sink` below it */
static Prop *add_prop(Game *g, int model, vec3 pos, float yaw, float scale, float sink)
{
    if (g->prop_count >= MAX_PROPS)
        return NULL;
    Prop *p = &g->props[g->prop_count++];
    p->model = model;
    p->no_shadow = model == PM_FLOWERS || model == PM_BOOKS || model == PM_CANDLE;
    p->burns = model == PM_SHRUB_A || model == PM_SHRUB_B || model == PM_FERN || model == PM_FLOWERS ||
               model == PM_SORREL || model == PM_PLANTER;
    p->growth = 1.0f;
    p->xf = TRANSFORM_AT(pos, yaw);
    p->pos[1] -= sink;
    glm_vec3_fill(p->scale, scale);
    prop_place(g, p);
    return p;
}

Prop *spawn_prop(Game *g, int model, vec3 pos, float yaw, float scale)
{
    return add_prop(g, model, pos, yaw, scale, 0.0f);
}

static LightSource *add_light(Game *g, LightKind kind, vec3 pos, vec3 normal, float yaw, bool lit);

LightSource *spawn_light(Game *g, LightKind kind, vec3 pos, vec3 normal, float yaw, bool lit)
{
    return add_light(g, kind, pos, normal, yaw, lit);
}

static LightSource *add_light(Game *g, LightKind kind, vec3 pos, vec3 normal, float yaw, bool lit)
{
    for (int i = 0; i < MAX_LIGHTS_SRC; i++) {
        LightSource *l = &g->lights[i];
        if (l->used)
            continue;
        *l = (LightSource){ .used = true, .kind = kind, .seed = frand() * 100.0f, .lit = lit };
        l->xf = TRANSFORM_AT(pos, yaw);
        glm_vec3_copy(normal, l->normal);
        return l;
    }
    return NULL;
}

/* what's in each chest, in map order */
static void fill_chest(Chest *ch, int index, bool gold)
{
    static const struct { ItemId id; int n; } tables[5][8] = {
        { { ITEM_MACE, 1 }, { ITEM_GRENADE, 3 }, { ITEM_POCKET_WATCH, 1 }, { ITEM_SWEET_POTATO, 1 } },
        { { ITEM_AXE, 1 }, { ITEM_SWEET_POTATO, 2 }, { ITEM_RUBBER_DUCK, 1 }, { ITEM_LANTERN, 1 } },
        { { ITEM_KEY, 1 }, { ITEM_GAS_MASK, 1 }, { ITEM_CHEESE, 2 }, { ITEM_COMPASS, 1 } },
        { { ITEM_MANA_POTION, 2 }, { ITEM_TORCH, 2 }, { ITEM_CHEESE, 1 } },
        { { ITEM_WARHAMMER, 1 }, { ITEM_SLEDGEHAMMER, 1 }, { ITEM_BOOMBOX, 1 }, { ITEM_SPECTACLES, 1 },
          { ITEM_GOBLET, 2 }, { ITEM_BINOCULARS, 1 }, { ITEM_GRENADE, 2 } },
    };
    int t = gold ? 4 : index % 4;
    ch->loot_n = 0;
    for (int i = 0; i < 8 && tables[t][i].id; i++) {
        ch->loot[ch->loot_n] = tables[t][i].id;
        ch->loot_count[ch->loot_n++] = tables[t][i].n;
    }
}

/* a creature, and a note of where it began so the volcano can bring it back */
static void remember_creature(Game *g, CreatureType type, int variant, vec3 pos, float yaw)
{
    Creature *c = creature_spawn(g, type, variant, pos, yaw);
    if (!c || g->cspawn_count >= MAX_CREATURES)
        return;
    CreatureSpawn *cs = &g->cspawns[g->cspawn_count];
    *cs = (CreatureSpawn){ .type = type, .variant = variant, .yaw = yaw };
    glm_vec3_copy(pos, cs->pos);
    Area a = level_area(&g->level, pos);
    cs->sheltered = a == AREA_CRYPT || a == AREA_UNDERSEA || pos[1] < SEA_Y - 10.0f;
    c->spawn = g->cspawn_count++;
}

static void spawn_world(Game *g)
{
    Level *lv = &g->level;
    static const ItemId tomb_gifts[] = { ITEM_TOME_FIREBALL, ITEM_TOME_FROST, ITEM_TOME_HEAL, ITEM_TOME_WISP,
                                         ITEM_TOME_BLINK, ITEM_MANA_POTION };
    for (int i = 0; i < lv->spawn_count; i++) {
        Spawn *s = &lv->spawns[i];
        float r = 0.8f + frand() * 0.5f;
        switch (s->kind) {
        case SPAWN_PLAYER:
            glm_vec3_copy(s->pos, g->spawn);
            g->spawn_yaw = s->yaw;
            break;
        case SPAWN_RAT: remember_creature(g, CR_RAT, 0, s->pos, s->yaw); break;
        case SPAWN_FOX: remember_creature(g, CR_FOX, 0, s->pos, s->yaw); break;
        case SPAWN_GOBLIN: remember_creature(g, CR_GOBLIN, 0, s->pos, s->yaw); break;
        case SPAWN_SLIME: remember_creature(g, CR_SLIME, 0, s->pos, s->yaw); break;
        case SPAWN_SKELETON: remember_creature(g, CR_SKELETON, s->variant, s->pos, s->yaw); break;
        case SPAWN_CREATURE: remember_creature(g, (CreatureType)s->variant, s->index, s->pos, s->yaw); break;
        case SPAWN_NPC:
        case SPAWN_VILLAGER: npc_add(g, s->index, s->pos, s->yaw); break;
        case SPAWN_ANIMAL: animal_spawn(g, (Species)s->variant, s->pos, s->yaw); break;
        case SPAWN_BOAT: boat_add(g, s->pos, s->yaw); break;
        case SPAWN_THING: thing_add(g, (ThingKind)s->variant, s->index, s->pos, s->yaw); break;
        case SPAWN_VENT:
            if (g->vent_count < MAX_VENTS)
                glm_vec3_copy(s->pos, g->vents[g->vent_count++]);
            break;
        case SPAWN_LAMP: {
            LightSource *l = add_light(g, (LightKind)s->variant, s->pos, (vec3){0, -1, 0}, s->yaw, true);
            if (l)
                l->empty = true;    /* part of the building: not to be taken */
            break;
        }
        case SPAWN_PROP: {
            int model = s->variant;
            Prop *p = add_prop(g, model, s->pos, s->yaw, s->normal[0] > 0.0f ? s->normal[0] : 1.0f, s->normal[1]);
            if (p && (model == PM_BUOY || model == PM_MARKER))
                p->floats = true;
            if (p && s->normal[2] != 0.0f) {
                p->roll = s->normal[2];
                prop_place(g, p);
            }
            if (p && (model == PM_SHIP || model == PM_SHIP_LARGE || model == PM_BARRELS))
                p->no_shadow = false;
            break;
        }
        case SPAWN_CHEST:
        case SPAWN_CHEST_GOLD: {
            if (g->chest_count >= MAX_CHESTS)
                break;
            Chest *ch = &g->chests[g->chest_count++];
            memset(ch, 0, sizeof *ch);
            ch->xf = TRANSFORM_AT(s->pos, s->yaw);
            ch->gold = s->kind == SPAWN_CHEST_GOLD;
            fill_chest(ch, s->index, ch->gold);
            pose_init(&ch->pose, &g->chest_model);
            break;
        }
        case SPAWN_TOMB:
        case SPAWN_TOMB_GREAT: {
            if (g->tomb_count >= MAX_TOMBS)
                break;
            Tomb *t = &g->tombs[g->tomb_count++];
            *t = (Tomb){ .great = s->kind == SPAWN_TOMB_GREAT, .glow = 0.5f };
            t->xf = TRANSFORM_AT(s->pos, s->yaw);
            if (t->great)
                glm_vec3_fill(t->scale, 1.3f);
            t->gift = t->great ? ITEM_TOME_LIGHTNING : tomb_gifts[s->index < 6 ? s->index : 5];
            break;
        }
        case SPAWN_TORCH:
            add_light(g, LIGHT_TORCH, s->pos, s->normal, s->yaw, s->index % 3 != 1);
            break;
        case SPAWN_LANTERN:
            add_light(g, LIGHT_LANTERN, s->pos, (vec3){0, 1, 0}, s->yaw, true);
            break;
        case SPAWN_FIREPIT:
            add_prop(g, PM_FIREPIT, s->pos, 0.0f, 1.0f, 0.0f);
            add_light(g, LIGHT_FIREPIT, (vec3){s->pos[0], s->pos[1] + 0.35f, s->pos[2]}, (vec3){0, 1, 0}, 0, true);
            break;
        case SPAWN_CHANDELIER: {
            const Model *m = &g->prop_models[PM_CHANDELIER];
            float scale = 1.4f;
            Prop *p = add_prop(g, PM_CHANDELIER, (vec3){s->pos[0], s->pos[1] - (m->max[1] - m->min[1]) * scale, s->pos[2]}, 0.0f, scale, 0.0f);
            if (p)
                p->no_shadow = true;
            add_light(g, LIGHT_CHANDELIER, (vec3){s->pos[0], s->pos[1] - 1.4f, s->pos[2]}, (vec3){0, -1, 0}, 0, true);
            break;
        }
        case SPAWN_DOOR:
        case SPAWN_DOOR_LOCKED: {
            if (g->door_count >= MAX_DOORS)
                break;
            Door *d = &g->doors[g->door_count++];
            memset(d, 0, sizeof *d);
            d->xf = TRANSFORM_AT(s->pos, s->yaw);
            d->locked = s->kind == SPAWN_DOOR_LOCKED;
            bool along_x = fabsf(s->yaw) < 0.1f;
            vec3 half = { along_x ? 1.05f : 0.14f, 0, along_x ? 0.14f : 1.05f };
            d->box = level_add_box(lv, (vec3){s->pos[0] - half[0], FLOOR_Y, s->pos[2] - half[2]},
                                       (vec3){s->pos[0] + half[0], FLOOR_Y + 4.1f, s->pos[2] + half[2]});
            pose_init(&d->pose, &g->door_model);
            break;
        }
        case SPAWN_AVOCADO: {
            Pickup *p = new_pickup(g, ITEM_AVOCADO, 1, s->pos);
            if (p)
                p->on_altar = true;
            break;
        }
        case SPAWN_PICKUP_DAGGER: new_pickup(g, ITEM_DAGGER, 1, (vec3){s->pos[0], s->pos[1] + 0.05f, s->pos[2]}); break;
        case SPAWN_PICKUP_SHIELD: new_pickup(g, ITEM_SHIELD, 1, (vec3){s->pos[0], s->pos[1] + 0.05f, s->pos[2]}); break;
        case SPAWN_STATUE: add_prop(g, PM_STATUE, s->pos, s->yaw, 1.7f, 0.0f); break;
        case SPAWN_BARREL: add_prop(g, PM_BARREL, s->pos, s->yaw, 1.0f, 0.0f); break;
        case SPAWN_CRATE: add_prop(g, PM_CRATE, s->pos, s->yaw, 1.0f, 0.0f); break;
        case SPAWN_VASE: add_prop(g, PM_VASE, s->pos, s->yaw, 1.0f, 0.0f); break;
        case SPAWN_BOULDER: add_prop(g, PM_BOULDER, s->pos, s->yaw, 1.0f, 0.25f); break;
        case SPAWN_SHRUB: add_prop(g, s->variant == 0 ? PM_SHRUB_A : PM_SHRUB_B, s->pos, s->yaw, r, 0.0f); break;
        case SPAWN_FERN:
            add_prop(g, s->variant == 0 ? PM_FERN : s->variant == 1 ? PM_SORREL : PM_FLOWERS, s->pos, s->yaw, r * 1.3f, 0.0f);
            break;
        case SPAWN_BENCH: add_prop(g, PM_BENCH, s->pos, s->yaw, 1.0f, 0.0f); break;
        case SPAWN_PLANT: add_prop(g, PM_PLANT, s->pos, s->yaw, 1.0f, 0.0f); break;
        case SPAWN_BOOKSHELF: add_prop(g, PM_BOOKSHELF, s->pos, s->yaw, 1.0f, 0.0f); break;
        case SPAWN_BOOKPILE: {
            /* a toppled shelf and its books spilled across the floor */
            add_prop(g, PM_BOOKS, s->pos, s->yaw, 1.0f, 0.0f);
            Prop *p = add_prop(g, PM_BOOKSHELF, s->pos, s->yaw, 1.0f, 0.0f);
            if (p) {
                /* tipped onto its back, turning about its middle rather than its feet */
                const Model *m = &g->prop_models[PM_BOOKSHELF];
                float depth = m->max[2] - m->min[2];
                p->pos[0] += 1.0f;
                p->pos[1] += depth * 0.5f;
                p->pitch = -GLM_PI_2f;
                transform_matrix(&p->xf, p->matrix);
                glm_translate(p->matrix, (vec3){0, -(m->min[1] + m->max[1]) * 0.5f, -(m->min[2] + m->max[2]) * 0.5f});
                prop_bounds(g, p);
            }
            add_prop(g, PM_CANDLE, (vec3){s->pos[0] - 0.8f, s->pos[1], s->pos[2] + 0.6f}, s->yaw, 1.0f, 0.0f);
            break;
        }
        case SPAWN_FOUNTAIN:
            if (g->fountain_count < 4)
                glm_vec3_copy(s->pos, g->fountains[g->fountain_count++]);
            break;
        default:
            break;
        }
    }

    /* the side chambers get furniture (the horses at the door are placed by world.c) */
    for (int i = 0; i < lv->spawn_count; i++) {
        Spawn *s = &lv->spawns[i];
        if (s->kind != SPAWN_NPC)
            continue;
        vec3 p = { s->pos[0] + 1.6f, s->pos[1], s->pos[2] - 1.2f };
        if (s->index == 2) {        /* the magister's study */
            add_prop(g, PM_TABLE, p, 0.3f, 1.0f, 0.0f);
            add_prop(g, PM_OIL_LAMP, (vec3){p[0], p[1] + 0.78f, p[2]}, 0.0f, 1.0f, 0.0f);
            LightSource *l = add_light(g, LIGHT_LANTERN, (vec3){p[0], p[1] + 0.9f, p[2]}, (vec3){0, 1, 0}, 0, true);
            if (l)
                l->empty = true;    /* a lamp's light, not a lantern you can take */
        } else if (s->index == 3 || s->index == 5) {
            add_prop(g, PM_CHAIR, p, 2.5f, 1.0f, 0.0f);
            add_prop(g, PM_COMMODE, (vec3){p[0] + 1.5f, p[1], p[2] - 1.5f}, 0.0f, 1.0f, 0.0f);
        } else if (s->index == 4) {
            add_prop(g, PM_CABINET, p, 3.14f, 1.0f, 0.0f);
            add_prop(g, PM_MIRROR, (vec3){p[0] - 2.0f, p[1] + 1.0f, p[2]}, 3.14f, 1.0f, 0.0f);
        }
    }
}

void respawn_player_at(Game *g, vec3 pos, float yaw)
{
    glm_vec3_copy(pos, g->pos);
    glm_vec3_zero(g->vel);
    g->hp = g->max_hp;
    g->mana = g->max_mana;
    g->dead = false;
    g->dead_t = 0;
    g->cam.fp_yaw = yaw;
    g->cam.fp_pitch = 0;
    g->action = ACT_IDLE;
    g->swimming = g->underwater = false;
    g->toxic = 0.0f;
    g->stamina = g->max_stamina;
    g->breath = g->max_breath;
}

static void respawn_player(Game *g)
{
    glm_vec3_copy(g->spawn, g->pos);
    glm_vec3_zero(g->vel);
    g->hp = g->max_hp;
    g->mana = g->max_mana;
    g->dead = false;
    g->dead_t = 0;
    g->cam.fp_yaw = g->spawn_yaw;
    g->cam.fp_pitch = 0;
    g->action = ACT_IDLE;
}

void game_init(Game *g, int width, int height)
{
    memset(g, 0, sizeof *g);
    g->width = width;
    g->height = height;
    settings_load(&g->settings);

    renderer_init(&g->r, width, height);
    model_init_joint_buffer();
    g->model_prog = shader_load("shaders/model.vert", "shaders/model.frag");
    g->shadow_prog = shader_load("shaders/shadow.vert", "shaders/shadow.frag");
    g->terrain_prog = shader_load("shaders/terrain.vert", "shaders/terrain.frag");
    g->water_prog = shader_load("shaders/model.vert", "shaders/water.frag");
    ui_init("assets/fonts/Cinzel.ttf", "assets/fonts/Spectral-Regular.ttf");

    /* the land: the ruins' clearing, hills, and beyond them the coast, the village's hill
     * and the volcano (world.c shapes it) */
    terrain_create(&g->terrain, 641, 2.0f * WORLD_HALF, 34.0f, FLOOR_Y - 0.06f, 150.0f, 1234, world_terrain_shape);
    renderer_set_heightmap(&g->r, g->terrain.height_tex, -WORLD_HALF, -WORLD_HALF, 2.0f * WORLD_HALF / (641 - 1), 641);
    level_build(&g->level);
    for (int i = 0; i < g->level.hole_count; i++) {
        Hole *h = &g->level.holes[i];
        terrain_add_hole(&g->terrain, h->x0, h->z0, h->x1, h->z1);
    }
    world_build(&g->world, &g->level, &g->terrain);
    level_build_cover(&g->level);
    renderer_set_cover(&g->r, g->level.cover_tex, g->level.cover_x0, g->level.cover_z0, COVER_RES);
    ocean_init(&g->ocean);
    g->slime_prog = shader_load("shaders/slime.vert", "shaders/slime.frag");
    particles_init(&g->ps);
    weather_init(&g->weather);
    items_load();
    Environment env = base_env(false);
    renderer_set_environment(&g->r, &env, 0.0f);
    items_make_icons(&g->r, g->model_prog);
    hands_init(&g->hands);

    /* each model's own fix (size, facing, standing on the ground) is set once here,
     * right after it loads; see model_adjust. NORMAL changes nothing but the grounding */
    const Transform NORMAL = TRANSFORM_AT(((vec3){ 0, 0, 0 }), 0.0f);
    load_or_die(&g->door_model, "assets/models/large_castle_door/large_castle_door.gltf", NULL);
    model_adjust(&g->door_model, &NORMAL, true);
    load_or_die(&g->chest_model, "assets/models/treasure_chest/treasure_chest.gltf", NULL);
    model_adjust(&g->chest_model, &NORMAL, true);
    for (int i = 0; i < PM_COUNT; i++) {
        char path[256];
        snprintf(path, sizeof path, "assets/models/%s/%s.gltf", PROP_FILES[i], PROP_FILES[i]);
        load_or_die(&g->prop_models[i], path, NULL);
        model_adjust(&g->prop_models[i], &NORMAL, true);
    }
    g->door_left = model_find_node(&g->door_model, "large_castle_door_left");
    g->door_right = model_find_node(&g->door_model, "large_castle_door_right");
    g->chest_lid = model_find_node(&g->chest_model, "treasure_chest_lid");
    creatures_load(g);
    beasts_load(g);
    tombs_build_models(g);
    player_load(g);
    animals_load(g);
    things_load(g);
    g->shop_npc = -1;

    spawn_world(g);
    level_finalize(&g->level);      /* now that doors and characters have their boxes */

    g->max_hp = 100.0f;
    g->max_mana = 100.0f;
    camera_init(&g->cam, (vec3){0, 0, 0}, 4.0f);
    g->cam.mode = CAM_FIRST_PERSON;
    respawn_player(g);
    g->slots[0] = (Slot){ ITEM_TORCH, 1 };
    g->selected = 1;
    g->fov = g->settings.fov;
    g->inv_held = -1;
    g->menu_drag = -1;
    settings_apply(g);
    volcano_init(g);
    message(g, COL_TEXT, "Rain drums on the old stones. Something scratches in the halls.");
}

void game_free(Game *g)
{
    for (int i = 0; i < g->door_count; i++)
        pose_free(&g->doors[i].pose);
    for (int i = 0; i < g->chest_count; i++)
        pose_free(&g->chests[i].pose);
    for (int i = 0; i < g->npc_count; i++) {
        pose_free(&g->npcs[i].pose);
        model_free(&g->npc_models[i]);
    }
    creatures_free(g);
    beasts_free(g);
    player_free(g);
    animals_free(g);
    things_free(g);
    world_free(&g->world);
    ocean_free(&g->ocean);
    glDeleteProgram(g->slime_prog);
    model_free(&g->door_model);
    model_free(&g->chest_model);
    model_free(&g->tomb_model);
    model_free(&g->tomb_lid_model);
    for (int i = 0; i < PM_COUNT; i++)
        model_free(&g->prop_models[i]);
    if (g->pad)
        SDL_CloseGamepad(g->pad);
    hands_free(&g->hands);
    items_free();
    weather_free(&g->weather);
    particles_free(&g->ps);
    level_free(&g->level);
    terrain_free(&g->terrain);
    ui_free();
    glDeleteProgram(g->model_prog);
    glDeleteProgram(g->shadow_prog);
    glDeleteProgram(g->terrain_prog);
    glDeleteProgram(g->water_prog);
    renderer_free(&g->r);
}

void game_resize(Game *g, int width, int height)
{
    g->width = width;
    g->height = height;
    renderer_resize(&g->r, width, height);
}

/* ---------- combat ---------- */

void hurt_player(Game *g, vec3 from, float dmg)
{
    if (g->dead)
        return;
    vec3 dir;
    glm_vec3_sub(g->pos, from, dir);
    dir[1] = 0;
    glm_vec3_normalize(dir);

    /* armour takes its share */
    dmg *= 1.0f - armor_total(g);
    /* a raised shield stops most of a hit coming from in front */
    vec3 fwd;
    flat_forward(g, fwd);
    if (g->action == ACT_BLOCK && (held_item(g) == ITEM_SHIELD || g->left == LEFT_SHIELD) && glm_vec3_dot(fwd, dir) < -0.3f) {
        dmg *= 0.15f;
        audio_play(SFX_BLOCK, 1.0f);
        vec3 hp;
        hands_right_pos(&g->hands, hp);
        fx_sparks(&g->ps, hp, 12);
    }

    g->hp -= dmg;
    if (dmg > 2.0f)
        audio_play(SFX_HURT, 0.9f);
    g->hurt_t = 0.45f;
    g->calm_t = 0.0f;
    g->shake_t = fmaxf(g->shake_t, 0.15f);
    glm_vec3_muladds(dir, 3.0f, g->vel);
    if (g->hp <= 0.0f) {
        g->hp = 0.0f;
        g->dead = true;
        g->dead_t = 0.0f;
        g->inv_open = false;
        g->journal_open = false;
        g->shop_npc = -1;
        fishing_cancel(g);
        if (g->boat_in >= 0)
            boat_leave(g);
        audio_play(SFX_DIE, 1.0f);
        message(g, COL_RED, "You have fallen.");
    }
}

/* the moment the weapon connects */
static void do_hit(Game *g, ItemId held)
{
    bool armed = held != ITEM_NONE && ITEMS[held].kind == KIND_WEAPON;
    float dmg = armed ? ITEMS[held].damage : 7.0f;
    float range = armed ? ITEMS[held].range : 1.7f;
    float knock = armed ? ITEMS[held].knockback : 2.0f;

    vec3 fwd;
    flat_forward(g, fwd);
    bool hit = false;
    for (int i = 0; i < MAX_CREATURES; i++) {
        Creature *c = &g->creatures[i];
        if (!c->used || c->state == ST_DEAD)
            continue;
        vec3 d;
        glm_vec3_sub(c->pos, g->pos, d);
        float dy = d[1];
        d[1] = 0;
        float len = glm_vec3_norm(d);
        if (len - creature_radius(c) > range || dy > 2.0f || dy < -2.2f)
            continue;
        glm_vec3_divs(d, fmaxf(len, 1e-3f), d);
        if (glm_vec3_dot(d, fwd) < 0.55f && len > 0.9f)
            continue;
        vec3 target = { c->pos[0], c->pos[1] + creature_height(c) * 0.5f, c->pos[2] };
        if (!level_line_clear(&g->level, g->head, target))
            continue;

        float amount = dmg * (0.85f + frand() * 0.3f);
        if (held == ITEM_TRIDENT && (g->swimming || g->wading_sea || creature_aquatic(c)))
            amount *= 2.0f;
        if (held == ITEM_EMBER_BLADE && c->type != CR_IMP)
            c->burn_t = 3.0f;
        if (frand() < 0.12f) {
            amount *= 2.0f;
            message(g, COL_GOLD, "A crushing blow!");
        }
        vec3 push;
        glm_vec3_scale(d, knock, push);
        audio_play_at(dmg > 30 ? SFX_HIT_HEAVY : SFX_HIT, c->pos, 1.0f);
        damage_creature(g, c, amount, push);
        hit = true;
    }
    if (hit)
        g->shake_t = fmaxf(g->shake_t, armed && ITEMS[held].damage > 30 ? 0.18f : 0.08f);

    /* a spade in the sand: anything buried right here comes up */
    if (held == ITEM_SPADE && !hit) {
        vec3 at;
        glm_vec3_copy(g->pos, at);
        glm_vec3_muladds(fwd, 1.2f, at);
        for (int i = 0; i < g->thing_count; i++) {
            Thing *t = &g->things[i];
            if (t->used && t->kind == THING_TREASURE && !t->done &&
                (t->pos[0] - at[0]) * (t->pos[0] - at[0]) + (t->pos[2] - at[2]) * (t->pos[2] - at[2]) < 2.2f * 2.2f) {
                thing_use(g, t);
                return;
            }
        }
        if (g->pos[1] < -0.7f) {
            audio_play(SFX_DIG, 0.8f);
            fx_dust(&g->ps, at, 4);
        }
    }
}

void explode(Game *g, vec3 at, float damage, float radius, bool hurts_player)
{
    fx_explosion(&g->ps, at);
    audio_play_at(SFX_EXPLOSION, at, 1.6f);
    g->flash_t = 0.4f;
    glm_vec3_copy(at, g->flash_pos);
    for (int i = 0; i < MAX_CREATURES; i++) {
        Creature *c = &g->creatures[i];
        if (!c->used || c->state == ST_DEAD)
            continue;
        float d = glm_vec3_distance(c->pos, at);
        if (d < radius) {
            vec3 push;
            glm_vec3_sub(c->pos, at, push);
            push[1] = 0;
            glm_vec3_normalize(push);
            glm_vec3_scale(push, 14.0f * (1.0f - d / radius), push);
            damage_creature(g, c, damage * (1.0f - d / radius * 0.6f), push);
        }
    }
    float pd = glm_vec3_distance(g->pos, at);
    if (hurts_player && pd < radius)
        hurt_player(g, at, damage * 0.45f * (1.0f - pd / radius));
    g->shake_t = fmaxf(g->shake_t, 0.6f * fmaxf(0.2f, 1.0f - pd / 30.0f));
}

/* ---------- the off hand: torch, lantern, or nothing ---------- */

static void cycle_left(Game *g)
{
    LeftHand order[4] = { LEFT_EMPTY, LEFT_TORCH, LEFT_LANTERN, LEFT_SHIELD };
    int at = g->left;
    for (int k = 1; k <= 4; k++) {
        LeftHand next = order[(at + k) % 4];
        if (next == LEFT_EMPTY || (next == LEFT_TORCH && has_item(g, ITEM_TORCH)) ||
            (next == LEFT_LANTERN && has_item(g, ITEM_LANTERN)) || (next == LEFT_SHIELD && has_item(g, ITEM_SHIELD))) {
            g->left = next;
            break;
        }
    }
    if (g->left == LEFT_TORCH)
        audio_play(SFX_TORCH_ON, 0.8f);
    else if (g->left == LEFT_LANTERN)
        audio_play(SFX_LANTERN, 1.0f);
    else if (g->left == LEFT_SHIELD) {
        audio_play(SFX_EQUIP, 0.8f);
        message(g, COL_TEXT, "You strap the kite shield to your left arm. (hold right mouse to block)");
    } else
        audio_play(SFX_TORCH_OFF, 0.5f);
}

/* spells cost less in a magister's robes */
static float spell_cost(const Game *g, ItemId id)
{
    return ITEMS[id].mana * (g->equip[EQ_BODY] == ITEM_MAGISTER_ROBES ? 0.75f : 1.0f);
}

/* ---------- player actions ---------- */

static void begin(Game *g, HandAction a, float dur)
{
    g->action = a;
    g->action_t = 0.0f;
    g->action_dur = dur;
    g->action_done = false;
}

void game_start_action(Game *g)
{
    if (g->dead || g->menu != MENU_NONE || g->action == ACT_SWING || g->action == ACT_THROW ||
        g->action == ACT_USE || g->action == ACT_CAST)
        return;
    ItemId id = held_item(g);
    if (id != ITEM_NONE && ITEMS[id].kind == KIND_ROD) {
        fishing_start(g);       /* casting, hooking and reeling are the rod's own business */
        return;
    }
    if (fishing_busy(g))
        return;
    if (id != ITEM_NONE && g->cooldowns[id] > 0.0f) {
        if (ITEMS[id].kind != KIND_SPELL)
            message(g, COL_TEXT, "The %s isn't ready yet.", ITEMS[id].name);
        return;
    }

    if (id == ITEM_NONE) {
        begin(g, ACT_SWING, 0.45f);
        audio_play(SFX_SWING, 0.6f);
        return;
    }
    const ItemDef *it = &ITEMS[id];
    switch (it->kind) {
    case KIND_WEAPON:
        begin(g, ACT_SWING, it->cooldown);
        audio_play(it->damage > 30 ? SFX_SWING_HEAVY : SFX_SWING, 1.0f);
        break;
    case KIND_THROWN: begin(g, ACT_THROW, it->cooldown); break;
    case KIND_FOOD: begin(g, ACT_USE, it->cooldown); break;
    case KIND_SPELL:
        if (g->mana < spell_cost(g, id)) {
            message(g, COL_MAGIC, "Not enough mana.");
            audio_play(SFX_NO_MANA, 1.0f);
            return;
        }
        begin(g, ACT_CAST, 0.6f);
        break;
    case KIND_KEY:
        message(g, COL_TEXT, "Heavy iron. It must fit a lock somewhere in these ruins.");
        break;
    case KIND_GADGET:
        if (id == ITEM_BINOCULARS || id == ITEM_MAGNIFIER)
            break;          /* held up, not clicked */
        if (id == ITEM_METAL_DETECTOR) {
            message(g, COL_TEXT, "Sweep it over the sand and listen for the beeps.");
            break;
        }
        begin(g, ACT_USE, 0.5f);
        break;
    case KIND_ARMOR:
        equip_item(g, id);
        break;
    case KIND_BAIT:
        message(g, COL_TEXT, "Bait goes on the hook by itself: the best you carry, each cast.");
        break;
    case KIND_FISH:
    case KIND_TREASURE:
        begin(g, ACT_USE, 0.8f);
        break;
    case KIND_ROD:
        break;
    }
}

/* what happens at the key moment of the current action */
static void action_effect(Game *g)
{
    int slot = g->selected;
    ItemId id = held_item(g);
    switch (g->action) {
    case ACT_SWING:
        do_hit(g, id);
        break;
    case ACT_CAST:
        g->mana -= spell_cost(g, id);
        g->cooldowns[id] = ITEMS[id].cooldown;
        magic_cast(g, ITEMS[id].spell);
        break;
    case ACT_THROW:
        for (int i = 0; i < MAX_GRENADES; i++) {
            Grenade *gr = &g->grenades[i];
            if (gr->used)
                continue;
            vec3 dir;
            look_dir(g, dir);
            *gr = (Grenade){ .used = true, .fuse = 2.4f };
            gr->xf = TRANSFORM_AT(g->eye, 0.0f);
            glm_vec3_muladds(dir, 0.5f, gr->pos);
            glm_vec3_scale(dir, 13.0f, gr->vel);
            gr->vel[1] += 3.0f;
            glm_vec3_muladds(g->vel, 0.5f, gr->vel);
            audio_play(SFX_THROW, 1.0f);
            use_up(g, slot);
            break;
        }
        break;
    case ACT_USE: {
        const ItemDef *it = &ITEMS[id];
        g->cooldowns[id] = it->cooldown;
        if (it->kind == KIND_FOOD) {
            audio_play(id == ITEM_GOBLET || id == ITEM_MANA_POTION ? SFX_DRINK : SFX_EAT, 1.0f);
            if (id == ITEM_AVOCADO) {
                g->max_hp += 25.0f;
                g->hp = g->max_hp;
                message(g, COL_GOLD, "The Sacred Avocado. Warmth floods through you. (+25 max health)");
            } else if (it->mana > 0) {
                g->mana = fminf(g->max_mana, g->mana + it->mana);
                message(g, COL_MAGIC, "Tastes of thunderstorms. (+%d mana)", (int)it->mana);
            } else if (id == ITEM_LIME) {
                g->hp = fminf(g->max_hp, g->hp + it->heal);
                g->toxic = 0.0f;
                g->sting_t = 0.0f;
                message(g, COL_TEXT, "Your whole face puckers. The burning in your lungs is gone. (+%d health)", (int)it->heal);
            } else {
                g->hp = fminf(g->max_hp, g->hp + it->heal);
                message(g, COL_TEXT, "You eat the %s. (+%d health)", it->name, (int)it->heal);
            }
            use_up(g, slot);
        } else if (id == ITEM_TORCH || id == ITEM_LANTERN) {
            LeftHand want = id == ITEM_TORCH ? LEFT_TORCH : LEFT_LANTERN;
            g->left = g->left == want ? LEFT_EMPTY : want;
            audio_play(id == ITEM_TORCH ? SFX_TORCH_ON : SFX_LANTERN, 0.8f);
        } else if (id == ITEM_RUBBER_DUCK) {
            message(g, COL_FUN, "SQUEAK!");
            audio_play(SFX_DUCK, 1.0f);
            for (int i = 0; i < MAX_CREATURES; i++) {
                Creature *c = &g->creatures[i];
                if (c->used && c->state != ST_DEAD && c->state != ST_DORMANT && glm_vec3_distance(c->pos, g->pos) < 30.0f)
                    c->flee_t = 8.0f;
            }
        } else if (id == ITEM_POCKET_WATCH) {
            g->slow_t = 6.0f;
            audio_play(SFX_WATCH, 1.0f);
            message(g, COL_FUN, "Tick... tock... The world slows to a crawl.");
        } else if (id == ITEM_BOOMBOX) {
            g->boombox_t = 10.0f;
            message(g, COL_FUN, "The boombox crackles to life. Everything nearby starts to dance.");
            for (int i = 0; i < MAX_CREATURES; i++) {
                Creature *c = &g->creatures[i];
                if (c->used && c->state != ST_DEAD && c->state != ST_DORMANT && glm_vec3_distance(c->pos, g->pos) < 25.0f)
                    c->dance_t = 10.0f;
            }
        } else if (id == ITEM_GAS_MASK) {
            g->mask_on = !g->mask_on;
            message(g, COL_TEXT, g->mask_on ? "You pull on the gas mask. It smells of old rubber."
                                            : "You take off the gas mask.");
        } else if (id == ITEM_SPECTACLES) {
            g->specs_on = !g->specs_on;
            message(g, COL_TEXT, g->specs_on ? "Through the spectacles, every creature's vigor is plain."
                                             : "You fold the spectacles away.");
        } else if (id == ITEM_COMPASS) {
            message(g, COL_TEXT, "The needle trembles, pointing somewhere no map would.");
        } else if (it->kind == KIND_FISH) {
            Animal *w = animal_near(g, SPECIES_WALRUS, 4.0f);
            if (w) {
                animal_feed(g, w);
                use_up(g, slot);
            } else if (id == ITEM_FISH_PUFFER) {
                audio_play(SFX_EAT, 1.0f);
                hurt_player(g, g->pos, 20.0f);
                g->toxic = fmaxf(g->toxic, 0.6f);
                message(g, COL_RED, "Your tongue goes numb, then everything else does. You were warned.");
                use_up(g, slot);
            } else {
                audio_play(SFX_EAT, 1.0f);
                g->hp = fminf(g->max_hp, g->hp + it->heal);
                message(g, COL_TEXT, "You eat the %s raw. Brave. (+%d health)", it->name, (int)it->heal);
                use_up(g, slot);
            }
        } else if (id == ITEM_GNOME) {
            vec3 fwd;
            flat_forward(g, fwd);
            glm_vec3_copy(g->pos, g->gnome_pos);
            glm_vec3_muladds(fwd, 1.5f, g->gnome_pos);
            g->gnome_pos[1] = ground_at(g, g->gnome_pos[0], g->gnome_pos[2], g->pos[1] + 0.5f);
            g->gnome_t = 20.0f;
            audio_play(SFX_GNOME, 1.0f);
            message(g, COL_FUN, "You set the gnome down. Every creature nearby stops to stare at him.");
        } else if (id == ITEM_UKULELE) {
            g->ukulele_t = 6.0f;
            audio_play(SFX_UKULELE, 1.0f);
            animals_serenade(g);
            for (int k = 0; k < g->npc_count; k++)
                if (glm_vec3_distance(g->npcs[k].pos, g->pos) < 14.0f)
                    g->npcs[k].dancing = true;
        } else if (id == ITEM_SELKIE_SCALE || id == ITEM_GILL_PEARL || id == ITEM_DOLPHIN_CHARM || id == ITEM_ANGLER_LAMP) {
            static const char *what[4] = {
                "The scale sinks into your skin with a cold shiver. You'll swim like a seal now.",
                "The pearl slides down like cold honey. Your lungs feel twice as deep.",
                "You knot the charm at your throat. The sea feels friendly, and far less tiring.",
                "The lamp's light seeps into your hands. The deep will glow around you now.",
            };
            int k = id == ITEM_SELKIE_SCALE ? 0 : id == ITEM_GILL_PEARL ? 1 : id == ITEM_DOLPHIN_CHARM ? 2 : 3;
            if (k == 0) g->selkie = true;
            if (k == 1) g->gill_pearl = true;
            if (k == 2) g->dolphin = true;
            if (k == 3) g->angler = true;
            audio_play(SFX_QUEST, 1.0f);
            message(g, COL_MAGIC, "%s", what[k]);
            use_up(g, slot);
        } else if (id == ITEM_FIRST_TIDE_EYE) {
            g->first_tide = true;
            audio_play(SFX_QUEST_DONE, 1.0f);
            message(g, COL_GOLD, "The Eye opens. The sea is no longer something you're in; it's something you're part of.");
            use_up(g, slot);
        } else if (id == ITEM_BOTTLE) {
            static const char *notes[5] = {
                "Whoever finds this: the fish off the lighthouse only bite after dark, and only for glowing bait. -- B.",
                "Day 40 at sea. The walruses have started whistling back at me. I think I'm winning the argument.",
                "The Court was never drowned. It chose the sea. Remember that, if the bell rings again.",
                "If found, please return to Coralie at the Curious Clam. She owes me eleven shells. -- a friend",
                "Old Ember sleeps for half an hour at a time. Whatever it burns comes back. That's the Covenant.",
            };
            reading_show(g, "A note in a bottle", notes[(int)(frand() * 5) % 5]);
            audio_play(SFX_PAGE, 1.0f);
            use_up(g, slot);
        } else if (id == ITEM_CONCH) {
            quest_talk(g, NULL);    /* the conch is the quests' business (see quest.c) */
        } else if (it->kind == KIND_TREASURE) {
            message(g, COL_TEXT, "%s", it->desc);
        }
        break;
    }
    default:
        break;
    }
}

/* G: put the held item down in front of you. a lantern stays lit where you set it */
static void drop_item(Game *g)
{
    ItemId id = held_item(g);
    if (id == ITEM_NONE || g->dead)
        return;
    vec3 fwd, at;
    flat_forward(g, fwd);
    glm_vec3_copy(g->pos, at);
    glm_vec3_muladds(fwd, 1.0f, at);
    at[1] = ground_at(g, at[0], at[2], g->pos[1] + 0.3f);
    if (id == ITEM_LANTERN) {
        add_light(g, LIGHT_LANTERN, at, (vec3){0, 1, 0}, g->cam.fp_yaw, true);
        audio_play(SFX_LANTERN, 1.0f);
        message(g, COL_TEXT, "You set the lantern down. It glows on without you.");
    } else {
        at[1] += 0.05f;
        new_pickup(g, id, 1, at);
        audio_play(SFX_CLICK, 1.0f);
    }
    use_up(g, g->selected);
}

/* ---------- interaction ---------- */

typedef enum { TGT_NONE, TGT_DOOR, TGT_CHEST, TGT_LIGHT, TGT_PICKUP, TGT_TOMB, TGT_NPC, TGT_THING, TGT_BOAT, TGT_WALRUS } TargetKind;

static TargetKind find_target(Game *g, int *index)
{
    vec3 dir;
    look_dir(g, dir);
    TargetKind best = TGT_NONE;
    float best_t = 1e9f;

    #define CONSIDER(kind, i, point, radius, reach, need_los) do {                     \
        vec3 rel;                                                                      \
        glm_vec3_sub(point, g->head, rel);                                             \
        float t = glm_vec3_dot(rel, dir);                                              \
        if (t > 0.0f && t < (reach) && t < best_t) {                                   \
            vec3 closest;                                                              \
            glm_vec3_copy(g->head, closest);                                           \
            glm_vec3_muladds(dir, t, closest);                                         \
            if (glm_vec3_distance(closest, point) < (radius)) {                        \
                vec3 near_pt;                                                          \
                glm_vec3_copy(point, near_pt);                                         \
                glm_vec3_muladds(dir, -0.35f, near_pt);                                \
                if (!(need_los) || level_line_clear(&g->level, g->head, near_pt)) {    \
                    best = kind; best_t = t; *index = i;                               \
                }                                                                      \
            }                                                                          \
        }                                                                              \
    } while (0)

    for (int i = 0; i < g->door_count; i++) {
        vec3 p = { g->doors[i].pos[0], FLOOR_Y + 1.6f, g->doors[i].pos[2] };
        CONSIDER(TGT_DOOR, i, p, 1.3f, REACH + 0.8f, false);
    }
    for (int i = 0; i < g->chest_count; i++) {
        /* aim at the lid: the chest's own collision box would block its center */
        vec3 p = { g->chests[i].pos[0], g->chests[i].pos[1] + 0.72f, g->chests[i].pos[2] };
        CONSIDER(TGT_CHEST, i, p, 0.8f, REACH, true);
    }
    for (int i = 0; i < g->tomb_count; i++) {
        vec3 p = { g->tombs[i].pos[0], g->tombs[i].pos[1] + 1.15f, g->tombs[i].pos[2] };
        CONSIDER(TGT_TOMB, i, p, 1.2f, REACH, true);
    }
    for (int i = 0; i < MAX_LIGHTS_SRC; i++) {
        LightSource *l = &g->lights[i];
        if (!l->used || l->kind == LIGHT_CHANDELIER || (l->kind == LIGHT_LANTERN && l->empty))
            continue;
        float r = l->kind == LIGHT_FIREPIT ? 1.0f : 0.6f;
        vec3 p = { l->pos[0], l->pos[1] + (l->kind == LIGHT_FIREPIT ? 0.25f : l->kind == LIGHT_LANTERN ? 0.3f : 0.0f), l->pos[2] };
        CONSIDER(TGT_LIGHT, i, p, r, REACH, l->kind == LIGHT_FIREPIT);
    }
    for (int i = 0; i < MAX_PICKUPS; i++) {
        if (!g->pickups[i].used)
            continue;
        vec3 p;
        glm_vec3_copy(g->pickups[i].pos, p);
        p[1] += 0.15f;
        CONSIDER(TGT_PICKUP, i, p, 0.6f, REACH, true);
    }
    for (int i = 0; i < g->npc_count; i++) {
        if (!npc_visible(g, &g->npcs[i]))
            continue;
        vec3 p = { g->npcs[i].pos[0], g->npcs[i].pos[1] + (g->npcs[i].swimming ? 0.2f : 1.2f), g->npcs[i].pos[2] };
        CONSIDER(TGT_NPC, i, p, 0.9f, REACH + 0.5f, false);
    }
    for (int i = 0; i < g->thing_count; i++) {
        Thing *th = &g->things[i];
        if (!th->used || !thing_prompt(g, th))
            continue;
        float r = th->kind == THING_SHELL ? 0.45f : th->kind == THING_WHIRLPOOL ? 3.0f : 0.8f;
        vec3 tp;
        glm_vec3_copy(th->pos, tp);
        bool far_ok = th->kind == THING_WHIRLPOOL;
        CONSIDER(TGT_THING, i, tp, r, REACH + (far_ok ? 8.0f : 0.3f), !far_ok);
    }
    for (int i = 0; i < g->boat_count; i++) {
        if (g->boats[i].occupied)
            continue;
        vec3 p = { g->boats[i].pos[0], g->boats[i].pos[1] + 0.4f, g->boats[i].pos[2] };
        CONSIDER(TGT_BOAT, i, p, 1.6f, REACH + 1.5f, false);
    }
    for (int i = 0; i < MAX_ANIMALS; i++) {
        Animal *a = &g->animals[i];
        if (!a->used || a->species != SPECIES_WALRUS || a->gone_t > 0.0f)
            continue;
        vec3 p = { a->pos[0], a->pos[1] + 0.5f, a->pos[2] };
        CONSIDER(TGT_WALRUS, i, p, 1.0f, REACH + 0.5f, false);
    }
    #undef CONSIDER
    return best;
}

static bool left_lit(const Game *g)
{
    return g->left == LEFT_TORCH || g->left == LEFT_LANTERN;
}

static void update_prompt(Game *g)
{
    g->prompt[0] = 0;
    if (g->dead || g->inv_open || g->menu != MENU_NONE || g->cam.mode != CAM_FIRST_PERSON || g->shop_npc >= 0 || g->journal_open)
        return;
    int i;
    const char *key = g->pad ? "[X]" : "[E]";
    if (g->boat_in >= 0) {
        snprintf(g->prompt, sizeof g->prompt, "%s Step out of the boat", key);
        return;
    }
    TargetKind target = find_target(g, &i);
    if (target == TGT_NONE && boat_near(g, 4.5f) >= 0 && !g->swimming) {
        snprintf(g->prompt, sizeof g->prompt, "%s Climb down into the rowing boat", key);
        return;
    }
    switch (target) {
    case TGT_DOOR: {
        Door *d = &g->doors[i];
        if (d->locked) {
            if (has_item(g, ITEM_KEY))
                snprintf(g->prompt, sizeof g->prompt, "%s Unlock with the Rusty Key", key);
            else
                snprintf(g->prompt, sizeof g->prompt, "Locked");
        } else {
            snprintf(g->prompt, sizeof g->prompt, "%s %s door", key, d->target > 0.5f ? "Close" : "Open");
        }
        break;
    }
    case TGT_CHEST:
        if (!g->chests[i].opened)
            snprintf(g->prompt, sizeof g->prompt, "%s Open the %schest", key, g->chests[i].gold ? "gilded " : "");
        break;
    case TGT_TOMB:
        if (!g->tombs[i].opened)
            snprintf(g->prompt, sizeof g->prompt, "%s Open the %stomb", key, g->tombs[i].great ? "great " : "");
        break;
    case TGT_LIGHT: {
        LightSource *l = &g->lights[i];
        if (l->kind == LIGHT_TORCH) {
            if (l->empty) {
                if (has_item(g, ITEM_TORCH))
                    snprintf(g->prompt, sizeof g->prompt, "%s Mount a torch", key);
                else
                    snprintf(g->prompt, sizeof g->prompt, "An empty sconce");
            } else if (!l->lit && left_lit(g)) {
                snprintf(g->prompt, sizeof g->prompt, "%s Light the torch", key);
            } else {
                snprintf(g->prompt, sizeof g->prompt, "%s Take the torch", key);
            }
        } else if (l->kind == LIGHT_LANTERN) {
            snprintf(g->prompt, sizeof g->prompt, "%s Pick up the lantern", key);
        } else {
            snprintf(g->prompt, sizeof g->prompt, "%s %s the fire", key, l->lit ? "Smother" : "Light");
        }
        break;
    }
    case TGT_PICKUP:
        snprintf(g->prompt, sizeof g->prompt, "%s Take %s", key, ITEMS[g->pickups[i].item].name);
        break;
    case TGT_NPC:
        snprintf(g->prompt, sizeof g->prompt, "%s Talk to %s", key, g->npcs[i].name[0] ? g->npcs[i].name : "them");
        break;
    case TGT_THING: {
        const char *what = thing_prompt(g, &g->things[i]);
        if (what)
            snprintf(g->prompt, sizeof g->prompt, "%s %s", key, what);
        break;
    }
    case TGT_BOAT:
        snprintf(g->prompt, sizeof g->prompt, "%s Climb into the rowing boat", key);
        break;
    case TGT_WALRUS: {
        ItemId held = held_item(g);
        if (held != ITEM_NONE && ITEMS[held].kind == KIND_FISH)
            snprintf(g->prompt, sizeof g->prompt, "Click: feed the walrus your %s", ITEMS[held].name);
        else
            snprintf(g->prompt, sizeof g->prompt, "%s Pat the walrus", key);
        break;
    }
    default:
        break;
    }
}

void game_interact(Game *g)
{
    if (g->dead || g->cam.mode != CAM_FIRST_PERSON || g->menu != MENU_NONE)
        return;
    if (g->boat_in >= 0) {
        boat_leave(g);
        return;
    }
    int i;
    TargetKind t = find_target(g, &i);
    if (t == TGT_NONE) {
        /* a boat alongside the pier, or right beside you in the water */
        int b = boat_near(g, g->swimming ? 3.0f : 4.5f);
        if (b >= 0)
            boat_board(g, b);
        return;
    }
    g->reach_t = 0.001f;        /* left hand reaches out */

    if (t == TGT_DOOR) {
        Door *d = &g->doors[i];
        if (d->locked) {
            if (!has_item(g, ITEM_KEY)) {
                message(g, COL_TEXT, "Locked fast. There must be a key somewhere.");
                audio_play_at(SFX_DOOR_LOCKED, d->pos, 1.0f);
                return;
            }
            d->locked = false;
            audio_play_at(SFX_UNLOCK, d->pos, 1.0f);
            message(g, COL_GOLD, "The rusty key turns with a groan.");
        }
        d->target = d->target > 0.5f ? 0.0f : 1.0f;
        audio_play_at(d->target > 0.5f ? SFX_DOOR_OPEN : SFX_DOOR_CLOSE, d->pos, 1.0f);
    } else if (t == TGT_CHEST) {
        Chest *ch = &g->chests[i];
        if (ch->opened)
            return;
        ch->opened = true;
        audio_play_at(SFX_CHEST, ch->pos, 1.0f);
        audio_play(SFX_PICKUP, 0.8f);
        vec3 at = { ch->pos[0], ch->pos[1] + 0.6f, ch->pos[2] };
        for (int k = 0; k < 12; k++)
            fx_sparkle(&g->ps, at);
        for (int k = 0; k < ch->loot_n; k++) {
            game_give(g, ch->loot[k], ch->loot_count[k]);
            if (ch->loot_count[k] > 1)
                message(g, COL_GOLD, "Found: %s x%d", ITEMS[ch->loot[k]].name, ch->loot_count[k]);
            else
                message(g, COL_GOLD, "Found: %s", ITEMS[ch->loot[k]].name);
        }
    } else if (t == TGT_TOMB) {
        tomb_open(g, &g->tombs[i]);
    } else if (t == TGT_LIGHT) {
        LightSource *l = &g->lights[i];
        if (l->kind == LIGHT_TORCH) {
            if (l->empty) {
                if (!has_item(g, ITEM_TORCH))
                    return;
                /* mount one of your torches; lit if your hand torch can light it */
                take_one(g, ITEM_TORCH);
                l->empty = false;
                l->lit = left_lit(g);
                audio_play_at(l->lit ? SFX_TORCH_ON : SFX_CLICK, l->pos, 1.0f);
            } else if (!l->lit && left_lit(g)) {
                l->lit = true;
                audio_play_at(SFX_TORCH_ON, l->pos, 1.0f);
            } else {
                game_give(g, ITEM_TORCH, 1);
                bool was_lit = l->lit;
                l->empty = true;
                l->lit = false;
                if (g->left == LEFT_EMPTY && was_lit)
                    g->left = LEFT_TORCH;
                audio_play_at(SFX_TORCH_ON, l->pos, 0.6f);
                message(g, COL_TEXT, "You take the torch from its sconce.");
            }
        } else if (l->kind == LIGHT_LANTERN) {
            game_give(g, ITEM_LANTERN, 1);
            l->used = false;
            if (g->left == LEFT_EMPTY)
                g->left = LEFT_LANTERN;
            audio_play(SFX_LANTERN, 1.0f);
            message(g, COL_TEXT, "You pick up the lantern. (T: carry it, G: set it down)");
        } else {
            l->lit = !l->lit;
            audio_play_at(l->lit ? SFX_TORCH_ON : SFX_TORCH_OFF, l->pos, 1.0f);
            if (!l->lit)
                fx_dust(&g->ps, l->pos, 4);
        }
    } else if (t == TGT_PICKUP) {
        Pickup *p = &g->pickups[i];
        if (p->item == ITEM_AVOCADO)
            message(g, COL_GOLD, "You lift the Sacred Avocado from the altar. It hums faintly.");
        else
            message(g, COL_TEXT, "Taken: %s", ITEMS[p->item].name);
        game_give(g, p->item, p->count);
        audio_play(SFX_PICKUP, 0.7f);
        p->used = false;
    } else if (t == TGT_NPC) {
        quest_talk(g, &g->npcs[i]);     /* the quests get first say; then chat, or a shop */
    } else if (t == TGT_THING) {
        thing_use(g, &g->things[i]);
    } else if (t == TGT_BOAT) {
        boat_board(g, i);
    } else if (t == TGT_WALRUS) {
        Animal *a = &g->animals[i];
        a->whistle_t = 2.6f;
        audio_play_at(SFX_WALRUS_WHISTLE, a->pos, 1.0f);
        message(g, COL_FUN, "The walrus puckers up and whistles at you, delighted.");
    }
}

static void update_actions(Game *g, float dt)
{
    ItemId held = held_item(g);

    /* the shield in the hotbar goes on the left arm */
    if (held == ITEM_SHIELD && g->left != LEFT_SHIELD)
        g->left = LEFT_SHIELD;
    if (g->left == LEFT_SHIELD && !has_item(g, ITEM_SHIELD))
        g->left = LEFT_EMPTY;

    /* held actions: shield up, binoculars or the magnifying glass to the eyes */
    bool hold_block = !g->dead && !g->inv_open &&
                      ((g->left == LEFT_SHIELD && g->rmb && held != ITEM_BINOCULARS && held != ITEM_HARPOON) ||
                       ((held == ITEM_BINOCULARS || held == ITEM_MAGNIFIER) && (g->lmb || g->rmb)));
    if (g->action == ACT_BLOCK && !hold_block)
        g->action = ACT_IDLE;
    if (g->action == ACT_IDLE && hold_block)
        g->action = ACT_BLOCK;

    if (g->action == ACT_SWING || g->action == ACT_THROW || g->action == ACT_USE || g->action == ACT_CAST) {
        g->action_t += dt / g->action_dur;
        float moment = g->action == ACT_SWING ? 0.42f : g->action == ACT_THROW ? 0.5f
                     : g->action == ACT_CAST ? 0.42f : 0.6f;
        if (!g->action_done && g->action_t >= moment) {
            g->action_done = true;
            action_effect(g);
        }
        if (g->action_t >= 1.0f)
            g->action = ACT_IDLE;
    }
    if (g->reach_t > 0.0f && (g->reach_t += dt / 0.45f) >= 1.0f)
        g->reach_t = 0.0f;

    for (int i = 0; i < ITEM_COUNT; i++)
        g->cooldowns[i] = fmaxf(0.0f, g->cooldowns[i] - dt);
    if ((g->left == LEFT_TORCH && !has_item(g, ITEM_TORCH)) || (g->left == LEFT_LANTERN && !has_item(g, ITEM_LANTERN)))
        g->left = LEFT_EMPTY;
    if (g->mask_on && !has_item(g, ITEM_GAS_MASK))
        g->mask_on = false;
    if (g->specs_on && !has_item(g, ITEM_SPECTACLES))
        g->specs_on = false;

    /* mana trickles back (faster under a magister's hat or cape) */
    float regen = 5.0f * (g->equip[EQ_HEAD] == ITEM_WIZARD_HAT ? 2.0f : 1.0f) * (g->equip[EQ_BACK] == ITEM_STARRY_CAPE ? 1.5f : 1.0f);
    if (!g->dead)
        g->mana = fminf(g->max_mana, g->mana + dt * regen);
}

static void update_grenades(Game *g, float dt)
{
    for (int i = 0; i < MAX_GRENADES; i++) {
        Grenade *gr = &g->grenades[i];
        if (!gr->used)
            continue;
        gr->fuse -= dt;
        gr->spin += dt * 9.0f;
        gr->vel[1] -= GRAVITY * dt;
        vec3 next;
        glm_vec3_copy(gr->pos, next);
        glm_vec3_muladds(gr->vel, dt, next);
        float ground = ground_at(g, next[0], next[2], gr->pos[1]);
        if (next[1] < ground + 0.06f) {
            next[1] = ground + 0.06f;
            if (gr->vel[1] < -2.0f)
                audio_play_at(SFX_BOUNCE, next, fminf(1.0f, -gr->vel[1] / 8.0f));
            if (gr->vel[1] < 0.0f)
                gr->vel[1] *= -0.35f;
            gr->vel[0] *= 0.65f;
            gr->vel[2] *= 0.65f;
        }
        vec3 test;
        glm_vec3_copy(next, test);
        test[1] -= 0.5f;        /* level_collide ignores boxes below feet + 0.45 */
        if (level_collide(&g->level, test, 0.08f, 0.6f)) {
            gr->vel[0] *= -0.45f;
            gr->vel[2] *= -0.45f;
            next[0] = test[0];
            next[2] = test[2];
        }
        glm_vec3_copy(next, gr->pos);
        if (gr->fuse <= 0.0f) {
            gr->used = false;
            explode(g, gr->pos, ITEMS[ITEM_GRENADE].damage, ITEMS[ITEM_GRENADE].range, true);
            message(g, COL_GOLD, "BOOM.");
        }
    }
}

/* ---------- the world's dangers, and things that wear off ---------- */

static void update_hazards(Game *g, float dt)
{
    g->gills_t = fmaxf(0.0f, g->gills_t - dt);
    g->shadow_t = fmaxf(0.0f, g->shadow_t - dt);
    g->gnome_t = fmaxf(0.0f, g->gnome_t - dt);
    g->sting_t = fmaxf(0.0f, g->sting_t - dt);
    if (g->ukulele_t > 0.0f && (g->ukulele_t -= dt) <= 0.0f)
        for (int i = 0; i < g->npc_count; i++)
            g->npcs[i].dancing = false;

    /* volcanic fumes: choking, unless you wear the gas mask */
    float fumes = vent_fumes(g, g->head);
    static float warned;
    warned -= dt;
    if (fumes > 0.05f && !g->mask_on && !g->dead) {
        g->toxic = fminf(1.0f, g->toxic + dt * (0.25f + fumes * 0.8f));
        if (g->toxic > 0.3f && frand() < dt * 1.3f) {
            hurt_player(g, g->pos, 2.0f + 5.0f * fumes);
            audio_play(SFX_COUGH, 0.8f);
        }
        if (warned <= 0.0f) {
            message(g, COL_RED, "The fumes burn your throat and eyes! A gas mask would keep them out.");
            warned = 8.0f;
        }
    } else {
        g->toxic = fmaxf(0.0f, g->toxic - dt * 0.3f);
    }

    /* hail stings on bare heads out in the open */
    if (g->weather.hail > 0.4f && !g->dead && level_sky_exposure(&g->level, g->head) > 0.5f && !g->underwater) {
        ItemId hat = g->equip[EQ_HEAD];
        bool covered = hat == ITEM_IRON_HELM || hat == ITEM_DIVING_HELM || hat == ITEM_FUR_HAT ||
                       g->equip[EQ_BACK] == ITEM_FUR_CLOAK || g->boat_in >= 0;
        if ((g->hail_t += dt) > 1.6f) {
            g->hail_t = 0.0f;
            audio_play(SFX_HAILSTONE, 0.7f);
            if (!covered) {
                hurt_player(g, (vec3){g->pos[0], g->pos[1] + 3.0f, g->pos[2]}, 2.0f);
                static float told;
                if (g->time - told > 30.0f) {
                    message(g, COL_RED, "Hailstones crack against your skull. Get under a roof, or a helmet.");
                    told = g->time;
                }
            }
        }
    }

    /* buoys ride the swell */
    for (int i = 0; i < g->prop_count; i++) {
        Prop *p = &g->props[i];
        if (!p->floats)
            continue;
        float surface, depth;
        if (!sea_at(g, p->pos[0], p->pos[2], &surface, &depth))
            continue;
        p->pos[1] = surface - 0.6f;
        p->pitch = sinf(g->time * 1.1f + p->pos[0]) * 0.08f;
        p->roll = cosf(g->time * 0.9f + p->pos[2]) * 0.08f;
        prop_place(g, p);
    }
    /* plants burnt by the eruption grow back */
    for (int i = 0; i < g->prop_count; i++) {
        Prop *p = &g->props[i];
        if (p->burns && p->growth < 1.0f && g->volcano.phase != VOLC_ERUPTING && g->volcano.phase != VOLC_ASH) {
            p->growth = fminf(1.0f, p->growth + dt * 0.08f);
            transform_matrix(&p->xf, p->matrix);
            glm_scale_uni(p->matrix, fmaxf(p->growth, 0.001f));
            prop_bounds(g, p);
        }
    }
}

/* ---------- update ---------- */

static void update_camera(Game *g, float dt)
{
    /* smooth zoom for the binoculars */
    float want = g->action == ACT_BLOCK && held_item(g) == ITEM_BINOCULARS ? 18.0f : g->settings.fov;
    g->fov += (want - g->fov) * (1.0f - expf(-dt * 10.0f));
    if (dt <= 0.0f)
        g->fov = want;

    float aspect = (float)g->width / (float)(g->height ? g->height : 1);
    glm_perspective(glm_rad(g->fov), aspect, 0.05f, 1500.0f, g->proj);
    /* hands use the same field of view as the world, so flames and lights line up */
    glm_perspective(glm_rad(g->settings.fov), aspect, 0.01f, 10.0f, g->hand_proj);

    if (g->cam.mode == CAM_FIRST_PERSON) {
        float dead_drop = g->dead && !g->third_person ? fminf(g->dead_t * 1.5f, 1.3f) : 0.0f;
        glm_vec3_copy(g->pos, g->head);
        g->head[1] += EYE_HEIGHT - g->land_t * 0.12f - g->crouch_amount * 0.6f;
        if (g->swimming)
            g->head[1] -= 0.2f;
        glm_vec3_copy(g->head, g->eye);
        g->eye[1] -= dead_drop;
        if (g->third_person)
            player_third_camera(g, dt);
        if (g->shake_t > 0.0f) {
            float s = g->shake_t * 0.12f;
            g->eye[0] += (frand() - 0.5f) * s;
            g->eye[1] += (frand() - 0.5f) * s;
            g->eye[2] += (frand() - 0.5f) * s;
        }
        glm_vec3_copy(g->eye, g->cam.position);
    } else {
        glm_vec3_copy(g->pos, g->head);
        g->head[1] += EYE_HEIGHT;
        camera_eye(&g->cam, g->eye);
    }
    camera_view(&g->cam, g->view);
    if (g->dead && g->cam.mode == CAM_FIRST_PERSON && !g->third_person) {
        mat4 roll;
        glm_rotate_make(roll, fminf(g->dead_t, 1.0f) * 1.2f, (vec3){0, 0, 1});
        glm_mat4_mul(roll, g->view, g->view);
    }
}

static void torch_flame_pos(const LightSource *l, vec3 out)
{
    glm_vec3_copy((float *)l->pos, out);
    glm_vec3_muladds((float *)l->normal, 0.13f, out);
    out[1] += 0.21f;
}

static void emit_effects(Game *g, float dt)
{
    for (int i = 0; i < MAX_LIGHTS_SRC; i++) {
        LightSource *l = &g->lights[i];
        if (!l->used || !l->lit || glm_vec3_distance(l->pos, g->eye) > 50.0f)
            continue;
        if (l->kind == LIGHT_TORCH) {
            vec3 p;
            torch_flame_pos(l, p);
            fx_flame(&g->ps, p, 1.0f, dt);
        } else if (l->kind == LIGHT_LANTERN && !l->empty) {
            fx_flame(&g->ps, (vec3){l->pos[0], l->pos[1] + 0.22f, l->pos[2]}, 0.3f, dt);
        } else if (l->kind == LIGHT_FIREPIT) {
            fx_flame(&g->ps, l->pos, 2.6f, dt);
        }
    }
    if (g->cam.mode == CAM_FIRST_PERSON && !g->dead) {
        if (g->left == LEFT_TORCH) {
            vec3 tip;
            hands_torch_tip(&g->hands, tip);
            fx_flame(&g->ps, tip, 0.55f, dt);
        } else if (g->left == LEFT_LANTERN) {
            vec3 lp;
            hands_lantern_pos(&g->hands, lp);
            fx_flame(&g->ps, lp, 0.18f, dt);
        }
        /* a spell tome in hand: motes of its color gather in the palm */
        ItemId held = held_item(g);
        if (held != ITEM_NONE && ITEMS[held].kind == KIND_SPELL && frand() < dt * 12.0f) {
            vec3 palm, c;
            hands_palm_pos(&g->hands, palm);
            glm_vec3_scale(ITEMS[held].glow, 0.25f, c);
            fx_mote(&g->ps, palm, c);
        }
    }
    if (g->boombox_t > 0.0f && frand() < dt * 12.0f) {
        vec3 hp;
        hands_right_pos(&g->hands, hp);
        fx_notes(&g->ps, hp);
    }
    for (int i = 0; i < g->chest_count; i++)
        if (g->chests[i].gold && !g->chests[i].opened && frand() < dt * 6.0f)
            fx_sparkle(&g->ps, (vec3){g->chests[i].pos[0], g->chests[i].pos[1] + 0.7f, g->chests[i].pos[2]});
    for (int i = 0; i < MAX_PICKUPS; i++)
        if (g->pickups[i].used && frand() < dt * (g->pickups[i].on_altar ? 10.0f : 3.0f))
            fx_sparkle(&g->ps, g->pickups[i].pos);
    for (int i = 0; i < MAX_CREATURES; i++) {
        Creature *c = &g->creatures[i];
        if (c->used && c->state == ST_DANCE && frand() < dt * 4.0f)
            fx_notes(&g->ps, (vec3){c->pos[0], c->pos[1] + creature_height(c) + 0.3f, c->pos[2]});
    }
    /* fountains: a spout of water */
    for (int i = 0; i < g->fountain_count; i++) {
        if (glm_vec3_distance(g->fountains[i], g->eye) > 40.0f)
            continue;
        for (int k = 0; k < 2; k++) {
            if (frand() > dt * 40.0f)
                continue;
            Particle p = {0};
            glm_vec3_copy((vec3){g->fountains[i][0], g->fountains[i][1] + 1.85f, g->fountains[i][2]}, p.pos);
            float a = frand() * 6.28f;
            glm_vec3_copy((vec3){cosf(a) * 0.9f, 1.4f + frand() * 0.5f, sinf(a) * 0.9f}, p.vel);
            glm_vec4_copy((vec4){0.6f, 0.7f, 0.75f, 0.55f}, p.color0);
            glm_vec4_copy((vec4){0.6f, 0.7f, 0.75f, 0.2f}, p.color1);
            p.size0 = 0.05f;
            p.size1 = 0.03f;
            p.max_life = p.life = 0.8f;
            p.gravity = 9.0f;
            particles_emit(&g->ps, &p);
        }
    }
}

/* the music, rain and other sounds follow where you are and what's around you */
static void update_audio(Game *g)
{
    float yaw = g->cam.mode == CAM_FIRST_PERSON ? g->cam.fp_yaw : g->cam.yaw;
    audio_set_listener(g->eye, yaw);

    Area area = level_area(&g->level, g->head);
    WeatherType w = g->weather.type;
    AudioScene sc = {
        .boombox = g->boombox_t > 0.0f,
        .slowmo = g->slow_t > 0.0f,
        .underground = area == AREA_CRYPT,
        .rain = g->weather.rain * (area == AREA_UNDERSEA ? 0.0f : 1.0f),
        .sheltered = g->eye[1] < level_cover(&g->level, g->eye[0], g->eye[2]) - 0.3f ? 1.0f : 0.0f,
        .submerged = g->underwater,
        .wind = g->weather.wind > 0.35f ? g->weather.gust : 0.0f,
        .hail = g->weather.hail,
        .groove = (int)(g->boombox_t > 0.0f ? g->quest.fish_caught % 3 : 0),
    };
    switch (area) {
    case AREA_CRYPT: sc.mood = MOOD_CRYPT; break;
    case AREA_PALACE: sc.mood = MOOD_PALACE; break;
    case AREA_GARDEN: sc.mood = g->night ? MOOD_NIGHT : MOOD_GARDEN; break;
    case AREA_LIBRARY: sc.mood = MOOD_LIBRARY; break;
    case AREA_BEACH: case AREA_PROMENADE: sc.mood = g->night ? MOOD_NIGHT : MOOD_BEACH; break;
    case AREA_MARINA: sc.mood = MOOD_MARINA; break;
    case AREA_VILLAGE: sc.mood = MOOD_VILLAGE; break;
    case AREA_VOLCANO: sc.mood = MOOD_VOLCANO; break;
    case AREA_OCEAN: sc.mood = MOOD_OCEAN; break;
    case AREA_UNDERSEA: sc.mood = MOOD_UNDERSEA; break;
    default:
        sc.mood = g->night ? MOOD_NIGHT : (w == WEATHER_RAIN || w == WEATHER_STORM) ? MOOD_RAIN
                : w == WEATHER_FOG ? MOOD_NIGHT : MOOD_DAY;
        break;
    }
    if (g->boat_in >= 0 || (g->swimming && area != AREA_MARINA))
        sc.mood = MOOD_OCEAN;
    /* inside the shop and the tavern, their own records play */
    if (glm_vec3_distance(g->head, (vec3){SHOP_X, g->head[1], SHOP_Z}) < 5.0f)
        sc.mood = MOOD_SHOP;
    if (fabsf(g->head[0] - TAVERN_X) < TAVERN_W * 0.5f && fabsf(g->head[2] - TAVERN_Z) < TAVERN_D * 0.5f && g->head[1] < VILLAGE_Y + 4.0f)
        sc.mood = MOOD_TAVERN;
    Npc *ancient = npc_find(g, NPC_ANCIENT);
    if (ancient && npc_visible(g, ancient) && glm_vec3_distance(ancient->pos, g->pos) < 18.0f)
        sc.mood = MOOD_ANCIENT;
    if (g->quest.bell_home && g->quest.bell_ring_t > 0.0f && area == AREA_VILLAGE)
        sc.mood = MOOD_FESTIVAL;
    if (g->volcano.phase == VOLC_ERUPTING || (g->volcano.phase == VOLC_STIRRING && area == AREA_VOLCANO))
        sc.mood = MOOD_ERUPTION;

    /* surf breaking along the shore, from the side the sea is on */
    float c = g->head[2] - coast_z(g->head[0]);
    sc.surf = glm_clamp(1.0f - fabsf(c - 26.0f) / 45.0f, 0.0f, 1.0f) * (g->head[1] > SEA_Y - 1.0f ? 1.0f : 0.0f);
    if (area == AREA_MARINA)
        sc.surf *= 0.4f;
    sc.surf *= 0.6f + storm_level(g) * 0.8f;
    sc.surf_pan = -sinf(yaw);       /* the sea lies to the south */
    sc.harbour = area == AREA_MARINA ? 1.0f : 0.0f;
    sc.chimes = area == AREA_VILLAGE ? 0.5f + g->weather.wind * 0.8f : 0.0f;
    float vd = sqrtf((g->head[0] - VOLCANO_X) * (g->head[0] - VOLCANO_X) + (g->head[2] - VOLCANO_Z) * (g->head[2] - VOLCANO_Z));
    sc.volcano = glm_clamp(1.0f - vd / 450.0f, 0.0f, 1.0f) * (g->volcano.phase == VOLC_STIRRING ? 1.0f : g->volcano.phase == VOLC_ERUPTING ? 1.4f : 0.25f);
    sc.lava = glm_clamp(1.0f - vd / 60.0f, 0.0f, 1.0f) * (g->head[1] > 60.0f ? 1.0f : 0.0f);
    if (g->weather.twister) {
        vec3 d;
        glm_vec3_sub(g->weather.twister_pos, g->head, d);
        d[1] = 0;
        float dist = glm_vec3_norm(d);
        sc.tornado = glm_clamp(1.0f - dist / 260.0f, 0.0f, 1.0f) * g->weather.twister_power;
        sc.tornado_pan = dist > 0.5f ? glm_vec3_dot(d, (vec3){cosf(yaw), 0, -sinf(yaw)}) / dist : 0.0f;
    }

    for (int i = 0; i < MAX_CREATURES; i++) {
        Creature *c = &g->creatures[i];
        if (c->used && (c->state == ST_CHASE || c->state == ST_ATTACK) &&
            glm_vec3_distance(c->pos, g->pos) < 30.0f)
            sc.danger = 1.0f;
    }
    if (g->shadow_t > 0.0f)
        sc.danger = 0.0f;

    /* the nearest crackling fire, panned to where it is */
    vec3 right = { cosf(yaw), 0, -sinf(yaw) };
    for (int i = 0; i < MAX_LIGHTS_SRC; i++) {
        LightSource *l = &g->lights[i];
        if (!l->used || !l->lit || l->kind == LIGHT_CHANDELIER)
            continue;
        vec3 d;
        glm_vec3_sub(l->pos, g->eye, d);
        float dist = glm_vec3_norm(d);
        float reach = l->kind == LIGHT_FIREPIT ? 16.0f : l->kind == LIGHT_TORCH ? 9.0f : 5.0f;
        float level = (1.0f - dist / reach) * (l->kind == LIGHT_FIREPIT ? 1.0f : 0.6f);
        if (level > sc.fire) {
            sc.fire = level;
            sc.fire_pan = dist > 0.3f ? glm_vec3_dot(d, right) / dist : 0.0f;
        }
    }
    if (g->left == LEFT_TORCH && !g->dead && sc.fire < 0.35f) {
        sc.fire = 0.35f;
        sc.fire_pan = -0.5f;
    }
    for (int i = 0; i < g->fountain_count; i++) {
        vec3 d;
        glm_vec3_sub(g->fountains[i], g->eye, d);
        float dist = glm_vec3_norm(d);
        float level = 1.0f - dist / 14.0f;
        if (level > sc.water) {
            sc.water = level;
            sc.water_pan = dist > 0.3f ? glm_vec3_dot(d, right) / dist : 0.0f;
        }
    }
    audio_set_scene(&sc);

    if (g->weather.thunder_now)
        audio_play(SFX_THUNDER, g->weather.thunder_volume * (sc.underground ? 0.25f : sc.sheltered > 0.5f ? 0.6f : 1.0f));
}

void game_update(Game *g, float dt, SDL_Window *win)
{
    (void)win;
    dt = fminf(dt, 0.05f);          /* don't explode after a hitch */
    if (g->menu != MENU_NONE) {
        /* paused: the world holds still */
        update_camera(g, 0.0f);
        return;
    }
    g->time += dt;
    g->title_t += dt;
    g->hurt_t = fmaxf(0.0f, g->hurt_t - dt);
    g->land_t = fmaxf(0.0f, g->land_t - dt * 3.0f);
    g->shake_t = fmaxf(0.0f, g->shake_t - dt);
    g->flash_t = fmaxf(0.0f, g->flash_t - dt);
    g->slow_t = fmaxf(0.0f, g->slow_t - dt);
    g->boombox_t = fmaxf(0.0f, g->boombox_t - dt);
    g->subtitle_t = fmaxf(0.0f, g->subtitle_t - dt);
    for (int i = 0; i < MAX_MESSAGES; i++)
        g->messages[i].t += dt;
    if (g->dead)
        g->dead_t += dt;
    g->calm_t += dt;
    if (!g->dead && g->calm_t > 6.0f)
        g->hp = fminf(g->max_hp, g->hp + 2.5f * dt);

    gamepad_update(g, dt);
    player_move(g, dt);
    update_actions(g, dt);
    update_grenades(g, dt);
    creatures_update(g, dt);
    magic_update(g, dt);
    npcs_update(g, dt);
    animals_update(g, dt);
    things_update(g, dt);
    boats_update(g, dt);
    fishing_update(g, dt);
    volcano_update(g, dt);
    quest_update(g, dt);
    update_hazards(g, dt);

    WeatherType before = g->weather.type;
    weather_update(&g->weather, dt, g->eye, &g->level, &g->terrain, &g->ps);
    if (g->weather.type != before) {
        static const char *notes[WEATHER_COUNT] = {
            "Rain begins to fall.", "Thunder rolls in over the hills.", "A thick fog creeps between the stones.",
            "The clouds break. The sky clears.", "The wind rises to a gale, howling over the stones.",
            "Hail comes rattling down out of a bruised sky. Find a roof, or a helmet.",
            "The sky turns a sick green. Out on the land, a funnel reaches down from the clouds...",
        };
        message(g, COL_FUN, "%s", notes[g->weather.type]);
    }

    for (int i = 0; i < g->door_count; i++) {
        Door *d = &g->doors[i];
        float step = dt * 1.3f;
        d->open += glm_clamp(d->target - d->open, -step, step);
        g->level.boxes[d->box].off = d->open > 0.35f;
        pose_reset(&d->pose, &g->door_model);
        /* the two leaves swing on their hinges, in opposite directions */
        float angle = sinf(d->open * GLM_PI_2f) * 1.75f;
        versor q;
        glm_quatv(q, angle, (vec3){0, 1, 0});
        glm_quat_mul(q, d->pose.r[g->door_left], d->pose.r[g->door_left]);
        glm_quatv(q, -angle, (vec3){0, 1, 0});
        glm_quat_mul(q, d->pose.r[g->door_right], d->pose.r[g->door_right]);
        pose_update(&g->door_model, &d->pose);
    }
    for (int i = 0; i < g->chest_count; i++) {
        Chest *ch = &g->chests[i];
        if (ch->opened && ch->open < 1.0f)
            ch->open = fminf(1.0f, ch->open + dt * 1.2f);
        pose_reset(&ch->pose, &g->chest_model);
        versor q;
        float t = 1.0f - (1.0f - ch->open) * (1.0f - ch->open);
        glm_quatv(q, -t * 1.75f, (vec3){1, 0, 0});
        glm_quat_mul(q, ch->pose.r[g->chest_lid], ch->pose.r[g->chest_lid]);
        pose_update(&g->chest_model, &ch->pose);
    }

    update_camera(g, dt);
    avatar_update(g, dt);
    emit_effects(g, dt);
    particles_update(&g->ps, dt);
    update_prompt(g);

    /* hands follow the camera */
    HandInput in = {0};
    in.held = held_item(g);
    in.action = g->action;
    in.action_t = g->action_t;
    in.swing = in.held != ITEM_NONE ? ITEMS[in.held].swing : SWING_PUNCH;
    in.left = g->left;
    in.reach_t = g->reach_t;
    in.speed = sqrtf(g->vel[0] * g->vel[0] + g->vel[2] * g->vel[2]);
    in.grounded = g->on_ground && !g->swimming;
    in.swimming = g->swimming;
    in.swim_phase = g->swim_phase;
    in.body_armor = g->equip[EQ_BODY];
    in.fishing = g->fish.state;
    in.mask_on = g->mask_on;
    in.look_dx = g->look_dx;
    in.look_dy = g->look_dy;
    in.hurt = g->hurt_t / 0.45f;
    in.land = g->land_t;
    if (in.held == ITEM_TORCH || in.held == ITEM_LANTERN || in.held == ITEM_SHIELD)
        in.held = ITEM_NONE;        /* lights and the shield are carried in the left hand */
    g->look_dx = g->look_dy = 0.0f;
    mat4 inv_view;
    glm_mat4_inv(g->view, inv_view);
    hands_update(&g->hands, &in, inv_view, dt);

    update_audio(g);
}

/* ---------- events ---------- */

static void set_mouse_mode(Game *g, SDL_Window *win)
{
    SDL_SetWindowRelativeMouseMode(win, g->cam.mode == CAM_FIRST_PERSON && !g->inv_open && g->menu == MENU_NONE &&
                                        g->shop_npc < 0 && !g->journal_open);
}

/* inventory layout, shared by drawing and clicking */
#define INV_COLS 9
#define INV_CELL 72.0f
#define INV_GAP  7.0f

static void inv_origin(const Game *g, float *x, float *y)
{
    float w = INV_COLS * INV_CELL + (INV_COLS - 1) * INV_GAP;
    *x = (g->width - w) * 0.5f;
    *y = g->height * 0.5f - 190.0f;
}

static void inv_cell(const Game *g, int i, float *x, float *y)
{
    float ox, oy;
    inv_origin(g, &ox, &oy);
    int row = i / INV_COLS, col = i % INV_COLS;
    *x = ox + col * (INV_CELL + INV_GAP);
    *y = oy + row * (INV_CELL + INV_GAP) + (row > 0 ? 18.0f : 0.0f);
}

static int inv_hover(const Game *g)
{
    for (int i = 0; i < INV_SLOTS; i++) {
        float x, y;
        inv_cell(g, i, &x, &y);
        if (g->mouse_x >= x && g->mouse_x < x + INV_CELL && g->mouse_y >= y && g->mouse_y < y + INV_CELL)
            return i;
    }
    return -1;
}

static void screenshot(Game *g)
{
    unsigned char *px = malloc((size_t)g->width * g->height * 4);
    renderer_read_pixels(&g->r, px);
    char name[64];
    snprintf(name, sizeof name, "screenshot-%ld.png", (long)SDL_GetTicks());
    stbi_flip_vertically_on_write(1);
    stbi_write_png(name, g->width, g->height, 4, px, g->width * 4);
    free(px);
    message(g, COL_TEXT, "Saved %s", name);
}

static void toggle_inventory(Game *g, SDL_Window *win)
{
    if (g->dead)
        return;
    g->inv_open = !g->inv_open;
    g->inv_held = -1;
    audio_play(SFX_CLICK, 1.0f);
    set_mouse_mode(g, win);
}

static void toggle_view(Game *g, SDL_Window *win)
{
    if (g->dead || g->inv_open)
        return;
    if (g->cam.mode == CAM_FIRST_PERSON) {
        /* orbit around where the player stands */
        g->cam.mode = CAM_ORBIT;
        glm_vec3_copy(g->pos, g->cam.target);
        g->cam.target[1] += 1.2f;
        g->cam.yaw = g->cam.fp_yaw;
        g->cam.pitch = 0.35f;
        g->cam.distance = 5.0f;
    } else {
        g->cam.mode = CAM_FIRST_PERSON;
    }
    set_mouse_mode(g, win);
}

static void cycle_weather(Game *g)
{
    weather_set(&g->weather, (WeatherType)((g->weather.type + 1) % WEATHER_COUNT));
}

bool game_event(Game *g, const SDL_Event *e, SDL_Window *win)
{
    if (e->type == SDL_EVENT_QUIT)
        return false;
    if (e->type == SDL_EVENT_MOUSE_MOTION) {
        int ww, wh;
        SDL_GetWindowSize(win, &ww, &wh);
        g->mouse_x = e->motion.x * g->width / (float)(ww ? ww : 1);
        g->mouse_y = e->motion.y * g->height / (float)(wh ? wh : 1);
    }
    if (e->type == SDL_EVENT_GAMEPAD_ADDED && !g->pad) {
        g->pad = SDL_OpenGamepad(e->gdevice.which);
        if (g->pad)
            message(g, COL_TEXT, "Gamepad connected: %s", SDL_GetGamepadName(g->pad));
    }
    if (e->type == SDL_EVENT_GAMEPAD_REMOVED && g->pad && e->gdevice.which == SDL_GetGamepadID(g->pad)) {
        SDL_CloseGamepad(g->pad);
        g->pad = NULL;
    }

    /* the pause menu takes everything while it's open */
    if (g->menu != MENU_NONE)
        return menu_event(g, e, win);
    /* then a shop, if one is open */
    if (g->shop_npc >= 0) {
        if (shop_event(g, e)) {
            set_mouse_mode(g, win);
            return true;
        }
    }

    switch (e->type) {
    case SDL_EVENT_KEY_DOWN:
        if (e->key.repeat)
            break;
        switch (e->key.key) {
        case SDLK_ESCAPE:
            if (g->reading_t > 0.0f)
                g->reading_t = 0.0f;
            else if (g->journal_open) {
                g->journal_open = false;
                set_mouse_mode(g, win);
            } else if (g->inv_open)
                toggle_inventory(g, win);
            else
                menu_open(g, win);
            break;
        case SDLK_J:
            if (!g->dead) {
                g->journal_open = !g->journal_open;
                g->inv_open = false;
                audio_play(SFX_PAGE, 1.0f);
                set_mouse_mode(g, win);
            }
            break;
        case SDLK_F10: toggle_view(g, win); break;
        case SDLK_F4: volcano_force(g); break;
        case SDLK_EQUALS: g->cam_dist = fmaxf(1.6f, g->cam_dist - 0.5f); break;
        case SDLK_MINUS: g->cam_dist = fminf(9.0f, g->cam_dist + 0.5f); break;
        case SDLK_TAB:
        case SDLK_I: toggle_inventory(g, win); break;
        case SDLK_V:
            if (g->cam.mode == CAM_FIRST_PERSON && !g->dead) {
                g->third_person = !g->third_person;
                message(g, COL_TEXT, g->third_person ? "Third person. (V: back to your own eyes, -/=: camera distance)"
                                                     : "First person.");
            }
            break;
        case SDLK_H:
            menu_open(g, win);
            g->menu = MENU_CONTROLS;
            break;
        case SDLK_N:
            game_set_night(g, !g->night);
            message(g, COL_TEXT, g->night ? "Night falls over the ruins." : "Dawn breaks.");
            break;
        case SDLK_F2: cycle_weather(g); break;
        case SDLK_M:
            audio_toggle_music();
            message(g, COL_TEXT, audio_music_on() ? "Music on." : "Music off.");
            break;
        case SDLK_E:
            if (g->reading_t > 0.0f)
                g->reading_t = 0.0f;
            else if (!g->inv_open)
                game_interact(g);
            break;
        case SDLK_T: cycle_left(g); break;
        case SDLK_G: if (!g->inv_open) drop_item(g); break;
        case SDLK_R:
            if (g->dead)
                respawn_player(g);
            break;
        case SDLK_F12: screenshot(g); break;
        default:
            if (e->key.key >= SDLK_1 && e->key.key <= SDLK_9) {
                int slot = (int)(e->key.key - SDLK_1);
                if (g->inv_open) {
                    /* in the satchel: put the hovered item on this hotbar slot */
                    int h = inv_hover(g);
                    if (h >= 0 && h != slot) {
                        Slot tmp = g->slots[slot];
                        g->slots[slot] = g->slots[h];
                        g->slots[h] = tmp;
                        audio_play(SFX_CLICK, 1.0f);
                    }
                } else {
                    game_select(g, slot);
                }
            }
            break;
        }
        if (!g->inv_open)
            camera_event(&g->cam, e);     /* F9 = fly */
        break;

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        switch (e->gbutton.button) {
        case SDL_GAMEPAD_BUTTON_START: menu_open(g, win); break;
        case SDL_GAMEPAD_BUTTON_BACK:
            menu_open(g, win);
            g->menu = MENU_CONTROLS;
            break;
        case SDL_GAMEPAD_BUTTON_WEST: game_interact(g); break;
        case SDL_GAMEPAD_BUTTON_NORTH: toggle_inventory(g, win); break;
        case SDL_GAMEPAD_BUTTON_EAST:
            if (g->inv_open)
                toggle_inventory(g, win);
            else
                g->crouching = !g->crouching;
            break;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            game_select(g, (g->selected + HOTBAR - 1) % HOTBAR);
            break;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            game_select(g, (g->selected + 1) % HOTBAR);
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_UP: cycle_left(g); break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: drop_item(g); break;
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK: toggle_view(g, win); break;
        case SDL_GAMEPAD_BUTTON_SOUTH:
            if (g->dead)
                respawn_player(g);
            break;
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (g->inv_open || g->dead)
            break;
        g->look_dx += e->motion.xrel;
        g->look_dy += e->motion.yrel;
        camera_event(&g->cam, e);
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (g->inv_open) {
            int i = inv_hover(g);
            if (i < 0)
                break;
            if (e->button.button == SDL_BUTTON_RIGHT) {
                /* right click: take it in hand right away */
                if (g->slots[i].id != ITEM_NONE && i >= HOTBAR) {
                    Slot tmp = g->slots[g->selected];
                    g->slots[g->selected] = g->slots[i];
                    g->slots[i] = tmp;
                } else if (i < HOTBAR) {
                    game_select(g, i);
                }
                audio_play(SFX_CLICK, 1.0f);
                break;
            }
            if (g->inv_held < 0) {
                if (g->slots[i].id != ITEM_NONE)
                    g->inv_held = i;
            } else {
                audio_play(SFX_CLICK, 1.0f);
                Slot tmp = g->slots[i];
                g->slots[i] = g->slots[g->inv_held];
                g->slots[g->inv_held] = tmp;
                g->inv_held = -1;
            }
            break;
        }
        if (e->button.button == SDL_BUTTON_LEFT) {
            g->lmb = true;
            if (g->cam.mode == CAM_FIRST_PERSON)
                game_start_action(g);
        } else if (e->button.button == SDL_BUTTON_RIGHT) {
            g->rmb = true;
            if (held_item(g) == ITEM_HARPOON && g->cam.mode == CAM_FIRST_PERSON && !g->dead && g->cooldowns[ITEM_HARPOON] <= 0.0f) {
                /* throw the harpoon along your aim */
                vec3 dir, from, vel;
                look_dir(g, dir);
                glm_vec3_copy(g->head, from);
                glm_vec3_muladds(dir, 0.5f, from);
                glm_vec3_scale(dir, 24.0f, vel);
                vel[1] += 1.5f;
                bolt_fire(g, BOLT_HARPOON, from, vel, false);
                audio_play(SFX_THROW, 1.0f);
                use_up(g, g->selected);
            }
        }
        if (g->cam.mode == CAM_ORBIT)
            camera_event(&g->cam, e);
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e->button.button == SDL_BUTTON_LEFT)
            g->lmb = false;
        else if (e->button.button == SDL_BUTTON_RIGHT)
            g->rmb = false;
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        if (g->cam.mode == CAM_FIRST_PERSON && !g->inv_open) {
            int step = e->wheel.y > 0 ? -1 : e->wheel.y < 0 ? 1 : 0;
            game_select(g, (g->selected + step + HOTBAR) % HOTBAR);
        } else {
            camera_event(&g->cam, e);
        }
        break;

    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        set_mouse_mode(g, win);
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (!g->dead)
            menu_open(g, win);          /* pause when you switch away */
        break;
    }
    return true;
}

/* ---------- rendering ---------- */

static void gather_lights(Game *g)
{
    Renderer *r = &g->r;
    renderer_clear_lights(r);
    float t = g->time;

    /* the player's own lights first, then the nearest others */
    if (g->cam.mode == CAM_FIRST_PERSON && !g->dead) {
        if (g->left == LEFT_TORCH) {
            vec3 tip;
            hands_torch_tip(&g->hands, tip);
            tip[1] += 0.15f;
            float f = 0.85f + 0.15f * sinf(t * 17.0f) * sinf(t * 7.3f);
            renderer_add_light(r, tip, (vec3){6.0f * f, 3.1f * f, 1.2f * f}, 10.0f);
        } else if (g->left == LEFT_LANTERN) {
            vec3 lp;
            hands_lantern_pos(&g->hands, lp);
            float f = 0.95f + 0.05f * sinf(t * 11.0f);
            renderer_add_light(r, lp, (vec3){5.0f * f, 3.6f * f, 2.0f * f}, 12.0f);
        }
        ItemId held = g->slots[g->selected].id;
        if (held != ITEM_NONE && ITEMS[held].kind == KIND_SPELL) {
            vec3 palm, c;
            hands_palm_pos(&g->hands, palm);
            glm_vec3_scale(ITEMS[held].glow, g->action == ACT_CAST ? 0.8f : 0.08f, c);
            renderer_add_light(r, palm, c, 4.0f);
        }
    }
    if (g->wisp)
        renderer_add_light(r, g->wisp_pos, (vec3){5.0f, 5.5f, 7.5f}, 14.0f);
    /* the angler's lamp: the deep glows around you */
    if (g->angler && g->underwater)
        renderer_add_light(r, g->head, (vec3){1.5f, 4.0f, 4.5f}, 16.0f);
    if (g->flash_t > 0.0f) {
        float f = g->flash_t / 0.4f;
        renderer_add_light(r, (vec3){g->flash_pos[0], g->flash_pos[1] + 1, g->flash_pos[2]},
                           (vec3){60.0f * f, 30.0f * f, 10.0f * f}, 22.0f);
    }
    for (int i = 0; i < MAX_BOLTS; i++) {
        Bolt *b = &g->bolts[i];
        if (b->used)
            renderer_add_light(r, b->pos, b->kind == BOLT_FIREBALL ? (vec3){12, 5, 1} : (vec3){5, 1.5f, 8}, 8.0f);
    }

    int order[MAX_LIGHTS_SRC + MAX_TOMBS], n = 0;
    float dist[MAX_LIGHTS_SRC + MAX_TOMBS];
    for (int i = 0; i < MAX_LIGHTS_SRC; i++) {
        if (!g->lights[i].used || !g->lights[i].lit)
            continue;
        dist[n] = glm_vec3_distance(g->lights[i].pos, g->eye);
        order[n++] = i;
    }
    for (int i = 0; i < g->tomb_count; i++) {
        if (g->tombs[i].opened)
            continue;
        dist[n] = glm_vec3_distance(g->tombs[i].pos, g->eye);
        order[n++] = MAX_LIGHTS_SRC + i;
    }
    /* nearest first (insertion sort over a few dozen) */
    for (int a = 1; a < n; a++)
        for (int b = a; b > 0 && dist[b] < dist[b - 1]; b--) {
            float td = dist[b]; dist[b] = dist[b - 1]; dist[b - 1] = td;
            int to = order[b]; order[b] = order[b - 1]; order[b - 1] = to;
        }

    for (int k = 0; k < n; k++) {
        if (order[k] >= MAX_LIGHTS_SRC) {
            Tomb *tb = &g->tombs[order[k] - MAX_LIGHTS_SRC];
            vec3 p = { tb->pos[0], tb->pos[1] + 1.3f, tb->pos[2] };
            float pulse = tb->glow * (0.75f + 0.25f * sinf(t * 2.5f));
            renderer_add_light(r, p, (vec3){0.3f * pulse, 1.2f * pulse, 3.0f * pulse}, 6.0f);
            continue;
        }
        LightSource *l = &g->lights[order[k]];
        float f = 0.82f + 0.18f * sinf(t * 13.0f + l->seed) * sinf(t * 7.7f + l->seed * 2.0f);
        vec3 p, c;
        glm_vec3_copy(l->pos, p);
        float radius;
        switch (l->kind) {
        case LIGHT_TORCH:
            glm_vec3_muladds(l->normal, 0.35f, p);
            p[1] += 0.35f;
            glm_vec3_copy((vec3){8.0f, 4.2f, 1.6f}, c);
            radius = 11.0f;
            break;
        case LIGHT_LANTERN:
            p[1] += 0.3f;
            glm_vec3_copy((vec3){4.0f, 2.8f, 1.4f}, c);
            radius = 8.0f;
            f = 0.95f + 0.05f * f;
            break;
        case LIGHT_CHANDELIER:
            glm_vec3_copy((vec3){14.0f, 9.0f, 4.5f}, c);
            radius = 16.0f;
            f = 0.97f + 0.03f * f;
            break;
        case LIGHT_LAMP:
            glm_vec3_copy((vec3){5.5f, 3.8f, 1.8f}, c);
            radius = 11.0f;
            f = 0.97f + 0.03f * f;
            break;
        case LIGHT_PAPER: {
            /* soft pinks, teals and golds, each lantern its own */
            static const float tints[3][3] = { { 4.0f, 1.6f, 2.0f }, { 1.2f, 3.4f, 3.6f }, { 4.4f, 3.0f, 1.0f } };
            int k = (int)l->seed % 3;
            glm_vec3_copy((vec3){tints[k][0], tints[k][1], tints[k][2]}, c);
            radius = 8.0f;
            f = 0.9f + 0.1f * f;
            break;
        }
        case LIGHT_BEACON:
            glm_vec3_copy((vec3){40.0f, 34.0f, 20.0f}, c);
            radius = 30.0f;
            f = 1.0f;
            break;
        case LIGHT_CORAL: {
            float pulse = 0.8f + 0.2f * sinf(t * 1.3f + l->seed);
            glm_vec3_copy(((int)l->seed % 2) ? (vec3){1.0f, 4.5f, 4.2f} : (vec3){4.2f, 1.5f, 3.0f}, c);
            radius = 10.0f;
            f = pulse;
            break;
        }
        case LIGHT_FORGE:
            glm_vec3_copy((vec3){12.0f, 4.5f, 1.0f}, c);
            radius = 10.0f;
            break;
        case LIGHT_EMBER:
            glm_vec3_copy((vec3){24.0f, 7.0f, 1.2f}, c);
            radius = 26.0f;
            f = 0.9f + 0.1f * f;
            break;
        default:
            p[1] += 0.6f;
            glm_vec3_copy((vec3){16.0f, 7.5f, 2.5f}, c);
            radius = 15.0f;
            break;
        }
        glm_vec3_scale(c, f, c);
        renderer_add_light(r, p, c, radius);
    }
}

/* everything that stands in the world, drawn with either the lit or the shadow program */
static void draw_things(Game *g, GLuint prog, mat4 vp, bool depth)
{
    DrawParams dp = { .depth_only = depth };
    mat4 ident = GLM_MAT4_IDENTITY_INIT;
    model_draw(&g->level.geometry, NULL, prog, vp, ident, &dp);

    world_draw(&g->world, prog, vp, g->eye, depth);

    /* skip what the camera (or, for shadows, the sun) can't see; big things carry further */
    Frustum fr;
    frustum_from(vp, &fr);
    float far = depth ? 55.0f : 100.0f;
    #define VISIBLE(c, r) (glm_vec3_distance(c, g->eye) < far + (r) * (depth ? 1.0f : 6.0f) && frustum_sphere(&fr, c, r))
    for (int i = 0; i < g->prop_count; i++) {
        Prop *p = &g->props[i];
        if ((depth && p->no_shadow) || !VISIBLE(p->center, p->radius) || (p->burns && p->growth < 0.02f))
            continue;
        model_draw(&g->prop_models[p->model], NULL, prog, vp, p->matrix, &dp);
    }
    if (g->gnome_t > 0.0f) {
        mat4 gx;
        glm_translate_make(gx, g->gnome_pos);
        glm_rotate_y(gx, g->time * 0.3f, gx);
        mat4 fit;
        model_fit(&ITEMS[ITEM_GNOME].model, 0.6f, fit);
        glm_translate(gx, (vec3){0, 0.3f, 0});
        glm_mat4_mul(gx, fit, gx);
        model_draw(&ITEMS[ITEM_GNOME].model, NULL, prog, vp, gx, &dp);
    }

    for (int i = 0; i < g->door_count; i++) {
        Door *d = &g->doors[i];
        vec3 dc = { d->pos[0], d->pos[1] + 2.0f, d->pos[2] };
        if (!VISIBLE(dc, 2.6f))
            continue;
        mat4 xf;
        transform_matrix(&d->xf, xf);
        model_draw(&g->door_model, &d->pose, prog, vp, xf, &dp);
    }
    for (int i = 0; i < g->chest_count; i++) {
        Chest *ch = &g->chests[i];
        if (!VISIBLE(ch->pos, 1.0f))
            continue;
        mat4 xf;
        transform_matrix(&ch->xf, xf);
        DrawParams cp = dp;
        if (ch->gold && !depth)
            glm_vec4_copy((vec4){0.12f, 0.08f, 0.0f, 0.0f}, cp.tint);
        model_draw(&g->chest_model, &ch->pose, prog, vp, xf, &cp);
    }

    /* wall torches (tilted out from the wall) and lanterns standing about */
    const Model *torch = items_torch_model();
    const Model *lantern = &ITEMS[ITEM_LANTERN].model;
    for (int i = 0; i < MAX_LIGHTS_SRC; i++) {
        LightSource *l = &g->lights[i];
        if (!l->used || !VISIBLE(l->pos, 0.8f))
            continue;
        mat4 xf;
        if (l->kind == LIGHT_TORCH && !l->empty) {
            /* tilted out from the wall, pivoting 0.3 m up its handle */
            Transform t = l->xf;
            t.pitch += 0.4f;
            transform_matrix(&t, xf);
            glm_translate(xf, (vec3){0, -0.3f, 0});
            model_draw(torch, NULL, prog, vp, xf, &dp);
        } else if (l->kind == LIGHT_LANTERN && !l->empty) {
            /* the item model, 0.5 m tall, standing on pos */
            float s = 0.5f / fmaxf(lantern->max[1] - lantern->min[1], 1e-3f);
            transform_matrix(&l->xf, xf);
            glm_scale_uni(xf, s);
            glm_translate(xf, (vec3){0, -lantern->min[1], 0});
            DrawParams lp = dp;
            if (!depth && l->lit)
                glm_vec4_copy((vec4){0.4f, 0.22f, 0.06f, 0.0f}, lp.tint);
            model_draw(lantern, NULL, prog, vp, xf, &lp);
        }
    }

    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *p = &g->pickups[i];
        if (!p->used || !ITEMS[p->item].loaded || !VISIBLE(p->pos, 1.0f))
            continue;
        const ItemDef *it = &ITEMS[p->item];
        mat4 xf, fit;
        float size = p->on_altar ? 0.26f : fmaxf(it->hold_size, 0.3f);
        Transform t = p->xf;
        t.pos[1] += size * 0.5f;
        if (p->on_altar) {
            t.pos[1] += sinf(g->time * 1.5f) * 0.03f + 0.2f;    /* bobs and turns on the altar */
            t.yaw += g->time * 0.6f;
        } else if (it->kind == KIND_WEAPON) {
            t.roll += GLM_PI_2f;        /* weapons lie on the floor */
        }
        transform_matrix(&t, xf);
        model_fit(&it->model, size, fit);
        glm_mat4_mul(xf, fit, xf);
        model_draw(&it->model, NULL, prog, vp, xf, &dp);
    }

    for (int i = 0; i < MAX_GRENADES; i++) {
        Grenade *gr = &g->grenades[i];
        if (!gr->used)
            continue;
        mat4 xf;
        transform_matrix(&gr->xf, xf);
        glm_rotate(xf, gr->spin, (vec3){1, 0, 0.3f});
        glm_mat4_mul(xf, ITEMS[ITEM_GRENADE].hold, xf);
        model_draw(&ITEMS[ITEM_GRENADE].model, NULL, prog, vp, xf, &dp);
    }

    #undef VISIBLE
    creatures_draw(g, prog, vp, depth);
    npcs_draw(g, prog, vp, depth);
    magic_draw(g, prog, vp, depth);
    spells_draw(g, prog, vp, depth);
    animals_draw(g, prog, vp, depth);
    things_draw(g, prog, vp, depth);
    boats_draw(g, prog, vp, depth);
    /* your own body: its shadow always, the body itself when you're looking from outside */
    if (g->cam.mode == CAM_FIRST_PERSON && (depth || g->third_person) && !g->cam.flying)
        avatar_draw(g, prog, vp, depth);
    if (!depth)
        fishing_draw(g, prog, vp);
}

void game_render_world(Game *g)
{
    Renderer *r = &g->r;
    Environment base = base_env(g->night), env;
    weather_apply(&g->weather, &base, &env);
    volcano_environment(g, &env);
    bool court = level_area(&g->level, g->eye) == AREA_UNDERSEA;
    float surface, depth;
    bool under = sea_at(g, g->eye[0], g->eye[2], &surface, &depth) && g->eye[1] < surface - 0.02f;
    renderer_set_sea(r, SEA_Y, under || court ? 1.0f : 0.0f, g->volcano.ash, court ? 1.0f : 0.0f);
    renderer_set_environment(r, &env, g->time);
    renderer_set_camera(r, g->view, g->proj, g->eye);
    renderer_set_camera_sky(r, level_sky_exposure(&g->level, g->eye));
    gather_lights(g);

    /* sun shadows follow you around */
    renderer_begin_shadow(r, g->pos, 45.0f);
    draw_things(g, g->shadow_prog, r->light_view_proj, true);

    renderer_begin_scene(r);
    /* near things first, the big terrain after: hidden ground is then skipped by the
     * depth test instead of being shaded and painted over */
    draw_things(g, g->model_prog, r->frame.view_proj, false);
    if (level_area(&g->level, g->eye) != AREA_CRYPT)     /* from the crypt you can't see the ground above */
        terrain_draw(&g->terrain, g->terrain_prog, r->frame.view_proj);
    world_draw_lava(&g->world, r->frame.view_proj);

    /* water last, see-through over everything solid: the flooded hall, then the sea */
    mat4 ident = GLM_MAT4_IDENTITY_INIT;
    DrawParams water = { .no_material = true };
    model_draw(&g->level.water, NULL, g->water_prog, r->frame.view_proj, ident, &water);
    ocean_draw(&g->ocean, g->eye, r->frame.view_proj, g->time, storm_level(g));
    slimes_draw(g, r->frame.view_proj);
    weather_draw_twister(&g->weather, r->frame.view_proj);
}

/* ---------- HUD ---------- */

static bool to_screen(Game *g, vec3 p, float *sx, float *sy)
{
    vec4 clip;
    glm_mat4_mulv(g->r.frame.view_proj, (vec4){p[0], p[1], p[2], 1.0f}, clip);
    if (clip[3] <= 0.1f)
        return false;
    *sx = (clip[0] / clip[3] * 0.5f + 0.5f) * g->width;
    *sy = (0.5f - clip[1] / clip[3] * 0.5f) * g->height;
    return true;
}

static void faded(const float c[4], float a, float out[4])
{
    out[0] = c[0];
    out[1] = c[1];
    out[2] = c[2];
    out[3] = c[3] * glm_clamp(a, 0, 1);
}

static void draw_slot(Game *g, int i, float x, float y, float size, bool selected, bool held)
{
    float bg[4] = { 0.06f, 0.05f, 0.04f, 0.7f };
    ui_rect(x, y, size, size, bg);
    ui_frame(x, y, size, size, selected ? 3.0f : 1.0f, selected ? COL_GOLD : COL_EDGE);
    if (held) {
        float glow[4] = { 1.0f, 0.8f, 0.3f, 0.25f };
        ui_rect(x, y, size, size, glow);
    }
    Slot *s = &g->slots[i];
    if (s->id != ITEM_NONE && ITEMS[s->id].icon)
        ui_image(ITEMS[s->id].icon, x + 4, y + 4, size - 8, size - 8, NULL, true);
    if (s->count > 1) {
        char n[16];
        snprintf(n, sizeof n, "%d", s->count);
        ui_text_shadow(FONT_SMALL, x + size - 6 - ui_text_width(FONT_SMALL, n), y + size - 24, COL_TEXT, n);
    }
    if (s->id != ITEM_NONE && g->cooldowns[s->id] > 0.0f && ITEMS[s->id].cooldown > 1.5f) {
        float f = g->cooldowns[s->id] / ITEMS[s->id].cooldown;
        float shade[4] = { 0, 0, 0, 0.6f };
        ui_rect(x, y + size * (1.0f - f), size, size * f, shade);
    }
    if (s->id != ITEM_NONE && ITEMS[s->id].kind == KIND_SPELL && g->mana < ITEMS[s->id].mana) {
        float blue[4] = { 0.05f, 0.1f, 0.4f, 0.45f };
        ui_rect(x, y, size, size, blue);
    }
}

static void bar(float x, float y, float w, float h, float frac, const float fill[4], const char *label)
{
    float back[4] = { 0.05f, 0.03f, 0.02f, 0.75f };
    ui_rect(x, y, w, h, back);
    ui_rect(x + 3, y + 3, (w - 6) * glm_clamp(frac, 0, 1), h - 6, fill);
    ui_frame(x, y, w, h, 1, COL_EDGE);
    ui_text_shadow(FONT_SMALL, x + 10, y + (h - 24) * 0.5f, COL_TEXT, label);
}

static void draw_hud(Game *g)
{
    float w = (float)g->width, h = (float)g->height;
    float col[4];

    /* title card */
    if (g->title_t < 8.0f) {
        float a = fminf(g->title_t / 1.0f, 1.0f) * fminf((8.0f - g->title_t) / 2.0f, 1.0f);
        const char *title = "The Ruins of Bezan";
        const char *sub = "Explore the halls. Wake the tombs. Visit the king. Then find the sea.";
        faded(COL_GOLD, a, col);
        ui_text_shadow(FONT_LARGE, (w - ui_text_width(FONT_LARGE, title)) * 0.5f, h * 0.22f, col, title);
        faded(COL_TEXT, a, col);
        ui_text_shadow(FONT_MEDIUM, (w - ui_text_width(FONT_MEDIUM, sub)) * 0.5f, h * 0.22f + 70, col, sub);
        const char *hint = g->pad ? "Back: controls    Start: pause" : "H: controls    Esc: pause";
        ui_text_shadow(FONT_SMALL, (w - ui_text_width(FONT_SMALL, hint)) * 0.5f, h * 0.22f + 112, col, hint);
    }

    /* creature health bars */
    for (int i = 0; i < MAX_CREATURES; i++) {
        Creature *c = &g->creatures[i];
        if (!c->used || c->state == ST_DEAD || c->state == ST_DORMANT || (!g->specs_on && c->bar_t <= 0.0f))
            continue;
        vec3 top = { c->pos[0], c->pos[1] + creature_height(c) + 0.45f, c->pos[2] };
        float sx, sy;
        if (glm_vec3_distance(top, g->eye) > 40.0f || !to_screen(g, top, &sx, &sy))
            continue;
        float bw = 70, bh = 7;
        float back[4] = { 0, 0, 0, 0.6f }, fill[4] = { 0.85f, 0.15f, 0.1f, 0.9f };
        ui_rect(sx - bw / 2 - 1, sy - 1, bw + 2, bh + 2, back);
        ui_rect(sx - bw / 2, sy, bw * c->hp / c->max_hp, bh, fill);
        if (g->specs_on) {
            char label[48];
            snprintf(label, sizeof label, "%s  %d/%d", creature_name(c), (int)c->hp, (int)c->max_hp);
            ui_text_shadow(FONT_SMALL, sx - ui_text_width(FONT_SMALL, label) / 2, sy - 24, COL_TEXT, label);
        }
    }

    if (g->cam.mode == CAM_FIRST_PERSON && !g->dead && !g->inv_open && g->menu == MENU_NONE) {
        float cx = w * 0.5f, cy = h * 0.5f;
        float ch[4] = { 1, 1, 1, g->prompt[0] ? 0.95f : 0.6f };
        ui_rect(cx - 1, cy - 9, 2, 6, ch);
        ui_rect(cx - 1, cy + 3, 2, 6, ch);
        ui_rect(cx - 9, cy - 1, 6, 2, ch);
        ui_rect(cx + 3, cy - 1, 6, 2, ch);
        if (g->prompt[0])
            ui_text_shadow(FONT_MEDIUM, cx - ui_text_width(FONT_MEDIUM, g->prompt) / 2, cy + 28, COL_TEXT, g->prompt);
    }

    /* subtitles for whoever is talking */
    if (g->subtitle_t > 0.0f && g->subtitle[0]) {
        float a = fminf(g->subtitle_t, 1.0f);
        float sw = fminf(w - 80, 900.0f);
        char line[220];
        snprintf(line, sizeof line, "%s", g->subtitle);
        /* wrap into two lines if it's long */
        if (ui_text_width(FONT_MEDIUM, line) > sw) {
            size_t len = strlen(line);
            for (size_t k = len / 2; k < len; k++)
                if (line[k] == ' ') {
                    line[k] = '\n';
                    break;
                }
        }
        float lines = strchr(line, '\n') ? 2.0f : 1.0f;
        float bh = 44 + lines * 32;
        float by = h - 190 - bh;
        float bg[4] = { 0, 0, 0, 0.5f * a };
        ui_rect((w - sw) * 0.5f - 20, by, sw + 40, bh, bg);
        faded(COL_GOLD, a, col);
        ui_text_shadow(FONT_SMALL, (w - ui_text_width(FONT_SMALL, g->subtitle_name)) * 0.5f, by + 6, col, g->subtitle_name);
        faded(COL_TEXT, a, col);
        ui_text_shadow(FONT_MEDIUM, (w - ui_text_width(FONT_MEDIUM, line)) * 0.5f, by + 34, col, line);
    }

    /* hotbar */
    float size = 58, gap = 6;
    float hx = (w - HOTBAR * size - (HOTBAR - 1) * gap) * 0.5f, hy = h - size - 22;
    for (int i = 0; i < HOTBAR; i++) {
        float x = hx + i * (size + gap);
        draw_slot(g, i, x, hy, size, i == g->selected, false);
        char n[4];
        snprintf(n, sizeof n, "%d", i + 1);
        float dim[4] = { 1, 1, 1, 0.5f };
        ui_text(FONT_SMALL, x + 5, hy + 2, dim, n);
    }
    ItemId held = held_item(g);
    const char *held_name = held != ITEM_NONE ? ITEMS[held].name : "Bare fists";
    ui_text_shadow(FONT_SMALL, (w - ui_text_width(FONT_SMALL, held_name)) * 0.5f, hy - 26, COL_TEXT, held_name);

    /* health and mana */
    float bx = 28, by = h - 86;
    char label[48];
    static const float RED[4] = { 0.7f, 0.1f, 0.08f, 0.95f }, BLUE[4] = { 0.15f, 0.3f, 0.85f, 0.95f };
    snprintf(label, sizeof label, "Health  %d / %d", (int)ceilf(g->hp), (int)g->max_hp);
    bar(bx, by, 280, 26, g->hp / g->max_hp, RED, label);
    snprintf(label, sizeof label, "Mana  %d / %d", (int)g->mana, (int)g->max_mana);
    bar(bx, by + 30, 280, 22, g->mana / g->max_mana, BLUE, label);

    /* swimming: how long before you tire, and your breath while under */
    if (g->swimming || g->stamina < g->max_stamina - 0.5f) {
        static const float GREEN[4] = { 0.25f, 0.7f, 0.35f, 0.95f };
        snprintf(label, sizeof label, g->stamina > 0.0f ? "Stamina" : "Exhausted!");
        bar(bx + 290, by + 30, 200, 22, g->stamina / g->max_stamina, GREEN, label);
    }
    if (g->underwater || g->breath < g->max_breath - 0.2f) {
        /* a row of bubbles, bursting one by one */
        int bubbles = (int)ceilf(g->breath / g->max_breath * 10.0f);
        for (int k = 0; k < 10; k++) {
            float bubble[4] = { 0.7f, 0.9f, 1.0f, k < bubbles ? 0.9f : 0.15f };
            ui_rect(bx + 290 + k * 20, by + 2, 14, 14, bubble);
            ui_frame(bx + 290 + k * 20, by + 2, 14, 14, 1, COL_EDGE);
        }
    }
    /* shells, the coast's money */
    {
        static const float SHELL_COL[SHELL_KINDS][4] = {
            { 0.95f, 0.93f, 0.88f, 1 }, { 1.0f, 0.62f, 0.72f, 1 }, { 0.5f, 0.75f, 1.0f, 1 }, { 1.0f, 0.8f, 0.3f, 1 } };
        float sx = bx, sy = by - 58;
        for (int k = 0; k < SHELL_KINDS; k++) {
            char n[16];
            snprintf(n, sizeof n, "%d", g->shells[k]);
            ui_rect(sx, sy + 5, 12, 12, SHELL_COL[k]);
            sx += 16 + ui_text_shadow(FONT_SMALL, sx + 16, sy, COL_TEXT, n) + 12;
        }
        char worth[40];
        snprintf(worth, sizeof worth, "shells  (worth %d)", shell_total(g));
        ui_text_shadow(FONT_SMALL, sx, sy, COL_EDGE, worth);
    }

    /* active effects */
    char fx[200] = "";
    if (g->slow_t > 0) snprintf(fx + strlen(fx), sizeof fx - strlen(fx), "Time slowed %.0fs   ", ceilf(g->slow_t));
    if (g->boombox_t > 0) snprintf(fx + strlen(fx), sizeof fx - strlen(fx), "Music %.0fs   ", ceilf(g->boombox_t));
    if (g->wisp) snprintf(fx + strlen(fx), sizeof fx - strlen(fx), "Wisp %.0fs   ", ceilf(g->wisp_t));
    if (g->mask_on) strncat(fx, "Gas mask   ", sizeof fx - strlen(fx) - 1);
    if (g->specs_on) strncat(fx, "Spectacles   ", sizeof fx - strlen(fx) - 1);
    if (g->crouching) strncat(fx, "Crouching   ", sizeof fx - strlen(fx) - 1);
    if (g->gills_t > 0) snprintf(fx + strlen(fx), sizeof fx - strlen(fx), "Gills %.0fs   ", ceilf(g->gills_t));
    if (g->shadow_t > 0) snprintf(fx + strlen(fx), sizeof fx - strlen(fx), "Unseen %.0fs   ", ceilf(g->shadow_t));
    if (g->toxic > 0.2f) strncat(fx, "Choking   ", sizeof fx - strlen(fx) - 1);
    if (g->boat_in >= 0) strncat(fx, "Rowing   ", sizeof fx - strlen(fx) - 1);
    if (g->cam.flying) strncat(fx, "Flying   ", sizeof fx - strlen(fx) - 1);
    if (fx[0])
        ui_text_shadow(FONT_SMALL, bx, by - 28, COL_FUN, fx);

    /* messages, newest at the bottom */
    float my = by - 90;
    for (int i = 0; i < MAX_MESSAGES; i++) {
        Message *m = &g->messages[i];
        if (!m->text[0] || m->t > 8.0f)
            continue;
        faded(m->color, (8.0f - m->t) / 2.0f, col);
        ui_text_shadow(FONT_SMALL, bx, my, col, m->text);
        my -= 26;
    }

    /* progress and weather, top right */
    int opened = 0, tombs = 0;
    for (int i = 0; i < g->chest_count; i++)
        opened += g->chests[i].opened;
    for (int i = 0; i < g->tomb_count; i++)
        tombs += g->tombs[i].opened;
    char prog[128];
    int relics = 0;
    for (int i = 0; i < 12; i++)
        relics += g->quest.relics[i];
    snprintf(prog, sizeof prog, "Chests %d / %d     Tombs %d / %d     Relics %d / 12     Creatures %d     %s",
             opened, g->chest_count, tombs, g->tomb_count, relics, g->creatures_left, weather_name(g->weather.type));
    ui_text_shadow(FONT_SMALL, w - 28 - ui_text_width(FONT_SMALL, prog), 20, COL_TEXT, prog);
    volcano_hud(g);

    /* the compass points at the nearest unopened chest or tomb */
    if (held == ITEM_COMPASS) {
        float bd = 1e9f;
        vec3 best = { 0, 0, 0 };
        for (int i = 0; i < g->chest_count; i++)
            if (!g->chests[i].opened && glm_vec3_distance(g->chests[i].pos, g->pos) < bd) {
                bd = glm_vec3_distance(g->chests[i].pos, g->pos);
                glm_vec3_copy(g->chests[i].pos, best);
            }
        for (int i = 0; i < g->tomb_count; i++)
            if (!g->tombs[i].opened && glm_vec3_distance(g->tombs[i].pos, g->pos) < bd) {
                bd = glm_vec3_distance(g->tombs[i].pos, g->pos);
                glm_vec3_copy(g->tombs[i].pos, best);
            }
        char text[120];
        if (bd > 1e8f) {
            snprintf(text, sizeof text, "The needle spins lazily. No treasure remains.");
        } else {
            vec3 d;
            glm_vec3_sub(best, g->pos, d);
            float a = wrap_angle(atan2f(-d[0], -d[2]) - g->cam.fp_yaw);
            static const char *dirs[8] = { "ahead", "ahead and to the left", "to your left", "behind you, left",
                                           "behind you", "behind you, right", "to your right", "ahead and to the right" };
            int sector = (int)floorf((a + GLM_PIf / 8) / (GLM_PIf / 4));
            sector = (sector % 8 + 8) % 8;
            const char *level_hint = d[1] < -2.0f ? ", below you" : d[1] > 2.0f ? ", above you" : "";
            snprintf(text, sizeof text, "Treasure lies %s%s, %d paces off", dirs[sector], level_hint, (int)bd);
        }
        ui_text_shadow(FONT_MEDIUM, (w - ui_text_width(FONT_MEDIUM, text)) * 0.5f, 60, COL_GOLD, text);
    }

    fishing_hud(g);

    /* inventory */
    if (g->inv_open) {
        float ox, oy;
        inv_origin(g, &ox, &oy);
        float pw = INV_COLS * INV_CELL + (INV_COLS - 1) * INV_GAP;
        float ph = 3 * INV_CELL + 2 * INV_GAP + 18 + 200;
        ui_rect(ox - 26, oy - 70, pw + 52, ph, COL_PANEL);
        ui_frame(ox - 26, oy - 70, pw + 52, ph, 2, COL_EDGE);
        ui_text(FONT_MEDIUM, ox, oy - 58, COL_GOLD, "Satchel");
        float dim[4] = { 0.95f, 0.9f, 0.8f, 0.6f };
        const char *help = "Click to move items. Right-click to hold one. Hover + 1-9 to set the hotbar.";
        ui_text(FONT_SMALL, ox + pw - ui_text_width(FONT_SMALL, help), oy - 50, dim, help);
        for (int i = 0; i < INV_SLOTS; i++) {
            float x, y;
            inv_cell(g, i, &x, &y);
            draw_slot(g, i, x, y, INV_CELL, i == g->selected, i == g->inv_held);
        }
        int hover = inv_hover(g);
        int show = hover >= 0 && g->slots[hover].id ? hover : g->inv_held;
        float ty = oy + 3 * INV_CELL + 2 * INV_GAP + 40;
        if (show >= 0 && g->slots[show].id) {
            const ItemDef *it = &ITEMS[g->slots[show].id];
            ui_text(FONT_MEDIUM, ox, ty, COL_GOLD, it->name);
            ui_text(FONT_SMALL, ox, ty + 38, COL_TEXT, it->desc);
            char stats[96] = "";
            if (it->kind == KIND_WEAPON)
                snprintf(stats, sizeof stats, "Damage %d    Reach %.1f m    Swing %.2f s",
                         (int)it->damage, it->range, it->cooldown);
            else if (it->kind == KIND_SPELL)
                snprintf(stats, sizeof stats, "Mana %d    Recharge %.1f s", (int)it->mana, it->cooldown);
            else if (it->kind == KIND_ARMOR)
                snprintf(stats, sizeof stats, "Armour %d%%    %s", (int)(it->armor * 100.0f + 0.5f),
                         g->equip[it->slot] == g->slots[show].id ? "Worn (click it in hand to take off)" : "Click it in hand to wear");
            else if (it->kind == KIND_ROD)
                snprintf(stats, sizeof stats, "Rod tier %d of 4", it->tier);
            if (stats[0])
                ui_text(FONT_SMALL, ox + pw - ui_text_width(FONT_SMALL, stats), ty + 6, COL_FUN, stats);
        }
        /* what you're wearing */
        static const char *slot_names[EQ_COUNT] = { "Head", "Body", "Back", "Feet" };
        float ex = ox + pw + 40, ey = oy - 20;
        ui_text(FONT_SMALL, ex, ey - 30, COL_GOLD, "Wearing");
        for (int k = 0; k < EQ_COUNT; k++) {
            float y = ey + k * (INV_CELL + 22);
            float bg[4] = { 0.06f, 0.05f, 0.04f, 0.7f };
            ui_rect(ex, y, INV_CELL, INV_CELL, bg);
            ui_frame(ex, y, INV_CELL, INV_CELL, 1, COL_EDGE);
            if (g->equip[k] != ITEM_NONE && ITEMS[g->equip[k]].icon)
                ui_image(ITEMS[g->equip[k]].icon, ex + 4, y + 4, INV_CELL - 8, INV_CELL - 8, NULL, true);
            float dim2[4] = { 0.95f, 0.9f, 0.8f, 0.6f };
            ui_text(FONT_SMALL, ex, y + INV_CELL, dim2, slot_names[k]);
        }
        char armor[48];
        snprintf(armor, sizeof armor, "Armour %d%%", (int)(armor_total(g) * 100.0f + 0.5f));
        ui_text(FONT_SMALL, ex, ey + EQ_COUNT * (INV_CELL + 22), COL_FUN, armor);
        if (g->inv_held >= 0 && ITEMS[g->slots[g->inv_held].id].icon)
            ui_image(ITEMS[g->slots[g->inv_held].id].icon, g->mouse_x - 32, g->mouse_y - 32, 64, 64, NULL, true);
    }
    if (g->journal_open)
        journal_draw(g);
    if (g->shop_npc >= 0)
        shop_draw(g);
    if (g->reading_t > 0.0f)
        reading_draw(g);

    if (g->dead) {
        float a = fminf(g->dead_t / 1.5f, 1.0f);
        const char *t1 = "You have fallen", *t2 = g->pad ? "Press A to rise again" : "Press R to rise again";
        faded(COL_RED, a, col);
        ui_text_shadow(FONT_LARGE, (w - ui_text_width(FONT_LARGE, t1)) * 0.5f, h * 0.38f, col, t1);
        faded(COL_TEXT, a, col);
        ui_text_shadow(FONT_MEDIUM, (w - ui_text_width(FONT_MEDIUM, t2)) * 0.5f, h * 0.38f + 72, col, t2);
    }

    /* gas mask: the world through two round lenses */
    if (g->mask_on && g->cam.mode == CAM_FIRST_PERSON && !g->third_person) {
        float edge[4] = { 0, 0, 0, 0.55f };
        ui_rect(0, 0, w, h * 0.06f, edge);
        ui_rect(0, h * 0.94f, w, h * 0.06f, edge);
        ui_rect(w * 0.5f - 6, 0, 12, h, edge);
    }

    if (g->menu != MENU_NONE)
        menu_draw(g);
}

void game_render_overlay(Game *g)
{
    particles_draw(&g->ps);
    weather_draw(&g->weather, g->eye);

    if (g->cam.mode == CAM_FIRST_PERSON && !g->dead && !g->third_person && !g->cam.flying) {
        renderer_begin_viewmodel(&g->r);
        mat4 vp;
        glm_mat4_mul(g->hand_proj, g->view, vp);
        HandInput in = {0};
        ItemId held = g->slots[g->selected].id;
        in.held = held == ITEM_TORCH || held == ITEM_LANTERN || held == ITEM_SHIELD ? ITEM_NONE : held;
        in.left = g->left;
        in.body_armor = g->equip[EQ_BODY];
        in.fishing = g->fish.state;
        hands_draw(&g->hands, &in, g->model_prog, vp);
    }

    float surface, depth;
    bool under = sea_at(g, g->eye[0], g->eye[2], &surface, &depth) && g->eye[1] < surface - 0.02f;
    PostEffects fx = {
        .damage = g->hurt_t / 0.45f,
        .slowmo = fminf(g->slow_t, 1.0f),
        .dead = g->dead ? fminf(g->dead_t / 2.0f, 1.0f) : 0.0f,
        .underwater = under ? 1.0f : 0.0f,
        .ash = g->volcano.ash,
        .toxic = g->toxic,
    };
    renderer_finish(&g->r, &fx);

    ui_begin(g->width, g->height);
    draw_hud(g);
    ui_end();
}
