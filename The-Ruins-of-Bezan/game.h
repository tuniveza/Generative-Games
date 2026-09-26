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
#include "ocean.h"
#include "terrain.h"
#include "weather.h"
#include "world.h"

#define MAX_CREATURES   128
#define MAX_DOORS       24
#define MAX_CHESTS      16
#define MAX_TOMBS       12
#define MAX_LIGHTS_SRC  192
#define MAX_PICKUPS     64
#define MAX_GRENADES    8
#define MAX_BOLTS       48
#define MAX_PROPS       720
#define MAX_NPCS        24
#define MAX_ANIMALS     72
#define MAX_THINGS      192
#define MAX_BOATS       12
#define MAX_VENTS       16
#define INV_SLOTS       36
#define HOTBAR          9
#define MAX_MESSAGES    6

/* where a thing stands in the world. `PLACED;` in a struct gives it the fields
 * pos, yaw, pitch, roll and scale, and the same fields as a whole Transform named xf:
 *     c->pos[1] += 1.0f;   c->scale[0] = 2.0f;   transform_matrix(&c->xf, m);
 * whatever creates the thing must set scale (to 1, 1, 1 for normal size) */
#define PLACED union { Transform xf; struct { vec3 pos; float yaw, pitch, roll; vec3 scale; }; }

/* your body */
#define EYE_HEIGHT 1.62f
#define PLAYER_R   0.35f
#define PLAYER_H   1.75f
#define STEP       0.45f
#define GRAVITY    18.0f
#define REACH      3.3f

/* ---------- creatures ---------- */

typedef enum {
    CR_RAT, CR_FOX, CR_SKELETON, CR_GOBLIN, CR_SLIME,
    CR_CRAB,        /* giant beach crabs, armoured in front */
    CR_JELLYFISH,   /* drift in the sea and sting whoever swims into them */
    CR_SHARK,       /* circle in deep water, bite swimmers */
    CR_BAT,         /* swoop through the crypt and the volcano at night */
    CR_IMP,         /* magma imps on Old Ember's slopes, throwing embers */
    CR_DROWNED,     /* barnacled skeletons of the Drowned Court */
    CR_WRAITH,      /* drift out of the ruins at night; hard to hit, drain mana */
    CR_EEL,         /* angler eels in the Court's flooded channels */
    CR_KING,        /* the Drowned King, who keeps the Tide Bell */
    CR_TYPE_COUNT
} CreatureType;
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
    float hover;            /* flyers and swimmers: height they keep */
    float phase;            /* per-creature offset for bobbing, pulsing, circling */
    int spawn;              /* index into Game.cspawns (what the volcano brings back), or -1 */
    Pose pose;
} Creature;

/* where each creature first appeared, so the world can be brought back after an eruption */
typedef struct {
    CreatureType type;
    int variant;
    vec3 pos;
    float yaw;
    bool sheltered;         /* underground or under the sea: the eruption doesn't reach it */
} CreatureSpawn;

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

typedef enum {
    LIGHT_TORCH, LIGHT_LANTERN, LIGHT_FIREPIT, LIGHT_CHANDELIER,
    LIGHT_LAMP,         /* street and house lamps: always there, not taken */
    LIGHT_PAPER,        /* Brinewick's paper lanterns, in soft colors */
    LIGHT_BEACON,       /* the lighthouse's lamp */
    LIGHT_CORAL,        /* the Drowned Court's glowing coral */
    LIGHT_FORGE,        /* Bram's forge */
    LIGHT_EMBER,        /* the volcano's vents and lava */
} LightKind;

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

typedef enum {
    BOLT_FIREBALL, BOLT_MAGE,
    BOLT_EMBER,     /* a magma imp's thrown coal */
    BOLT_STAR,      /* a seeking star (Tome of Seeking Stars) */
    BOLT_METEOR,    /* a burning stone falling from the sky */
    BOLT_HARPOON,   /* a thrown harpoon */
    BOLT_INK,       /* the Drowned King's black water */
} BoltKind;

typedef struct {
    bool used;
    BoltKind kind;
    vec3 pos, vel;
    float life;
    bool hostile;           /* fired by a skeleton mage at you */
    int target;             /* seeking stars: the creature they chase, or -1 */
} Bolt;

