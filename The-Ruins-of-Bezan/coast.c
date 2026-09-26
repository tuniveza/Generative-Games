#include "game.h"
#include "world.h"

#include <stdint.h>

/* The coast south of the ruins:
 *   the promenade   a raised walk along the top of the beach, with lamps, benches,
 *                   bronze sea creatures on plinths, stone tablets telling the coast's
 *                   history, and the Hall of Tides where found relics are kept
 *   the beach       white sand into chalky turquoise water; the Curious Clam (a shop
 *                   that takes shells), palms, parasols, and the walruses' rocks
 *   the marina      a stone quay, three piers with rowing boats, a moored ship, the
 *                   harbourmaster's office, and a breakwater out to the lighthouse */

static float rnd(int i, int salt)
{
    uint32_t h = (uint32_t)i * 2654435761u ^ (uint32_t)salt * 40503u;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return (h & 0xffffff) / (float)0xffffff;
}

static Spawn *thing(WorldBuild *wb, int kind, int index, vec3 pos, float yaw)
{
    Spawn *s = level_add_spawn(wb->lv, SPAWN_THING, pos, yaw);
    s->variant = kind;
    s->index = index;
    return s;
}

static void lamp(WorldBuild *wb, int kind, vec3 pos)
{
    level_add_spawn(wb->lv, SPAWN_LAMP, pos, 0.0f)->variant = kind;
}

/* ---------- small pieces ---------- */

/* a palm: a curving, tapering trunk and a crown of drooping fronds */
void coast_palm(WorldBuild *wb, ChunkId c, vec3 base, float height, int seed)
{
    float yaw = rnd(seed, 1) * 6.28f;
    vec3 p;
    glm_vec3_copy(base, p);
    p[1] -= 0.3f;
    float lean = 0.0f, seg = height / 8.0f;
    mat4 xf;
    for (int i = 0; i < 8; i++) {
        lean += 0.035f + i * 0.01f + rnd(seed, 2) * 0.02f;
        glm_translate_make(xf, p);
        glm_rotate_y(xf, yaw, xf);
        glm_rotate_x(xf, lean, xf);
        float r0 = 0.24f - i * 0.013f;
        wb_xcylinder(wb, c, WM_PALM_BARK, xf, r0, r0 - 0.013f, seg * 1.04f, 10);
        vec3 up = { xf[1][0], xf[1][1], xf[1][2] };
        glm_vec3_muladds(up, seg, p);
    }
    /* coconuts */
    for (int k = 0; k < 3; k++) {
        float a = k * 2.1f + yaw;
        wb_ball(wb, c, WM_TIMBER, (vec3){p[0] + cosf(a) * 0.22f, p[1] - 0.2f, p[2] + sinf(a) * 0.22f}, (vec3){0.13f, 0.14f, 0.13f}, 8);
    }
    /* fronds: a spine of segments that rise and then droop, with leaflets on both sides */
    int fronds = 9;
    for (int f = 0; f < fronds; f++) {
        float a = f * 6.2832f / fronds + rnd(seed, 10 + f) * 0.4f;
        float pitch = 0.55f + rnd(seed, 30 + f) * 0.3f;
        vec3 q;
        glm_vec3_copy(p, q);
        for (int k = 0; k < 6; k++) {
            float len = 0.62f;
            vec3 dir = { sinf(a) * cosf(pitch), sinf(pitch), cosf(a) * cosf(pitch) };
            vec3 mid;
            glm_vec3_copy(q, mid);
            glm_vec3_muladds(dir, len * 0.5f, mid);
            float w = 0.42f * (1.0f - k / 7.0f);
            for (int side = -1; side <= 1; side += 2) {
                glm_translate_make(xf, mid);
                glm_rotate_y(xf, a, xf);
                glm_rotate_x(xf, -pitch, xf);
                glm_rotate_z(xf, side * 0.35f, xf);
                glm_translate(xf, (vec3){side * w * 0.5f, 0, 0});
                wb_xbox(wb, c, WM_LEAVES, xf, (vec3){w * 0.5f, 0.012f, len * 0.52f});
            }
            glm_vec3_muladds(dir, len, q);
            pitch -= 0.32f;
        }
    }
    wb_solid(wb, (vec3){base[0] - 0.25f, base[1] - 0.5f, base[2] - 0.25f}, (vec3){base[0] + 0.25f, base[1] + 2.0f, base[2] + 0.25f});
}

/* a beach parasol with a striped look (two alternating colors of cloth) */
static void parasol(WorldBuild *wb, vec3 base, int mat, float tilt, int seed)
{
    mat4 xf;
    glm_translate_make(xf, base);
    glm_rotate_z(xf, tilt, xf);
    wb_xcylinder(wb, WC_COAST, WM_CLOTH_WHITE, xf, 0.035f, 0.03f, 2.3f, 8);
    glm_translate(xf, (vec3){0, 2.05f, 0});
    for (int k = 0; k < 8; k++) {
        mat4 w;
        glm_mat4_copy(xf, w);
        glm_rotate_y(w, k * GLM_PI_4f + rnd(seed, 3), w);
        glm_rotate_x(w, 0.32f, w);
        glm_translate(w, (vec3){0, 0, 0.62f});
        wb_xbox(wb, WC_COAST, k % 2 ? mat : WM_CLOTH_WHITE, w, (vec3){0.26f, 0.015f, 0.64f});
    }
    /* a towel on the sand beside it */
    glm_translate_make(xf, (vec3){base[0] + 0.9f, base[1] + 0.02f, base[2] + 0.4f});
    glm_rotate_y(xf, rnd(seed, 5) * 1.0f, xf);
    wb_xbox(wb, WC_COAST, mat, xf, (vec3){0.45f, 0.01f, 0.9f});
}

