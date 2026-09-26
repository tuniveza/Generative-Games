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
    SFX_COUNT
} Sfx;

typedef enum {
    MOOD_DAY, MOOD_RAIN, MOOD_NIGHT, MOOD_CRYPT, MOOD_PALACE, MOOD_GARDEN, MOOD_LIBRARY,
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

/* voices: load a WAV once, then speak it from a spot in the world (one at a time) */
int audio_load_clip(const char *path);                  /* -1 if missing */
float audio_clip_length(int clip);                      /* seconds */
void audio_speak(int clip, vec3 pos);
void audio_stop_speaking(void);
bool audio_speaking(void);

#endif
