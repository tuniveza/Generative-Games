#include "game.h"
#include "world.h"

#include <stdint.h>

/* The Drowned Court, on the sea floor far south of the beach. The whirlpool (opened by
 * the Conch of the Deep at the bell buoy) sets you down in its arrival hall; the air
 * inside is the sea's gift and stays. Rooms of coral stone and shell mosaic, glowing
 * pearls and runes, lit by the sun far above through a great glass dome.
 *
 *   arrival hall (north)   the rising current home
 *   the dome court         coral gardens, the Sea Queen's statue, doors to every wing
 *   bell gallery (west)    four bells to ring in the queen's order
 *   tide engine (east)     three levers, an eel channel
 *   pearl garden (n-west)  five shell plates to walk in the pearls' order
 *   throne room (south)    behind a gate with three seals: the Drowned King, the bell */

#define CX PALACE_X
#define CZ PALACE_Z
#define F  PALACE_FLOOR

static float rnd(int i, int salt)
{
    uint32_t h = (uint32_t)i * 2654435761u ^ (uint32_t)salt * 1013904223u;
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

static void glow(WorldBuild *wb, vec3 pos)
{
    level_add_spawn(wb->lv, SPAWN_LAMP, pos, 0.0f)->variant = LIGHT_CORAL;
}

static void creature(WorldBuild *wb, int type, vec3 pos, float yaw)
{
    level_add_spawn(wb->lv, SPAWN_CREATURE, pos, yaw)->variant = type;
}

/* a room: shell floor, coral walls with doorways (N, E, S, W; 0 = no door, -1 = no wall),
 * a vaulted roof, pilasters and a band of glowing runes */
static void room(WorldBuild *wb, float x0, float z0, float x1, float z1, float h, const float doors[4])
{
    const int C = WC_UNDERSEA;
    wb_block(wb, C, WM_SHELL_FLOOR, (vec3){x0, F - 0.3f, z0}, (vec3){x1, F, z1});
    float t = 0.4f;
    for (int side = 0; side < 4; side++) {
        if (doors[side] < 0.0f)
            continue;
        bool along_x = side == 0 || side == 2;
        float fixed = side == 0 ? z0 : side == 1 ? x1 : side == 2 ? z1 : x0;
        float a = along_x ? x0 : z0, b = along_x ? x1 : z1, mid = (a + b) * 0.5f;
        float gap = doors[side];
        float spans[2][2] = { { a, gap > 0 ? mid - gap * 0.5f : b }, { gap > 0 ? mid + gap * 0.5f : b, b } };
        for (int k = 0; k < 2; k++) {
            if (spans[k][1] <= spans[k][0] + 0.01f)
                continue;
            vec3 mn = { along_x ? spans[k][0] : fixed - t, F, along_x ? fixed - t : spans[k][0] };
            vec3 mx = { along_x ? spans[k][1] : fixed + t, F + h, along_x ? fixed + t : spans[k][1] };
            wb_block(wb, C, WM_CORAL_WALL, mn, mx);
        }
        if (gap > 0) {      /* the lintel over the doorway */
            vec3 mn = { along_x ? mid - gap * 0.5f : fixed - t, F + 3.6f, along_x ? fixed - t : mid - gap * 0.5f };
            vec3 mx = { along_x ? mid + gap * 0.5f : fixed + t, F + h, along_x ? fixed + t : mid + gap * 0.5f };
            wb_block(wb, C, WM_CORAL_FORT, mn, mx);
        }
        /* runes glowing along the wall, above head height */
        vec3 rmn = { along_x ? a + 0.5f : fixed - t - 0.02f, F + 3.9f, along_x ? fixed - t - 0.02f : a + 0.5f };
        vec3 rmx = { along_x ? b - 0.5f : fixed + t + 0.02f, F + 4.05f, along_x ? fixed + t + 0.02f : b - 0.5f };
        wb_box(wb, C, WM_RUNE, rmn, rmx);
    }
    /* pilasters at the corners and a vaulted ceiling */
    for (int c = 0; c < 4; c++) {
        float px = c % 2 ? x1 - 0.6f : x0 + 0.6f, pz = c / 2 ? z1 - 0.6f : z0 + 0.6f;
        wb_block(wb, C, WM_CORAL_FORT, (vec3){px - 0.5f, F, pz - 0.5f}, (vec3){px + 0.5f, F + h, pz + 0.5f});
        wb_ball(wb, C, WM_PEARL, (vec3){px, F + h - 0.6f, pz}, (vec3){0.25f, 0.25f, 0.25f}, 10);
    }
    wb_block(wb, C, WM_CORAL_FORT, (vec3){x0 - 0.4f, F + h, z0 - 0.4f}, (vec3){x1 + 0.4f, F + h + 0.6f, z1 + 0.4f});
    mat4 xf;
    bool along = x1 - x0 > z1 - z0;
    glm_translate_make(xf, (vec3){(x0 + x1) * 0.5f, F + h + 0.6f + 0.7f, (z0 + z1) * 0.5f});
    if (along)
        glm_rotate_y(xf, GLM_PI_2f, xf);
    wb_wedge(wb, C, WM_CORAL_WALL, xf, (vec3){(along ? z1 - z0 : x1 - x0) * 0.5f + 0.4f, 0.7f, (along ? x1 - x0 : z1 - z0) * 0.5f + 0.4f});
}

static void coral_cluster(WorldBuild *wb, vec3 at, int seed, float size)
{
    for (int k = 0; k < 6; k++) {
        float a = rnd(seed, k) * 6.28f, r = rnd(seed, 10 + k) * 1.2f * size;
        vec3 p = { at[0] + cosf(a) * r, at[1], at[2] + sinf(a) * r };
        int m = k % 3 == 0 ? WM_CORAL_PINK : k % 3 == 1 ? WM_CORAL_ORANGE : WM_PEARL;
        if (m == WM_PEARL) {
            wb_ball(wb, WC_UNDERSEA, WM_CORAL_PINK, (vec3){p[0], p[1] + 0.2f * size, p[2]}, (vec3){0.35f * size, 0.25f * size, 0.35f * size}, 10);
            continue;
        }
        /* branching fans */
        mat4 xf;
        glm_translate_make(xf, p);
        glm_rotate_z(xf, (rnd(seed, 20 + k) - 0.5f) * 0.6f, xf);
        float h = (0.6f + rnd(seed, 30 + k) * 1.2f) * size;
        wb_xcylinder(wb, WC_UNDERSEA, m, xf, 0.09f * size, 0.03f * size, h, 6);
        for (int b = 0; b < 3; b++) {
            mat4 br;
            glm_mat4_copy(xf, br);
            glm_translate(br, (vec3){0, h * (0.4f + b * 0.2f), 0});
            glm_rotate_y(br, rnd(seed, 40 + b + k) * 6.28f, br);
            glm_rotate_z(br, 0.7f, br);
            wb_xcylinder(wb, WC_UNDERSEA, m, br, 0.05f * size, 0.015f * size, h * 0.5f, 5);
        }
    }
}

static void kelp(WorldBuild *wb, vec3 at, float h, int seed)
{
    float bend = 0.0f;
    vec3 p;
    glm_vec3_copy(at, p);
    for (int k = 0; k < (int)(h / 0.8f); k++) {
        mat4 xf;
        glm_translate_make(xf, p);
        glm_rotate_y(xf, rnd(seed, k) * 6.28f, xf);
        glm_rotate_z(xf, bend, xf);
        wb_xbox(wb, WC_UNDERSEA, WM_KELP, xf, (vec3){0.12f, 0.42f, 0.015f});
        p[1] += 0.8f;
        p[0] += sinf(bend) * 0.8f;
        bend = sinf(k * 0.7f + seed) * 0.25f;
    }
}

void undersea_build(WorldBuild *wb)
{
    const int C = WC_UNDERSEA;
    /* the platform the Court stands on, raised from the sea floor */
    wb_block(wb, C, WM_CORAL_GROUND, (vec3){CX - 50.0f, F - 6.0f, CZ - 46.0f}, (vec3){CX + 50.0f, F - 0.3f, CZ + 50.0f});

    /* arrival hall, and the corridor into the dome court */
    room(wb, CX - 8.0f, CZ - 40.0f, CX + 8.0f, CZ - 24.0f, 7.0f, (float[4]){ 0, 0, 5.0f, 3.0f });
    thing(wb, THING_EXIT_CURRENT, 0, (vec3){CX, F + 0.1f, CZ - 38.0f}, 0.0f);
    wb_cylinder(wb, C, WM_PEARL, (vec3){CX, F, CZ - 38.0f}, 1.2f, 1.2f, 0.05f, 20);
    glow(wb, (vec3){CX - 5.0f, F + 3.0f, CZ - 32.0f});
    glow(wb, (vec3){CX + 5.0f, F + 3.0f, CZ - 32.0f});
    for (int s = -1; s <= 1; s += 2)
        wb_prop(wb, PM_LION, (vec3){CX + s * 7.3f, F + 2.0f, CZ - 30.0f}, -s * GLM_PI_2f, 2.0f);
    room(wb, CX - 3.0f, CZ - 24.0f, CX + 3.0f, CZ - 16.0f, 5.0f, (float[4]){ -1, 0, -1, 0 });

    /* the dome court: a ring of wall under a glass dome, open to the light */
    float R = 16.0f;
    for (int k = 0; k < 20; k++) {
        float a0 = k * 6.2832f / 20.0f, a1 = (k + 1) * 6.2832f / 20.0f, am = (a0 + a1) * 0.5f;
        float dx = cosf(am), dz = sinf(am);
        /* doors: north (the corridor), west, east, south (the throne gate) */
        bool door = (fabsf(dz + 1.0f) < 0.1f) || (fabsf(dx + 1.0f) < 0.1f) || (fabsf(dx - 1.0f) < 0.1f) || (fabsf(dz - 1.0f) < 0.1f);
        if (door)
            continue;
        vec3 c = { CX + dx * R, F, CZ + dz * R };
        mat4 xf;
        glm_translate_make(xf, (vec3){c[0], F + 2.5f, c[2]});
        glm_rotate_y(xf, -am, xf);
        wb_xbox(wb, C, WM_CORAL_WALL, xf, (vec3){0.5f, 2.5f, R * 0.165f});
        wb_solid(wb, (vec3){c[0] - 2.0f, F, c[2] - 2.0f}, (vec3){c[0] + 2.0f, F + 5.0f, c[2] + 2.0f});
        glm_translate(xf, (vec3){-0.52f, 1.5f, 0});
        wb_xbox(wb, C, WM_RUNE, xf, (vec3){0.02f, 0.08f, R * 0.14f});
    }
    wb_cylinder(wb, C, WM_SHELL_FLOOR, (vec3){CX, F - 0.25f, CZ}, R, R, 0.26f, 40);
    wb_cylinder(wb, C, WM_CORAL_FORT, (vec3){CX, F + 5.0f, CZ}, R + 0.6f, R + 0.6f, 0.5f, 40);
    wb_ball(wb, C, WM_GLASS, (vec3){CX, F + 5.4f, CZ}, (vec3){R + 0.6f, 11.0f, R + 0.6f}, 48);
    /* the Sea Queen, looking up at the light */
    wb_block(wb, C, WM_MARBLE, (vec3){CX - 2.0f, F, CZ - 2.0f}, (vec3){CX + 2.0f, F + 0.6f, CZ + 2.0f});
    wb_block(wb, C, WM_CORAL_FORT, (vec3){CX - 1.4f, F + 0.6f, CZ - 1.4f}, (vec3){CX + 1.4f, F + 2.8f, CZ + 1.4f});
    wb_prop(wb, PM_BUST, (vec3){CX, F + 2.8f, CZ}, GLM_PIf, 6.5f);
    wb_cylinder(wb, C, WM_GOLD, (vec3){CX, F + 5.9f, CZ + 0.1f}, 0.75f, 0.85f, 0.35f, 16);
    for (int k = 0; k < 6; k++)
        wb_cylinder(wb, C, WM_GOLD, (vec3){CX + cosf(k * 1.047f) * 0.75f, F + 6.2f, CZ + 0.1f + sinf(k * 1.047f) * 0.75f}, 0.08f, 0.0f, 0.4f, 4);
    thing(wb, THING_LORE, 10, (vec3){CX, F + 1.2f, CZ - 2.1f}, 0.0f);
    thing(wb, THING_RELIC, 9, (vec3){CX + 1.6f, F + 0.7f, CZ + 1.6f}, 0.0f);
    for (int k = 0; k < 8; k++) {
        float a = k * GLM_PI_4f + 0.4f;
        coral_cluster(wb, (vec3){CX + cosf(a) * 10.0f, F, CZ + sinf(a) * 10.0f}, k, 1.2f);
        if (k % 2)
            glow(wb, (vec3){CX + cosf(a) * 10.0f, F + 1.0f, CZ + sinf(a) * 10.0f});
        kelp(wb, (vec3){CX + cosf(a + 0.3f) * 13.5f, F, CZ + sinf(a + 0.3f) * 13.5f}, 4.0f, k);
    }
    creature(wb, CR_DROWNED, (vec3){CX - 8.0f, F, CZ + 6.0f}, 0.0f);
    creature(wb, CR_DROWNED, (vec3){CX + 8.0f, F, CZ + 6.0f}, 0.0f);
    creature(wb, CR_CRAB, (vec3){CX + 6.0f, F, CZ - 8.0f}, 0.0f);

    /* the bell gallery (west) */
    float gx0 = CX - 34.0f, gx1 = CX - R - 0.5f;
    room(wb, gx0, CZ - 8.0f, gx1, CZ + 8.0f, 7.0f, (float[4]){ 0, 3.5f, 0, 0 });
    for (int k = 0; k < 4; k++) {
        float x = gx0 + 4.0f + k * 3.5f;
        wb_box(wb, C, WM_GOLD, (vec3){x - 0.05f, F + 3.2f, CZ - 0.05f}, (vec3){x + 0.05f, F + 7.0f, CZ + 0.05f});
        thing(wb, THING_CHIME, k, (vec3){x, F + 3.2f, CZ}, 0.0f);
    }
    thing(wb, THING_LORE, 11, (vec3){gx0 + 0.8f, F + 1.5f, CZ}, GLM_PI_2f);
    wb_box(wb, C, WM_RUNE, (vec3){gx0 + 0.4f, F + 0.8f, CZ - 1.5f}, (vec3){gx0 + 0.45f, F + 2.6f, CZ + 1.5f});
    glow(wb, (vec3){gx0 + 8.0f, F + 3.0f, CZ - 5.0f});
    glow(wb, (vec3){gx0 + 8.0f, F + 3.0f, CZ + 5.0f});
    creature(wb, CR_DROWNED, (vec3){gx0 + 6.0f, F, CZ + 5.0f}, 1.0f);
    creature(wb, CR_DROWNED, (vec3){gx0 + 10.0f, F, CZ - 5.0f}, 2.0f);

    /* the tide engine (east): a channel of water across the room, three levers */
    float ex0 = CX + R + 0.5f, ex1 = CX + 34.0f;
    room(wb, ex0, CZ - 8.0f, ex1, CZ + 8.0f, 7.0f, (float[4]){ 0, 0, 0, 3.5f });
    /* the channel: sunk into the floor, railed off, full of dark water and eels */
    wb_box(wb, C, WM_BUBBLE, (vec3){ex0 + 4.0f, F - 0.35f, CZ - 3.0f}, (vec3){ex1 - 2.0f, F - 0.3f, CZ + 3.0f});
    for (int s = -1; s <= 1; s += 2)
        wb_block(wb, C, WM_CORAL_FORT, (vec3){ex0 + 4.0f, F, CZ + s * 3.0f - 0.2f}, (vec3){ex1 - 2.0f, F + 0.9f, CZ + s * 3.0f + 0.2f});
    wb_block(wb, C, WM_CORAL_FORT, (vec3){ex0 + 3.8f, F, CZ - 3.0f}, (vec3){ex0 + 4.2f, F + 0.9f, CZ + 3.0f});
    creature(wb, CR_EEL, (vec3){ex0 + 9.0f, F - 0.9f, CZ}, 0.0f);
    creature(wb, CR_EEL, (vec3){ex0 + 13.0f, F - 0.9f, CZ}, 3.0f);
    for (int k = 0; k < 3; k++) {
        float x = ex0 + 5.0f + k * 4.0f;
        wb_block(wb, C, WM_CORAL_FORT, (vec3){x - 0.4f, F, CZ + 6.6f}, (vec3){x + 0.4f, F + 0.5f, CZ + 7.4f});
        thing(wb, THING_LEVER, k, (vec3){x, F + 0.5f, CZ + 7.0f}, 0.0f);
    }
    thing(wb, THING_LORE, 12, (vec3){ex0 + 1.2f, F + 1.5f, CZ + 7.2f}, GLM_PIf);
    wb_box(wb, C, WM_RUNE, (vec3){ex0 + 0.5f, F + 0.8f, CZ + 7.5f}, (vec3){ex0 + 2.0f, F + 2.4f, CZ + 7.55f});
    glow(wb, (vec3){ex0 + 8.0f, F + 3.0f, CZ - 6.0f});
    glow(wb, (vec3){ex0 + 12.0f, F + 3.0f, CZ + 5.0f});
    for (int k = 0; k < 4; k++)
        wb_cylinder(wb, C, WM_BRASS, (vec3){ex0 + 3.0f + k * 3.5f, F, CZ - 7.2f}, 0.45f, 0.45f, 5.5f, 14);

    /* the pearl garden (north-west, off the arrival hall) */
    float px0 = CX - 30.0f, px1 = CX - 8.9f, pz0 = CZ - 42.0f, pz1 = CZ - 22.0f;
    room(wb, px0, pz0, px1, pz1, 7.0f, (float[4]){ 0, 3.0f, 0, 0 });
    float pcx = (px0 + px1) * 0.5f, pcz = (pz0 + pz1) * 0.5f;
    thing(wb, THING_PLATE, 5, (vec3){pcx, F, pcz}, 0.0f);
    for (int k = 0; k < 5; k++) {
        float a = k * 6.2832f / 5.0f - GLM_PI_2f;
        vec3 p = { pcx + cosf(a) * 6.0f, F, pcz + sinf(a) * 6.0f };
        thing(wb, THING_PLATE, k, p, 0.0f);
        wb_block(wb, C, WM_CORAL_FORT, (vec3){p[0] + cosf(a) * 2.0f - 0.3f, F, p[2] + sinf(a) * 2.0f - 0.3f},
                 (vec3){p[0] + cosf(a) * 2.0f + 0.3f, F + 1.6f, p[2] + sinf(a) * 2.0f + 0.3f});
        wb_ball(wb, C, WM_PEARL, (vec3){p[0] + cosf(a) * 2.0f, F + 1.85f, p[2] + sinf(a) * 2.0f}, (vec3){0.25f, 0.25f, 0.25f}, 12);
        coral_cluster(wb, (vec3){p[0] + cosf(a + 0.6f) * 3.5f, F, p[2] + sinf(a + 0.6f) * 3.5f}, 20 + k, 0.8f);
    }
    thing(wb, THING_LORE, 13, (vec3){px0 + 0.8f, F + 1.5f, pcz}, GLM_PI_2f);
    wb_box(wb, C, WM_RUNE, (vec3){px0 + 0.4f, F + 0.8f, pcz - 1.2f}, (vec3){px0 + 0.45f, F + 2.4f, pcz + 1.2f});
    thing(wb, THING_RELIC, 11, (vec3){px0 + 2.0f, F + 0.05f, pz0 + 2.0f}, 0.0f);
    glow(wb, (vec3){pcx, F + 4.0f, pcz});
    creature(wb, CR_DROWNED, (vec3){px0 + 4.0f, F, pz1 - 4.0f}, 0.0f);
    creature(wb, CR_WRAITH, (vec3){px1 - 4.0f, F, pz0 + 4.0f}, 0.0f);

    /* the throne gate (its three seals and the slab itself are things: see quest.c) */
    room(wb, CX - 3.0f, CZ + R - 0.5f, CX + 3.0f, CZ + R + 6.0f, 6.0f, (float[4]){ -1, 0, -1, 0 });
    court_gate_box = level_add_box(wb->lv, (vec3){CX - 3.0f, F, CZ + R + 2.6f}, (vec3){CX + 3.0f, F + 5.0f, CZ + R + 3.4f});
    thing(wb, THING_SEAL, 3, (vec3){CX, F, CZ + R + 3.0f}, 0.0f);

    /* the throne room: a long hall to a dais, the King, and the bell */
    float tz0 = CZ + R + 6.0f, tz1 = CZ + 46.0f;
    room(wb, CX - 14.0f, tz0, CX + 14.0f, tz1, 9.0f, (float[4]){ 5.0f, 0, 0, 0 });
    wb_block(wb, C, WM_MARBLE, (vec3){CX - 6.0f, F, tz1 - 14.0f}, (vec3){CX + 6.0f, F + 0.4f, tz1 - 4.0f});
    wb_block(wb, C, WM_MARBLE, (vec3){CX - 4.0f, F + 0.4f, tz1 - 12.0f}, (vec3){CX + 4.0f, F + 0.8f, tz1 - 5.0f});
    wb_block(wb, C, WM_GOLD, (vec3){CX - 1.2f, F + 0.8f, tz1 - 6.5f}, (vec3){CX + 1.2f, F + 4.5f, tz1 - 5.5f});
    for (int k = 0; k < 6; k++) {
        float z = tz0 + 3.0f + k * 4.0f;
        for (int s = -1; s <= 1; s += 2) {
            wb_cylinder(wb, C, WM_CORAL_FORT, (vec3){CX + s * 9.0f, F, z}, 0.6f, 0.55f, 9.0f, 14);
            wb_solid(wb, (vec3){CX + s * 9.0f - 0.6f, F, z - 0.6f}, (vec3){CX + s * 9.0f + 0.6f, F + 9.0f, z + 0.6f});
            if (k % 2)
                glow(wb, (vec3){CX + s * 8.0f, F + 3.5f, z});
        }
    }
    thing(wb, THING_LORE, 14, (vec3){CX - 5.0f, F + 1.3f, tz0 + 1.5f}, 0.0f);
    thing(wb, THING_RELIC, 10, (vec3){CX + 11.5f, F + 0.05f, tz1 - 2.5f}, 0.0f);
    creature(wb, CR_KING, (vec3){CX, F + 0.8f, tz1 - 9.0f}, GLM_PIf);

    /* outside the glass: kelp forests, coral heads, a wreck, and fish */
    for (int k = 0; k < 60; k++) {
        float a = rnd(k, 90) * 6.2832f, r = 54.0f + rnd(k, 91) * 30.0f;
        float x = CX + cosf(a) * r, z = CZ + sinf(a) * r;
        float y = terrain_height(wb->terrain, x, z);
        if (k % 3)
            kelp(wb, (vec3){x, y, z}, 6.0f + rnd(k, 92) * 10.0f, k);
        else
            coral_cluster(wb, (vec3){x, y, z}, 50 + k, 2.0f);
    }
    Spawn *wreck = level_add_spawn(wb->lv, SPAWN_PROP, (vec3){CX + 70.0f, F - 5.5f, CZ - 24.0f}, 0.8f);
    wreck->variant = PM_SHIP_LARGE;
    wreck->normal[2] = 0.32f;
    for (int k = 0; k < 6; k++) {
        float a = k * 1.047f;
        level_add_spawn(wb->lv, SPAWN_ANIMAL, (vec3){CX + cosf(a) * 45.0f, -28.0f, CZ + sinf(a) * 45.0f}, 0.0f)->variant = SPECIES_FISH;
    }
    level_add_spawn(wb->lv, SPAWN_ANIMAL, (vec3){CX - 60.0f, -20.0f, CZ + 10.0f}, 0.0f)->variant = SPECIES_TURTLE;
}