/* an iron lamp post with a lantern on a curled arm */
static void lamp_post(WorldBuild *wb, ChunkId c, vec3 base, float facing)
{
    wb_cylinder(wb, c, WM_IRON, base, 0.09f, 0.06f, 3.3f, 10);
    wb_cylinder(wb, c, WM_IRON, base, 0.16f, 0.12f, 0.35f, 10);
    mat4 xf;
    vec3 head = { base[0] + sinf(facing) * 0.45f, base[1] + 3.15f, base[2] + cosf(facing) * 0.45f };
    glm_translate_make(xf, (vec3){base[0] + sinf(facing) * 0.22f, base[1] + 3.3f, base[2] + cosf(facing) * 0.22f});
    glm_rotate_y(xf, facing, xf);
    wb_xbox(wb, c, WM_IRON, xf, (vec3){0.03f, 0.03f, 0.25f});
    wb_box(wb, c, WM_LAMP_GLOW, (vec3){head[0] - 0.12f, head[1] - 0.32f, head[2] - 0.12f}, (vec3){head[0] + 0.12f, head[1], head[2] + 0.12f});
    wb_box(wb, c, WM_IRON, (vec3){head[0] - 0.16f, head[1], head[2] - 0.16f}, (vec3){head[0] + 0.16f, head[1] + 0.06f, head[2] + 0.16f});
    wb_solid(wb, (vec3){base[0] - 0.12f, base[1], base[2] - 0.12f}, (vec3){base[0] + 0.12f, base[1] + 3.2f, base[2] + 0.12f});
    lamp(wb, LIGHT_LAMP, (vec3){head[0], head[1] - 0.2f, head[2]});
}

/* a rounded shore rock (drawn, and solid enough to stand on) */
static void rock(WorldBuild *wb, vec3 c, vec3 r, bool solid)
{
    wb_ball(wb, WC_COAST, WM_SHORE_ROCK, c, r, 12);
    if (solid)
        wb_solid(wb, (vec3){c[0] - r[0] * 0.75f, c[1] - r[1], c[2] - r[2] * 0.75f},
                 (vec3){c[0] + r[0] * 0.75f, c[1] + r[1] * 0.8f, c[2] + r[2] * 0.75f});
}

/* a stone tablet with a carved history of the coast */
static void stele(WorldBuild *wb, ChunkId c, vec3 base, float yaw, int index)
{
    mat4 xf;
    glm_translate_make(xf, (vec3){base[0], base[1] + 0.8f, base[2]});
    glm_rotate_y(xf, yaw, xf);
    wb_xbox(wb, c, WM_SEA_BRICK, xf, (vec3){0.5f, 0.8f, 0.12f});
    glm_translate(xf, (vec3){0, 0.25f, 0.125f});
    wb_xbox(wb, c, WM_RUNE, xf, (vec3){0.32f, 0.3f, 0.005f});
    wb_solid(wb, (vec3){base[0] - 0.4f, base[1], base[2] - 0.4f}, (vec3){base[0] + 0.4f, base[1] + 1.6f, base[2] + 0.4f});
    thing(wb, THING_LORE, index, (vec3){base[0], base[1] + 1.1f, base[2]}, yaw);
}

/* a plinth with something bronze on it */
static void statue(WorldBuild *wb, int model, vec3 base, float yaw, float scale)
{
    wb_block(wb, WC_COAST, WM_MARBLE, (vec3){base[0] - 1.3f, base[1], base[2] - 1.3f}, (vec3){base[0] + 1.3f, base[1] + 0.3f, base[2] + 1.3f});
    wb_block(wb, WC_COAST, WM_SEA_BRICK, (vec3){base[0] - 1.0f, base[1] + 0.3f, base[2] - 1.0f}, (vec3){base[0] + 1.0f, base[1] + 1.4f, base[2] + 1.0f});
    wb_box(wb, WC_COAST, WM_MARBLE, (vec3){base[0] - 1.1f, base[1] + 1.4f, base[2] - 1.1f}, (vec3){base[0] + 1.1f, base[1] + 1.55f, base[2] + 1.1f});
    wb_prop(wb, model, (vec3){base[0], base[1] + 1.55f, base[2]}, yaw, scale);
}

/* ---------- the promenade ---------- */

