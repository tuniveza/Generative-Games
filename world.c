#include "world.h"
#include "game.h"
#include "shader.h"

#include <stdio.h>
#include <string.h>

int court_gate_box = -1;

/* ---------- the shape of the land ---------- */

static float smoothstep(float e0, float e1, float x)
{
    float t = glm_clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/* repeatable wobble for shapes (-1..1) */
static float wobble(float x, float z)
{
    return sinf(x * 0.071f + sinf(z * 0.053f) * 1.7f) * 0.5f + sinf(z * 0.089f - x * 0.031f) * 0.5f;
}

float coast_z(float x)
{
    /* straight along the promenade, wandering beyond it */
    float wander = 8.0f * sinf(x * 0.011f) + 5.0f * sinf(x * 0.029f + 1.3f);
    return 120.0f + wander * smoothstep(60.0f, 160.0f, fabsf(x));
}

/* height across the shore at c meters past the coast line (negative = inland) */
static float shore_profile(float c)
{
    if (c < -8.0f)
        return glm_lerp(-0.56f, -0.9f, smoothstep(-40.0f, -8.0f, c));
    if (c < 28.0f)                          /* the beach, down to the waterline */
        return glm_lerp(-0.9f, SEA_Y - 0.15f, smoothstep(-8.0f, 28.0f, c) * 0.6f + (c + 8.0f) / 36.0f * 0.4f);
    if (c < 60.0f)
        return glm_lerp(SEA_Y - 0.15f, -6.5f, (c - 28.0f) / 32.0f);
    if (c < 200.0f)
        return glm_lerp(-6.5f, -30.0f, smoothstep(60.0f, 200.0f, c));
    return glm_lerp(-30.0f, -42.0f, smoothstep(200.0f, 320.0f, c));
}

/* Old Ember: a concave cone with a crater, and ridges running down its flanks */
static float volcano_height(float x, float z)
{
    float dx = x - VOLCANO_X, dz = z - VOLCANO_Z;
    float r = sqrtf(dx * dx + dz * dz);
    if (r > VOLCANO_R)
        return -1000.0f;
    float a = atan2f(dz, dx);
    float ridges = (sinf(a * 7.0f) * 0.6f + sinf(a * 13.0f + 1.0f) * 0.4f) * 5.0f * (r / VOLCANO_R) * (1.0f - r / VOLCANO_R) * 4.0f;
    if (r < CRATER_R)                       /* the bowl of the crater */
        return glm_lerp(CRATER_Y, RIM_Y, smoothstep(CRATER_R * 0.55f, CRATER_R, r));
    float t = 1.0f - (r - CRATER_R) / (VOLCANO_R - CRATER_R);
    return RIM_Y * powf(t, 1.7f) + ridges + FLOOR_Y;
}

float world_terrain_shape(float x, float z, float h)
{
    /* the coast: the land dips to a beach and the sea floor falls away beyond */
    float c = z - coast_z(x) + wobble(x, z) * 2.0f;
    float shore = shore_profile(c);
    h = glm_lerp(h, shore, smoothstep(-45.0f, -12.0f, c));

    /* the harbour basin is dredged deep enough for ships */
    float in_x = smoothstep(MARINA_X0 - 10.0f, MARINA_X0 + 6.0f, x) * smoothstep(MARINA_X1 + 12.0f, MARINA_X1 - 6.0f, x);
    float in_z = smoothstep(MARINA_Z0 - 4.0f, MARINA_Z0 + 8.0f, z) * smoothstep(MARINA_Z1 + 30.0f, MARINA_Z1, z);
    h = glm_lerp(h, fminf(h, -7.5f), in_x * in_z);

    /* Brinewick's low hill, flat on top */
    float dv = sqrtf((x - VILLAGE_X) * (x - VILLAGE_X) + (z - VILLAGE_Z) * (z - VILLAGE_Z));
    float hill = smoothstep(VILLAGE_R + 30.0f, VILLAGE_R, dv);
    h = glm_lerp(h, VILLAGE_Y - 0.02f + wobble(x * 3.0f, z * 3.0f) * 0.05f * (dv > VILLAGE_R), hill);

    /* the volcano rises out of whatever's there */
    float v = volcano_height(x, z);
    if (v > h) {
        float k = smoothstep(v - 6.0f, v + 6.0f, v + (v - h) * 0.5f);
        h = glm_lerp(h, v, k);
    }

    /* the sea floor is leveled where the Drowned Court stands */
    float dp = sqrtf((x - PALACE_X) * (x - PALACE_X) + (z - PALACE_Z) * (z - PALACE_Z));
    h = glm_lerp(h, PALACE_FLOOR - 4.0f, smoothstep(75.0f, 55.0f, dp));
    return h;
}

Area world_area(vec3 p)
{
    float x = p[0], z = p[2];
    float dp = sqrtf((x - PALACE_X) * (x - PALACE_X) + (z - PALACE_Z) * (z - PALACE_Z));
    if (dp < 62.0f && p[1] < SEA_Y - 12.0f)
        return AREA_UNDERSEA;
    if (x > MARINA_X0 - 4.0f && x < MARINA_X1 + 14.0f && z > MARINA_Z0 - 22.0f && z < LIGHTHOUSE_Z + 12.0f)
        return AREA_MARINA;
    if (x > PROM_X0 && x < PROM_X1 + 4.0f && z > PROM_Z0 - 8.0f && z < PROM_Z1 + 3.0f)
        return AREA_PROMENADE;
    float dv = sqrtf((x - VILLAGE_X) * (x - VILLAGE_X) + (z - VILLAGE_Z) * (z - VILLAGE_Z));
    if (dv < VILLAGE_R + 16.0f)
        return AREA_VILLAGE;
    float dx = x - VOLCANO_X, dz = z - VOLCANO_Z;
    if (dx * dx + dz * dz < VOLCANO_R * VOLCANO_R)
        return AREA_VOLCANO;
    float c = z - coast_z(x);
    if (c > 40.0f)
        return AREA_OCEAN;
    if (c > -10.0f)
        return AREA_BEACH;
    return AREA_OUTSIDE;
}

/* ---------- building helpers ---------- */

static void grow(Chunk *c, vec3 mn, vec3 mx)
{
    glm_vec3_minv(c->min, mn, c->min);
    glm_vec3_maxv(c->max, mx, c->max);
}

static float tile_for(int mat)
{
    switch (mat) {
    case WM_SAND: case WM_DAMP_SAND: return 4.0f;
    case WM_THATCH: case WM_ROOF_TILES: return 2.5f;
    case WM_PLANKS: case WM_DECK: case WM_OLD_WOOD: case WM_TIMBER: return 2.0f;
    case WM_SHELL_FLOOR: return 2.0f;
    case WM_DARK_ROCK: case WM_SHORE_ROCK: case WM_BURNT: return 4.0f;
    default: return 2.5f;
    }
}

void wb_box(WorldBuild *wb, ChunkId c, int mat, vec3 min, vec3 max)
{
    vec3 center, half;
    glm_vec3_center(min, max, center);
    glm_vec3_sub(max, min, half);
    glm_vec3_scale(half, 0.5f, half);
    mat4 xf;
    glm_translate_make(xf, center);
    mb_box(&wb->chunks[c].mb[mat], xf, half, tile_for(mat));
    grow(&wb->chunks[c], min, max);
}

void wb_block(WorldBuild *wb, ChunkId c, int mat, vec3 min, vec3 max)
{
    wb_box(wb, c, mat, min, max);
    level_add_box(wb->lv, min, max);
}

void wb_solid(WorldBuild *wb, vec3 min, vec3 max)
{
    level_add_box(wb->lv, min, max);
}

static void grow_xf(Chunk *ch, mat4 xf, float r)
{
    vec3 p = { xf[3][0], xf[3][1], xf[3][2] };
    float s = fmaxf(glm_vec3_norm(xf[0]), fmaxf(glm_vec3_norm(xf[1]), glm_vec3_norm(xf[2])));
    vec3 mn = { p[0] - r * s, p[1] - r * s, p[2] - r * s }, mx = { p[0] + r * s, p[1] + r * s, p[2] + r * s };
    grow(ch, mn, mx);
}

void wb_xbox(WorldBuild *wb, ChunkId c, int mat, mat4 xf, vec3 half)
{
    mb_box(&wb->chunks[c].mb[mat], xf, half, tile_for(mat));
    grow_xf(&wb->chunks[c], xf, glm_vec3_norm(half));
}

void wb_xcylinder(WorldBuild *wb, ChunkId c, int mat, mat4 xf, float r0, float r1, float height, int segments)
{
    mb_cylinder(&wb->chunks[c].mb[mat], xf, r0, r1, height, segments, tile_for(mat));
    grow_xf(&wb->chunks[c], xf, fmaxf(fmaxf(r0, r1), height));
}

void wb_cylinder(WorldBuild *wb, ChunkId c, int mat, vec3 base, float r0, float r1, float height, int segments)
{
    mat4 xf;
    glm_translate_make(xf, base);
    wb_xcylinder(wb, c, mat, xf, r0, r1, height, segments);
}

void wb_ball(WorldBuild *wb, ChunkId c, int mat, vec3 center, vec3 radii, int segments)
{
    mat4 xf;
    glm_translate_make(xf, center);
    glm_scale(xf, radii);
    mb_capsule(&wb->chunks[c].mb[mat], xf, 1.0f, 0.0f, segments);
    vec3 mn, mx;
    glm_vec3_sub(center, radii, mn);
    glm_vec3_add(center, radii, mx);
    grow(&wb->chunks[c], mn, mx);
}

void wb_wedge(WorldBuild *wb, ChunkId c, int mat, mat4 xf, vec3 half)
{
    mb_wedge(&wb->chunks[c].mb[mat], xf, half, tile_for(mat));
    grow_xf(&wb->chunks[c], xf, glm_vec3_norm(half));
}

/* one wall of a house: from a to b (along x or z) with openings cut in it */
typedef struct { float at, width, bottom, top; } Opening;

static void wall_run(WorldBuild *wb, const House *h, bool along_x, float fixed, float a, float b,
                     const Opening *holes, int nholes)
{
    float t = 0.15f, y0 = h->pos[1] - 0.2f, y1 = h->pos[1] + h->height;
    /* the solid spans between openings, then above and below each */
    float cur = a;
    for (int i = 0; i <= nholes; i++) {
        float end = i < nholes ? holes[i].at - holes[i].width * 0.5f : b;
        if (end > cur + 0.01f) {
            vec3 mn = { along_x ? cur : fixed - t, y0, along_x ? fixed - t : cur };
            vec3 mx = { along_x ? end : fixed + t, y1, along_x ? fixed + t : end };
            wb_block(wb, h->chunk, h->wall, mn, mx);
        }
        if (i < nholes) {
            float s0 = holes[i].at - holes[i].width * 0.5f, s1 = holes[i].at + holes[i].width * 0.5f;
            float spans[2][2] = { { y0, h->pos[1] + holes[i].bottom }, { h->pos[1] + holes[i].top, y1 } };
            for (int k = 0; k < 2; k++) {
                if (spans[k][1] <= spans[k][0] + 0.01f)
                    continue;
                vec3 mn = { along_x ? s0 : fixed - t, spans[k][0], along_x ? fixed - t : s0 };
                vec3 mx = { along_x ? s1 : fixed + t, spans[k][1], along_x ? fixed + t : s1 };
                wb_block(wb, h->chunk, h->wall, mn, mx);
            }
            /* a painted frame around windows and doors */
            vec3 fmn = { along_x ? s0 - 0.08f : fixed - t - 0.04f, h->pos[1] + holes[i].top, along_x ? fixed - t - 0.04f : s0 - 0.08f };
            vec3 fmx = { along_x ? s1 + 0.08f : fixed + t + 0.04f, h->pos[1] + holes[i].top + 0.12f, along_x ? fixed + t + 0.04f : s1 + 0.08f };
            wb_box(wb, h->chunk, h->trim, fmn, fmx);
            cur = s1;
        }
    }
}

void wb_house(WorldBuild *wb, const House *h)
{
    float x0 = h->pos[0] - h->w * 0.5f, x1 = h->pos[0] + h->w * 0.5f;
    float z0 = h->pos[2] - h->d * 0.5f, z1 = h->pos[2] + h->d * 0.5f;
    float y = h->pos[1], top = y + h->height;

    /* floor on a stone footing that reaches down into the slope */
    wb_block(wb, h->chunk, WM_STONE, (vec3){x0 - 0.2f, y - 3.0f, z0 - 0.2f}, (vec3){x1 + 0.2f, y - 0.2f, z1 + 0.2f});
    wb_block(wb, h->chunk, h->floor, (vec3){x0, y - 0.2f, z0}, (vec3){x1, y, z1});

    /* walls: south (+z), east, north, west */
    for (int side = 0; side < 4; side++) {
        bool along_x = side == 0 || side == 2;
        float fixed = side == 0 ? z1 : side == 1 ? x1 : side == 2 ? z0 : x0;
        float a = along_x ? x0 : z0, b = along_x ? x1 : z1;
        float len = b - a, mid = (a + b) * 0.5f;
        Opening holes[3];
        int n = 0;
        if (side == h->door_side) {
            if (h->open_front) {
                holes[n++] = (Opening){ mid, len - 1.2f, 0.95f, h->height - 0.5f };
            } else {
                if (len > 5.0f)
                    holes[n++] = (Opening){ a + len * 0.22f, 0.9f, 1.1f, 2.0f };
                holes[n++] = (Opening){ mid, 1.3f, 0.0f, 2.3f };
                if (len > 5.0f)
                    holes[n++] = (Opening){ b - len * 0.22f, 0.9f, 1.1f, 2.0f };
            }
        } else if (len > 3.0f) {
            holes[n++] = (Opening){ mid, 0.9f, 1.1f, 2.0f };
        }
        wall_run(wb, h, along_x, fixed, a, b, holes, n);
    }

    /* dark timbers at the corners and along the top */
    for (int cx = 0; cx < 2; cx++)
        for (int cz = 0; cz < 2; cz++) {
            float px = cx ? x1 : x0, pz = cz ? z1 : z0;
            wb_box(wb, h->chunk, h->trim, (vec3){px - 0.2f, y - 0.2f, pz - 0.2f}, (vec3){px + 0.2f, top, pz + 0.2f});
        }
    wb_box(wb, h->chunk, h->trim, (vec3){x0 - 0.2f, top - 0.25f, z0 - 0.2f}, (vec3){x1 + 0.2f, top, z0 + 0.1f});
    wb_box(wb, h->chunk, h->trim, (vec3){x0 - 0.2f, top - 0.25f, z1 - 0.1f}, (vec3){x1 + 0.2f, top, z1 + 0.2f});
    /* a ceiling you'll never touch, so the rain and the sky know it's indoors */
    wb_solid(wb, (vec3){x0, top, z0}, (vec3){x1, top + 0.1f, z1});

    /* gabled roof along the longer side */
    bool ridge_x = h->w >= h->d;
    float span = ridge_x ? h->d : h->w, run = ridge_x ? h->w : h->d;
    float rise = span * (h->thatch ? 0.62f : 0.38f);
    float over = h->thatch ? 0.6f : 0.4f;
    mat4 xf;
    /* gable ends in the wall's plaster */
    for (int e = -1; e <= 1; e += 2) {
        glm_translate_make(xf, (vec3){h->pos[0] + (ridge_x ? e * (run * 0.5f - 0.15f) : 0.0f), top + rise * 0.5f,
                                      h->pos[2] + (ridge_x ? 0.0f : e * (run * 0.5f - 0.15f))});
        if (ridge_x)
            glm_rotate_y(xf, GLM_PI_2f, xf);
        wb_wedge(wb, h->chunk, h->wall, xf, (vec3){span * 0.5f, rise * 0.5f, 0.15f});
    }
    /* two slopes, overhanging the walls */
    float slope_len = sqrtf((span * 0.5f + over) * (span * 0.5f + over) + rise * rise * (1.0f + 2.0f * over / span) * (1.0f + 2.0f * over / span));
    float angle = atan2f(rise, span * 0.5f);
    float thick = h->thatch ? 0.35f : 0.12f;
    for (int s = -1; s <= 1; s += 2) {
        vec3 c = { h->pos[0], top + rise * 0.5f - (over * 0.5f) * tanf(angle), h->pos[2] };
        if (ridge_x)
            c[2] += s * (span * 0.25f + over * 0.5f);
        else
            c[0] += s * (span * 0.25f + over * 0.5f);
        glm_translate_make(xf, c);
        if (ridge_x)
            glm_rotate_x(xf, s * angle, xf);
        else
            glm_rotate_z(xf, -s * angle, xf);
        vec3 half = { ridge_x ? run * 0.5f + over : slope_len * 0.5f, thick, ridge_x ? slope_len * 0.5f : run * 0.5f + over };
        wb_xbox(wb, h->chunk, h->roof, xf, half);
    }
    /* the ridge beam */
    glm_translate_make(xf, (vec3){h->pos[0], top + rise + thick * 0.5f, h->pos[2]});
    wb_xbox(wb, h->chunk, h->thatch ? h->roof : h->trim, xf,
            (vec3){ridge_x ? run * 0.5f + over : 0.2f, 0.14f, ridge_x ? 0.2f : run * 0.5f + over});
}

float wb_ground(WorldBuild *wb, float x, float z)
{
    float t = terrain_height(wb->terrain, x, z);
    float b = level_ground(wb->lv, x, z, 1000.0f, 0.0f);
    return fmaxf(t, b);
}

Spawn *wb_prop(WorldBuild *wb, int model, vec3 pos, float yaw, float scale)
{
    Spawn *s = level_add_spawn(wb->lv, SPAWN_PROP, pos, yaw);
    s->variant = model;
    s->normal[0] = scale;
    return s;
}

/* ---------- the finished world ---------- */

static void make_materials(Material mats[WM_COUNT])
{
    static const char *dirs[WM_COUNT] = {
        [WM_SAND] = "coast_sand_01", [WM_DAMP_SAND] = "damp_beach_sand",
        [WM_PLASTER_BLUE] = "blue_plaster_weathered", [WM_PLASTER_RED] = "red_plaster_weathered",
        [WM_PLASTER_YELLOW] = "yellow_plaster", [WM_PLASTER_WHITE] = "white_plaster_rough_01",
        [WM_THATCH] = "thatch_roof_angled", [WM_ROOF_TILES] = "clay_roof_tiles_02",
        [WM_SHELL_FLOOR] = "shell_floor_01", [WM_PLANKS] = "weathered_planks", [WM_DECK] = "wood_floor_deck",
        [WM_SEA_TILES] = "seaworn_stone_tiles", [WM_SEA_BRICK] = "seaworn_sandstone_brick",
        [WM_CORAL_WALL] = "coral_stone_wall", [WM_CORAL_FORT] = "coral_fort_wall_01",
        [WM_CORAL_GROUND] = "coral_ground_02", [WM_DARK_ROCK] = "dark_rock", [WM_PALM_BARK] = "palm_bark",
        [WM_SHORE_ROCK] = "low_tide_rocks", [WM_OLD_WOOD] = "old_planks_02", [WM_TIMBER] = "old_wood_floor",
        [WM_MARBLE] = "marble_01", [WM_STONE] = "medieval_blocks_02", [WM_BURNT] = "burned_ground_01",
        [WM_COBBLE] = "mossy_cobblestone",
    };
    for (int i = 0; i < WM_COUNT; i++) {
        if (dirs[i]) {
            char path[128];
            snprintf(path, sizeof path, "assets/textures/%s", dirs[i]);
            material_from_dir(&mats[i], path);
        }
    }
    material_color(&mats[WM_LEAVES], 0.08f, 0.26f, 0.06f, 0.6f, 0.0f);
    material_color(&mats[WM_BLOSSOM], 0.42f, 0.2f, 0.62f, 0.7f, 0.0f);
    material_color(&mats[WM_BLOSSOM_PALE], 0.62f, 0.42f, 0.78f, 0.7f, 0.0f);
    material_color(&mats[WM_CLOTH_RED], 0.62f, 0.12f, 0.09f, 0.9f, 0.0f);
    material_color(&mats[WM_CLOTH_BLUE], 0.1f, 0.3f, 0.55f, 0.9f, 0.0f);
    material_color(&mats[WM_CLOTH_WHITE], 0.78f, 0.76f, 0.7f, 0.9f, 0.0f);
    material_color(&mats[WM_CLOTH_YELLOW], 0.8f, 0.6f, 0.15f, 0.9f, 0.0f);
    material_color(&mats[WM_BRASS], 0.75f, 0.55f, 0.25f, 0.3f, 1.0f);
    material_color(&mats[WM_IRON], 0.12f, 0.12f, 0.13f, 0.5f, 1.0f);
    material_color(&mats[WM_GOLD], 0.9f, 0.65f, 0.22f, 0.22f, 1.0f);
    material_color(&mats[WM_ROPE], 0.45f, 0.36f, 0.22f, 0.95f, 0.0f);
    material_color(&mats[WM_OBSIDIAN], 0.02f, 0.018f, 0.025f, 0.08f, 0.0f);
    material_color(&mats[WM_CORAL_PINK], 0.85f, 0.35f, 0.45f, 0.7f, 0.0f);
    material_color(&mats[WM_CORAL_ORANGE], 0.9f, 0.42f, 0.12f, 0.7f, 0.0f);
    material_color(&mats[WM_KELP], 0.12f, 0.3f, 0.08f, 0.6f, 0.0f);
    material_color(&mats[WM_PAINT_TEAL], 0.1f, 0.45f, 0.42f, 0.6f, 0.0f);
    material_color(&mats[WM_PAINT_CORAL], 0.85f, 0.38f, 0.3f, 0.6f, 0.0f);
    material_color(&mats[WM_LAMP_GLOW], 0.9f, 0.7f, 0.4f, 0.4f, 0.0f);
    glm_vec3_copy((vec3){5.0f, 3.2f, 1.4f}, mats[WM_LAMP_GLOW].emissive);
    material_color(&mats[WM_PAPER_PINK], 0.9f, 0.5f, 0.6f, 0.8f, 0.0f);
    glm_vec3_copy((vec3){2.6f, 0.9f, 1.1f}, mats[WM_PAPER_PINK].emissive);
    material_color(&mats[WM_PAPER_CYAN], 0.5f, 0.85f, 0.85f, 0.8f, 0.0f);
    glm_vec3_copy((vec3){0.6f, 2.2f, 2.3f}, mats[WM_PAPER_CYAN].emissive);
    material_color(&mats[WM_PAPER_GOLD], 0.95f, 0.8f, 0.4f, 0.8f, 0.0f);
    glm_vec3_copy((vec3){3.0f, 2.0f, 0.6f}, mats[WM_PAPER_GOLD].emissive);
    material_color(&mats[WM_PEARL], 0.8f, 0.85f, 0.9f, 0.15f, 0.0f);
    glm_vec3_copy((vec3){0.8f, 1.6f, 2.0f}, mats[WM_PEARL].emissive);
    material_color(&mats[WM_EMBER], 0.3f, 0.05f, 0.0f, 0.8f, 0.0f);
    glm_vec3_copy((vec3){7.0f, 1.8f, 0.2f}, mats[WM_EMBER].emissive);
    material_color(&mats[WM_RUNE], 0.05f, 0.1f, 0.12f, 0.4f, 0.0f);
    glm_vec3_copy((vec3){0.3f, 2.2f, 2.6f}, mats[WM_RUNE].emissive);
    material_color(&mats[WM_GLASS], 0.55f, 0.8f, 0.85f, 0.04f, 0.0f);
    mats[WM_GLASS].base_color[3] = 0.18f;
    mats[WM_GLASS].alpha_mode = ALPHA_BLEND;
    mats[WM_GLASS].double_sided = true;
    material_color(&mats[WM_BUBBLE], 0.7f, 0.9f, 1.0f, 0.05f, 0.0f);
    mats[WM_BUBBLE].base_color[3] = 0.25f;
    mats[WM_BUBBLE].alpha_mode = ALPHA_BLEND;
    glm_vec3_copy((vec3){0.1f, 0.3f, 0.35f}, mats[WM_BUBBLE].emissive);
}

/* the crater's lava lake: a wide disc, rippled and glowing in shaders/lava.frag */
static void build_lava(World *w)
{
    MeshBuilder mb;
    mb_init(&mb);
    mat4 xf;
    glm_translate_make(xf, (vec3){VOLCANO_X, LAVA_Y - 0.3f, VOLCANO_Z});
    mb_cylinder(&mb, xf, CRATER_R * 0.86f, CRATER_R * 0.86f, 0.3f, 48, 4.0f);
    Material m;
    material_color(&m, 1, 0.3f, 0.05f, 0.6f, 0.0f);
    model_from_builders(&w->lava, &mb, &m, 1);
    mb_free(&mb);
    w->lava_prog = shader_load("shaders/model.vert", "shaders/lava.frag");
}

/* the ruins' statues were far too small: stand them on plinths, at life size and more */
static void statues(WorldBuild *wb)
{
    Level *lv = wb->lv;
    int count = lv->spawn_count;
    for (int i = 0; i < count; i++) {
        Spawn *s = &lv->spawns[i];
        if (s->kind == SPAWN_STATUE) {
            vec3 c;
            glm_vec3_copy(s->pos, c);
            wb_block(wb, WC_COAST, WM_MARBLE, (vec3){c[0] - 1.25f, c[1], c[2] - 1.25f}, (vec3){c[0] + 1.25f, c[1] + 0.25f, c[2] + 1.25f});
            wb_block(wb, WC_COAST, WM_STONE, (vec3){c[0] - 1.0f, c[1] + 0.25f, c[2] - 1.0f}, (vec3){c[0] + 1.0f, c[1] + 0.95f, c[2] + 1.0f});
            s->pos[1] += 0.95f;
        }
        if ((s->kind == SPAWN_DOOR || s->kind == SPAWN_DOOR_LOCKED) && s->pos[2] < -55.0f) {
            /* great stone horses flank the palace door (the model is a 22 cm figurine) */
            for (int side = -1; side <= 1; side += 2) {
                vec3 at = { s->pos[0] + side * 6.5f, FLOOR_Y, s->pos[2] + 7.5f };
                wb_block(wb, WC_COAST, WM_MARBLE, (vec3){at[0] - 2.0f, at[1], at[2] - 1.5f}, (vec3){at[0] + 2.0f, at[1] + 0.3f, at[2] + 1.5f});
                wb_block(wb, WC_COAST, WM_STONE, (vec3){at[0] - 1.7f, at[1] + 0.3f, at[2] - 1.2f}, (vec3){at[0] + 1.7f, at[1] + 1.4f, at[2] + 1.2f});
                wb_box(wb, WC_COAST, WM_MARBLE, (vec3){at[0] - 1.8f, at[1] + 1.4f, at[2] - 1.3f}, (vec3){at[0] + 1.8f, at[1] + 1.55f, at[2] + 1.3f});
                wb_prop(wb, PM_HORSE, (vec3){at[0], at[1] + 1.55f, at[2]}, side * (GLM_PI_2f + 0.35f), 13.0f);
            }
        }
    }
}

void world_build(World *w, Level *lv, const Terrain *t)
{
    memset(w, 0, sizeof *w);
    static WorldBuild wb;
    memset(&wb, 0, sizeof wb);
    wb.lv = lv;
    wb.terrain = t;
    for (int c = 0; c < WC_COUNT; c++) {
        for (int m = 0; m < WM_COUNT; m++)
            mb_init(&wb.chunks[c].mb[m]);
        glm_vec3_fill(wb.chunks[c].min, 1e9f);
        glm_vec3_fill(wb.chunks[c].max, -1e9f);
    }

    statues(&wb);
    coast_build(&wb);
    village_build(&wb);
    volcano_build(&wb);
    undersea_build(&wb);

    Material mats[WM_COUNT];
    make_materials(mats);
    for (int c = 0; c < WC_COUNT; c++) {
        Chunk *ch = &wb.chunks[c];
        WorldChunk *out = &w->chunks[c];
        model_from_builders(&out->model, ch->mb, mats, WM_COUNT);
        if (ch->min[0] < ch->max[0]) {
            glm_vec3_center(ch->min, ch->max, out->center);
            out->radius = glm_vec3_distance(ch->min, ch->max) * 0.5f;
        }
        for (int m = 0; m < WM_COUNT; m++)
            mb_free(&ch->mb[m]);
    }
    build_lava(w);
}

void world_free(World *w)
{
    for (int c = 0; c < WC_COUNT; c++)
        model_free(&w->chunks[c].model);
    model_free(&w->lava);
    glDeleteProgram(w->lava_prog);
}

void world_draw(World *w, GLuint prog, mat4 view_proj, vec3 eye, bool depth)
{
    Frustum fr;
    frustum_from(view_proj, &fr);
    DrawParams dp = { .depth_only = depth };
    mat4 ident = GLM_MAT4_IDENTITY_INIT;
    for (int c = 0; c < WC_COUNT; c++) {
        WorldChunk *ch = &w->chunks[c];
        if (ch->radius <= 0.0f)
            continue;
        float far = depth ? 120.0f : 900.0f;
        if (glm_vec3_distance(ch->center, eye) > far + ch->radius || !frustum_sphere(&fr, ch->center, ch->radius))
            continue;
        /* the Drowned Court is only seen from inside it (or diving right over it) */
        if (c == WC_UNDERSEA && eye[1] > SEA_Y + 2.0f)
            continue;
        model_draw(&ch->model, NULL, prog, view_proj, ident, &dp);
    }
}

void world_draw_lava(World *w, mat4 view_proj)
{
    mat4 ident = GLM_MAT4_IDENTITY_INIT;
    DrawParams dp = { .no_material = true };
    model_draw(&w->lava, NULL, w->lava_prog, view_proj, ident, &dp);
}
