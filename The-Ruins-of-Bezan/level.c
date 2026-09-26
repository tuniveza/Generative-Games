#include "level.h"
#include "meshgen.h"
#include "world.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The world, one character per 4x4 m cell (north is up, -Z in the world). Three layers:
 * the ground, an upper floor, and the crypt below. tools/compose_map.py puts them
 * together from smaller blocks and checks that doors sit in walls.
 *
 * ground:
 *   #  ruin wall       %  crumbled low wall   D  door           L  locked door
 *   .  stone floor     :  cobbled path        ,  rubble on grass (space) grass
 *   P  pillar          p  broken pillar       A  altar (world origin)
 *   T  wall torch      l  lantern             F  fire pit
 *   C  chest           G  golden chest        S  player start
 *   r  rat  f  fox  g  goblin  j  slime       x  dagger   h  shield
 *   s  statue  b  barrel  c  crate  v  vase  o  boulder
 *   y  shrub  Y  ferns and flowers  e  bench  U  fountain
 *   B  bookshelf  q  fallen books  w  flooded floor (knee-deep water)
 *   ^  stairs up (rising north)   <  stairs down to the crypt (descending south)
 *   palace:  W wall  I window  O great door  M marble  m parquet  R carpet
 *            Q column  H throne  * chandelier  u potted plant  1-6 characters
 * upper:  _  floor (parapets grow along its edges)   C g l as above
 * crypt:  #  rock  D  archway  .  floor  P  pillar  T  torch  Z  tomb  X  great tomb
 *         z  sleeping skeleton  j  slime  C  chest */
static const char *MAP_GROUND[] = {
    "                                              ",
    "          WWWIWWWIWWWWIWWIWWWWIWWWIW          ",
    "          WBmmBmBWMMMMMHHMMMMMWm5mmW          ",
    "          WmmmmmmmWQm*MMMM*mQWmmmmmW          ",
    "          Wmm2mmmmWMRMMMMMMRMWmm*mmW          ",
    "          WBm*mmmBWQMRM1MMRMQWmmmmmW          ",
    "          WmmmmmmmWMMRMMMMRMMWummmuW          ",
    "          WmmmummmMM*RMMMMR*MMmmm4mW          ",
    "          WBmmmmmmWQMRMMMMRMQWmmmmmW          ",
    "          WmmmmmmmWMMRMMMMRMMWmmemmW          ",
    "          WBBmmBBmWQ*RMMMMR*QWummmuW          ",
    "          WWWWWWWWWMMRMMMMRMMWWWWWWW          ",
    "          WmummmumWQMRMMMMRMQWmummuW          ",
    "          Wmmm*mmmMMMRMMMMRMMMmm3mmW          ",
    "          WWWWWWWWWWWWWOWWWWWWWWWWWW          ",
    "                     Q::6Q                    ",
    "           yy  Y  e ::::::: e  Y  yy          ",
    "          Y  y   Y  :::::::  Y   y  Y         ",
    "         y  Y  y    ::l::::    y  Y   y       ",
    "          ####%%#######D#######%%###          ",
    "          #T.....h....#.....#......########## ",
    "          #..P..P..P..#..r..#.^.C..##B.B.B.T# ",
    "          #...........D.....D.^..gT##...q...# ",
    "          #..P..P..P..#..C..#......##B.l.B.g# ",
    "          #T.....r....#..b..#v.f..c##...q...# ",
    " %%%%#%%%%####D########%%D####%%#D###B.B..B.# ",
    " %yY y Yy%%    ,    ,      ,       %#T.....l# ",
    " % ::::: %%  p   s        s    p   %###D##### ",
    " %Y:   :YDD    ,   ..........  ,   D........% ",
    " % : U : %%  ........A...........  %#####D### ",
    " %y:g   :%%    ,   ..........      %#wwwwwww# ",
    " % :   :y%%  p   s   S x  s    p   %#wjwwwww# ",
    " %Y:::::Y%%  ,       ,      ,  o   %#wwwwwjw# ",
    " %yy e yy%####D#######%%###D#########wwwwwww# ",
    " %  Y  j %#....T.#b...........#...G##wwwjwww# ",
    " %%%%D%%%%#.r....#..P.....P...#....##T.www.T# ",
    "          #...C..D....<r......L..l.########## ",
    "          #v.....#..P.<...P.f.#....#          ",
    "          #c..l..#...........T#T...#          ",
    "          ####%%#####%%#######%#####          ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
};
static const char *MAP_UPPER[] = {
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                             ______           ",
    "                             _ C___           ",
    "                             _ _g__           ",
    "                             __l___           ",
    "                             ______           ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
};
static const char *MAP_CRYPT[] = {
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "                                              ",
    "          #########################           ",
    "          #Z.Z.Z#.......#Z.T..j..Z#           ",
    "          #.....#..z.z..#.........#           ",
    "          #..z..D...P...D.z..P..z.#           ",
    "          #T....#.......#.........#           ",
    "          ###D#########D#####D#####           ",
    "          #........#.....#.......T#           ",
    "          #.j..T...D..X..D..z..Z..#           ",
    "          #........#.....#........#           ",
    "          #########################           ",
    "                                              ",
    "                                              ",
};

/* map regions, in cells, for music and ambience (see tools/compose_map.py) */
typedef struct { int x0, y0, x1, y1; Area area; } Region;
static const Region REGIONS[] = {
    { 10,  1, 35, 14, AREA_PALACE },
    {  1, 25,  9, 35, AREA_GARDEN },
    {  8, 15, 38, 18, AREA_GARDEN },
    { 36, 20, 44, 27, AREA_LIBRARY },
    { 36, 29, 44, 36, AREA_FLOODED },
    { 10, 19, 35, 39, AREA_RUINS },
};

enum {
    MAT_BRICK, MAT_BLOCKS, MAT_MOSSY, MAT_FLOOR, MAT_COBBLE, MAT_PILLAR,
    MAT_MARBLE_FLOOR, MAT_MARBLE, MAT_PALACE_WALL, MAT_PARQUET, MAT_VELVET,
    MAT_WOOD_FLOOR, MAT_CRYPT_WALL, MAT_CRYPT_FLOOR, MAT_GOLD,
    MAT_COUNT
};

static const char *MAT_DIRS[MAT_COUNT] = {
    [MAT_BRICK] = "castle_brick_07", [MAT_BLOCKS] = "medieval_blocks_02",
    [MAT_MOSSY] = "mossy_stone_wall", [MAT_FLOOR] = "monastery_stone_floor",
    [MAT_COBBLE] = "mossy_cobblestone", [MAT_PILLAR] = "medieval_blocks_02",
    [MAT_MARBLE_FLOOR] = "marble_mosaic_tiles", [MAT_MARBLE] = "marble_01",
    [MAT_PALACE_WALL] = "white_sandstone_blocks_02", [MAT_PARQUET] = "herringbone_parquet",
    [MAT_VELVET] = "velour_velvet", [MAT_WOOD_FLOOR] = "old_wood_floor",
    [MAT_CRYPT_WALL] = "sandstone_blocks_08", [MAT_CRYPT_FLOOR] = "mossy_sandstone",
};
static const float MAT_TILE[MAT_COUNT] = {
    2.5f, 3.0f, 3.0f, 3.0f, 2.5f, 2.0f, 2.0f, 2.5f, 3.0f, 2.0f, 1.5f, 2.5f, 3.0f, 3.0f, 1.0f,
};

/* repeatable random number in 0..1 for a cell and a purpose */
static float rnd(int x, int y, int salt)
{
    uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)salt * 83492791u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (h & 0xffffff) / (float)0xffffff;
}

char level_cell(const Level *lv, Layer layer, int x, int y)
{
    if (x < 0 || y < 0 || x >= lv->w || y >= lv->h)
        return ' ';
    return lv->cells[layer][y * lv->w + x];
}

