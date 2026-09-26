#ifndef LEVEL_H
#define LEVEL_H

#include "model.h"
#include "terrain.h"      /* MAX_HOLES */

#define CELL        4.0f            /* meters per map character */
#define FLOOR_Y    -0.5f            /* top of the ground-floor stone */
#define UPPER_Y     4.5f            /* top of the upper floor */
#define CRYPT_Y    -5.5f            /* top of the crypt floor */
#define PALACE_TOP (FLOOR_Y + 9.0f) /* palace roof */
#define SUNKEN_Y   (FLOOR_Y - 0.8f) /* floor of the flooded hall */
#define WATER_Y    (FLOOR_Y - 0.25f)
#define WALL_T      1.1f            /* wall thickness */
#define WORLD_HALF  400.0f          /* the whole world (terrain, coast, volcano) spans +-this */

typedef enum { LAYER_GROUND, LAYER_UPPER, LAYER_CRYPT, LAYER_COUNT } Layer;

typedef enum {
    AREA_OUTSIDE, AREA_RUINS, AREA_GARDEN, AREA_LIBRARY, AREA_FLOODED,
    AREA_CRYPT, AREA_PALACE, AREA_UPPER,
    /* beyond the ruins (world.c) */
    AREA_BEACH, AREA_PROMENADE, AREA_MARINA, AREA_VILLAGE, AREA_VOLCANO, AREA_OCEAN, AREA_UNDERSEA,
} Area;

/* solid box the player and creatures can't walk through (but can stand on) */
typedef struct {
    vec3 min, max;
    bool off;               /* ignored (an open door) */
} Box;

typedef enum {
    SPAWN_PLAYER,
    SPAWN_RAT, SPAWN_FOX, SPAWN_SKELETON, SPAWN_GOBLIN, SPAWN_SLIME,
    SPAWN_NPC,              /* index = which character (1..6) */
    SPAWN_CHEST, SPAWN_CHEST_GOLD,
    SPAWN_TORCH,            /* wall sconce; `normal` points away from the wall */
    SPAWN_LANTERN,          /* a lit lantern standing on the floor, can be picked up */
    SPAWN_FIREPIT,
    SPAWN_DOOR, SPAWN_DOOR_LOCKED,
    SPAWN_TOMB, SPAWN_TOMB_GREAT,
    SPAWN_AVOCADO, SPAWN_PICKUP_DAGGER, SPAWN_PICKUP_SHIELD,
    SPAWN_STATUE, SPAWN_BARREL, SPAWN_CRATE, SPAWN_VASE, SPAWN_BOULDER,
    SPAWN_SHRUB, SPAWN_FERN, SPAWN_BENCH, SPAWN_FOUNTAIN, SPAWN_BOOKSHELF, SPAWN_BOOKPILE,
    SPAWN_CHANDELIER, SPAWN_PLANT, SPAWN_CABINET,
    /* beyond the ruins (world.c and friends) */
    SPAWN_PROP,             /* variant = PM_ model, normal[0] = scale, normal[1] = sink */
    SPAWN_CREATURE,         /* variant = CreatureType, index = its own variant */
    SPAWN_ANIMAL,           /* variant = Species */
    SPAWN_VILLAGER,         /* index = which resident (npc.c) */
    SPAWN_LAMP,             /* a lamp that stays where it is: variant = LightKind */
    SPAWN_BOAT,             /* a rowing boat moored here */
    SPAWN_THING,            /* something to use: variant = ThingKind, index = which one (quest.c) */
    SPAWN_VENT,             /* a fumarole breathing poison gas */
    SPAWN_KIND_COUNT
} SpawnKind;

typedef struct {
    SpawnKind kind;
    vec3 pos;               /* on the floor (torches: on the wall, chandeliers: hanging) */
    float yaw;              /* radians around +Y; 0 = facing +Z */
    vec3 normal;
    int index;              /* nth spawn of this kind, or the NPC number */
    int variant;            /* e.g. which skeleton */
} Spawn;

typedef struct {
    float x0, z0, x1, z1;   /* terrain is cut away here (stairwells into the crypt) */
} Hole;

typedef struct {
    int w, h;
    char *cells[LAYER_COUNT];
    int origin_x, origin_y; /* map cell at world (0, 0): the altar */

    Model geometry;         /* all stone, wood, marble...; one primitive per material */
    Model water;            /* water surfaces, drawn with shaders/water */

    Box *boxes;  int box_count, box_cap;
    Spawn *spawns; int spawn_count, spawn_cap;
    Hole holes[MAX_HOLES]; int hole_count;
    vec3 min, max;          /* world bounds */

    /* boxes sorted into CELL-sized squares over the whole world (not just the map),
     * for fast collision queries */
    int grid_w, grid_h;
    float grid_x0, grid_z0; /* world position of square (0, 0)'s corner */
    int *grid_start;        /* grid_w*grid_h + 1 offsets into grid_items */
    int *grid_items;

    /* what covers each spot (0.5 m grid): the highest solid surface. used for rain,
     * wet surfaces and how much sky light reaches indoors */
    float *cover;
    int cover_w, cover_h;
    float cover_x0, cover_z0;
    GLuint cover_tex;
} Level;

#define COVER_RES 0.5f

void level_build(Level *lv);
/* the "what's overhead" grid, from every box so far: call once the world's buildings
 * are in (world_build), before the game adds doors and characters */
void level_build_cover(Level *lv);
/* call after adding the game's own boxes (doors, tombs...) so queries see them */
void level_finalize(Level *lv);
void level_free(Level *lv);

char level_cell(const Level *lv, Layer layer, int cx, int cy);
char level_cell_at(const Level *lv, float x, float z);          /* ground layer */
void level_cell_center(const Level *lv, int cx, int cy, vec3 out);
void level_world_to_cell(const Level *lv, float x, float z, int *cx, int *cy);
Area level_area(const Level *lv, vec3 p);

int level_add_box(Level *lv, vec3 min, vec3 max);
Spawn *level_add_spawn(Level *lv, SpawnKind kind, vec3 pos, float yaw);

/* highest walkable surface under (x, z) that is no more than `step` above `feet`:
 * floors, stairs, rubble, walls you're on top of. -1000 if nothing (use the terrain) */
float level_ground(const Level *lv, float x, float z, float feet, float step);

/* true where the terrain is cut away (stairwells) */
bool level_hole(const Level *lv, float x, float z);

/* pushes a vertical cylinder (radius r, from feet to feet + height) out of every
 * box it overlaps. returns true if it hit something */
bool level_collide(const Level *lv, vec3 feet_pos, float r, float height);

/* true if the straight line from a to b doesn't pass through any box */
bool level_line_clear(const Level *lv, vec3 a, vec3 b);

/* height of whatever is overhead at (x, z) (roof, floor above, or the ground itself) */
float level_cover(const Level *lv, float x, float z);

/* 1 = open sky above p, 0 = under a roof or underground (sky_exposure in common.glsl) */
float level_sky_exposure(const Level *lv, vec3 p);

#endif