static void promenade(WorldBuild *wb)
{
    float x0 = PROM_X0, x1 = PROM_X1, z0 = PROM_Z0, z1 = PROM_Z1, y = PROM_Y;
    /* the walk itself, and steps up from the grass on the land side */
    wb_block(wb, WC_COAST, WM_SEA_TILES, (vec3){x0, -2.5f, z0}, (vec3){x1, y, z1});
    wb_block(wb, WC_COAST, WM_SEA_BRICK, (vec3){x0, -2.5f, z0 - 1.0f}, (vec3){x1, y - 0.33f, z0});
    wb_block(wb, WC_COAST, WM_SEA_BRICK, (vec3){x0, -2.5f, z0 - 2.0f}, (vec3){x1, y - 0.66f, z0 - 1.0f});

    /* the sea wall's face and a balustrade, with stairs down to the sand */
    for (float x = x0; x < x1 - 0.01f; x += 22.0f) {
        float xe = fminf(x + 22.0f, x1);
        float gap0 = x + 9.5f, gap1 = x + 12.5f;
        bool stairs = xe - x > 14.0f;
        float spans[2][2] = { { x, stairs ? gap0 : xe }, { stairs ? gap1 : xe, xe } };
        for (int k = 0; k < 2; k++) {
            if (spans[k][1] <= spans[k][0] + 0.01f)
                continue;
            wb_block(wb, WC_COAST, WM_SEA_BRICK, (vec3){spans[k][0], y, z1 - 0.35f}, (vec3){spans[k][1], y + 0.85f, z1});
            wb_box(wb, WC_COAST, WM_MARBLE, (vec3){spans[k][0] - 0.05f, y + 0.85f, z1 - 0.42f}, (vec3){spans[k][1] + 0.05f, y + 0.97f, z1 + 0.07f});
        }
        if (stairs) {
            /* down to the beach, step by step, until the sand */
            for (int s = 0; s < 8; s++) {
                float top = y - 0.27f * (s + 1);
                float sz0 = z1 + s * 0.55f, sz1 = sz0 + 0.55f;
                float ground = terrain_height(wb->terrain, (gap0 + gap1) * 0.5f, sz1);
                if (top < ground - 0.1f)
                    break;
                wb_block(wb, WC_COAST, WM_SEA_BRICK, (vec3){gap0, -2.5f, sz0}, (vec3){gap1, top, sz1});
            }
        }
        /* a lamp at every post, and a bench looking out to sea between them */
        lamp_post(wb, WC_COAST, (vec3){x + 0.5f, y, z1 - 0.9f}, GLM_PIf);
        if (xe - x > 14.0f) {
            wb_prop(wb, PM_BENCH, (vec3){x + 5.0f, y, z0 + 1.2f}, 0.0f, 1.0f);
            wb_solid(wb, (vec3){x + 4.2f, y, z0 + 0.6f}, (vec3){x + 5.8f, y + 0.45f, z0 + 1.8f});
            wb_prop(wb, PM_BENCH, (vec3){x + 17.0f, y, z0 + 1.2f}, 0.0f, 1.0f);
            wb_solid(wb, (vec3){x + 16.2f, y, z0 + 0.6f}, (vec3){x + 17.8f, y + 0.45f, z0 + 1.8f});
            wb_prop(wb, PM_FLOWERS, (vec3){x + 11.0f, y, z0 + 0.8f}, rnd((int)x, 4) * 6.0f, 1.4f);
        }
    }

    /* bunting between the lamps: little flags in three colors */
    for (float x = x0 + 0.5f; x < x1 - 22.0f; x += 22.0f) {
        for (int k = 0; k < 20; k++) {
            float f = (k + 0.5f) / 20.0f;
            float sag = sinf(f * GLM_PIf) * 0.7f;
            vec3 p = { x + f * 22.0f, y + 3.45f - sag, z1 - 0.9f };
            static const int cols[3] = { WM_CLOTH_RED, WM_CLOTH_YELLOW, WM_CLOTH_BLUE };
            mat4 xf;
            glm_translate_make(xf, p);
            glm_rotate_z(xf, GLM_PI_4f, xf);
            wb_xbox(wb, WC_COAST, cols[k % 3], xf, (vec3){0.16f, 0.16f, 0.01f});
        }
    }

    /* bronze sea creatures on their plinths: the whale, the shark and the ray */
    statue(wb, PM_WHALE, (vec3){-62.0f, y, 104.0f}, 0.4f, 2.2f);
    statue(wb, PM_SHARK, (vec3){-12.0f, y, 104.0f}, -0.6f, 3.2f);
    statue(wb, PM_RAY, (vec3){38.0f, y, 104.0f}, 0.2f, 2.6f);

    /* the coast's history, carved on tablets along the walk */
    static const float lore_x[6] = { -84.0f, -38.0f, 6.0f, 24.0f, 58.0f, 70.0f };
    for (int i = 0; i < 6; i++)
        stele(wb, WC_COAST, (vec3){lore_x[i], y, z0 + 0.8f}, 0.0f, i);

    /* the old harbour cannon, still pointed out to sea */
    wb_prop(wb, PM_CANNON, (vec3){70.0f, y, 105.5f}, 0.0f, 1.0f);
    wb_solid(wb, (vec3){69.2f, y, 104.5f}, (vec3){70.8f, y + 0.8f, 107.0f});

    /* the Hall of Tides: a little museum where found relics go on show */
    House hall = { .pos = { -88.0f, y, 91.0f }, .w = 14.0f, .d = 10.0f, .height = 4.2f, .door_side = 0,
                   .wall = WM_PLASTER_WHITE, .roof = WM_ROOF_TILES, .floor = WM_MARBLE, .trim = WM_SEA_BRICK,
                   .chunk = WC_COAST };
    wb_house(wb, &hall);
    /* columns either side of its door */
    for (int s = -1; s <= 1; s += 2) {
        wb_cylinder(wb, WC_COAST, WM_MARBLE, (vec3){-88.0f + s * 1.6f, y, 96.9f}, 0.25f, 0.22f, 4.2f, 16);
        wb_solid(wb, (vec3){-88.0f + s * 1.6f - 0.25f, y, 96.65f}, (vec3){-88.0f + s * 1.6f + 0.25f, y + 4.2f, 97.15f});
    }
    /* twelve pedestals around the walls, waiting for relics */
    for (int i = 0; i < 12; i++) {
        float px, pz;
        if (i < 5) { px = -93.5f + i * 2.75f; pz = 87.2f; }
        else if (i < 8) { px = -94.0f; pz = 88.8f + (i - 5) * 2.2f; }
        else if (i < 11) { px = -82.0f; pz = 88.8f + (i - 8) * 2.2f; }
        else { px = -88.0f; pz = 90.5f; }
        wb_block(wb, WC_COAST, WM_MARBLE, (vec3){px - 0.35f, y, pz - 0.35f}, (vec3){px + 0.35f, y + 1.0f, pz + 0.35f});
        thing(wb, THING_PEDESTAL, i, (vec3){px, y + 1.0f, pz}, 0.0f);
    }
    lamp(wb, LIGHT_LAMP, (vec3){-88.0f, y + 3.6f, 91.0f});
    level_add_spawn(wb->lv, SPAWN_VILLAGER, (vec3){-86.0f, y, 92.5f}, 0.0f)->index = NPC_CURATOR;
}

/* ---------- the beach ---------- */