static char G(const Level *lv, int x, int y) { return level_cell(lv, LAYER_GROUND, x, y); }
static char U(const Level *lv, int x, int y) { return level_cell(lv, LAYER_UPPER, x, y); }
static char K(const Level *lv, int x, int y) { return level_cell(lv, LAYER_CRYPT, x, y); }

static bool in_area(int x, int y, Area area)
{
    for (size_t i = 0; i < sizeof REGIONS / sizeof REGIONS[0]; i++) {
        const Region *r = &REGIONS[i];
        if (r->area == area && x >= r->x0 && x <= r->x1 && y >= r->y0 && y <= r->y1)
            return true;
    }
    return false;
}

static bool in_palace(int x, int y) { return in_area(x, y, AREA_PALACE); }
static bool is_wall(char c) { return c == '#' || c == '%' || c == 'D' || c == 'L' || c == 'W' || c == 'I' || c == 'O'; }
static bool is_solid_wall(char c) { return c == '#' || c == '%' || c == 'W' || c == 'I'; }
static bool is_stairs(char c) { return c == '^' || c == '<'; }
static bool has_floor(char c)
{
    return c != ' ' && c != ',' && !is_solid_wall(c) && c != '<' && c != 'y' && c != 'Y' && c != 'w';
}

void level_cell_center(const Level *lv, int cx, int cy, vec3 out)
{
    out[0] = (cx - lv->origin_x) * CELL;
    out[1] = FLOOR_Y;
    out[2] = (cy - lv->origin_y) * CELL;
}

void level_world_to_cell(const Level *lv, float x, float z, int *cx, int *cy)
{
    *cx = (int)floorf(x / CELL + 0.5f) + lv->origin_x;
    *cy = (int)floorf(z / CELL + 0.5f) + lv->origin_y;
}

char level_cell_at(const Level *lv, float x, float z)
{
    int cx, cy;
    level_world_to_cell(lv, x, z, &cx, &cy);
    return G(lv, cx, cy);
}

Area level_area(const Level *lv, vec3 p)
{
    int cx, cy;
    level_world_to_cell(lv, p[0], p[2], &cx, &cy);
    if (p[1] < FLOOR_Y - 2.0f && p[1] > CRYPT_Y - 3.0f && K(lv, cx, cy) != ' ')
        return AREA_CRYPT;
    if (p[1] > UPPER_Y - 0.6f && U(lv, cx, cy) != ' ')
        return AREA_UPPER;
    for (size_t i = 0; i < sizeof REGIONS / sizeof REGIONS[0]; i++) {
        const Region *r = &REGIONS[i];
        if (cx >= r->x0 && cx <= r->x1 && cy >= r->y0 && cy <= r->y1)
            return r->area;
    }
    return world_area(p);
}

int level_add_box(Level *lv, vec3 min, vec3 max)
{
    if (lv->box_count == lv->box_cap) {
        lv->box_cap = lv->box_cap ? lv->box_cap * 2 : 1024;
        lv->boxes = realloc(lv->boxes, lv->box_cap * sizeof *lv->boxes);
    }
    Box *b = &lv->boxes[lv->box_count];
    glm_vec3_copy(min, b->min);
    glm_vec3_copy(max, b->max);
    b->off = false;
    return lv->box_count++;
}

Spawn *level_add_spawn(Level *lv, SpawnKind kind, vec3 pos, float yaw)
{
    if (lv->spawn_count == lv->spawn_cap) {
        lv->spawn_cap = lv->spawn_cap ? lv->spawn_cap * 2 : 128;
        lv->spawns = realloc(lv->spawns, lv->spawn_cap * sizeof *lv->spawns);
    }
    int index = 0;
    for (int i = 0; i < lv->spawn_count; i++)
        if (lv->spawns[i].kind == kind)
            index++;
    Spawn *s = &lv->spawns[lv->spawn_count++];
    memset(s, 0, sizeof *s);
    s->kind = kind;
    glm_vec3_copy(pos, s->pos);
    s->yaw = yaw;
    s->index = index;
    return s;
}

static Spawn *add_spawn(Level *lv, SpawnKind kind, vec3 pos, float yaw)
{
    return level_add_spawn(lv, kind, pos, yaw);
}

static void add_hole(Level *lv, Hole h)
{
    if (lv->hole_count >= MAX_HOLES) {
        fprintf(stderr, "level: more than %d holes, ignoring one\n", MAX_HOLES);
        return;
    }
    lv->holes[lv->hole_count++] = h;
}

/* ---------- geometry helpers ---------- */

typedef struct {
    Level *lv;
    MeshBuilder mb[MAT_COUNT];
    MeshBuilder water;
} Build;

static void box_mesh(MeshBuilder *mb, vec3 min, vec3 max, float tile)
{
    vec3 center, half;
    glm_vec3_center(min, max, center);
    glm_vec3_sub(max, min, half);
    glm_vec3_scale(half, 0.5f, half);
    mat4 xf;
    glm_translate_make(xf, center);
    mb_box(mb, xf, half, tile);
}

/* axis-aligned block that's also solid */
static void block(Build *b, int mat, vec3 min, vec3 max)
{
    box_mesh(&b->mb[mat], min, max, MAT_TILE[mat]);
    level_add_box(b->lv, min, max);
}
    static float change = 1.0f;
static void pillar(Build *b, int mat, float x, float y, float z, float height, float r, bool capital)
{
    mat4 xf;
   
    block(b, mat, (vec3){x - r - 0.13f, y, z - r - 0.13f}, (vec3){x + r + 0.13f, y + 0.35f, z + r + 0.13f});
    glm_translate_make(xf, (vec3){x, y + 0.35f, z});
    mb_cylinder(&b->mb[mat], xf, r, r * 0.9f, height - 0.35f - (capital ? 0.4f : 0.0f), 20, MAT_TILE[mat]);
    level_add_box(b->lv, (vec3){x - r, y, z - r}, (vec3){x + r, y + height, z + r});
  ;
  if (capital) {
        glm_translate_make(xf, (vec3){x, y + height - 0.2f, z});
        glm_rotate(xf, change, (vec3){1.0f, 0.0f, 0.0f});
        
        mb_box(&b->mb[mat], xf, (vec3){r + 0.18f, 0.2f, r + 0.18f}, MAT_TILE[mat]);
    }
}

/* a heap of tumbled stones */
static void rubble(Build *b, int mat, vec3 c, int cx, int cy, int count, float spread)
{
    for (int i = 0; i < count; i++) {
        float s = 0.2f + rnd(cx, cy, 100 + i) * 0.45f;
        float px = c[0] + (rnd(cx, cy, 200 + i) - 0.5f) * spread;
        float pz = c[2] + (rnd(cx, cy, 300 + i) - 0.5f) * spread;
        mat4 xf;
        glm_translate_make(xf, (vec3){px, c[1] + s * 0.7f - 0.05f, pz});
        glm_rotate(xf, rnd(cx, cy, 400 + i) * GLM_PIf, (vec3){0, 1, 0});
        glm_rotate(xf, (rnd(cx, cy, 500 + i) - 0.5f) * 0.6f, (vec3){1, 0, 0});
        vec3 half = { s * (1.0f + rnd(cx, cy, 600 + i)), s * 0.7f, s };
        mb_box(&b->mb[mat], xf, half, 2.0f);
        if (s > 0.4f)       /* only the big ones are worth bumping into */
            level_add_box(b->lv, (vec3){px - s, c[1] - 0.1f, pz - s}, (vec3){px + s, c[1] + s * 1.2f, pz + s});
    }
}

