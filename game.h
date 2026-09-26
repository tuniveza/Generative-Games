#ifndef GAME_H
#define GAME_H

#include <SDL3/SDL.h>

#include "audio.h"
#include "camera.h"
#include "hands.h"
#include "items.h"
#include "level.h"
#include "model.h"
#include "particles.h"
#include "renderer.h"
#include "terrain.h"
#include "weather.h"

#define MAX_CREATURES   64
#define MAX_DOORS       24
#define MAX_CHESTS      12
#define MAX_TOMBS       12
#define MAX_LIGHTS_SRC  64
#define MAX_PICKUPS     48
#define MAX_GRENADES    8
#define MAX_BOLTS       24
#define MAX_PROPS       256
#define MAX_NPCS        8
#define INV_SLOTS       27
#define HOTBAR          9
#define MAX_MESSAGES    6

/* where a thing stands in the world. `PLACED;` in a struct gives it the fields
 * pos, yaw, pitch, roll and scale, and the same fields as a whole Transform named xf:
 *     c->pos[1] += 1.0f;   c->scale[0] = 2.0f;   transform_matrix(&c->xf, m);
 * whatever creates the thing must set scale (to 1, 1, 1 for normal size) */
#define PLACED union { Transform xf; struct { vec3 pos; float yaw, pitch, roll; vec3 scale; }; }

/* ---------- creatures ---------- */

typedef enum { CR_RAT, CR_FOX, CR_SKELETON, CR_GOBLIN, CR_SLIME, CR_TYPE_COUNT } CreatureType;
typedef enum {
    ST_IDLE, ST_WANDER, ST_CHASE, ST_ATTACK, ST_FLEE, ST_DANCE,
    ST_DORMANT,             /* skeletons lying in the crypt */
    ST_AWAKEN,              /* getting up */
    ST_DEAD,
} CreatureState;

typedef struct {
    bool used;
    CreatureType type;
    int variant;            /* skeleton kind, or slime size (0 big .. 2 small) */
    CreatureState state;
    PLACED;
    vec3 home, target, knock;
    float hp, max_hp;
    float state_t, anim_t, attack_t;
    bool attack_hit;
    float hit_flash, bar_t, death_t;
    float detour, lost_t;
    bool aggro, resurrected;
    float flee_t, dance_t, frozen_t, burn_t;
    float hop_t;            /* slimes: time in the current hop */
    float vy;               /* slimes: vertical speed while hopping */
    Pose pose;
} Creature;

/* ---------- world objects ---------- */

typedef struct {
    PLACED;
    float open, target;
    bool locked;
    int box;
    Pose pose;
} Door;

typedef struct {
    PLACED;
    float open;
    bool opened, gold;
    ItemId loot[8];
    int loot_count[8];
    int loot_n;
    Pose pose;
} Chest;

typedef struct {
    PLACED;
    float open;             /* lid slides aside as open goes 0 -> 1 */
    bool opened, great;
    ItemId gift;            /* the spell inside */
    float glow;             /* runes pulse brighter as you come near */
} Tomb;

typedef enum { LIGHT_TORCH, LIGHT_LANTERN, LIGHT_FIREPIT, LIGHT_CHANDELIER } LightKind;

typedef struct {
    bool used;
    LightKind kind;
    PLACED;
    vec3 normal;
    float seed;
    bool lit;
    bool empty;             /* a wall sconce whose torch was taken */
} LightSource;

typedef struct {
    bool used;
    ItemId item;
    int count;
    PLACED;
    bool on_altar;
} Pickup;

typedef struct {
    bool used;
    PLACED;
    vec3 vel;
    float fuse, spin;
} Grenade;

typedef enum { BOLT_FIREBALL, BOLT_MAGE } BoltKind;

typedef struct {
    bool used;
    BoltKind kind;
    vec3 pos, vel;
    float life;
    bool hostile;           /* fired by a skeleton mage at you */
} Bolt;

typedef struct {
    PLACED;
    int model;              /* index into Game.prop_models */
    mat4 matrix;            /* from xf (plus any extra turn a prop needs), kept since props never move */
    bool no_shadow;         /* small or airy things don't need to cast */
    vec3 center;            /* bounding sphere, for skipping what's off screen */
    float radius;
} Prop;