static void beach_shop(WorldBuild *wb)
{
    /* the Curious Clam: a thatched hut on stilts, its counter open to the promenade */
    float cx = -22.0f, cz = 124.0f, fy = -0.3f;
    for (int i = 0; i < 6; i++) {
        float px = cx + (i % 3 - 1) * 3.2f, pz = cz + (i / 3 ? 2.2f : -2.2f);
        wb_cylinder(wb, WC_COAST, WM_OLD_WOOD, (vec3){px, fy - 2.5f, pz}, 0.13f, 0.12f, 2.4f, 8);
    }
    House hut = { .pos = { cx, fy, cz }, .w = 7.0f, .d = 5.0f, .height = 2.7f, .door_side = 2,
                  .wall = WM_PLANKS, .roof = WM_THATCH, .floor = WM_DECK, .trim = WM_PAINT_TEAL,
                  .thatch = true, .open_front = true, .chunk = WC_COAST };
    wb_house(wb, &hut);
    /* a deck in front, with steps down to the sand */
    wb_block(wb, WC_COAST, WM_DECK, (vec3){cx - 4.5f, fy - 0.25f, cz - 5.0f}, (vec3){cx + 4.5f, fy, cz - 2.5f});
    for (int s = 0; s < 3; s++) {
        float top = fy - 0.3f * (s + 1);
        wb_block(wb, WC_COAST, WM_DECK, (vec3){cx - 1.2f, -2.5f, cz - 5.0f - (s + 1) * 0.5f}, (vec3){cx + 1.2f, top, cz - 5.0f - s * 0.5f});
    }
    /* the counter just inside, and a striped awning over the opening */
    wb_block(wb, WC_COAST, WM_PAINT_TEAL, (vec3){cx - 2.8f, fy, cz - 2.2f}, (vec3){cx + 2.8f, fy + 1.0f, cz - 1.6f});
    wb_box(wb, WC_COAST, WM_DECK, (vec3){cx - 2.9f, fy + 1.0f, cz - 2.3f}, (vec3){cx + 2.9f, fy + 1.08f, cz - 1.5f});
    for (int k = 0; k < 9; k++) {
        mat4 xf;
        glm_translate_make(xf, (vec3){cx - 3.2f + (k + 0.5f) * (6.4f / 9.0f), fy + 2.6f, cz - 3.3f});
        glm_rotate_x(xf, -0.45f, xf);
        wb_xbox(wb, WC_COAST, k % 2 ? WM_CLOTH_RED : WM_CLOTH_WHITE, xf, (vec3){6.4f / 18.0f, 0.02f, 0.85f});
    }
    /* a shell-shaped sign on a post */
    wb_cylinder(wb, WC_COAST, WM_OLD_WOOD, (vec3){cx + 4.2f, fy - 2.0f, cz - 5.6f}, 0.08f, 0.07f, 4.4f, 8);
    wb_ball(wb, WC_COAST, WM_PAINT_CORAL, (vec3){cx + 4.2f, fy + 2.6f, cz - 5.6f}, (vec3){0.7f, 0.5f, 0.08f}, 14);
    /* what's for sale, on the counter and the shelves behind */
    wb_prop(wb, PM_REGISTER, (vec3){cx + 1.6f, fy + 1.08f, cz - 1.9f}, GLM_PIf, 0.8f);
    wb_prop(wb, PM_SHELVES, (vec3){cx - 2.2f, fy, cz + 2.0f}, GLM_PIf, 1.0f);
    wb_prop(wb, PM_SHELVES, (vec3){cx + 2.2f, fy, cz + 2.0f}, GLM_PIf, 1.0f);
    wb_prop(wb, PM_GNOME, (vec3){cx - 3.0f, fy, cz + 1.0f}, 2.6f, 1.0f);
    wb_prop(wb, PM_UKULELE, (vec3){cx - 2.2f, fy + 1.13f, cz + 2.0f}, GLM_PIf, 1.0f);
    wb_prop(wb, PM_BANANAS, (vec3){cx - 1.0f, fy + 1.08f, cz - 1.9f}, 0.4f, 1.0f);
    wb_prop(wb, PM_SHELL, (vec3){cx + 0.2f, fy + 1.08f, cz - 1.9f}, 1.2f, 1.8f);
    wb_prop(wb, PM_WICKER, (vec3){cx + 2.2f, fy + 0.02f, cz + 1.2f}, 0.3f, 1.0f);
    wb_prop(wb, PM_ELEPHANT, (vec3){cx + 2.2f, fy + 1.13f, cz + 2.0f}, 3.4f, 1.3f);
    lamp(wb, LIGHT_PAPER, (vec3){cx, fy + 2.3f, cz - 2.6f});
    level_add_spawn(wb->lv, SPAWN_VILLAGER, (vec3){cx - 0.4f, fy, cz - 0.6f}, GLM_PIf)->index = NPC_SHOPKEEPER;
}