/* a wall along one edge of a cell, e.g. a parapet */
static void edge_wall(Build *b, int mat, vec3 c, int dx, int dy, float y0, float y1, float thick)
{
    float e = CELL * 0.5f;
    vec3 mn, mx;
    if (dx) {
        float x = c[0] + dx * (e - thick * 0.5f);
        glm_vec3_copy((vec3){x - thick * 0.5f, y0, c[2] - e}, mn);
        glm_vec3_copy((vec3){x + thick * 0.5f, y1, c[2] + e}, mx);
    } else {
        float z = c[2] + dy * (e - thick * 0.5f);
        glm_vec3_copy((vec3){c[0] - e, y0, z - thick * 0.5f}, mn);
        glm_vec3_copy((vec3){c[0] + e, y1, z + thick * 0.5f}, mx);
    }
    block(b, mat, mn, mx);
}

/* ---------- ground walls ---------- */

static int wall_material(int cx, int cy)
{
    if (in_palace(cx, cy))
        return MAT_PALACE_WALL;
    /* regions of 3x3 cells share a stone type, so it doesn't look like a patchwork */
    float r = rnd(cx / 3, cy / 3, 7);
    return r < 0.4f ? MAT_BLOCKS : r < 0.75f ? MAT_BRICK : MAT_MOSSY;
}

/* does the upper floor rest on this wall? */
static bool carries_upper(const Level *lv, int cx, int cy)
{
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (U(lv, cx + dx, cy + dy) == '_' || U(lv, cx + dx, cy + dy) == 'C' ||
                U(lv, cx + dx, cy + dy) == 'g' || U(lv, cx + dx, cy + dy) == 'l')
                return true;
    return false;
}

static float wall_height(const Level *lv, int cx, int cy)
{
    char c = G(lv, cx, cy);
    if (in_palace(cx, cy))
        return PALACE_TOP - FLOOR_Y;
    if (c == '%')
        return carries_upper(lv, cx, cy) ? UPPER_Y - FLOOR_Y : 0.6f + rnd(cx, cy, 1) * 1.1f;
    //height of walls
    float h = 4.2f + rnd(cx, cy, 1) * 2.4f;
    static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    for (int i = 0; i < 4; i++) {
        char n = G(lv, cx + dx[i], cy + dy[i]);
        if (n == 'D' || n == 'L')
            h = fmaxf(h, 5.3f);         /* tall enough to span a doorway */
    }
    if (carries_upper(lv, cx, cy))
        h = UPPER_Y - FLOOR_Y + (rnd(cx, cy, 2) < 0.5f ? 0.0f : 1.5f + rnd(cx, cy, 3) * 2.0f);
    return h;
}

static void build_wall(Build *b, int cx, int cy)
{
    Level *lv = b->lv;
    vec3 c;
    level_cell_center(lv, cx, cy, c);
    int mat = wall_material(cx, cy);
    float h = wall_height(lv, cx, cy);
    float t = WALL_T * 0.5f, e = CELL * 0.5f;
    char ch = G(lv, cx, cy);
    bool low = ch == '%' && h < 2.0f;
    bool palace = in_palace(cx, cy);
    bool keep_flat = palace || carries_upper(lv, cx, cy);

    block(b, mat, (vec3){c[0] - t, FLOOR_Y - 0.3f, c[2] - t}, (vec3){c[0] + t, FLOOR_Y + h, c[2] + t});

    static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    for (int i = 0; i < 4; i++) {
        if (!is_wall(G(lv, cx + dx[i], cy + dy[i])))
            continue;
        for (int half = 0; half < 2; half++) {
            float a0 = t + (e - t) * half * 0.5f, a1 = t + (e - t) * (half + 1) * 0.5f;
            float hh = h;
            if (!keep_flat)
                hh -= low ? rnd(cx, cy, 20 + i * 2 + half) * 0.4f
                          : (rnd(cx, cy, 20 + i * 2 + half) < 0.35f ? rnd(cx, cy, 40 + i) * 1.8f : 0.0f);
            vec3 mn, mx;
            if (dx[i]) {
                float x0 = c[0] + dx[i] * a0, x1 = c[0] + dx[i] * a1;
                glm_vec3_copy((vec3){fminf(x0, x1), FLOOR_Y - 0.3f, c[2] - t}, mn);
                glm_vec3_copy((vec3){fmaxf(x0, x1), FLOOR_Y + hh, c[2] + t}, mx);
            } else {
                float z0 = c[2] + dy[i] * a0, z1 = c[2] + dy[i] * a1;
                glm_vec3_copy((vec3){c[0] - t, FLOOR_Y - 0.3f, fminf(z0, z1)}, mn);
                glm_vec3_copy((vec3){c[0] + t, FLOOR_Y + hh, fmaxf(z0, z1)}, mx);
            }
            block(b, mat, mn, mx);
        }
    }

    /* palace walls stand on a marble plinth */
    if (palace) {
        box_mesh(&b->mb[MAT_MARBLE], (vec3){c[0] - t - 0.12f, FLOOR_Y, c[2] - t - 0.12f},
                 (vec3){c[0] + t + 0.12f, FLOOR_Y + 0.5f, c[2] + t + 0.12f}, 2.0f);
        return;
    }

    /* fallen stones at the foot of broken walls */
    if (low || rnd(cx, cy, 3) < 0.3f) {
        bool along_x = is_wall(G(lv, cx + 1, cy)) || is_wall(G(lv, cx - 1, cy));
        for (int side = -1; side <= 1; side += 2) {
            vec3 p = { c[0], FLOOR_Y, c[2] };
            if (along_x)
                p[2] += side * (t + 0.8f);
            else
                p[0] += side * (t + 0.8f);
            int nx = along_x ? cx : cx + side, ny = along_x ? cy + side : cy;
            char n = G(lv, nx, ny);
            if (!is_wall(n) && n != 'w' && !is_stairs(n) && !in_palace(nx, ny) && U(lv, nx, ny) == ' ')
                rubble(b, mat, p, cx * 3 + side, cy, 3 + (int)(rnd(cx, cy, 4) * 3), 2.0f);
        }
    }
}

/* wall with an opening in the middle: doors, the palace door, windows */
static void build_opening(Build *b, int cx, int cy, float open_half, float bottom, float top, SpawnKind door)
{
    Level *lv = b->lv;
    vec3 c;
    level_cell_center(lv, cx, cy, c);
    int mat = wall_material(cx, cy);
    bool along_x = is_wall(G(lv, cx - 1, cy)) || is_wall(G(lv, cx + 1, cy));
    float h = along_x ? fmaxf(wall_height(lv, cx - 1, cy), wall_height(lv, cx + 1, cy))
                      : fmaxf(wall_height(lv, cx, cy - 1), wall_height(lv, cx, cy + 1));
    h = fmaxf(h, top + 1.2f);
    if (in_palace(cx, cy))
        h = PALACE_TOP - FLOOR_Y;

    float t = WALL_T * 0.5f, e = CELL * 0.5f;
    float spans[4][4] = {
        { -e, -open_half, -0.3f, h },
        { open_half, e, -0.3f, h },
        { -open_half, open_half, top, h },
        { -open_half, open_half, -0.3f, bottom },
    };
    for (int i = 0; i < 4; i++) {
        float a0 = spans[i][0], a1 = spans[i][1], y0 = spans[i][2], y1 = spans[i][3];
        if (y1 <= y0 + 0.01f)
            continue;       /* doors have no sill */
        vec3 mn, mx;
        if (along_x) {
            glm_vec3_copy((vec3){c[0] + a0, FLOOR_Y + y0, c[2] - t}, mn);
            glm_vec3_copy((vec3){c[0] + a1, FLOOR_Y + y1, c[2] + t}, mx);
        } else {
            glm_vec3_copy((vec3){c[0] - t, FLOOR_Y + y0, c[2] + a0}, mn);
            glm_vec3_copy((vec3){c[0] + t, FLOOR_Y + y1, c[2] + a1}, mx);
        }
        block(b, mat, mn, mx);
    }

    if (door != SPAWN_KIND_COUNT)
        add_spawn(lv, door, c, along_x ? 0.0f : GLM_PI_2f);
}

/* ---------- objects ---------- */

