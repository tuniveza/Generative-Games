#include "game.h"
#include "world.h"

#include <stdint.h>

/* Brinewick: a fishing village on a low chalk hill west of the ruins, above its own
 * little cove. Pastel plaster (chalky blue, coral, butter yellow, whitewash) under
 * thatch and clay tiles; shell-mosaic lanes; strings of paper lanterns in pink, teal and
 * gold criss-crossing the square; a great blossom tree in the middle of it all; shell
 * wind chimes; washing on the line. Its bell tower stands empty: the Drowned Court took
 * the bell. On the cliff's edge, three shrine lanterns show the tide the way home. */

#define VX VILLAGE_X
#define VZ VILLAGE_Z
#define VY VILLAGE_Y

void coast_palm(WorldBuild *wb, ChunkId c, vec3 base, float height, int seed);

static float rnd(int i, int salt)
{
    uint32_t h = (uint32_t)i * 2654435761u ^ (uint32_t)salt * 69069u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return (h & 0xffffff) / (float)0xffffff;
}

static Spawn *thing(WorldBuild *wb, int kind, int index, vec3 pos)
{
    Spawn *s = level_add_spawn(wb->lv, SPAWN_THING, pos, 0.0f);
    s->variant = kind;
    s->index = index;
    return s;
}

static void lamp(WorldBuild *wb, int kind, vec3 pos)
{
    level_add_spawn(wb->lv, SPAWN_LAMP, pos, 0.0f)->variant = kind;
}

/* the great blossom tree: a twisting trunk, forking branches, clouds of purple bloom */
static void blossom_tree(WorldBuild *wb, vec3 base, float size, int seed)
{
    /* trunk and branches, grown from the base up */
    typedef struct { vec3 p; float yaw, tilt, r, len; int depth; } Twig;
    Twig stack[64];
    int n = 0;
    stack[n++] = (Twig){ { base[0], base[1] - 0.3f, base[2] }, 0.0f, 0.08f, 0.5f * size, 3.6f * size, 0 };
    int k = 0;
    while (n > 0 && k < 90) {
        Twig t = stack[--n];
        k++;
        mat4 xf;
        glm_translate_make(xf, t.p);
        glm_rotate_y(xf, t.yaw, xf);
        glm_rotate_z(xf, t.tilt, xf);
        wb_xcylinder(wb, WC_VILLAGE, WM_TIMBER, xf, t.r, t.r * 0.7f, t.len, 9);
        vec3 up = { xf[1][0], xf[1][1], xf[1][2] }, end;
        glm_vec3_copy(t.p, end);
        glm_vec3_muladds(up, t.len, end);
        if (t.depth >= 3) {
            /* a cloud of blossom at every tip */
            for (int b = 0; b < 3; b++) {
                vec3 c = { end[0] + (rnd(k, 10 + b) - 0.5f) * 1.6f * size, end[1] + (rnd(k, 20 + b) - 0.2f) * 1.0f * size,
                           end[2] + (rnd(k, 30 + b) - 0.5f) * 1.6f * size };
                float r = (0.9f + rnd(k, 40 + b) * 0.8f) * size;
                wb_ball(wb, WC_VILLAGE, b % 2 ? WM_BLOSSOM_PALE : WM_BLOSSOM, c, (vec3){r, r * 0.75f, r}, 10);
            }
            continue;
        }
        int forks = t.depth == 0 ? 3 : 2;
        for (int f = 0; f < forks && n < 62; f++) {
            float yaw = t.yaw + (f * 6.2832f / forks) + rnd(seed * 100 + k, f) * 1.2f;
            float tilt = 0.45f + rnd(seed * 100 + k, 5 + f) * 0.45f;
            stack[n++] = (Twig){ { end[0], end[1], end[2] }, yaw, tilt, t.r * 0.62f, t.len * 0.72f, t.depth + 1 };
        }
    }
    wb_solid(wb, (vec3){base[0] - 0.5f * size, base[1], base[2] - 0.5f * size}, (vec3){base[0] + 0.5f * size, base[1] + 3.0f * size, base[2] + 0.5f * size});
    /* a ring bench around its foot */
    wb_cylinder(wb, WC_VILLAGE, WM_OLD_WOOD, (vec3){base[0], base[1], base[2]}, 1.6f * size, 1.6f * size, 0.45f, 16);
}