/* the six planes of a view volume, for skipping things outside it */
typedef struct {
    vec4 planes[6];
} Frustum;

void frustum_from(mat4 view_proj, Frustum *f);
bool frustum_sphere(const Frustum *f, vec3 center, float radius);

typedef struct {
    ItemId id;
    int count;
} Slot;

typedef struct {
    char text[160];
    float t;
    vec4 color;
} Message;

/* ---------- palace characters ---------- */

#define MAX_LINES 6
typedef struct {
    char id[16];            /* "king", "mage"... matches assets/dialogue.txt */
    char name[32];
    int model;              /* index into Game.npc_models */
    PLACED;
    float home_yaw;
    Pose pose;
    int line_count;
    char lines[MAX_LINES][200];
    int clips[MAX_LINES];
    int next_line;
    bool greeted;
    float talk_t;           /* > 0 while speaking */
    float anim_t;
} Npc;

enum {
    PM_STATUE, PM_BARREL, PM_CRATE, PM_VASE, PM_BOULDER, PM_LANTERN, PM_FIREPIT,
    PM_SHRUB_A, PM_SHRUB_B, PM_FERN, PM_FLOWERS, PM_SORREL, PM_STUMP, PM_BENCH,
    PM_BOOKSHELF, PM_BOOKS, PM_CANDLE, PM_CHANDELIER, PM_PLANT, PM_CABINET, PM_COMMODE,
    PM_MIRROR, PM_HORSE, PM_TABLE, PM_CHAIR, PM_OIL_LAMP,
    PM_COUNT
};

enum { SK_MINION, SK_WARRIOR, SK_ROGUE, SK_MAGE, SK_COUNT };
enum { WPN_BLADE, WPN_AXE, WPN_STAFF, WPN_SHIELD, WPN_COUNT };

/* animations looked up by name in each KayKit character */
typedef struct {
    int idle, walk, run, attack, hit, death, awaken, dormant, cast, talk, sit, cheer;
    int hand_r, hand_l;     /* weapon attachment bones */
} Rig;

typedef struct {
    float mouse_sens;       /* 1 = default */
    bool invert_y;
    float fov;
    float music_volume, sfx_volume;
    bool auto_weather;
} Settings;

typedef enum { MENU_NONE, MENU_MAIN, MENU_OPTIONS, MENU_CONTROLS } MenuPage;

typedef struct {
    int width, height;
    float time;

    Renderer r;
    GLuint model_prog, shadow_prog, terrain_prog, water_prog;
    Terrain terrain;
    Level level;
    Particles ps;
    Hands hands;
    Camera cam;
    Weather weather;

    /* models */
    Model door_model, chest_model, fox_model, rat_model, slime_model, tomb_model, tomb_lid_model;
    Model skel_models[SK_COUNT], goblin_model, weapon_models[WPN_COUNT];
    Rig skel_rigs[SK_COUNT], goblin_rig;
    Model prop_models[PM_COUNT];
    Model npc_models[MAX_NPCS];
    Rig npc_rigs[MAX_NPCS];
    int door_left, door_right, chest_lid;
    int fox_idle, fox_walk, fox_run;
    int rat_bone[8];

    /* the world's things */
    Creature creatures[MAX_CREATURES];
    Door doors[MAX_DOORS];          int door_count;
    Chest chests[MAX_CHESTS];       int chest_count;
    Tomb tombs[MAX_TOMBS];          int tomb_count;
    LightSource lights[MAX_LIGHTS_SRC];
    Pickup pickups[MAX_PICKUPS];
    Grenade grenades[MAX_GRENADES];
    Bolt bolts[MAX_BOLTS];
    Prop props[MAX_PROPS];          int prop_count;
    Npc npcs[MAX_NPCS];             int npc_count;
    vec3 fountains[4];              int fountain_count;
    vec3 spawn;
    float spawn_yaw;

    /* the player */
    vec3 pos, vel;
    float hp, max_hp, mana, max_mana;
    bool on_ground, dead, crouching;
    float dead_t, hurt_t, land_t, shake_t, calm_t, step_dist, crouch_amount;
    float fov;
    Slot slots[INV_SLOTS];
    int selected;
    HandAction action;
    float action_t, action_dur;
    bool action_done;
    float cooldowns[ITEM_COUNT];
    LeftHand left;
    bool mask_on, specs_on, lmb, rmb;
    float slow_t, boombox_t, reach_t;
    float look_dx, look_dy;
    float flash_t;
    vec3 flash_pos;
    bool wisp;
    float wisp_t;
    vec3 wisp_pos;
    float spell_glow;

    /* ui */
    bool inv_open, show_help, night;
    MenuPage menu;
    int menu_hover, menu_drag;
    int inv_held;
    float mouse_x, mouse_y;
    Message messages[MAX_MESSAGES];
    float title_t;
    char prompt[96];
    int creatures_left;
    char subtitle_name[32];
    char subtitle[200];
    float subtitle_t;
    float weather_note_t;

    Settings settings;
    SDL_Gamepad *pad;
    bool pad_rt, pad_lt;    /* trigger states last frame */

    mat4 view, proj, hand_proj;
    vec3 eye;
} Game;