/* direction of an adjacent solid wall in a layer, or false */
static bool wall_dir(const Level *lv, Layer layer, int cx, int cy, int *dx, int *dy)
{
    static const int ox[4] = { 0, 1, 0, -1 }, oy[4] = { -1, 0, 1, 0 };
    for (int i = 0; i < 4; i++) {
        char n = level_cell(lv, layer, cx + ox[i], cy + oy[i]);
        if (n == '#' || n == '%' || n == 'W' || n == 'I') {
            *dx = ox[i];
            *dy = oy[i];
            return true;
        }
    }
    return false;
}

/* yaw that faces direction (dx, dz) with the model's front (+Z) */
static float face(float dx, float dz) { return atan2f(dx, dz); }

/* a spawn pushed back against the nearest wall, facing out */
static Spawn *against_wall(Level *lv, Layer layer, SpawnKind kind, int cx, int cy, vec3 c, float inset)
{
    int dx, dy;
    vec3 p;
    glm_vec3_copy(c, p);
    float yaw = rnd(cx, cy, 5) * 6.28f;
    if (wall_dir(lv, layer, cx, cy, &dx, &dy)) {
        float gap = layer == LAYER_CRYPT ? CELL * 0.5f : CELL * 0.5f - WALL_T * 0.5f;
        p[0] += dx * (gap - inset);
        p[2] += dy * (gap - inset);
        yaw = face(-dx, -dy);
    }
    return add_spawn(lv, kind, p, yaw);
}

static void solid(Level *lv, vec3 c, float hx, float hz, float h)
{
    level_add_box(lv, (vec3){c[0] - hx, c[1], c[2] - hz}, (vec3){c[0] + hx, c[1] + h, c[2] + hz});
}

/* things that can stand on any layer */
static bool common_object(Build *b, Layer layer, int cx, int cy, char ch, vec3 c)
{
    Level *lv = b->lv;
    int dx, dy;
    float jitter = (rnd(cx, cy, 9) - 0.5f) * 1.2f;
    switch (ch) {
    case 'r': add_spawn(lv, SPAWN_RAT, c, rnd(cx, cy, 5) * 6.28f); return true;
    case 'f': add_spawn(lv, SPAWN_FOX, c, rnd(cx, cy, 5) * 6.28f); return true;
    case 'g': add_spawn(lv, SPAWN_GOBLIN, c, rnd(cx, cy, 5) * 6.28f); return true;
    case 'j': add_spawn(lv, SPAWN_SLIME, c, rnd(cx, cy, 5) * 6.28f); return true;
    case 'l':
        add_spawn(lv, SPAWN_LANTERN, (vec3){c[0] + jitter * 0.5f, c[1], c[2] - jitter * 0.5f}, rnd(cx, cy, 5) * 6.0f);
        return true;
    case 'C':
    case 'G': {
        Spawn *s = against_wall(lv, layer, ch == 'G' ? SPAWN_CHEST_GOLD : SPAWN_CHEST, cx, cy, c, 0.55f);
        solid(lv, s->pos, 0.5f, 0.5f, 0.6f);
        return true;
    }
    case 'T':
        if (wall_dir(lv, layer, cx, cy, &dx, &dy)) {
            float inset = layer == LAYER_CRYPT ? CELL * 0.5f - 0.08f : CELL * 0.5f - WALL_T * 0.5f - 0.08f;
            vec3 p = { c[0] + dx * inset, c[1] + 2.3f, c[2] + dy * inset };
            Spawn *s = add_spawn(lv, SPAWN_TORCH, p, face(-dx, -dy));
            glm_vec3_copy((vec3){-(float)dx, 0.0f, -(float)dy}, s->normal);
        }
        return true;
    case 'P': {
        float h = layer == LAYER_CRYPT ? FLOOR_Y - 0.6f - CRYPT_Y : 6.0f;
        pillar(b, layer == LAYER_CRYPT ? MAT_CRYPT_WALL : MAT_PILLAR, c[0], c[1], c[2], h, 0.42f, true);
        return true;
    }
    }
    return false;
}