/* a string of paper lanterns between two points, sagging, in alternating colors */
static void lantern_string(WorldBuild *wb, vec3 a, vec3 b, int seed)
{
    float len = glm_vec3_distance(a, b);
    int count = (int)(len / 1.3f);
    static const int papers[3] = { WM_PAPER_PINK, WM_PAPER_CYAN, WM_PAPER_GOLD };
    for (int i = 1; i < count; i++) {
        float t = (float)i / count;
        vec3 p;
        glm_vec3_lerp(a, b, t, p);
        p[1] -= sinf(t * GLM_PIf) * len * 0.08f;
        int m = papers[(i + seed) % 3];
        wb_ball(wb, WC_VILLAGE, m, (vec3){p[0], p[1] - 0.22f, p[2]}, (vec3){0.16f, 0.21f, 0.16f}, 8);
        wb_box(wb, WC_VILLAGE, WM_TIMBER, (vec3){p[0] - 0.05f, p[1] - 0.02f, p[2] - 0.05f}, (vec3){p[0] + 0.05f, p[1] + 0.02f, p[2] + 0.05f});
        if (i % 3 == 1) {
            Spawn *s = level_add_spawn(wb->lv, SPAWN_LAMP, (vec3){p[0], p[1] - 0.25f, p[2]}, 0.0f);
            s->variant = LIGHT_PAPER;
        }
    }
    /* the cord */
    for (int i = 0; i < count; i++) {
        float t0 = (float)i / count, t1 = (float)(i + 1) / count;
        vec3 p0, p1;
        glm_vec3_lerp(a, b, t0, p0);
        glm_vec3_lerp(a, b, t1, p1);
        p0[1] -= sinf(t0 * GLM_PIf) * len * 0.08f;
        p1[1] -= sinf(t1 * GLM_PIf) * len * 0.08f;
        vec3 mid, d;
        glm_vec3_center(p0, p1, mid);
        glm_vec3_sub(p1, p0, d);
        mat4 xf;
        glm_translate_make(xf, mid);
        glm_rotate_y(xf, atan2f(d[0], d[2]), xf);
        glm_rotate_x(xf, -atan2f(d[1], sqrtf(d[0] * d[0] + d[2] * d[2])), xf);
        wb_xbox(wb, WC_VILLAGE, WM_ROPE, xf, (vec3){0.01f, 0.01f, glm_vec3_norm(d) * 0.5f});
    }
}

static void pole(WorldBuild *wb, vec3 base, float h)
{
    wb_cylinder(wb, WC_VILLAGE, WM_OLD_WOOD, base, 0.09f, 0.07f, h, 8);
    wb_solid(wb, (vec3){base[0] - 0.1f, base[1], base[2] - 0.1f}, (vec3){base[0] + 0.1f, base[1] + h, base[2] + 0.1f});
}

/* shell wind chimes hanging from an eave */
static void chimes(WorldBuild *wb, vec3 at)
{
    wb_box(wb, WC_VILLAGE, WM_TIMBER, (vec3){at[0] - 0.2f, at[1], at[2] - 0.02f}, (vec3){at[0] + 0.2f, at[1] + 0.03f, at[2] + 0.02f});
    for (int k = 0; k < 5; k++) {
        float x = at[0] - 0.16f + k * 0.08f, len = 0.3f + (k % 3) * 0.12f;
        wb_box(wb, WC_VILLAGE, WM_ROPE, (vec3){x - 0.004f, at[1] - len, at[2] - 0.004f}, (vec3){x + 0.004f, at[1], at[2] + 0.004f});
        wb_ball(wb, WC_VILLAGE, k % 2 ? WM_PAPER_PINK : WM_PLASTER_WHITE, (vec3){x, at[1] - len - 0.03f, at[2]}, (vec3){0.03f, 0.04f, 0.012f}, 6);
    }
}