void game_init(Game *g, int width, int height);
void game_free(Game *g);

/* returns false when the game wants to quit */
bool game_event(Game *g, const SDL_Event *e, SDL_Window *win);
void game_update(Game *g, float dt, SDL_Window *win);

/* 3D scene into the HDR target; afterwards the frame's view_proj is valid for
 * extra drawing (main.c's triangle) before game_render_overlay */
void game_render_world(Game *g);
/* particles, rain, hands, post-processing, HUD, and on to the window */
void game_render_overlay(Game *g);

void game_resize(Game *g, int width, int height);

/* test helpers used by --shot (main.c) */
void game_give(Game *g, ItemId id, int count);
void game_select(Game *g, int slot);
void game_start_action(Game *g);
void game_set_night(Game *g, bool night);
void game_interact(Game *g);

/* ---------- shared between game.c and its helpers ---------- */

void message(Game *g, const float color[4], const char *fmt, ...);
extern const float COL_TEXT[4], COL_GOLD[4], COL_RED[4], COL_FUN[4], COL_MAGIC[4], COL_EDGE[4];
void load_or_die(Model *m, const char *path, const ModelOptions *opts);     /* exits if missing */
float wrap_angle(float a);          /* into -pi..pi */
float ground_at(const Game *g, float x, float z, float feet);
void look_dir(const Game *g, vec3 out);
void flat_forward(const Game *g, vec3 out);
bool has_item(const Game *g, ItemId id);
void hurt_player(Game *g, vec3 from, float dmg);
void explode(Game *g, vec3 at, float damage, float radius, bool hurts_player);

/* creatures.c */
void creatures_load(Game *g);
void creatures_free(Game *g);
Creature *creature_spawn(Game *g, CreatureType type, int variant, vec3 pos, float yaw);
void creatures_update(Game *g, float dt);
void creatures_draw(Game *g, GLuint prog, mat4 vp, bool depth);
void damage_creature(Game *g, Creature *c, float dmg, vec3 push);
float creature_radius(const Creature *c);
float creature_height(const Creature *c);
const char *creature_name(const Creature *c);

/* magic.c: spells, projectiles, tombs */
void magic_cast(Game *g, Spell spell);
void magic_update(Game *g, float dt);
void magic_draw(Game *g, GLuint prog, mat4 vp, bool depth);
void tombs_build_models(Game *g);
void tomb_open(Game *g, Tomb *t);
void bolt_fire(Game *g, BoltKind kind, vec3 from, vec3 vel, bool hostile);

/* npc.c: palace characters */
void npc_add(Game *g, int number, vec3 pos);
void npcs_update(Game *g, float dt);
void npcs_draw(Game *g, GLuint prog, mat4 vp, bool depth);
void npc_talk(Game *g, Npc *n);

/* menu.c: pause menu, options, settings file, gamepad */
void settings_load(Settings *s);
void settings_save(const Settings *s);
void settings_apply(Game *g);
void menu_open(Game *g, SDL_Window *win);
void menu_close(Game *g, SDL_Window *win);
bool menu_event(Game *g, const SDL_Event *e, SDL_Window *win);   /* false = quit */
void menu_draw(Game *g);
void gamepad_update(Game *g, float dt);

#endif