static void build_object(Build *b, int cx, int cy, char ch)
{
    Level *lv = b->lv;
    vec3 c;
    level_cell_center(lv, cx, cy, c);
    if (ch == 'w')
        c[1] = SUNKEN_Y;
    if (common_object(b, LAYER_GROUND, cx, cy, ch, c))
        return;
    float jitter = (rnd(cx, cy, 9) - 0.5f) * 1.2f;

    switch (ch) {
    case 'S': add_spawn(lv, SPAWN_PLAYER, c, 0.0f); break;
    case 'x': add_spawn(lv, SPAWN_PICKUP_DAGGER, c, 0.3f); break;
    case 'h': add_spawn(lv, SPAWN_PICKUP_SHIELD, c, 0.0f); break;
    case 'F':
        add_spawn(lv, SPAWN_FIREPIT, c, 0.0f);
        solid(lv, c, 0.7f, 0.7f, 0.35f);
        break;
    case 's':
        add_spawn(lv, SPAWN_STATUE, c, face(-c[0], -c[2]));   /* statues face the altar */
        solid(lv, c, 0.7f, 0.7f, 1.8f);
        break;
    case 'b': {
        vec3 p = { c[0] + jitter, c[1], c[2] - jitter };
        add_spawn(lv, SPAWN_BARREL, p, rnd(cx, cy, 5) * 6.0f);
        solid(lv, p, 0.3f, 0.3f, 0.9f);
        break;
    }
    case 'c': {
        Spawn *s = against_wall(lv, LAYER_GROUND, SPAWN_CRATE, cx, cy, c, 0.5f);
        solid(lv, s->pos, 0.45f, 0.45f, 0.35f);
        break;
    }
    case 'v': add_spawn(lv, SPAWN_VASE, (vec3){c[0] + jitter, c[1], c[2] + jitter}, rnd(cx, cy, 5) * 6.0f); break;
    case 'o':
        add_spawn(lv, SPAWN_BOULDER, c, rnd(cx, cy, 5) * 6.0f);
        level_add_box(lv, (vec3){c[0] - 0.8f, FLOOR_Y - 0.5f, c[2] - 0.8f}, (vec3){c[0] + 0.8f, FLOOR_Y + 0.9f, c[2] + 0.8f});
        break;
    case 'p': {
        /* snapped pillar with its top drum lying on the ground beside it */
        float h = 1.2f + rnd(cx, cy, 6) * 1.6f;
        pillar(b, MAT_PILLAR, c[0], c[1], c[2], h, 0.42f, false);
        mat4 xf;
        float a = rnd(cx, cy, 7) * 6.28f;
        glm_translate_make(xf, (vec3){c[0] + cosf(a) * 1.6f, FLOOR_Y + 0.38f, c[2] + sinf(a) * 1.6f});
        glm_rotate(xf, a, (vec3){0, 1, 0});
        glm_rotate(xf, GLM_PI_2f, (vec3){0, 0, 1});
        glm_translate(xf, (vec3){0, -0.9f, 0});
        mb_cylinder(&b->mb[MAT_PILLAR], xf, 0.4f, 0.4f, 1.8f, 20, 2.0f);
        rubble(b, MAT_PILLAR, c, cx, cy, 3, 2.5f);
        break;
    }
    case ',':
        rubble(b, wall_material(cx, cy), c, cx, cy, 5 + (int)(rnd(cx, cy, 8) * 5), 3.0f);
        break;
    case 'A': {
        /* the shrine: a ring of pillars, some broken, and a pedestal for the relic */
        static const float heights[6] = { 6.0f, 2.2f, 4.8f, 1.3f, 6.0f, 3.1f };
        for (int i = 0; i < 6; i++) {
            float a = i * GLM_PIf / 3.0f;     /* none straight north or south: keeps the view open */
            pillar(b, MAT_PILLAR, c[0] + cosf(a) * 5.5f, c[1], c[2] + sinf(a) * 5.5f, heights[i], 0.42f, heights[i] > 5.0f);
        }
        block(b, MAT_BLOCKS, (vec3){c[0] - 0.45f, FLOOR_Y, c[2] - 3.05f}, (vec3){c[0] + 0.45f, FLOOR_Y + 1.0f, c[2] - 2.15f});
        add_spawn(lv, SPAWN_AVOCADO, (vec3){c[0], FLOOR_Y + 1.0f, c[2] - 2.6f}, 0.0f);
        break;
    }

    /* garden */
    case 'y':
        for (int i = 0; i < 2; i++)
            add_spawn(lv, SPAWN_SHRUB, (vec3){c[0] + (rnd(cx, cy, 30 + i) - 0.5f) * 2.4f, c[1],
                                             c[2] + (rnd(cx, cy, 40 + i) - 0.5f) * 2.4f},
                      rnd(cx, cy, 50 + i) * 6.28f)->variant = (int)(rnd(cx, cy, 60 + i) * 4.0f);   /* 0: the big bushy one */
        break;
    case 'Y':
        for (int i = 0; i < 3; i++)
            add_spawn(lv, SPAWN_FERN, (vec3){c[0] + (rnd(cx, cy, 30 + i) - 0.5f) * 3.0f, c[1],
                                            c[2] + (rnd(cx, cy, 40 + i) - 0.5f) * 3.0f},
                      rnd(cx, cy, 50 + i) * 6.28f)->variant = i;
        break;
    case 'e': {
        Spawn *s = add_spawn(lv, SPAWN_BENCH, c, in_palace(cx, cy) ? 0.0f : rnd(cx, cy, 5) < 0.5f ? 0.0f : GLM_PI_2f);
        solid(lv, s->pos, 0.8f, 0.8f, 0.45f);
        break;
    }
    case 'U': {
        /* fountain: a round stone basin with a spout, and water in it */
        mat4 xf;
        glm_translate_make(xf, c);
        mb_cylinder(&b->mb[MAT_MARBLE], xf, 1.6f, 1.6f, 0.6f, 32, 2.0f);
        glm_translate_make(xf, (vec3){c[0], c[1] + 0.6f, c[2]});
        mb_cylinder(&b->mb[MAT_MARBLE], xf, 0.25f, 0.18f, 1.2f, 16, 1.0f);
        glm_translate_make(xf, (vec3){c[0], c[1] + 1.75f, c[2]});
        mb_cylinder(&b->mb[MAT_MARBLE], xf, 0.55f, 0.1f, 0.12f, 20, 1.0f);
        glm_translate_make(xf, (vec3){c[0], c[1] + 0.61f, c[2]});
        mb_box(&b->water, xf, (vec3){1.45f, 0.005f, 1.45f}, 1.0f);
        solid(lv, c, 1.5f, 1.5f, 0.6f);
        add_spawn(lv, SPAWN_FOUNTAIN, c, 0.0f);
        break;
    }

    /* library */
    case 'B': {
        Spawn *s = against_wall(lv, LAYER_GROUND, SPAWN_BOOKSHELF, cx, cy, c, 0.35f);
        solid(lv, s->pos, 0.55f, 0.55f, 2.0f);
        break;
    }
    case 'q':
        add_spawn(lv, SPAWN_BOOKPILE, c, rnd(cx, cy, 5) * 6.28f);
        break;

    /* palace */
    case 'Q':
        pillar(b, MAT_MARBLE, c[0], c[1], c[2], PALACE_TOP - FLOOR_Y - 0.5f, 0.45f, true);
        break;
    case 'H':
        if (G(lv, cx - 1, cy) == 'H') {
            /* the throne, between the two H cells, on a two-step dais */
            float x = c[0] - CELL * 0.5f, z = c[2] - 0.3f;
            block(b, MAT_MARBLE, (vec3){x - 3.0f, FLOOR_Y, z - 1.6f}, (vec3){x + 3.0f, FLOOR_Y + 0.25f, z + 2.4f});
            block(b, MAT_MARBLE, (vec3){x - 2.0f, FLOOR_Y + 0.25f, z - 1.6f}, (vec3){x + 2.0f, FLOOR_Y + 0.5f, z + 1.4f});
            block(b, MAT_GOLD, (vec3){x - 0.75f, FLOOR_Y + 0.5f, z - 0.4f}, (vec3){x + 0.75f, FLOOR_Y + 1.0f, z + 0.5f});
            box_mesh(&b->mb[MAT_VELVET], (vec3){x - 0.65f, FLOOR_Y + 1.0f, z - 0.35f}, (vec3){x + 0.65f, FLOOR_Y + 1.12f, z + 0.45f}, 1.0f);
            block(b, MAT_GOLD, (vec3){x - 0.8f, FLOOR_Y + 0.5f, z - 0.6f}, (vec3){x + 0.8f, FLOOR_Y + 3.2f, z - 0.4f});
            box_mesh(&b->mb[MAT_VELVET], (vec3){x - 0.6f, FLOOR_Y + 1.1f, z - 0.41f}, (vec3){x + 0.6f, FLOOR_Y + 2.9f, z - 0.35f}, 1.0f);
            for (int s = -1; s <= 1; s += 2)
                box_mesh(&b->mb[MAT_GOLD], (vec3){x + s * 0.8f - 0.1f, FLOOR_Y + 0.5f, z - 0.4f},
                         (vec3){x + s * 0.8f + 0.1f, FLOOR_Y + 1.5f, z + 0.5f}, 1.0f);
        }
        break;
    case '*':
        add_spawn(lv, SPAWN_CHANDELIER, (vec3){c[0], PALACE_TOP - 0.5f, c[2]}, 0.0f);
        break;
    case 'u': add_spawn(lv, SPAWN_PLANT, (vec3){c[0] + jitter * 0.6f, c[1], c[2] + jitter * 0.6f}, rnd(cx, cy, 5) * 6.0f); break;
    case '1': case '2': case '3': case '4': case '5': case '6':
        add_spawn(lv, SPAWN_NPC, c, 0.0f)->index = ch - '0';
        break;
    default:
        break;
    }
}

/* ---------- stairs ---------- */

/* a staircase over two cells, the top at (cx, top_y). up stairs rise to the north onto
 * the upper floor; down stairs sink to the south into the crypt */
static void build_stairs(Build *b, int cx, int top_y, bool up)
{
    Level *lv = b->lv;
    vec3 c;
    level_cell_center(lv, cx, top_y, c);
    float z_north = c[2] - CELL * 0.5f, z_south = c[2] + CELL * 1.5f;
    const int steps = 20;
    float run = (z_south - z_north) / steps;

    if (up) {
        /* step i rises from the south end */
        for (int i = 0; i < steps; i++) {
            float z1 = z_south - i * run, z0 = z1 - run;
            float top = FLOOR_Y + (UPPER_Y - FLOOR_Y) * (i + 1) / (float)steps;
            block(b, MAT_BLOCKS, (vec3){c[0] - 1.3f, FLOOR_Y, z0}, (vec3){c[0] + 1.3f, top, z1});
        }
        return;
    }

    /* down: step i sinks from the north end */
    for (int i = 0; i < steps; i++) {
        float z0 = z_north + i * run, z1 = z0 + run;
        float top = FLOOR_Y - (FLOOR_Y - CRYPT_Y) * (i + 1) / (float)steps;
        block(b, MAT_CRYPT_WALL, (vec3){c[0] - 1.3f, CRYPT_Y, z0}, (vec3){c[0] + 1.3f, top, z1});
    }
    /* railings around the stairwell at ground level, open to the north */
    vec3 c2;
    level_cell_center(lv, cx, top_y + 1, c2);
    for (int s = -1; s <= 1; s += 2) {
        edge_wall(b, MAT_BLOCKS, c, s, 0, FLOOR_Y - 0.6f, FLOOR_Y + 0.9f, 0.35f);
        edge_wall(b, MAT_BLOCKS, c2, s, 0, FLOOR_Y - 0.6f, FLOOR_Y + 0.9f, 0.35f);
    }
    edge_wall(b, MAT_BLOCKS, c2, 0, 1, FLOOR_Y - 0.6f, FLOOR_Y + 0.9f, 0.35f);
    add_hole(lv, (Hole){ c[0] - CELL * 0.5f, z_north, c[0] + CELL * 0.5f, z_south });
}