/* a whirlwind from the Tome of Gales, or stone spikes from the Tome of Thorns */
typedef struct {
    bool used;
    int kind;               /* 0 = whirlwind, 1 = spikes */
    vec3 pos, dir;
    float t, life;
} SpellEffect;
#define MAX_EFFECTS 8

typedef struct {
    PLACED;
    int model;              /* index into Game.prop_models */
    mat4 matrix;            /* from xf (plus any extra turn a prop needs), kept since props never move */
    bool no_shadow;         /* small or airy things don't need to cast */
    bool floats;            /* rides the waves (buoys): its matrix is updated every frame */
    bool burns;             /* plants: the volcano's fire burns them away, then they regrow */
    float growth;           /* 1 = whole, 0 = burnt to nothing */
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

/* ---------- people ---------- */

/* who's who: the palace's residents first (the map's digits 1..6), then the coast's */
enum {
    NPC_KING = 1, NPC_MAGE, NPC_GUARD, NPC_JESTER, NPC_PILGRIM, NPC_HERALD,
    NPC_ELDER,          /* Elder Maren of Brinewick */
    NPC_SHOPKEEPER,     /* Coralie, at the Curious Clam */
    NPC_HARBOURMASTER,  /* Quill */
    NPC_FISHER,         /* Old Brine, at the end of the long pier */
    NPC_ANCIENT,        /* the Unmoored, older than the sea; only on clear nights */
    NPC_TAVERN,         /* Tamsin, at the Salted Eel */
    NPC_LAMPLIGHTER,    /* Pell */
    NPC_CHILD,          /* Wren, who swims in the cove */
    NPC_NETMENDER,      /* Hollis */
    NPC_CURATOR,        /* Juniper, keeper of the Hall of Tides */
    NPC_SWIMMER,        /* Sunny, doing lengths off the beach */
    NPC_SMITH,          /* Bram, at the forge */
    NPC_KINDS
};

typedef enum { BEHAVE_STAND, BEHAVE_WANDER, BEHAVE_SWIM, BEHAVE_SHOP } NpcBehave;

#define MAX_LINES 20
typedef struct {
    char id[16];            /* "king", "mage"... matches assets/dialogue.txt */
    char name[32];
    int kind;               /* NPC_ */
    int model;              /* index into Game.npc_models */
    PLACED;
    float home_yaw;
    vec3 home, target;      /* wanderers stroll around home; swimmers do lengths */
    NpcBehave behave;
    float wander_t;
    Pose pose;
    int line_count;
    char lines[MAX_LINES][220];
    char tags[MAX_LINES][16];   /* "" for everyday lines; quest lines carry a tag (see quest.c) */
    int clips[MAX_LINES];
    int next_line;
    bool greeted;
    float talk_t;           /* > 0 while speaking */
    float anim_t;
    float fade;             /* 1 = fully there; the Unmoored fades in and out */
    bool swimming, dancing, cheering;
    float gone_t;           /* > 0: taken by the eruption, back when it reaches 0 */
} Npc;

/* ---------- wildlife (animals.c): harmless, mostly ---------- */

typedef enum { SPECIES_WALRUS, SPECIES_CRAB, SPECIES_TURTLE, SPECIES_FISH, SPECIES_GULL, SPECIES_DOLPHIN, SPECIES_COUNT } Species;

typedef struct {
    bool used;
    Species species;
    PLACED;
    vec3 home, target, vel;
    int state;
    float state_t, anim_t, phase;
    float whistle_t;        /* walruses: > 0 while whistling */
    float call_t;           /* time to the next call/whistle */
    int fed;                /* walruses: fish they've been given */
    float gone_t;           /* > 0: taken by the eruption */
    Pose pose;
} Animal;

/* ---------- things to use (quest.c) ---------- */

typedef enum {
    THING_SHELL,            /* a shell on the sand: index = SHELL_ color */
    THING_RELIC,            /* one of the twelve relics of the First Tide */
    THING_CHART,            /* one of the harbourmaster's three lost tide charts */
    THING_LORE,             /* a carved tablet: its story is shown when read */
    THING_PEDESTAL,         /* the Hall of Tides: where each donated relic stands */
    THING_SHRINE_LANTERN,   /* Brinewick's three shrine lanterns */
    THING_NOTICEBOARD,      /* odd jobs for shells */
    THING_WHIRLPOOL,        /* where the conch opens the way to the Drowned Court */
    THING_EMBER_HEART,      /* on its altar at Old Ember's crater */
    THING_BELL_TOWER,       /* Brinewick's empty belfry */
    THING_TREASURE,         /* buried in the sand: the detector finds it, the spade digs */
    THING_CHIME,            /* the Court's bell gallery puzzle */
    THING_LEVER,            /* the Court's tide engine puzzle */
    THING_PLATE,            /* the Court's pearl garden puzzle */
    THING_SEAL,             /* the three seals on the throne room door */
    THING_EXIT_CURRENT,     /* the rising current back up to the surface */
    THING_ANVIL,            /* Bram's anvil */
    THING_KIND_COUNT
} ThingKind;

enum { SHELL_WHITE, SHELL_PINK, SHELL_BLUE, SHELL_GOLD, SHELL_KINDS };
extern const int SHELL_VALUE[SHELL_KINDS];
extern const char *SHELL_NAME[SHELL_KINDS];

typedef struct {
    bool used;
    ThingKind kind;
    int index;
    PLACED;
    bool done;              /* taken, lit, read, solved... */
    float anim, glow;
    float respawn_t;        /* shells come back after the eruption */
} Thing;

/* a rowing boat: board it with E, row with WASD */
typedef struct {
    bool used;
    PLACED;
    vec3 vel;
    float spin;             /* turning speed */
    float row_t;            /* oar stroke phase */
    bool occupied;
} Boat;

/* ---------- quests (quest.c) ---------- */

typedef enum {
    Q_MAIN,         /* The Ember and the Tide: Brinewick's lost bell */
    Q_RELICS,       /* Relics of the First Tide */
    Q_WALRUS,       /* The Walrus Chorus */
    Q_BIG_ONE,      /* Old Brine's Big One */
    Q_JOB,          /* whatever the notice board asks for */
    Q_COUNT
} QuestId;

typedef struct {
    int stage[Q_COUNT];
    bool lanterns[3];
    bool charts[3];
    bool relics[12];
    bool lore_read[16];
    int walrus_fed;
    int fish_caught;
    int job, job_count, job_goal;   /* the notice board's current job */
    bool puzzles[3];                /* the Court's bell gallery, tide engine, pearl garden */
    int chime_step, plate_step;
    int levers[3];
    bool court_open, king_dead;
    bool bell_home;
    float bell_ring_t;              /* the bell tower rings now and then once the bell is back */
    bool donated[12];               /* relics standing in the Hall of Tides */
    bool eye_given;                 /* the Unmoored's gift */
    bool conch_caught;
    float show_t;                   /* the pearl garden showing its sequence */
    int show_i;
    float whirl_arrive;             /* > 0 while the whirlpool carries you down */
    float festival_t;               /* the village celebrating the bell */
} QuestState;

/* ---------- fishing (fishing.c) ---------- */

typedef enum { FISH_IDLE, FISH_CAST, FISH_WAIT, FISH_BITE, FISH_REEL, FISH_LANDED } FishState;

typedef struct {
    FishState state;
    float t;
    vec3 bobber, bob_vel;
    float wait;             /* until something bites */
    ItemId hooked;          /* what's on the line */
    ItemId bait;            /* what was on the hook */
    /* the reeling bar: keep the catch zone over the fish until the meter fills */
    float zone, zone_vel;
    float fish_y, fish_target, fish_speed;
    float meter;
    float landed_t;
    ItemId landed;
} Fishing;

/* ---------- Old Ember (volcano.c) ---------- */

#define ERUPTION_PERIOD 1800.0f     /* seconds between eruptions */

typedef enum { VOLC_CALM, VOLC_STIRRING, VOLC_ERUPTING, VOLC_ASH, VOLC_REBIRTH } VolcanoPhase;

typedef struct {
    float t;                /* seconds since the last rebirth */
    VolcanoPhase phase;
    float phase_t;
    float front;            /* how far the burning cloud has spread from the crater */
    float ash;              /* 0..1 how thick the ash in the air is */
    float bomb_t;
    bool caught_you;
    vec3 bombs[12], bomb_vel[12];
    bool bomb_on[12];
} Volcano;

enum {
    PM_STATUE, PM_BARREL, PM_CRATE, PM_VASE, PM_BOULDER, PM_LANTERN, PM_FIREPIT,
    PM_SHRUB_A, PM_SHRUB_B, PM_FERN, PM_FLOWERS, PM_SORREL, PM_STUMP, PM_BENCH,
    PM_BOOKSHELF, PM_BOOKS, PM_CANDLE, PM_CHANDELIER, PM_PLANT, PM_CABINET, PM_COMMODE,
    PM_MIRROR, PM_HORSE, PM_TABLE, PM_CHAIR, PM_OIL_LAMP,
    /* the coast */
    PM_WHALE, PM_SHARK, PM_RAY, PM_BUST, PM_LION, PM_CANNON, PM_SHIP, PM_SHIP_LARGE,
    PM_BUOY, PM_MARKER, PM_LIFEBUOY, PM_BARRELS, PM_BUCKET, PM_SHELL, PM_REGISTER,
    PM_GNOME, PM_UKULELE, PM_BANANAS, PM_WICKER, PM_ELEPHANT, PM_SHELVES,
    /* Brinewick */
    PM_DIYA, PM_HANG_LANTERN, PM_LANTERN_CHANDELIER, PM_WINE_BARREL, PM_PICNIC, PM_PCHAIR,
    PM_PTABLE, PM_SPINNING, PM_PLANTER, PM_TEASET, PM_CHESS,
    /* Old Ember */
    PM_PUMICE, PM_PUMICE_SMALL,
    PM_COUNT
};

enum { SK_MINION, SK_WARRIOR, SK_ROGUE, SK_MAGE, SK_COUNT };
enum { WPN_BLADE, WPN_AXE, WPN_STAFF, WPN_SHIELD, WPN_COUNT };

/* animations looked up by name in each KayKit character */
typedef struct {
    int idle, walk, run, attack, hit, death, awaken, dormant, cast, talk, sit, cheer;
    int hand_r, hand_l;     /* weapon attachment bones */
} Rig;

/* ---------- you, seen from outside (player.c) ---------- */

enum { AV_ROGUE, AV_KNIGHT, AV_BARBARIAN, AV_MAGE, AV_COUNT };

typedef struct {
    Model models[AV_COUNT];     /* the rogue is you; the others lend their armour */
    Pose poses[AV_COUNT];
    int *bone_map[AV_COUNT];    /* each model's node -> the rogue's node of the same name */
    unsigned char *skip[AV_COUNT];  /* which meshes are hidden right now */
    Rig rig;
    int head;                   /* bone for hats, helmets and the gas mask */
    float anim_t, yaw, action_t;
    int anim, last_anim;
    float blend;
} Avatar;


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
    Model door_model, chest_model, fox_model, rat_model, slime_model, slime_body_model, tomb_model, tomb_lid_model;
    Model skel_models[SK_COUNT], goblin_model, weapon_models[WPN_COUNT];
    Rig skel_rigs[SK_COUNT], goblin_rig;
    Model prop_models[PM_COUNT];
    Model npc_models[MAX_NPCS];
    Rig npc_rigs[MAX_NPCS];
    Model crab_model, jelly_model, shark_model, bat_model, eel_model, imp_model, drowned_model, wraith_model;
    Rig imp_rig, drowned_rig, wraith_rig;
    Model animal_models[SPECIES_COUNT];
    Model shell_models[SHELL_KINDS], relic_model, chart_model, bell_model, crystal_model, lore_glow_model;
    Model bobber_model, boat_model, oar_model;
    GLuint slime_prog, tornado_prog;
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
    SpellEffect effects[MAX_EFFECTS];
    Prop props[MAX_PROPS];          int prop_count;
    Npc npcs[MAX_NPCS];             int npc_count;
    vec3 fountains[4];              int fountain_count;
    vec3 spawn;
    float spawn_yaw;

    /* beyond the ruins */
    World world;
    Ocean ocean;
    Animal animals[MAX_ANIMALS];
    Thing things[MAX_THINGS];       int thing_count;
    Boat boats[MAX_BOATS];          int boat_count;
    int boat_in;                    /* the boat you're rowing, or -1 */
    vec3 vents[MAX_VENTS];          int vent_count;
    CreatureSpawn cspawns[MAX_CREATURES]; int cspawn_count;
    Volcano volcano;
    QuestState quest;
    Fishing fish;

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
    vec3 head;              /* your eyes, where you aim and reach from (the camera may be behind you) */
    bool third_person;
    float cam_dist;         /* third person: how far back the camera sits */
    ItemId equip[EQ_COUNT]; /* what you're wearing */
    Avatar avatar;
    int shells[SHELL_KINDS];
    /* swimming */
    bool swimming, underwater, wading_sea;
    float stamina, max_stamina, breath, max_breath;
    float surface_y;        /* the water's height where you are (-1000 = none) */
    float swim_phase;
    /* spells and treasures that change you */
    float gills_t, shadow_t;
    bool selkie, gill_pearl, dolphin, angler, first_tide;
    float toxic;            /* 0..1 how badly the fumes are getting to you */
    float sting_t;          /* jellyfish */
    float hail_t;
    float gnome_t;          /* the gnome you set down, and how long they'll stare */
    vec3 gnome_pos;
    float ukulele_t;
    float detector_t;       /* next beep */
    float whirl_t;          /* > 0 while the whirlpool is taking you down */
    float court_t;          /* fade when arriving at or leaving the Court */

    /* ui */
    bool inv_open, show_help, night;
    bool journal_open;
    int shop_npc;           /* whose shop is open, or -1 */
    int shop_scroll;
    char reading_title[64]; /* a tablet, a note, a relic's story: shown until you move on */
    char reading[720];
    float reading_t;
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
void game_give_quiet(Game *g, ItemId id, int count);    /* game_give, without the full-satchel note */
int count_item(const Game *g, ItemId id);
void take_items(Game *g, ItemId id, int count);
float armor_total(const Game *g);   /* 0..0.8 of each blow your armour takes */
bool is_day(const Game *g);
float storm_level(const Game *g);   /* 0..1 how rough the sea is */
/* the sea's surface over (x, z) and how deep the water is there; false = no sea here */
bool sea_at(const Game *g, float x, float z, float *surface, float *depth);
void add_shells(Game *g, int color, int count);
int shell_total(const Game *g);                 /* their worth in white shells */
bool pay_shells(Game *g, int price);            /* false if you can't afford it */
Prop *spawn_prop(Game *g, int model, vec3 pos, float yaw, float scale);
Pickup *spawn_pickup(Game *g, ItemId id, int count, vec3 pos);
void respawn_player_at(Game *g, vec3 pos, float yaw);
LightSource *spawn_light(Game *g, LightKind kind, vec3 pos, vec3 normal, float yaw, bool lit);
float wrap_angle(float a);          /* into -pi..pi */
float ground_at(const Game *g, float x, float z, float feet);
void look_dir(const Game *g, vec3 out);
void flat_forward(const Game *g, vec3 out);
bool has_item(const Game *g, ItemId id);
void hurt_player(Game *g, vec3 from, float dmg);
void explode(Game *g, vec3 at, float damage, float radius, bool hurts_player);

/* creatures.c (and beasts.c for the coast's creatures) */
void creatures_load(Game *g);
void beasts_load(Game *g);
void beasts_free(Game *g);
void creatures_free(Game *g);
Creature *creature_spawn(Game *g, CreatureType type, int variant, vec3 pos, float yaw);
void creatures_update(Game *g, float dt);
void creatures_draw(Game *g, GLuint prog, mat4 vp, bool depth);
void damage_creature(Game *g, Creature *c, float dmg, vec3 push);
float creature_radius(const Creature *c);
float creature_height(const Creature *c);
const char *creature_name(const Creature *c);
bool creature_aquatic(const Creature *c);
void creatures_regenerate(Game *g, bool sheltered_too);    /* after the eruption */
void creatures_burn(Game *g, vec3 center, float radius);    /* the eruption's front */
void beast_update(Game *g, Creature *c, float dt, float dist, vec3 to_player);  /* the new types' behaviour */
void beast_pose(Game *g, Creature *c);
const Model *beast_model(Game *g, const Creature *c);
const Rig *beast_rig(Game *g, const Creature *c);
void beast_draw(Game *g, Creature *c, GLuint prog, mat4 vp, mat4 xf, const DrawParams *dp);
void slimes_draw(Game *g, mat4 vp);         /* glossy jelly, after everything solid */

/* magic.c: spells, projectiles, tombs */
void magic_cast(Game *g, Spell spell);
void spells_draw(Game *g, GLuint prog, mat4 vp, bool depth);   /* meteors, whirlwinds, spikes */
void magic_update(Game *g, float dt);
void magic_draw(Game *g, GLuint prog, mat4 vp, bool depth);
void tombs_build_models(Game *g);
void tomb_open(Game *g, Tomb *t);
void bolt_fire(Game *g, BoltKind kind, vec3 from, vec3 vel, bool hostile);

/* npc.c: the palace's residents and the coast's people */
void npc_add(Game *g, int number, vec3 pos, float yaw);
void npcs_update(Game *g, float dt);
void npcs_draw(Game *g, GLuint prog, mat4 vp, bool depth);
void npc_talk(Game *g, Npc *n);
void npc_say(Game *g, Npc *n, const char *tag);     /* a quest line, by tag; false-y if none */
bool npc_has_line(const Npc *n, const char *tag);
Npc *npc_find(Game *g, int kind);
bool npc_visible(const Game *g, const Npc *n);      /* the Unmoored hides */
void npcs_flee_eruption(Game *g);

/* player.c: moving, swimming, boats, and you in third person */
void player_load(Game *g);
void player_free(Game *g);
void player_move(Game *g, float dt);                /* walking, swimming, rowing */
void player_third_camera(Game *g, float dt);        /* sets g->eye behind you */
void avatar_update(Game *g, float dt);
void avatar_draw(Game *g, GLuint prog, mat4 vp, bool depth);
void equip_item(Game *g, ItemId id);                /* put on / take off */
void boat_add(Game *g, vec3 pos, float yaw);
void boats_update(Game *g, float dt);
void boats_draw(Game *g, GLuint prog, mat4 vp, bool depth);
void boat_board(Game *g, int i);
void boat_leave(Game *g);
int boat_near(Game *g, float reach);

/* animals.c: the coast's wildlife */
void animals_load(Game *g);
void animals_free(Game *g);
void animal_spawn(Game *g, Species s, vec3 pos, float yaw);
void animals_update(Game *g, float dt);
void animals_draw(Game *g, GLuint prog, mat4 vp, bool depth);
Animal *animal_near(Game *g, Species s, float reach);
void animal_feed(Game *g, Animal *a);
void animals_serenade(Game *g);     /* the ukulele */
void animals_regenerate(Game *g);

/* quest.c: things to use, the quests, the journal, shops and shells */
void things_load(Game *g);
void things_free(Game *g);
Thing *thing_add(Game *g, ThingKind kind, int index, vec3 pos, float yaw);
void things_update(Game *g, float dt);
void things_draw(Game *g, GLuint prog, mat4 vp, bool depth);
const char *thing_prompt(Game *g, Thing *t);        /* NULL = nothing to do */
void thing_use(Game *g, Thing *t);
void quest_talk(Game *g, Npc *n);                   /* talking to someone the quests care about */
void quest_fish_caught(Game *g, ItemId fish);
void quest_creature_killed(Game *g, Creature *c);
void quest_update(Game *g, float dt);
void quest_regenerate(Game *g);
void journal_draw(Game *g);
void shop_open(Game *g, Npc *n);
bool shop_event(Game *g, const SDL_Event *e);       /* true if it used the event */
void shop_draw(Game *g);
void reading_show(Game *g, const char *title, const char *text);
void reading_draw(Game *g);

/* fishing.c */
void fishing_start(Game *g);                        /* the rod's click */
void fishing_update(Game *g, float dt);
void fishing_draw(Game *g, GLuint prog, mat4 vp);
void fishing_hud(Game *g);
void fishing_cancel(Game *g);
bool fishing_busy(const Game *g);

/* volcano.c: Old Ember wakes every half hour */
void volcano_init(Game *g);
void volcano_update(Game *g, float dt);
void volcano_environment(const Game *g, Environment *env);
void volcano_hud(Game *g);
void volcano_force(Game *g);                        /* F4: wake it now */
float vent_fumes(const Game *g, vec3 p);            /* 0..1 how thick the poison gas is here */

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