static void beach(WorldBuild *wb)
{
    beach_shop(wb);

    /* palms along the back of the beach */
    for (int i = 0; i < 26; i++) {
        float x = -260.0f + i * 13.0f + rnd(i, 1) * 6.0f;
        if (x > MARINA_X0 - 8.0f)
            break;
        if (x > PROM_X0 - 3.0f && x < PROM_X1)
            x += 0.0f;
        float z = coast_z(x) - 3.0f + rnd(i, 2) * 10.0f;
        if (x > PROM_X0 - 4.0f && z < PROM_Z1 + 3.0f)
            z = PROM_Z1 + 3.0f + rnd(i, 3) * 4.0f;
        if (fabsf(x + 22.0f) < 7.0f)
            continue;               /* not through the shop */
        float y = terrain_height(wb->terrain, x, z);
        if (y < SEA_Y + 0.4f)
            continue;
        coast_palm(wb, WC_COAST, (vec3){x, y, z}, 6.5f + rnd(i, 4) * 3.5f, i);
    }

    /* parasols and towels on the sand */
    static const int cloth[4] = { WM_CLOTH_RED, WM_CLOTH_YELLOW, WM_CLOTH_BLUE, WM_PAINT_TEAL };
    for (int i = 0; i < 9; i++) {
        float x = -55.0f + i * 12.0f + rnd(i, 7) * 5.0f;
        if (fabsf(x + 22.0f) < 7.0f)
            x += 9.0f;
        float z = 124.0f + rnd(i, 8) * 8.0f;
        parasol(wb, (vec3){x, terrain_height(wb->terrain, x, z), z}, cloth[i % 4], (rnd(i, 9) - 0.5f) * 0.2f, i);
    }

    /* a lifeguard's tower */
    float lx = 20.0f, lz = 128.0f, ly = terrain_height(wb->terrain, lx, lz);
    for (int k = 0; k < 4; k++)
        wb_cylinder(wb, WC_COAST, WM_OLD_WOOD, (vec3){lx + (k % 2 ? 1.0f : -1.0f), ly - 0.3f, lz + (k / 2 ? 1.0f : -1.0f)}, 0.08f, 0.08f, 2.7f, 8);
    wb_block(wb, WC_COAST, WM_DECK, (vec3){lx - 1.2f, ly + 2.2f, lz - 1.2f}, (vec3){lx + 1.2f, ly + 2.4f, lz + 1.2f});
    wb_box(wb, WC_COAST, WM_CLOTH_RED, (vec3){lx - 1.2f, ly + 2.4f, lz + 1.1f}, (vec3){lx + 1.2f, ly + 3.0f, lz + 1.2f});
    parasol(wb, (vec3){lx, ly + 2.4f, lz}, WM_CLOTH_RED, 0.0f, 99);
    for (int s = 0; s < 7; s++) {
        float top = ly + 0.31f * (s + 1);
        wb_block(wb, WC_COAST, WM_OLD_WOOD, (vec3){lx - 0.5f, top - 0.08f, lz - 1.2f - (7 - s) * 0.3f}, (vec3){lx + 0.5f, top, lz - 1.2f - (6 - s) * 0.3f});
    }

    /* a sandcastle someone worked very hard on */
    float sx = -5.0f, sz = 134.0f, sy = terrain_height(wb->terrain, sx, sz);
    wb_box(wb, WC_COAST, WM_SAND, (vec3){sx - 0.8f, sy - 0.1f, sz - 0.8f}, (vec3){sx + 0.8f, sy + 0.4f, sz + 0.8f});
    for (int k = 0; k < 4; k++)
        wb_cylinder(wb, WC_COAST, WM_SAND, (vec3){sx + (k % 2 ? 0.8f : -0.8f), sy, sz + (k / 2 ? 0.8f : -0.8f)}, 0.28f, 0.2f, 0.75f, 12);
    wb_cylinder(wb, WC_COAST, WM_SAND, (vec3){sx, sy + 0.4f, sz}, 0.4f, 0.0f, 0.6f, 12);
    wb_cylinder(wb, WC_COAST, WM_OLD_WOOD, (vec3){sx, sy + 0.9f, sz}, 0.01f, 0.01f, 0.4f, 4);
    wb_box(wb, WC_COAST, WM_CLOTH_RED, (vec3){sx, sy + 1.15f, sz - 0.005f}, (vec3){sx + 0.18f, sy + 1.28f, sz + 0.005f});

    /* driftwood and a rowing boat upturned on the sand */
    for (int i = 0; i < 7; i++) {
        float x = -110.0f + i * 23.0f + rnd(i, 11) * 8.0f, z = 136.0f + rnd(i, 12) * 6.0f;
        mat4 xf;
        glm_translate_make(xf, (vec3){x, terrain_height(wb->terrain, x, z) + 0.12f, z});
        glm_rotate_y(xf, rnd(i, 13) * 6.28f, xf);
        glm_rotate_z(xf, GLM_PI_2f, xf);
        wb_xcylinder(wb, WC_COAST, WM_OLD_WOOD, xf, 0.14f, 0.09f, 2.2f + rnd(i, 14) * 1.5f, 8);
    }

    /* the walruses' rocks, half in the water at the west end of the beach */
    for (int i = 0; i < 9; i++) {
        float x = -72.0f + i * 4.2f + rnd(i, 20) * 2.0f, z = 139.0f + rnd(i, 21) * 9.0f;
        float y = terrain_height(wb->terrain, x, z);
        rock(wb, (vec3){x, y + 0.1f, z}, (vec3){1.6f + rnd(i, 22), 0.55f + rnd(i, 23) * 0.35f, 1.3f + rnd(i, 24)}, true);
    }
    for (int i = 0; i < 5; i++) {
        float x = -70.0f + i * 7.5f, z = 141.0f + (i % 2) * 4.0f;
        level_add_spawn(wb->lv, SPAWN_ANIMAL, (vec3){x, terrain_height(wb->terrain, x, z) + 0.6f, z}, rnd(i, 25) * 6.28f)->variant = SPECIES_WALRUS;
    }
    /* rocks and pools along both ends */
    for (int i = 0; i < 16; i++) {
        float x = (i < 8 ? -125.0f - i * 9.0f : 60.0f + (i - 8) * 2.5f) + rnd(i, 30) * 4.0f;
        float z = coast_z(x) + 20.0f + rnd(i, 31) * 16.0f;
        rock(wb, (vec3){x, terrain_height(wb->terrain, x, z) + 0.2f, z}, (vec3){1.0f + rnd(i, 32) * 1.6f, 0.6f + rnd(i, 33), 1.0f + rnd(i, 34)}, true);
    }

    /* life on the sand and in the shallows */
    for (int i = 0; i < 8; i++) {
        float x = -100.0f + i * 22.0f + rnd(i, 40) * 10.0f, z = 128.0f + rnd(i, 41) * 12.0f;
        level_add_spawn(wb->lv, SPAWN_ANIMAL, (vec3){x, terrain_height(wb->terrain, x, z), z}, rnd(i, 42) * 6.28f)->variant = SPECIES_CRAB;
    }
    for (int i = 0; i < 6; i++) {
        float x = -90.0f + i * 30.0f, z = 165.0f + rnd(i, 43) * 25.0f;
        level_add_spawn(wb->lv, SPAWN_ANIMAL, (vec3){x, SEA_Y - 1.2f, z}, rnd(i, 44) * 6.28f)->variant = SPECIES_TURTLE;
    }
    for (int i = 0; i < 10; i++) {
        float x = -120.0f + i * 25.0f, z = 175.0f + rnd(i, 45) * 70.0f;
        level_add_spawn(wb->lv, SPAWN_ANIMAL, (vec3){x, SEA_Y - 2.5f - rnd(i, 46) * 3.0f, z}, 0.0f)->variant = SPECIES_FISH;
    }
    for (int i = 0; i < 7; i++) {
        float x = -80.0f + i * 25.0f, z = 118.0f + rnd(i, 47) * 20.0f;
        level_add_spawn(wb->lv, SPAWN_ANIMAL, (vec3){x, 4.0f + rnd(i, 48) * 6.0f, z}, 0.0f)->variant = SPECIES_GULL;
    }
    level_add_spawn(wb->lv, SPAWN_ANIMAL, (vec3){10.0f, SEA_Y - 1.0f, 230.0f}, 0.0f)->variant = SPECIES_DOLPHIN;
    level_add_spawn(wb->lv, SPAWN_ANIMAL, (vec3){-60.0f, SEA_Y - 1.0f, 250.0f}, 1.0f)->variant = SPECIES_DOLPHIN;
    /* and the two things in the sea you shouldn't swim into */
    for (int i = 0; i < 6; i++) {
        float x = -40.0f + i * 22.0f, z = 185.0f + rnd(i, 50) * 40.0f;
        level_add_spawn(wb->lv, SPAWN_CREATURE, (vec3){x, SEA_Y - 1.5f, z}, 0.0f)->variant = CR_JELLYFISH;
    }
    level_add_spawn(wb->lv, SPAWN_CREATURE, (vec3){-20.0f, SEA_Y - 5.0f, 265.0f}, 0.0f)->variant = CR_SHARK;
    level_add_spawn(wb->lv, SPAWN_CREATURE, (vec3){90.0f, SEA_Y - 5.0f, 290.0f}, 2.0f)->variant = CR_SHARK;
    for (int i = 0; i < 5; i++) {
        float x = -80.0f + i * 35.0f + rnd(i, 51) * 10.0f, z = 136.0f + rnd(i, 52) * 8.0f;
        level_add_spawn(wb->lv, SPAWN_CREATURE, (vec3){x, terrain_height(wb->terrain, x, z), z}, 0.0f)->variant = CR_CRAB;
    }

    /* shells to find on the sand (the shop's currency), relics and tide charts */
    for (int i = 0; i < 34; i++) {
        float x = -150.0f + rnd(i, 60) * 225.0f, z = 124.0f + rnd(i, 61) * 22.0f;
        float y = terrain_height(wb->terrain, x, z);
        if (y < SEA_Y + 0.1f)
            continue;
        float r = rnd(i, 62);
        int color = r < 0.55f ? SHELL_WHITE : r < 0.8f ? SHELL_PINK : r < 0.95f ? SHELL_BLUE : SHELL_GOLD;
        thing(wb, THING_SHELL, color, (vec3){x, y + 0.03f, z}, rnd(i, 63) * 6.28f);
    }
    thing(wb, THING_RELIC, 0, (vec3){-66.0f, terrain_height(wb->terrain, -66.0f, 146.5f) + 0.9f, 146.5f}, 0.0f);   /* on the walrus rocks */
    thing(wb, THING_RELIC, 1, (vec3){-5.3f, terrain_height(wb->terrain, -5.0f, 134.0f) + 0.45f, 133.3f}, 0.0f);  /* in the sandcastle */
    thing(wb, THING_RELIC, 2, (vec3){-140.0f, terrain_height(wb->terrain, -140.0f, 136.0f) + 0.05f, 136.0f}, 0.0f);
    thing(wb, THING_RELIC, 3, (vec3){lx, ly + 2.45f, lz - 0.5f}, 0.0f);                                           /* up the lifeguard tower */
    thing(wb, THING_CHART, 0, (vec3){-71.0f, terrain_height(wb->terrain, -71.0f, 141.0f) + 0.8f, 141.0f}, 0.0f);

    /* a swimmer doing lengths off the beach */
    level_add_spawn(wb->lv, SPAWN_VILLAGER, (vec3){-30.0f, SEA_Y, 160.0f}, 0.0f)->index = NPC_SWIMMER;
}