/* ---------- upper floor ---------- */

static void build_upper(Build *b, int cx, int cy)
{
    Level *lv = b->lv;
    char ch = U(lv, cx, cy);
    if (ch == ' ')
        return;
    vec3 c;
    level_cell_center(lv, cx, cy, c);
    block(b, MAT_FLOOR, (vec3){c[0] - CELL * 0.5f, UPPER_Y - 0.4f, c[2] - CELL * 0.5f},
          (vec3){c[0] + CELL * 0.5f, UPPER_Y, c[2] + CELL * 0.5f});

    /* parapets on every open edge, except where the stairs arrive */
    static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    for (int i = 0; i < 4; i++) {
        if (U(lv, cx + dx[i], cy + dy[i]) != ' ')
            continue;
        if (dx[i] == 0 && dy[i] == 1 && G(lv, cx, cy + 1) == '^')
            continue;
        edge_wall(b, MAT_BLOCKS, (vec3){c[0], UPPER_Y, c[2]}, dx[i], dy[i], UPPER_Y, UPPER_Y + 1.0f, 0.35f);
    }

    c[1] = UPPER_Y;
    common_object(b, LAYER_UPPER, cx, cy, ch, c);
}

/* ---------- crypt ---------- */

static void build_crypt(Build *b, int cx, int cy)
{
    Level *lv = b->lv;
    char ch = K(lv, cx, cy);
    if (ch == ' ')
        return;
    vec3 c;
    level_cell_center(lv, cx, cy, c);
    float e = CELL * 0.5f, ceil = FLOOR_Y - 0.6f;
    c[1] = CRYPT_Y;

    if (ch == '#') {
        /* crypt walls are the living rock: whole cells, but only where they face a room */
        bool faces = false;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                if (K(lv, cx + dx, cy + dy) != '#' && K(lv, cx + dx, cy + dy) != ' ')
                    faces = true;
        if (faces)
            block(b, MAT_CRYPT_WALL, (vec3){c[0] - e, CRYPT_Y - 0.3f, c[2] - e}, (vec3){c[0] + e, ceil, c[2] + e});
        return;
    }
    if (ch == 'D') {
        /* archway through the rock */
        bool along_x = K(lv, cx - 1, cy) == '#' || K(lv, cx + 1, cy) == '#';
        float spans[3][4] = { { -e, -1.1f, CRYPT_Y - 0.3f, ceil }, { 1.1f, e, CRYPT_Y - 0.3f, ceil },
                              { -1.1f, 1.1f, CRYPT_Y + 3.0f, ceil } };
        for (int i = 0; i < 3; i++) {
            vec3 mn, mx;
            if (along_x) {
                glm_vec3_copy((vec3){c[0] + spans[i][0], spans[i][2], c[2] - e}, mn);
                glm_vec3_copy((vec3){c[0] + spans[i][1], spans[i][3], c[2] + e}, mx);
            } else {
                glm_vec3_copy((vec3){c[0] - e, spans[i][2], c[2] + spans[i][0]}, mn);
                glm_vec3_copy((vec3){c[0] + e, spans[i][3], c[2] + spans[i][1]}, mx);
            }
            block(b, MAT_CRYPT_WALL, mn, mx);
        }
    }

    /* floor, and a ceiling unless the stairs come down through it */
    block(b, MAT_CRYPT_FLOOR, (vec3){c[0] - e, CRYPT_Y - 0.3f, c[2] - e}, (vec3){c[0] + e, CRYPT_Y, c[2] + e});
    if (G(lv, cx, cy) != '<')
        block(b, MAT_CRYPT_WALL, (vec3){c[0] - e, ceil, c[2] - e}, (vec3){c[0] + e, ceil + 0.3f, c[2] + e});

    if (common_object(b, LAYER_CRYPT, cx, cy, ch, c))
        return;
    switch (ch) {
    case 'Z':
    case 'X': {
        /* tombs lie with their heads to the wall */
        Spawn *s = against_wall(lv, LAYER_CRYPT, ch == 'X' ? SPAWN_TOMB_GREAT : SPAWN_TOMB, cx, cy, c, 1.25f);
        bool across = fabsf(sinf(s->yaw)) > 0.5f;
        float big = ch == 'X' ? 1.3f : 1.0f;
        solid(lv, s->pos, (across ? 1.15f : 0.6f) * big, (across ? 0.6f : 1.15f) * big, 1.0f);
        break;
    }
    case 'z':
        add_spawn(lv, SPAWN_SKELETON, c, rnd(cx, cy, 5) * 6.28f)->variant = (int)(rnd(cx, cy, 6) * 4.0f) % 4;
        break;
    }
}

/* ---------- build ---------- */

static void build_floor(Build *b, int cx, int cy, char ch)
{
    Level *lv = b->lv;
    vec3 c;
    level_cell_center(lv, cx, cy, c);
    int m;
    if (in_palace(cx, cy))
        m = strchr("mBue2345", ch) ? MAT_PARQUET : MAT_MARBLE_FLOOR;
    else if (in_area(cx, cy, AREA_LIBRARY))
        m = MAT_WOOD_FLOOR;
    else if ((cy >= 26 && cy <= 33 && cx >= 10 && cx <= 35) || ch == ':')
        m = MAT_COBBLE;
    else
        m = MAT_FLOOR;
    if (is_solid_wall(ch))
        m = in_palace(cx, cy) ? MAT_MARBLE_FLOOR : MAT_FLOOR;

    float top = ch == 'w' ? SUNKEN_Y : FLOOR_Y;
    mat4 xf;
    glm_translate_make(xf, (vec3){c[0], top - 0.15f, c[2]});
    mb_box(&b->mb[m], xf, (vec3){CELL * 0.5f, 0.15f, CELL * 0.5f}, MAT_TILE[m]);
    level_add_box(lv, (vec3){c[0] - CELL * 0.5f, top - 0.3f, c[2] - CELL * 0.5f},
                      (vec3){c[0] + CELL * 0.5f, top, c[2] + CELL * 0.5f});

    if (ch == 'R')
        box_mesh(&b->mb[MAT_VELVET], (vec3){c[0] - 1.1f, FLOOR_Y, c[2] - CELL * 0.5f},
                 (vec3){c[0] + 1.1f, FLOOR_Y + 0.02f, c[2] + CELL * 0.5f}, 1.5f);

    if (ch == 'w') {
        /* knee-deep water, and a step up to any dry neighbor */
        mat4 w;
        glm_translate_make(w, (vec3){c[0], WATER_Y, c[2]});
        mb_box(&b->water, w, (vec3){CELL * 0.5f, 0.005f, CELL * 0.5f}, 1.0f);
        static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        for (int i = 0; i < 4; i++) {
            char n = G(lv, cx + dx[i], cy + dy[i]);
            if (n == 'w' || is_solid_wall(n))
                continue;
            float px = c[0] + dx[i] * (CELL * 0.5f - 0.5f), pz = c[2] + dy[i] * (CELL * 0.5f - 0.5f);
            float hx = dx[i] ? 0.5f : CELL * 0.5f, hz = dy[i] ? 0.5f : CELL * 0.5f;
            block(b, MAT_FLOOR, (vec3){px - hx, SUNKEN_Y, pz - hz}, (vec3){px + hx, SUNKEN_Y + 0.4f, pz + hz});
        }
    }

    /* the palace has a roof */
    if (in_palace(cx, cy))
        block(b, MAT_MARBLE, (vec3){c[0] - CELL * 0.5f, PALACE_TOP - 0.5f, c[2] - CELL * 0.5f},
              (vec3){c[0] + CELL * 0.5f, PALACE_TOP, c[2] + CELL * 0.5f});
}

