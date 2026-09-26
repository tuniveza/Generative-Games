#ifndef WORLD_H
#define WORLD_H

#include "level.h"
#include "meshgen.h"
#include "terrain.h"

/* Everything beyond the ruins' map: the coast to the south (beach, promenade, marina,
 * the open sea), the village of Brinewick to the west, the volcano Old Ember to the
 * north-east, and the Drowned Court far out under the sea. The regions are built in
 * code (coast.c, village.c, volcano.c, undersea.c) into one model per region. */

#define SEA_Y       -3.0f           /* the ocean's surface */

/* landmarks (x, z). the volcano's must match VOLCANO_XZ / VOLCANO_R in shaders/common.glsl */
#define VOLCANO_X    230.0f
#define VOLCANO_Z   -230.0f
#define VOLCANO_R    140.0f         /* where its slopes meet the land */
#define CRATER_R      24.0f
#define RIM_Y        112.0f         /* top of the crater's rim */
#define CRATER_Y      94.0f         /* the crater floor... */
#define LAVA_Y        95.5f         /* ...and the lava lake in it */

#define VILLAGE_X   -170.0f         /* Brinewick, on its low chalk hill above a cove */
#define VILLAGE_Z     30.0f
#define VILLAGE_R     58.0f
#define VILLAGE_Y      1.5f         /* the hilltop's level, where the houses stand */

#define PROM_Z0      100.0f         /* the promenade runs along the top of the beach */
#define PROM_Z1      108.0f
#define PROM_X0     -100.0f
#define PROM_X1       76.0f
#define PROM_Y         0.4f         /* its walkway, a step above the grass and well above the sand */

#define MARINA_X0     80.0f         /* the harbour basin, east of the promenade */
#define MARINA_X1    176.0f
#define MARINA_Z0    116.0f
#define MARINA_Z1    214.0f
#define QUAY_Y        -0.5f
#define LIGHTHOUSE_X 178.0f
#define LIGHTHOUSE_Z 232.0f

#define SHOP_X       -22.0f         /* the Curious Clam, on the beach */
#define SHOP_Z       124.0f
#define TAVERN_X    -150.0f         /* the Salted Eel, on Brinewick's square */
#define TAVERN_Z      30.0f
#define TAVERN_W      12.0f
#define TAVERN_D       9.0f

#define PALACE_X      40.0f         /* the Drowned Court, on the sea floor */
#define PALACE_Z     330.0f
#define PALACE_FLOOR -36.0f
#define COURT_ENTRY_Z (PALACE_Z - 34.0f)    /* where the whirlpool sets you down in the Court */
#define WHIRL_X       40.0f         /* where its whirlpool opens (a bell buoy marks it) */
#define WHIRL_Z      282.0f

/* the line where the beach meets the land, as z for a given x (south of it: sand, sea) */
float coast_z(float x);

/* terrain_create's shaper: carves the coast and the sea floor, lifts the village's hill
 * and the volcano, levels the sea floor under the palace */
float world_terrain_shape(float x, float z, float h);

/* which region a point is in, or AREA_OUTSIDE (level_area asks this beyond the map) */
Area world_area(vec3 p);

/* ---------- building (shared by coast.c, village.c, volcano.c, undersea.c) ---------- */

enum {
    /* textured */
    WM_SAND, WM_DAMP_SAND, WM_PLASTER_BLUE, WM_PLASTER_RED, WM_PLASTER_YELLOW, WM_PLASTER_WHITE,
    WM_THATCH, WM_ROOF_TILES, WM_SHELL_FLOOR, WM_PLANKS, WM_DECK, WM_SEA_TILES, WM_SEA_BRICK,
    WM_CORAL_WALL, WM_CORAL_FORT, WM_CORAL_GROUND, WM_DARK_ROCK, WM_PALM_BARK, WM_SHORE_ROCK,
    WM_OLD_WOOD, WM_TIMBER, WM_MARBLE, WM_STONE, WM_BURNT, WM_COBBLE,
    /* plain colors */
    WM_LEAVES, WM_BLOSSOM, WM_BLOSSOM_PALE, WM_CLOTH_RED, WM_CLOTH_BLUE, WM_CLOTH_WHITE,
    WM_CLOTH_YELLOW, WM_BRASS, WM_IRON, WM_GOLD, WM_ROPE, WM_OBSIDIAN, WM_CORAL_PINK,
    WM_CORAL_ORANGE, WM_KELP, WM_PAINT_TEAL, WM_PAINT_CORAL,
    /* glowing */
    WM_LAMP_GLOW, WM_PAPER_PINK, WM_PAPER_CYAN, WM_PAPER_GOLD, WM_PEARL, WM_EMBER, WM_RUNE,
    /* see-through */
    WM_GLASS, WM_BUBBLE,
    WM_COUNT
};