/* ---------- the marina ---------- */

static void pier(WorldBuild *wb, float x, float z0, float z1, int index)
{
    float y = QUAY_Y, w = 1.6f;
    wb_block(wb, WC_MARINA, WM_DECK, (vec3){x - w, y - 0.3f, z0}, (vec3){x + w, y, z1});
    wb_box(wb, WC_MARINA, WM_OLD_WOOD, (vec3){x - w - 0.1f, y - 0.5f, z0}, (vec3){x - w, y, z1});
    wb_box(wb, WC_MARINA, WM_OLD_WOOD, (vec3){x + w, y - 0.5f, z0}, (vec3){x + w + 0.1f, y, z1});
    for (float z = z0 + 2.0f; z <= z1; z += 4.0f)
        for (int s = -1; s <= 1; s += 2) {
            float px = x + s * (w + 0.15f);
            wb_cylinder(wb, WC_MARINA, WM_OLD_WOOD, (vec3){px, -8.5f, z}, 0.18f, 0.18f, 8.5f + y + 0.7f, 8);
            wb_solid(wb, (vec3){px - 0.18f, -8.5f, z - 0.18f}, (vec3){px + 0.18f, y + 0.7f, z + 0.18f});
            wb_cylinder(wb, WC_MARINA, WM_ROPE, (vec3){px, y + 0.3f, z}, 0.2f, 0.2f, 0.12f, 8);
        }
    /* crates and a lifebuoy at the end, and boats tied along the side */
    wb_prop(wb, PM_CRATE, (vec3){x - 0.8f, y, z1 - 1.0f}, 0.3f, 1.0f);
    wb_solid(wb, (vec3){x - 1.3f, y, z1 - 1.5f}, (vec3){x - 0.3f, y + 0.7f, z1 - 0.5f});
    wb_prop(wb, PM_LIFEBUOY, (vec3){x + 1.2f, y + 0.02f, z1 - 3.0f}, 0.0f, 1.0f);
    for (int k = 0; k < 2; k++) {
        float bz = z0 + 12.0f + k * 18.0f + index * 4.0f;
        level_add_spawn(wb->lv, SPAWN_BOAT, (vec3){x + (k ? -1.0f : 1.0f) * (w + 1.6f), SEA_Y, bz}, 0.0f);
    }
}

