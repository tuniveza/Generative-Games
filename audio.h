#ifndef AUDIO_H
#define AUDIO_H

#include <cglm/cglm.h>
#include <stdbool.h>

/* every sound effect and all the music are synthesized in code; only the palace
 * characters' voices come from files (assets/voices, made with tools/make_voices.sh) */
typedef enum {
    SFX_STEP_STONE, SFX_STEP_GRASS, SFX_STEP_WATER, SFX_JUMP, SFX_LAND,
    SFX_SWING, SFX_SWING_HEAVY, SFX_HIT, SFX_HIT_HEAVY, SFX_BLOCK,
    SFX_RAT_SQUEAK, SFX_RAT_DIE, SFX_FOX_BARK, SFX_FOX_DIE,
    SFX_BONES, SFX_SKELETON_DIE, SFX_GOBLIN, SFX_GOBLIN_DIE, SFX_SLIME, SFX_SLIME_SPLIT,
    SFX_HURT, SFX_DIE,
    SFX_DOOR_OPEN, SFX_DOOR_CLOSE, SFX_DOOR_LOCKED, SFX_UNLOCK,
    SFX_CHEST, SFX_TOMB, SFX_PICKUP, SFX_TORCH_ON, SFX_TORCH_OFF, SFX_LANTERN,
    SFX_EAT, SFX_DRINK, SFX_DUCK, SFX_WATCH, SFX_THROW, SFX_EXPLOSION, SFX_BOUNCE,
    SFX_CAST_FIRE, SFX_CAST_FROST, SFX_CAST_LIGHTNING, SFX_CAST_HEAL, SFX_CAST_WISP,
    SFX_BLINK, SFX_MAGIC_HIT, SFX_NO_MANA, SFX_THUNDER, SFX_CLICK,
    /* the sea */
    SFX_SPLASH, SFX_SWIM, SFX_GASP, SFX_BUBBLES, SFX_DROWN, SFX_STEP_SAND, SFX_STEP_WOOD,
    SFX_GULL, SFX_WALRUS_WHISTLE, SFX_WALRUS_GRUNT, SFX_DOLPHIN, SFX_OAR, SFX_BOAT_CREAK,
    SFX_CAST_LINE, SFX_REEL, SFX_BITE, SFX_CATCH, SFX_LINE_SNAP,
    SFX_JELLY_STING, SFX_SHARK_BITE, SFX_CRAB, SFX_CONCH, SFX_WHIRLPOOL,
    /* people and things */
    SFX_SHELLS, SFX_BUY, SFX_QUEST, SFX_QUEST_DONE, SFX_BELL, SFX_CHIME, SFX_LEVER, SFX_PLATE,
    SFX_GATE, SFX_PAGE, SFX_EQUIP, SFX_DIG, SFX_DETECTOR, SFX_UKULELE, SFX_GNOME, SFX_SHOP_BELL,
    /* Old Ember and the weather */
    SFX_RUMBLE, SFX_ERUPTION, SFX_LAVA_BOMB, SFX_HISS, SFX_COUGH, SFX_HAILSTONE, SFX_GUST, SFX_REBIRTH,
    /* the new creatures */
    SFX_BAT, SFX_IMP, SFX_WRAITH, SFX_EEL, SFX_KING_ROAR, SFX_DROWNED,
    /* the new spells */
    SFX_CAST_TIDE, SFX_CAST_METEOR, SFX_CAST_SPIKES, SFX_CAST_SHADOW, SFX_CAST_GALE, SFX_CAST_STARS,
    SFX_CAST_GILLS,
    SFX_COUNT
} Sfx;

typedef enum {
    MOOD_DAY, MOOD_RAIN, MOOD_NIGHT, MOOD_CRYPT, MOOD_PALACE, MOOD_GARDEN, MOOD_LIBRARY,
    /* the coast, each in its own genre */
    MOOD_BEACH,     /* bossa nova: nylon guitar, brushed shaker, walking bass */
    MOOD_VILLAGE,   /* a folk jig in 6/8: fiddle over a drone and a frame drum */
    MOOD_MARINA,    /* a sea shanty: accordion oom-pah and a tin whistle */
    MOOD_OCEAN,     /* out on the open water: a slow harp waltz */
    MOOD_UNDERSEA,  /* the Drowned Court: glass bells, choir and whale song */
    MOOD_VOLCANO,   /* taiko drums and a low brass drone */
    MOOD_TAVERN,    /* the Salted Eel: a quick reel for dancing */
    MOOD_SHOP,      /* the Curious Clam: a lazy lo-fi beat with a kalimba */
    MOOD_ANCIENT,   /* near the Unmoored: something very old and strange */
    MOOD_ERUPTION,  /* Old Ember wakes */
    MOOD_FESTIVAL,  /* the bell is home */
    MOOD_COUNT
} Mood;

/* what the soundscape should be doing; set once per frame */
typedef struct {
    Mood mood;
    bool boombox;           /* funk groove replaces the music */
    bool slowmo;            /* everything drops in pitch and dulls */
    bool underground;       /* crypt: drips and a hollow drone */
    float danger;           /* 0..1 creatures are hunting you: war drums */
    float rain;             /* 0..1 how hard it's raining */
    float sheltered;        /* 0..1 under a roof: the rain is muffled */
    float fire;             /* 0..1 loudness of the nearest fire's crackle */
    float fire_pan;         /* -1 left .. 1 right */
    float water;            /* 0..1 nearest fountain */
    float water_pan;
    float surf;             /* 0..1 waves breaking on a nearby shore */
    float surf_pan;
    bool submerged;         /* your head is under water: everything muffled, bubbles */
    float wind;             /* 0..1 a wind storm howling */
    float hail;             /* 0..1 hail rattling down */
    float tornado;          /* 0..1 a tornado's roar */
    float tornado_pan;
    float volcano;          /* 0..1 Old Ember's rumble */
    float lava;             /* 0..1 bubbling lava nearby */
    float harbour;          /* 0..1 ropes creaking, rigging clinking */
    float chimes;           /* 0..1 Brinewick's shell wind chimes */
    int groove;             /* which record the boombox is playing */
} AudioScene;

bool audio_init(void);      /* false (and silent) if there's no audio device */
void audio_shutdown(void);

void audio_set_listener(vec3 pos, float yaw);
void audio_set_scene(const AudioScene *scene);
void audio_set_volumes(float music, float effects);    /* 0..1 each */
void audio_toggle_music(void);
bool audio_music_on(void);

void audio_play(Sfx s, float volume);                   /* in your head (UI, your own body) */
void audio_play_at(Sfx s, vec3 pos, float volume);      /* in the world: panned and faded */
void audio_play_pitch(Sfx s, float volume, float pitch); /* in your head, at another pitch (1 = as made) */
void audio_play_at_pitch(Sfx s, vec3 pos, float volume, float pitch);

/* voices: load a WAV once, then speak it from a spot in the world (one at a time) */
int audio_load_clip(const char *path);                  /* -1 if missing */
float audio_clip_length(int clip);                      /* seconds */
void audio_speak(int clip, vec3 pos);
void audio_stop_speaking(void);
bool audio_speaking(void);

#endif