typedef enum { WC_COAST, WC_MARINA, WC_VILLAGE, WC_VOLCANO, WC_UNDERSEA, WC_COUNT } ChunkId;

/* one region's geometry while it's being built: a mesh builder per material */
typedef struct {
    MeshBuilder mb[WM_COUNT];
    vec3 min, max;          /* grows with every piece added, for culling */
} Chunk;

typedef struct {
    Level *lv;
    const Terrain *terrain;
    Chunk chunks[WC_COUNT];
} WorldBuild;

/* a box that's seen and solid / only seen */
void wb_block(WorldBuild *wb, ChunkId c, int mat, vec3 min, vec3 max);
void wb_box(WorldBuild *wb, ChunkId c, int mat, vec3 min, vec3 max);
/* a box placed by any transform (not solid), half-size `half` */
void wb_xbox(WorldBuild *wb, ChunkId c, int mat, mat4 xf, vec3 half);
/* an upright cylinder from `base`, and a sphere-ish blob */
void wb_cylinder(WorldBuild *wb, ChunkId c, int mat, vec3 base, float r0, float r1, float height, int segments);
void wb_xcylinder(WorldBuild *wb, ChunkId c, int mat, mat4 xf, float r0, float r1, float height, int segments);
void wb_ball(WorldBuild *wb, ChunkId c, int mat, vec3 center, vec3 radii, int segments);
/* a triangular prism (see mb_wedge), not solid */
void wb_wedge(WorldBuild *wb, ChunkId c, int mat, mat4 xf, vec3 half);

/* a small house: floor, four walls with a doorway and windows, corner timbers and a
 * gabled roof. walls are axis-aligned (collision boxes can't turn), so door_side picks
 * which wall faces the street: 0 = south (+z), 1 = east (+x), 2 = north, 3 = west */
typedef struct {
    vec3 pos;               /* middle of the floor */
    float w, d, height;     /* along x, along z, walls */
    int door_side;
    int wall, roof, floor, trim;
    bool thatch;            /* steep and shaggy; otherwise a lower tiled roof */
    bool open_front;        /* a stall or shop: the door wall is a wide open counter */
    ChunkId chunk;
} House;
void wb_house(WorldBuild *wb, const House *h);

/* solid, but nothing drawn (a railing's gaps, a roof you can't reach) */
void wb_solid(WorldBuild *wb, vec3 min, vec3 max);
/* the ground height at (x, z): terrain, or the highest box already built there */
float wb_ground(WorldBuild *wb, float x, float z);
/* something from Game.prop_models standing at pos */
Spawn *wb_prop(WorldBuild *wb, int model, vec3 pos, float yaw, float scale);

void coast_build(WorldBuild *wb);       /* coast.c: beach, promenade, marina, lighthouse */
void village_build(WorldBuild *wb);     /* village.c: Brinewick */
void volcano_build(WorldBuild *wb);     /* volcano.c: Old Ember's crater and vents */
void undersea_build(WorldBuild *wb);    /* undersea.c: the Drowned Court */

/* ---------- the finished world ---------- */

typedef struct {
    Model model;
    vec3 center;
    float radius;
} WorldChunk;

typedef struct {
    WorldChunk chunks[WC_COUNT];
    Model lava;             /* the crater's lake, drawn with shaders/lava */
    GLuint lava_prog;
} World;

extern int court_gate_box;  /* the throne room's sealed gate, solid until the three seals glow */

void world_build(World *w, Level *lv, const Terrain *t);
void world_free(World *w);
/* the regions near enough to matter; depth = the shadow pass */
void world_draw(World *w, GLuint prog, mat4 view_proj, vec3 eye, bool depth);
void world_draw_lava(World *w, mat4 view_proj);

#endif