static void lighthouse(WorldBuild *wb)
{
    float x = LIGHTHOUSE_X, z = LIGHTHOUSE_Z, y = QUAY_Y + 0.3f;
    /* a round stone platform at the breakwater's end */
    wb_cylinder(wb, WC_MARINA, WM_SEA_BRICK, (vec3){x, -8.0f, z}, 9.0f, 8.5f, 8.0f + y, 32);
    wb_solid(wb, (vec3){x - 7.5f, -8.0f, z - 7.5f}, (vec3){x + 7.5f, y, z + 7.5f});
    /* the tower: white and red bands */
    float r = 3.4f, h = 0.0f;
    for (int band = 0; band < 6; band++) {
        float bh = 3.4f, r1 = r - 0.12f;
        wb_cylinder(wb, WC_MARINA, band % 2 ? WM_CLOTH_RED : WM_PLASTER_WHITE, (vec3){x, y + h, z}, r, r1, bh, 28);
        h += bh;
        r = r1;
    }
    wb_solid(wb, (vec3){x - 3.2f, y, z - 3.2f}, (vec3){x + 3.2f, y + h, z + 3.2f});
    /* a door facing the breakwater, a gallery, the lantern room and its cap */
    wb_box(wb, WC_MARINA, WM_TIMBER, (vec3){x - 0.6f, y, z - 3.45f}, (vec3){x + 0.6f, y + 2.2f, z - 3.2f});
    wb_cylinder(wb, WC_MARINA, WM_IRON, (vec3){x, y + h, z}, r + 1.0f, r + 1.0f, 0.2f, 28);
    wb_cylinder(wb, WC_MARINA, WM_IRON, (vec3){x, y + h + 0.2f, z}, r + 1.0f, r + 1.0f, 0.06f, 28);
    wb_cylinder(wb, WC_MARINA, WM_GLASS, (vec3){x, y + h + 0.2f, z}, r * 0.75f, r * 0.75f, 2.4f, 20);
    wb_cylinder(wb, WC_MARINA, WM_LAMP_GLOW, (vec3){x, y + h + 0.6f, z}, 0.6f, 0.6f, 1.4f, 12);
    wb_cylinder(wb, WC_MARINA, WM_CLOTH_RED, (vec3){x, y + h + 2.6f, z}, r * 0.85f, 0.2f, 1.6f, 20);
    wb_ball(wb, WC_MARINA, WM_BRASS, (vec3){x, y + h + 4.3f, z}, (vec3){0.25f, 0.25f, 0.25f}, 8);
    lamp(wb, LIGHT_BEACON, (vec3){x, y + h + 1.3f, z});
    /* things left out on the platform */
    wb_prop(wb, PM_BARRELS, (vec3){x + 4.5f, y, z + 2.0f}, 1.2f, 0.7f);
    wb_solid(wb, (vec3){x + 3.5f, y, z + 0.8f}, (vec3){x + 5.8f, y + 0.9f, z + 3.4f});
    thing(wb, THING_RELIC, 4, (vec3){x - 5.5f, y + 0.05f, z + 3.0f}, 0.0f);
    thing(wb, THING_CHART, 1, (vec3){x + 4.6f, y + 0.95f, z + 2.0f}, 0.0f);
    /* on clear nights someone very old waits here, looking out to sea */
    level_add_spawn(wb->lv, SPAWN_VILLAGER, (vec3){x + 2.0f, y, z + 6.0f}, 0.0f)->index = NPC_ANCIENT;
}