void level_build_cover(Level *lv)
{
    /* rasterize every solid box's top into a height grid */
    free(lv->cover);
    glDeleteTextures(1, &lv->cover_tex);
    lv->cover_x0 = lv->min[0] - CELL;
    lv->cover_z0 = lv->min[2] - CELL;
    lv->cover_w = (int)((lv->max[0] - lv->min[0] + 2 * CELL) / COVER_RES) + 1;
    lv->cover_h = (int)((lv->max[2] - lv->min[2] + 2 * CELL) / COVER_RES) + 1;
    lv->cover = malloc(lv->cover_w * lv->cover_h * sizeof *lv->cover);
    for (int i = 0; i < lv->cover_w * lv->cover_h; i++)
        lv->cover[i] = -1000.0f;
    for (int i = 0; i < lv->box_count; i++) {
        const Box *b = &lv->boxes[i];
        int x0 = (int)ceilf((b->min[0] - lv->cover_x0) / COVER_RES);
        int x1 = (int)floorf((b->max[0] - lv->cover_x0) / COVER_RES);
        int z0 = (int)ceilf((b->min[2] - lv->cover_z0) / COVER_RES);
        int z1 = (int)floorf((b->max[2] - lv->cover_z0) / COVER_RES);
        for (int z = z0 > 0 ? z0 : 0; z <= z1 && z < lv->cover_h; z++)
            for (int x = x0 > 0 ? x0 : 0; x <= x1 && x < lv->cover_w; x++)
                if (b->max[1] > lv->cover[z * lv->cover_w + x])
                    lv->cover[z * lv->cover_w + x] = b->max[1];
    }

    glCreateTextures(GL_TEXTURE_2D, 1, &lv->cover_tex);
    glTextureStorage2D(lv->cover_tex, 1, GL_R32F, lv->cover_w, lv->cover_h);
    glTextureSubImage2D(lv->cover_tex, 0, 0, 0, lv->cover_w, lv->cover_h, GL_RED, GL_FLOAT, lv->cover);
    glTextureParameteri(lv->cover_tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(lv->cover_tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(lv->cover_tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(lv->cover_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void level_build(Level *lv)
{
    memset(lv, 0, sizeof *lv);
    lv->h = (int)(sizeof MAP_GROUND / sizeof MAP_GROUND[0]);
    lv->w = (int)strlen(MAP_GROUND[0]);
    const char **maps[LAYER_COUNT] = { MAP_GROUND, MAP_UPPER, MAP_CRYPT };
    static const int heights[LAYER_COUNT] = {
        sizeof MAP_GROUND / sizeof MAP_GROUND[0], sizeof MAP_UPPER / sizeof MAP_UPPER[0],
        sizeof MAP_CRYPT / sizeof MAP_CRYPT[0],
    };
    for (int l = 0; l < LAYER_COUNT; l++) {
        if (heights[l] != lv->h) {
            fprintf(stderr, "level: map layer %d has %d rows, not %d\n", l, heights[l], lv->h);
            exit(1);
        }
        lv->cells[l] = malloc(lv->w * lv->h);
        for (int y = 0; y < lv->h; y++) {
            if ((int)strlen(maps[l][y]) != lv->w) {
                fprintf(stderr, "level: map layer %d row %d is %d wide, not %d\n",
                        l, y, (int)strlen(maps[l][y]), lv->w);
                exit(1);
            }
            memcpy(&lv->cells[l][y * lv->w], maps[l][y], lv->w);
        }
    }
    for (int y = 0; y < lv->h; y++)
        for (int x = 0; x < lv->w; x++)
            if (G(lv, x, y) == 'A') {
                lv->origin_x = x;
                lv->origin_y = y;
            }

    static Build b;
    memset(&b, 0, sizeof b);
    b.lv = lv;
    Material mats[MAT_COUNT];
    for (int i = 0; i < MAT_COUNT; i++) {
        mb_init(&b.mb[i]);
        if (i == MAT_GOLD) {
            material_color(&mats[i], 0.9f, 0.62f, 0.22f, 0.3f, 1.0f);
        } else {
            char dir[128];
            snprintf(dir, sizeof dir, "assets/textures/%s", MAT_DIRS[i]);
            material_from_dir(&mats[i], dir);
        }
    }
    mb_init(&b.water);

    for (int y = 0; y < lv->h; y++) {
        for (int x = 0; x < lv->w; x++) {
            char ch = G(lv, x, y);
            if (has_floor(ch) || is_solid_wall(ch) || ch == 'w')
                build_floor(&b, x, y, ch);
            if (is_solid_wall(ch) && ch != 'I')
                build_wall(&b, x, y);
            else if (ch == 'D' || ch == 'L' || ch == 'O')
                build_opening(&b, x, y, 1.05f, 0.0f, 4.06f, ch == 'L' ? SPAWN_DOOR_LOCKED : SPAWN_DOOR);
            else if (ch == 'I')
                build_opening(&b, x, y, 1.1f, 2.4f, 7.0f, SPAWN_KIND_COUNT);
            else
                build_object(&b, x, y, ch);

            if (ch == '^' && G(lv, x, y - 1) != '^')
                build_stairs(&b, x, y, true);
            if (ch == '<' && G(lv, x, y - 1) != '<')
                build_stairs(&b, x, y, false);

            build_upper(&b, x, y);
            build_crypt(&b, x, y);
        }
    }

    /* the flooded hall sits below the terrain: cut the ground away over it */
    int wx0 = lv->w, wy0 = lv->h, wx1 = -1, wy1 = -1;
    for (int y = 0; y < lv->h; y++)
        for (int x = 0; x < lv->w; x++)
            if (G(lv, x, y) == 'w') {
                wx0 = x < wx0 ? x : wx0;
                wy0 = y < wy0 ? y : wy0;
                wx1 = x > wx1 ? x : wx1;
                wy1 = y > wy1 ? y : wy1;
            }
    if (wx1 >= 0) {
        vec3 a, c;
        level_cell_center(lv, wx0, wy0, a);
        level_cell_center(lv, wx1, wy1, c);
        add_hole(lv, (Hole){ a[0] - CELL * 0.5f, a[2] - CELL * 0.5f, c[0] + CELL * 0.5f, c[2] + CELL * 0.5f });
    }

    model_from_builders(&lv->geometry, b.mb, mats, MAT_COUNT);
    Material water;
    material_color(&water, 0.1f, 0.2f, 0.22f, 0.05f, 0.0f);
    water.alpha_mode = ALPHA_BLEND;         /* see-through, drawn after everything solid */
    model_from_builders(&lv->water, &b.water, &water, 1);
    for (int i = 0; i < MAT_COUNT; i++)
        mb_free(&b.mb[i]);
    mb_free(&b.water);

    /* the world reaches well past the map: the coast, the village, the volcano */
    glm_vec3_copy((vec3){-WORLD_HALF, CRYPT_Y, -WORLD_HALF}, lv->min);
    glm_vec3_copy((vec3){WORLD_HALF, PALACE_TOP, WORLD_HALF}, lv->max);
    lv->grid_x0 = -WORLD_HALF;
    lv->grid_z0 = -WORLD_HALF;
    lv->grid_w = lv->grid_h = (int)(2.0f * WORLD_HALF / CELL);
    level_finalize(lv);
}

/* which collision square (x, z) falls in; may be outside the grid */
static void grid_cell(const Level *lv, float x, float z, int *gx, int *gz)
{
    *gx = (int)floorf((x - lv->grid_x0) / CELL);
    *gz = (int)floorf((z - lv->grid_z0) / CELL);
}

void level_finalize(Level *lv)
{
    /* sort boxes into the squares they overlap (counting pass, then filling pass) */
    free(lv->grid_start);
    free(lv->grid_items);
    lv->grid_start = NULL;
    lv->grid_items = NULL;
    int cells = lv->grid_w * lv->grid_h;
    int *count = calloc(cells + 1, sizeof *count);
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < lv->box_count; i++) {
            const Box *b = &lv->boxes[i];
            int x0, z0, x1, z1;
            grid_cell(lv, b->min[0] - 0.01f, b->min[2] - 0.01f, &x0, &z0);
            grid_cell(lv, b->max[0] + 0.01f, b->max[2] + 0.01f, &x1, &z1);
            for (int z = z0 < 0 ? 0 : z0; z <= z1 && z < lv->grid_h; z++) {
                for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < lv->grid_w; x++) {
                    int c = z * lv->grid_w + x;
                    if (pass == 0)
                        count[c]++;
                    else
                        lv->grid_items[lv->grid_start[c] + count[c]++] = i;
                }
            }
        }
        if (pass == 0) {
            lv->grid_start = malloc((cells + 1) * sizeof *lv->grid_start);
            int total = 0;
            for (int c = 0; c < cells; c++) {
                lv->grid_start[c] = total;
                total += count[c];
            }
            lv->grid_start[cells] = total;
            lv->grid_items = malloc((total ? total : 1) * sizeof *lv->grid_items);
            memset(count, 0, (cells + 1) * sizeof *count);
        }
    }
    free(count);
}

void level_free(Level *lv)
{
    model_free(&lv->geometry);
    model_free(&lv->water);
    for (int l = 0; l < LAYER_COUNT; l++)
        free(lv->cells[l]);
    free(lv->boxes);
    free(lv->spawns);
    free(lv->grid_start);
    free(lv->grid_items);
    free(lv->cover);
    glDeleteTextures(1, &lv->cover_tex);
    memset(lv, 0, sizeof *lv);
}

/* ---------- queries ---------- */

bool level_hole(const Level *lv, float x, float z)
{
    for (int i = 0; i < lv->hole_count; i++) {
        const Hole *h = &lv->holes[i];
        if (x > h->x0 && x < h->x1 && z > h->z0 && z < h->z1)
            return true;
    }
    return false;
}

float level_ground(const Level *lv, float x, float z, float feet, float step)
{
    float best = -1000.0f;
    int cx, cy;
    grid_cell(lv, x, z, &cx, &cy);
    if (cx < 0 || cy < 0 || cx >= lv->grid_w || cy >= lv->grid_h)
        return best;
    int c = cy * lv->grid_w + cx;
    for (int k = lv->grid_start[c]; k < lv->grid_start[c + 1]; k++) {
        const Box *b = &lv->boxes[lv->grid_items[k]];
        if (b->off || x < b->min[0] || x > b->max[0] || z < b->min[2] || z > b->max[2])
            continue;
        if (b->max[1] <= feet + step && b->max[1] > best)
            best = b->max[1];
    }
    return best;
}

bool level_collide(const Level *lv, vec3 p, float r, float height)
{
    bool hit = false;
    int cx, cy;
    grid_cell(lv, p[0], p[2], &cx, &cy);
    for (int gy = cy - 1; gy <= cy + 1; gy++) {
        for (int gx = cx - 1; gx <= cx + 1; gx++) {
            if (gx < 0 || gy < 0 || gx >= lv->grid_w || gy >= lv->grid_h)
                continue;
            int c = gy * lv->grid_w + gx;
            for (int k = lv->grid_start[c]; k < lv->grid_start[c + 1]; k++) {
                const Box *b = &lv->boxes[lv->grid_items[k]];
                /* standing on top of it, or passing under it, isn't a collision */
                if (b->off || b->max[1] <= p[1] + 0.45f || b->min[1] >= p[1] + height)
                    continue;
                float nx = glm_clamp(p[0], b->min[0], b->max[0]);
                float nz = glm_clamp(p[2], b->min[2], b->max[2]);
                float dx = p[0] - nx, dz = p[2] - nz;
                float d2 = dx * dx + dz * dz;
                if (d2 >= r * r)
                    continue;
                hit = true;
                if (d2 > 1e-8f) {
                    float d = sqrtf(d2);
                    p[0] += dx / d * (r - d);
                    p[2] += dz / d * (r - d);
                } else {
                    /* center is inside the box: push out the shortest way */
                    float pushes[4] = { p[0] - b->min[0] + r, b->max[0] - p[0] + r,
                                        p[2] - b->min[2] + r, b->max[2] - p[2] + r };
                    int m = 0;
                    for (int j = 1; j < 4; j++)
                        if (pushes[j] < pushes[m])
                            m = j;
                    if (m == 0) p[0] -= pushes[0];
                    if (m == 1) p[0] += pushes[1];
                    if (m == 2) p[2] -= pushes[2];
                    if (m == 3) p[2] += pushes[3];
                }
            }
        }
    }
    return hit;
}

static bool segment_hits_box(const Box *bx, vec3 a, vec3 d)
{
    float t0 = 0.0f, t1 = 1.0f;
    for (int k = 0; k < 3; k++) {
        if (fabsf(d[k]) < 1e-6f) {
            if (a[k] < bx->min[k] || a[k] > bx->max[k])
                return false;
            continue;
        }
        float ta = (bx->min[k] - a[k]) / d[k], tb = (bx->max[k] - a[k]) / d[k];
        if (ta > tb) { float t = ta; ta = tb; tb = t; }
        t0 = fmaxf(t0, ta);
        t1 = fminf(t1, tb);
        if (t0 > t1)
            return false;
    }
    return true;
}

bool level_line_clear(const Level *lv, vec3 a, vec3 b)
{
    vec3 d;
    glm_vec3_sub(b, a, d);
    /* the squares along the segment, each tested once */
    int x0, z0, x1, z1;
    grid_cell(lv, fminf(a[0], b[0]), fminf(a[2], b[2]), &x0, &z0);
    grid_cell(lv, fmaxf(a[0], b[0]), fmaxf(a[2], b[2]), &x1, &z1);
    float len = sqrtf(d[0] * d[0] + d[2] * d[2]);
    for (int gz = z0; gz <= z1; gz++) {
        for (int gx = x0; gx <= x1; gx++) {
            if (gx < 0 || gz < 0 || gx >= lv->grid_w || gz >= lv->grid_h)
                continue;
            /* skip squares the segment passes far from */
            if (len > 1e-3f) {
                float ccx = lv->grid_x0 + (gx + 0.5f) * CELL, ccz = lv->grid_z0 + (gz + 0.5f) * CELL;
                float cross = fabsf((ccx - a[0]) * d[2] - (ccz - a[2]) * d[0]) / len;
                if (cross > CELL * 0.75f)
                    continue;
            }
            int c = gz * lv->grid_w + gx;
            for (int k = lv->grid_start[c]; k < lv->grid_start[c + 1]; k++) {
                const Box *bx = &lv->boxes[lv->grid_items[k]];
                if (!bx->off && segment_hits_box(bx, a, d))
                    return false;
            }
        }
    }
    return true;
}

float level_cover(const Level *lv, float x, float z)
{
    int gx = (int)((x - lv->cover_x0) / COVER_RES + 0.5f);
    int gz = (int)((z - lv->cover_z0) / COVER_RES + 0.5f);
    if (gx < 0 || gz < 0 || gx >= lv->cover_w || gz >= lv->cover_h)
        return -1000.0f;
    return lv->cover[gz * lv->cover_w + gx];
}

static float smoothstep(float e0, float e1, float x)
{
    float t = glm_clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float level_sky_exposure(const Level *lv, vec3 p)
{
    float open = smoothstep(0.35f, 0.05f, level_cover(lv, p[0], p[2]) - p[1]);
    return open * smoothstep(FLOOR_Y - 3.0f, FLOOR_Y - 1.0f, p[1]);
}