/* washing drying on a line between two poles */
static void washing(WorldBuild *wb, vec3 a, vec3 b)
{
    pole(wb, (vec3){a[0], a[1] - 2.2f, a[2]}, 2.3f);
    pole(wb, (vec3){b[0], b[1] - 2.2f, b[2]}, 2.3f);
    static const int cloth[5] = { WM_CLOTH_RED, WM_CLOTH_WHITE, WM_CLOTH_BLUE, WM_CLOTH_YELLOW, WM_PAINT_TEAL };
    for (int i = 0; i < 5; i++) {
        vec3 p;
        glm_vec3_lerp(a, b, (i + 0.7f) / 6.0f, p);
        wb_box(wb, WC_VILLAGE, cloth[i], (vec3){p[0] - 0.25f, p[1] - 0.7f, p[2] - 0.01f}, (vec3){p[0] + 0.25f, p[1], p[2] + 0.01f});
    }
}

void village_build(WorldBuild *wb)
{
    Level *lv = wb->lv;

    /* the square: shell mosaic in a cobbled ring, the tree in the middle */
    wb_cylinder(wb, WC_VILLAGE, WM_COBBLE, (vec3){VX, VY - 0.25f, VZ}, 13.0f, 13.0f, 0.27f, 40);
    wb_cylinder(wb, WC_VILLAGE, WM_SHELL_FLOOR, (vec3){VX, VY - 0.2f, VZ}, 10.0f, 10.0f, 0.23f, 40);
    blossom_tree(wb, (vec3){VX, VY, VZ}, 1.25f, 1);

    /* lanes out from the square to the doors */
    static const float lanes[6][4] = {
        { VX, VZ - 12.0f, VX, VZ - 22.0f }, { VX + 12.0f, VZ, VX + 20.0f, VZ }, { VX - 12.0f, VZ, VX - 20.0f, VZ },
        { VX, VZ + 12.0f, VX, VZ + 46.0f }, { VX - 9.0f, VZ + 9.0f, VX - 14.0f, VZ + 18.0f }, { VX + 9.0f, VZ + 9.0f, VX + 20.0f, VZ + 18.0f },
    };
    for (int i = 0; i < 6; i++) {
        float x0 = fminf(lanes[i][0], lanes[i][2]) - 1.2f, x1 = fmaxf(lanes[i][0], lanes[i][2]) + 1.2f;
        float z0 = fminf(lanes[i][1], lanes[i][3]) - 1.2f, z1 = fmaxf(lanes[i][1], lanes[i][3]) + 1.2f;
        wb_box(wb, WC_VILLAGE, WM_SHELL_FLOOR, (vec3){x0, VY - 0.2f, z0}, (vec3){x1, VY + 0.03f, z1});
    }

    /* houses, doors to the square */
    House elder = { .pos = { VX, VY, VZ - 27.0f }, .w = 11.0f, .d = 8.0f, .height = 3.4f, .door_side = 0,
                    .wall = WM_PLASTER_BLUE, .roof = WM_THATCH, .floor = WM_PLANKS, .trim = WM_TIMBER, .thatch = true, .chunk = WC_VILLAGE };
    wb_house(wb, &elder);
    House tavern = { .pos = { TAVERN_X, VY, TAVERN_Z }, .w = TAVERN_W, .d = TAVERN_D, .height = 3.8f, .door_side = 3,
                     .wall = WM_PLASTER_YELLOW, .roof = WM_ROOF_TILES, .floor = WM_PLANKS, .trim = WM_TIMBER, .chunk = WC_VILLAGE };
    wb_house(wb, &tavern);
    House forge = { .pos = { VX - 25.0f, VY, VZ }, .w = 8.0f, .d = 9.0f, .height = 3.2f, .door_side = 1,
                    .wall = WM_PLASTER_RED, .roof = WM_ROOF_TILES, .floor = WM_STONE, .trim = WM_TIMBER, .open_front = true, .chunk = WC_VILLAGE };
    wb_house(wb, &forge);
    static const struct { float x, z, w, d; int door, wall, roof; } cottages[] = {
        { VX - 18.0f, VZ + 22.0f, 7.0f, 6.0f, 1, WM_PLASTER_WHITE, WM_THATCH },      /* Pell's */
        { VX + 22.0f, VZ + 22.0f, 7.0f, 6.0f, 3, WM_PLASTER_RED, WM_THATCH },
        { VX + 18.0f, VZ - 20.0f, 8.0f, 6.0f, 3, WM_PLASTER_WHITE, WM_ROOF_TILES },
        { VX - 20.0f, VZ - 20.0f, 7.0f, 7.0f, 1, WM_PLASTER_YELLOW, WM_THATCH },
        { VX - 36.0f, VZ - 8.0f, 6.0f, 7.0f, 1, WM_PLASTER_BLUE, WM_THATCH },
        { VX + 38.0f, VZ - 6.0f, 6.0f, 6.0f, 3, WM_PLASTER_BLUE, WM_ROOF_TILES },
        { VX - 34.0f, VZ + 24.0f, 7.0f, 6.0f, 0, WM_PLASTER_RED, WM_ROOF_TILES },
    };
    for (size_t i = 0; i < sizeof cottages / sizeof cottages[0]; i++) {
        House h = { .pos = { cottages[i].x, VY, cottages[i].z }, .w = cottages[i].w, .d = cottages[i].d, .height = 3.0f,
                    .door_side = cottages[i].door, .wall = cottages[i].wall, .roof = cottages[i].roof, .floor = WM_PLANKS,
                    .trim = i % 2 ? WM_PAINT_TEAL : WM_PAINT_CORAL, .thatch = cottages[i].roof == WM_THATCH, .chunk = WC_VILLAGE };
        wb_house(wb, &h);
        lamp(wb, LIGHT_LAMP, (vec3){cottages[i].x, VY + 2.4f, cottages[i].z});
        if (i < 4)
            chimes(wb, (vec3){cottages[i].x + (cottages[i].door == 1 ? cottages[i].w * 0.5f + 0.3f : -cottages[i].w * 0.5f - 0.3f),
                               VY + 2.9f, cottages[i].z + 1.2f});
    }

    /* inside the tavern: the bar, barrels, tables, and Tamsin behind the bar */
    float tx = TAVERN_X, tz = TAVERN_Z;
    wb_block(wb, WC_VILLAGE, WM_OLD_WOOD, (vec3){tx + 3.2f, VY, tz - 3.0f}, (vec3){tx + 3.9f, VY + 1.1f, tz + 3.0f});
    wb_box(wb, WC_VILLAGE, WM_TIMBER, (vec3){tx + 3.1f, VY + 1.1f, tz - 3.1f}, (vec3){tx + 4.0f, VY + 1.18f, tz + 3.1f});
    level_add_spawn(lv, SPAWN_VILLAGER, (vec3){tx + 4.8f, VY, tz}, -GLM_PI_2f)->index = NPC_TAVERN;
    wb_prop(wb, PM_WINE_BARREL, (vec3){tx + 5.2f, VY, tz - 3.0f}, 0.3f, 1.0f);
    wb_prop(wb, PM_WINE_BARREL, (vec3){tx + 5.2f, VY, tz + 3.0f}, 1.1f, 1.0f);
    for (int k = 0; k < 3; k++) {
        vec3 tp = { tx - 2.5f, VY, tz - 2.5f + k * 2.6f };
        wb_prop(wb, PM_PTABLE, tp, GLM_PI_2f, 0.5f);
        wb_solid(wb, (vec3){tp[0] - 0.4f, VY, tp[2] - 0.7f}, (vec3){tp[0] + 0.4f, VY + 0.9f, tp[2] + 0.7f});
        wb_prop(wb, PM_PCHAIR, (vec3){tp[0] - 1.0f, VY, tp[2]}, GLM_PI_2f, 1.0f);
        wb_prop(wb, PM_PCHAIR, (vec3){tp[0] + 1.0f, VY, tp[2]}, -GLM_PI_2f, 1.0f);
    }
    wb_prop(wb, PM_CHESS, (vec3){tx - 2.5f, VY + 0.48f, tz}, 0.3f, 1.0f);
    wb_prop(wb, PM_LANTERN_CHANDELIER, (vec3){tx, VY + 2.0f, tz}, 0.0f, 1.2f);
    lamp(wb, LIGHT_LAMP, (vec3){tx, VY + 2.6f, tz});
    thing(wb, THING_RELIC, 7, (vec3){tx + 3.5f, VY + 1.25f, tz + 2.0f});
    /* two chimneys */
    wb_box(wb, WC_VILLAGE, WM_STONE, (vec3){tx - 4.5f, VY + 3.8f, tz - 3.8f}, (vec3){tx - 3.5f, VY + 8.0f, tz - 2.8f});
    wb_box(wb, WC_VILLAGE, WM_STONE, (vec3){tx + 3.5f, VY + 3.8f, tz + 2.8f}, (vec3){tx + 4.5f, VY + 7.5f, tz + 3.8f});

    /* the forge: an anvil, a glowing hearth, Bram */
    float fx = VX - 25.0f, fz = VZ;
    wb_block(wb, WC_VILLAGE, WM_STONE, (vec3){fx - 3.5f, VY, fz - 3.0f}, (vec3){fx - 1.5f, VY + 1.2f, fz + 0.5f});
    wb_box(wb, WC_VILLAGE, WM_EMBER, (vec3){fx - 3.2f, VY + 1.2f, fz - 2.7f}, (vec3){fx - 1.8f, VY + 1.28f, fz + 0.2f});
    wb_box(wb, WC_VILLAGE, WM_STONE, (vec3){fx - 3.3f, VY + 1.3f, fz - 2.8f}, (vec3){fx - 1.7f, VY + 3.2f, fz - 2.2f});
    lamp(wb, LIGHT_FORGE, (vec3){fx - 2.5f, VY + 1.8f, fz - 1.2f});
    wb_block(wb, WC_VILLAGE, WM_IRON, (vec3){fx + 0.2f, VY, fz + 1.2f}, (vec3){fx + 0.8f, VY + 0.55f, fz + 1.8f});
    wb_block(wb, WC_VILLAGE, WM_IRON, (vec3){fx - 0.1f, VY + 0.55f, fz + 1.1f}, (vec3){fx + 1.1f, VY + 0.85f, fz + 1.9f});
    thing(wb, THING_ANVIL, 0, (vec3){fx + 0.5f, VY + 0.9f, fz + 1.5f});
    level_add_spawn(lv, SPAWN_VILLAGER, (vec3){fx + 1.2f, VY, fz - 1.0f}, GLM_PI_2f)->index = NPC_SMITH;
    wb_prop(wb, PM_BARREL, (vec3){fx - 3.0f, VY, fz + 2.8f}, 0.0f, 1.0f);

    /* Elder Maren waits outside her door; others about their business */
    level_add_spawn(lv, SPAWN_VILLAGER, (vec3){VX + 1.5f, VY, VZ - 21.5f}, 0.0f)->index = NPC_ELDER;
    level_add_spawn(lv, SPAWN_VILLAGER, (vec3){VX - 12.0f, VY, VZ + 16.0f}, 0.0f)->index = NPC_LAMPLIGHTER;
    level_add_spawn(lv, SPAWN_VILLAGER, (vec3){VX + 12.0f, VY, VZ + 30.0f}, 0.0f)->index = NPC_NETMENDER;
    thing(wb, THING_LORE, 6, (vec3){VX + 4.5f, VY + 1.1f, VZ - 10.5f});
    wb_block(wb, WC_VILLAGE, WM_STONE, (vec3){VX + 4.0f, VY, VZ - 11.0f}, (vec3){VX + 5.0f, VY + 1.6f, VZ - 10.7f});
    wb_box(wb, WC_VILLAGE, WM_RUNE, (vec3){VX + 4.15f, VY + 0.9f, VZ - 10.72f}, (vec3){VX + 4.85f, VY + 1.4f, VZ - 10.68f});

    /* the well */
    vec3 well = { VX + 7.0f, VY, VZ + 6.0f };
    wb_cylinder(wb, WC_VILLAGE, WM_STONE, well, 1.1f, 1.1f, 0.9f, 16);
    wb_solid(wb, (vec3){well[0] - 1.0f, VY, well[2] - 1.0f}, (vec3){well[0] + 1.0f, VY + 0.9f, well[2] + 1.0f});
    for (int s = -1; s <= 1; s += 2)
        wb_box(wb, WC_VILLAGE, WM_TIMBER, (vec3){well[0] + s * 0.9f - 0.08f, VY + 0.9f, well[2] - 0.08f}, (vec3){well[0] + s * 0.9f + 0.08f, VY + 2.4f, well[2] + 0.08f});
    wb_box(wb, WC_VILLAGE, WM_TIMBER, (vec3){well[0] - 1.0f, VY + 2.3f, well[2] - 0.08f}, (vec3){well[0] + 1.0f, VY + 2.45f, well[2] + 0.08f});
    wb_prop(wb, PM_BUCKET, (vec3){well[0] + 0.4f, VY + 0.9f, well[2] + 0.2f}, 0.0f, 0.45f);

    /* market stalls under striped awnings */
    for (int k = 0; k < 2; k++) {
        vec3 st = { VX + (k ? 7.0f : -8.0f), VY, VZ + (k ? -8.0f : 8.0f) };
        wb_block(wb, WC_VILLAGE, WM_OLD_WOOD, (vec3){st[0] - 1.5f, VY, st[2] - 0.6f}, (vec3){st[0] + 1.5f, VY + 0.9f, st[2] + 0.6f});
        for (int p = 0; p < 4; p++)
            wb_cylinder(wb, WC_VILLAGE, WM_OLD_WOOD, (vec3){st[0] + (p % 2 ? 1.4f : -1.4f), VY, st[2] + (p / 2 ? 0.7f : -0.7f)}, 0.05f, 0.05f, 2.3f, 6);
        for (int s = 0; s < 6; s++)
            wb_box(wb, WC_VILLAGE, s % 2 ? (k ? WM_PAINT_TEAL : WM_PAINT_CORAL) : WM_CLOTH_WHITE,
                   (vec3){st[0] - 1.6f + s * 0.534f, VY + 2.3f, st[2] - 0.9f}, (vec3){st[0] - 1.6f + (s + 1) * 0.534f, VY + 2.34f, st[2] + 0.9f});
        wb_prop(wb, k ? PM_WICKER : PM_TEASET, (vec3){st[0], VY + 0.9f, st[2]}, k * 1.2f, 1.0f);
    }

    /* the bell tower, its belfry empty */
    float bx = VX + 26.0f, bz = VZ - 30.0f;
    wb_block(wb, WC_VILLAGE, WM_STONE, (vec3){bx - 2.2f, VY - 1.0f, bz - 2.2f}, (vec3){bx + 2.2f, VY + 9.0f, bz + 2.2f});
    for (int c = 0; c < 4; c++) {
        float px = bx + (c % 2 ? 1.9f : -1.9f), pz = bz + (c / 2 ? 1.9f : -1.9f);
        wb_block(wb, WC_VILLAGE, WM_STONE, (vec3){px - 0.3f, VY + 9.0f, pz - 0.3f}, (vec3){px + 0.3f, VY + 12.0f, pz + 0.3f});
    }
    wb_box(wb, WC_VILLAGE, WM_STONE, (vec3){bx - 2.4f, VY + 12.0f, bz - 2.4f}, (vec3){bx + 2.4f, VY + 12.5f, bz + 2.4f});
    wb_cylinder(wb, WC_VILLAGE, WM_ROOF_TILES, (vec3){bx, VY + 12.5f, bz}, 3.0f, 0.1f, 4.0f, 4);
    wb_box(wb, WC_VILLAGE, WM_TIMBER, (vec3){bx - 2.0f, VY + 11.6f, bz - 0.12f}, (vec3){bx + 2.0f, VY + 11.8f, bz + 0.12f});
    thing(wb, THING_BELL_TOWER, 0, (vec3){bx, VY + 11.5f, bz});
    /* the bell rope hangs down to where you can reach it */
    wb_box(wb, WC_VILLAGE, WM_ROPE, (vec3){bx + 2.25f, VY + 1.0f, bz - 0.02f}, (vec3){bx + 2.3f, VY + 9.0f, bz + 0.02f});
    thing(wb, THING_BELL_TOWER, 1, (vec3){bx + 2.6f, VY + 1.4f, bz});

    /* the lantern shrine on the cliff's edge, looking out over the cove */
    float sz = VZ + 50.0f;
    wb_block(wb, WC_VILLAGE, WM_MARBLE, (vec3){VX - 1.0f, VY, sz - 0.6f}, (vec3){VX + 1.0f, VY + 0.9f, sz + 0.6f});
    wb_box(wb, WC_VILLAGE, WM_RUNE, (vec3){VX - 0.8f, VY + 0.91f, sz - 0.4f}, (vec3){VX + 0.8f, VY + 0.93f, sz + 0.4f});
    thing(wb, THING_LORE, 7, (vec3){VX, VY + 1.2f, sz});
    for (int k = 0; k < 3; k++) {
        float x = VX + (k - 1) * 5.0f, z = sz + (k == 1 ? 2.5f : 0.0f);
        wb_block(wb, WC_VILLAGE, WM_STONE, (vec3){x - 0.35f, VY, z - 0.35f}, (vec3){x + 0.35f, VY + 1.8f, z + 0.35f});
        wb_box(wb, WC_VILLAGE, WM_IRON, (vec3){x - 0.3f, VY + 1.8f, z - 0.3f}, (vec3){x + 0.3f, VY + 1.85f, z + 0.3f});
        for (int c = 0; c < 4; c++)
            wb_box(wb, WC_VILLAGE, WM_IRON, (vec3){x + (c % 2 ? 0.22f : -0.28f), VY + 1.85f, z + (c / 2 ? 0.22f : -0.28f)},
                   (vec3){x + (c % 2 ? 0.28f : -0.22f), VY + 2.45f, z + (c / 2 ? 0.28f : -0.22f)});
        wb_cylinder(wb, WC_VILLAGE, WM_IRON, (vec3){x, VY + 2.45f, z}, 0.4f, 0.05f, 0.35f, 4);
        thing(wb, THING_SHRINE_LANTERN, k, (vec3){x, VY + 2.1f, z});
    }

    /* paper lanterns criss-crossing the square, on poles */
    vec3 poles[8];
    for (int k = 0; k < 8; k++) {
        float a = k * GLM_PI_4f + 0.3f;
        glm_vec3_copy((vec3){VX + cosf(a) * 12.5f, VY, VZ + sinf(a) * 12.5f}, poles[k]);
        pole(wb, poles[k], 4.2f);
    }
    for (int k = 0; k < 8; k++) {
        vec3 a = { poles[k][0], VY + 4.1f, poles[k][2] }, b = { poles[(k + 3) % 8][0], VY + 4.1f, poles[(k + 3) % 8][2] };
        lantern_string(wb, a, b, k);
        vec3 c = { poles[(k + 1) % 8][0], VY + 4.1f, poles[(k + 1) % 8][2] };
        lantern_string(wb, a, c, k + 1);
    }
    washing(wb, (vec3){VX + 15.0f, VY + 2.2f, VZ + 34.0f}, (vec3){VX + 22.0f, VY + 2.2f, VZ + 34.0f});

    /* gardens: planters, benches, palms and smaller blossom trees */
    for (int k = 0; k < 8; k++) {
        float a = k * GLM_PI_4f;
        wb_prop(wb, PM_PLANTER, (vec3){VX + cosf(a) * 11.0f, VY, VZ + sinf(a) * 11.0f}, a + GLM_PI_2f, 1.0f);
    }
    wb_prop(wb, PM_BENCH, (vec3){VX - 5.0f, VY, VZ - 9.5f}, 0.0f, 1.0f);
    wb_prop(wb, PM_BENCH, (vec3){VX + 5.0f, VY, VZ + 9.5f}, GLM_PIf, 1.0f);
    wb_prop(wb, PM_SPINNING, (vec3){VX - 16.0f, VY, VZ + 25.5f}, 0.5f, 1.0f);
    blossom_tree(wb, (vec3){VX + 30.0f, VY, VZ + 10.0f}, 0.8f, 2);
    blossom_tree(wb, (vec3){VX - 40.0f, VY, VZ + 8.0f}, 0.75f, 3);
    blossom_tree(wb, (vec3){VX - 8.0f, VY, VZ - 40.0f}, 0.85f, 4);
    for (int k = 0; k < 7; k++) {
        float a = rnd(k, 70) * 6.2832f, r = VILLAGE_R * (0.75f + rnd(k, 71) * 0.2f);
        float x = VX + cosf(a) * r, z = VZ + sinf(a) * r;
        coast_palm(wb, WC_VILLAGE, (vec3){x, terrain_height(wb->terrain, x, z), z}, 7.0f + rnd(k, 72) * 3.0f, 100 + k);
    }
    for (int k = 0; k < 14; k++) {
        float a = rnd(k, 80) * 6.2832f, r = 14.0f + rnd(k, 81) * 40.0f;
        vec3 p = { VX + cosf(a) * r, VY, VZ + sinf(a) * r };
        Spawn *s = wb_prop(wb, k % 2 ? PM_FLOWERS : PM_FERN, p, a, 1.3f);
        (void)s;
    }

    /* the cove below: a jetty, a boat, drying racks, and Wren swimming */
    float cz = coast_z(VX) + 4.0f;
    float jy = QUAY_Y + 0.2f;
    wb_block(wb, WC_VILLAGE, WM_DECK, (vec3){VX - 1.4f, jy - 0.3f, cz - 8.0f}, (vec3){VX + 1.4f, jy, cz + 18.0f});
    for (float z = cz - 6.0f; z <= cz + 18.0f; z += 4.0f)
        for (int s = -1; s <= 1; s += 2)
            wb_cylinder(wb, WC_VILLAGE, WM_OLD_WOOD, (vec3){VX + s * 1.5f, -8.0f, z}, 0.16f, 0.16f, 8.0f + jy + 0.6f, 8);
    level_add_spawn(lv, SPAWN_BOAT, (vec3){VX + 3.4f, SEA_Y, cz + 10.0f}, 0.0f);
    thing(wb, THING_RELIC, 6, (vec3){VX - 1.0f, jy + 0.05f, cz + 17.0f});
    for (int k = 0; k < 3; k++) {
        float x = VX - 8.0f - k * 3.0f, z = cz - 6.0f;
        float y = terrain_height(wb->terrain, x, z);
        for (int s = -1; s <= 1; s += 2)
            wb_cylinder(wb, WC_VILLAGE, WM_OLD_WOOD, (vec3){x + s * 1.2f, y - 0.2f, z}, 0.05f, 0.05f, 1.9f, 6);
        wb_box(wb, WC_VILLAGE, WM_OLD_WOOD, (vec3){x - 1.3f, y + 1.6f, z - 0.03f}, (vec3){x + 1.3f, y + 1.66f, z + 0.03f});
        for (int f = 0; f < 4; f++)
            wb_box(wb, WC_VILLAGE, WM_PAINT_TEAL, (vec3){x - 0.9f + f * 0.55f, y + 1.1f, z - 0.02f}, (vec3){x - 0.75f + f * 0.55f, y + 1.6f, z + 0.02f});
    }
    level_add_spawn(lv, SPAWN_VILLAGER, (vec3){VX - 10.0f, SEA_Y, cz + 22.0f}, 0.0f)->index = NPC_CHILD;
    level_add_spawn(lv, SPAWN_ANIMAL, (vec3){VX + 10.0f, SEA_Y - 2.0f, cz + 26.0f}, 0.0f)->variant = SPECIES_FISH;
    level_add_spawn(lv, SPAWN_ANIMAL, (vec3){VX - 4.0f, 5.0f, cz + 4.0f}, 0.0f)->variant = SPECIES_GULL;
    /* buried treasure in the cove and on the main beach, for the metal detector */
    thing(wb, THING_TREASURE, 0, (vec3){VX + 12.0f, terrain_height(wb->terrain, VX + 12.0f, cz + 2.0f), cz + 2.0f});
    thing(wb, THING_TREASURE, 1, (vec3){-95.0f, terrain_height(wb->terrain, -95.0f, 128.0f), 128.0f});
    thing(wb, THING_TREASURE, 2, (vec3){35.0f, terrain_height(wb->terrain, 35.0f, 131.0f), 131.0f});
    thing(wb, THING_TREASURE, 3, (vec3){-180.0f, terrain_height(wb->terrain, -180.0f, 118.0f), 118.0f});
    thing(wb, THING_TREASURE, 4, (vec3){-40.0f, terrain_height(wb->terrain, -40.0f, 138.0f), 138.0f});
    /* the road from the ruins' garden gate */
    for (float x = -52.0f; x > VX + VILLAGE_R - 6.0f; x -= 3.0f) {
        float z = -4.0f + (x + 52.0f) / (VX + VILLAGE_R - 6.0f + 52.0f) * (VZ + 4.0f);
        float y = terrain_height(wb->terrain, x, z);
        wb_box(wb, WC_VILLAGE, WM_SHELL_FLOOR, (vec3){x - 1.6f, y - 0.15f, z - 1.3f}, (vec3){x + 1.6f, y + 0.04f, z + 1.3f});
    }
}