static void marina(WorldBuild *wb)
{
    float y = QUAY_Y;
    /* the quay: stone along the basin's north edge, joined to the promenade by steps */
    wb_block(wb, WC_MARINA, WM_SEA_BRICK, (vec3){MARINA_X0 - 4.0f, -8.0f, 98.0f}, (vec3){MARINA_X1 + 8.0f, y, MARINA_Z0 + 2.0f});
    for (int s = 0; s < 3; s++)
        wb_block(wb, WC_MARINA, WM_SEA_TILES, (vec3){PROM_X1, -2.5f, PROM_Z0 + s * 0.0f}, (vec3){MARINA_X0 - 4.0f + 0.0f, PROM_Y - 0.3f * (s + 1), PROM_Z1});
    wb_block(wb, WC_MARINA, WM_SEA_TILES, (vec3){PROM_X1, -2.5f, PROM_Z0}, (vec3){PROM_X1 + 1.3f, PROM_Y - 0.3f, PROM_Z1});
    wb_block(wb, WC_MARINA, WM_SEA_TILES, (vec3){PROM_X1 + 1.3f, -2.5f, PROM_Z0}, (vec3){PROM_X1 + 2.6f, PROM_Y - 0.6f, PROM_Z1});
    /* bollards and lamps along the quay's edge */
    for (float x = MARINA_X0; x < MARINA_X1; x += 9.0f) {
        wb_cylinder(wb, WC_MARINA, WM_IRON, (vec3){x + 3.0f, y, MARINA_Z0 + 1.4f}, 0.2f, 0.16f, 0.6f, 10);
        wb_solid(wb, (vec3){x + 2.8f, y, MARINA_Z0 + 1.2f}, (vec3){x + 3.2f, y + 0.6f, MARINA_Z0 + 1.6f});
        if ((int)(x - MARINA_X0) % 18 == 0)
            lamp_post(wb, WC_MARINA, (vec3){x + 7.0f, y, MARINA_Z0 + 1.0f}, 0.0f);
    }
    /* three piers */
    pier(wb, 100.0f, MARINA_Z0 + 2.0f, MARINA_Z0 + 58.0f, 0);
    pier(wb, 122.0f, MARINA_Z0 + 2.0f, MARINA_Z0 + 64.0f, 1);
    pier(wb, 144.0f, MARINA_Z0 + 2.0f, MARINA_Z0 + 52.0f, 2);
    thing(wb, THING_CHART, 2, (vec3){121.2f, y + 0.72f, MARINA_Z0 + 63.0f}, 0.0f);
    thing(wb, THING_RELIC, 5, (vec3){144.5f, y + 0.05f, MARINA_Z0 + 50.0f}, 0.0f);

    /* the breakwater out to the lighthouse */
    wb_block(wb, WC_MARINA, WM_SEA_BRICK, (vec3){LIGHTHOUSE_X - 6.0f, -8.0f, MARINA_Z0 - 2.0f}, (vec3){LIGHTHOUSE_X + 6.0f, y + 0.3f, LIGHTHOUSE_Z - 7.0f});
    for (float zz = MARINA_Z0 + 4.0f; zz < LIGHTHOUSE_Z - 12.0f; zz += 10.0f) {
        rock(wb, (vec3){LIGHTHOUSE_X + 7.5f, SEA_Y + 0.2f, zz + rnd((int)zz, 70) * 4.0f}, (vec3){2.0f, 1.2f, 2.2f}, false);
        wb_cylinder(wb, WC_MARINA, WM_IRON, (vec3){LIGHTHOUSE_X - 5.4f, y + 0.3f, zz}, 0.18f, 0.14f, 0.6f, 10);
    }
    lighthouse(wb);

    /* a ship moored inside the breakwater, and buoys marking the harbour mouth */
    wb_prop(wb, PM_SHIP, (vec3){163.0f, SEA_Y - 2.4f, 178.0f}, GLM_PIf, 1.0f);
    wb_solid(wb, (vec3){160.0f, -8.0f, 166.0f}, (vec3){166.0f, SEA_Y + 2.5f, 190.0f});
    level_add_spawn(wb->lv, SPAWN_PROP, (vec3){128.0f, SEA_Y, 222.0f}, 0.0f)->variant = PM_MARKER;
    level_add_spawn(wb->lv, SPAWN_PROP, (vec3){156.0f, SEA_Y, 226.0f}, 0.0f)->variant = PM_BUOY;
    Spawn *bell = level_add_spawn(wb->lv, SPAWN_PROP, (vec3){WHIRL_X + 3.0f, SEA_Y, WHIRL_Z}, 0.0f);
    bell->variant = PM_BUOY;
    thing(wb, THING_WHIRLPOOL, 0, (vec3){WHIRL_X, SEA_Y, WHIRL_Z}, 0.0f);

    /* the harbourmaster's office, and the fish market on the quay */
    House office = { .pos = { 94.0f, y, 108.0f }, .w = 8.0f, .d = 6.0f, .height = 3.2f, .door_side = 0,
                     .wall = WM_PLASTER_WHITE, .roof = WM_ROOF_TILES, .floor = WM_PLANKS, .trim = WM_CLOTH_BLUE,
                     .chunk = WC_MARINA };
    wb_house(wb, &office);
    lamp(wb, LIGHT_LAMP, (vec3){94.0f, y + 2.7f, 108.0f});
    level_add_spawn(wb->lv, SPAWN_VILLAGER, (vec3){94.5f, y, 107.0f}, 0.0f)->index = NPC_HARBOURMASTER;
    thing(wb, THING_NOTICEBOARD, 0, (vec3){99.2f, y + 1.4f, 111.3f}, 0.0f);
    wb_box(wb, WC_MARINA, WM_OLD_WOOD, (vec3){98.4f, y + 0.8f, 111.2f}, (vec3){100.0f, y + 2.0f, 111.3f});
    for (int k = 0; k < 3; k++) {
        float sx = 112.0f + k * 8.0f;
        for (int p = 0; p < 4; p++)
            wb_cylinder(wb, WC_MARINA, WM_OLD_WOOD, (vec3){sx + (p % 2 ? 1.6f : -1.6f), y, 108.0f + (p / 2 ? 1.2f : -1.2f)}, 0.07f, 0.07f, 2.4f, 6);
        wb_block(wb, WC_MARINA, WM_OLD_WOOD, (vec3){sx - 1.7f, y, 107.0f}, (vec3){sx + 1.7f, y + 0.9f, 109.4f});
        for (int st = 0; st < 6; st++) {
            mat4 xf;
            glm_translate_make(xf, (vec3){sx - 1.7f + (st + 0.5f) * (3.4f / 6.0f), y + 2.45f, 108.0f});
            glm_rotate_z(xf, 0.0f, xf);
            wb_xbox(wb, WC_MARINA, st % 2 ? (k == 1 ? WM_CLOTH_BLUE : WM_CLOTH_RED) : WM_CLOTH_WHITE, xf, (vec3){3.4f / 12.0f, 0.02f, 1.5f});
        }
        wb_prop(wb, PM_WICKER, (vec3){sx - 0.8f, y + 0.9f, 108.2f}, k * 1.3f, 1.0f);
        wb_prop(wb, PM_BUCKET, (vec3){sx + 0.9f, y + 0.9f, 108.0f}, k * 0.7f, 0.6f);
    }
    level_add_spawn(wb->lv, SPAWN_VILLAGER, (vec3){121.5f, y, MARINA_Z0 + 60.0f}, GLM_PIf)->index = NPC_FISHER;
    wb_prop(wb, PM_BARRELS, (vec3){150.0f, y, 108.0f}, 0.3f, 0.8f);
    wb_solid(wb, (vec3){148.2f, y, 106.6f}, (vec3){151.8f, y + 0.9f, 109.4f});

    /* fish and jellyfish in the basin, gulls on the posts */
    for (int i = 0; i < 6; i++)
        level_add_spawn(wb->lv, SPAWN_ANIMAL, (vec3){108.0f + i * 10.0f, SEA_Y - 2.0f, 150.0f + (i % 3) * 18.0f}, 0.0f)->variant = SPECIES_FISH;
    for (int i = 0; i < 4; i++)
        level_add_spawn(wb->lv, SPAWN_ANIMAL, (vec3){101.8f + i * 14.0f, y + 1.2f, 140.0f + i * 5.0f}, 0.0f)->variant = SPECIES_GULL;
    level_add_spawn(wb->lv, SPAWN_ANIMAL, (vec3){112.0f, SEA_Y - 0.6f, 200.0f}, 0.0f)->variant = SPECIES_WALRUS;
}

void coast_build(WorldBuild *wb)
{
    promenade(wb);
    beach(wb);
    marina(wb);
}
