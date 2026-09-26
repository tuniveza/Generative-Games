#include "audio.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Everything here is synthesized, except the characters' voices:
 *   - sound effects are rendered once at startup into small sample buffers
 *     (a few random variants each, so repeats don't sound identical)
 *   - music, wind, rain, birds, crickets, drips, fire and water are generated live
 * Output is 48 kHz stereo float. */

#define SR         48000
#define PI_F       3.14159265f
#define VARIANTS   4
#define MAX_VOICES 48
#define MAX_CLIPS  192

/* ---------- small DSP helpers ---------- */

static uint32_t rng_state = 0x9E3779B9u;
static float rnd(void)      /* -1 .. 1 */
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return (rng_state >> 8) / 8388608.0f - 1.0f;
}
static float rnd01(void) { return rnd() * 0.5f + 0.5f; }

/* Chamberlin state-variable filter: low-pass and band-pass outputs */
typedef struct { float lp, bp; } Svf;

static void svf(Svf *f, float in, float cutoff, float q)
{
    float c = 2.0f * sinf(PI_F * fminf(cutoff, SR / 6.5f) / SR);
    f->lp += c * f->bp;
    float hp = in - f->lp - q * f->bp;
    f->bp += c * hp;
}

static float onepole(float *state, float in, float cutoff)
{
    float a = 1.0f - expf(-2.0f * PI_F * cutoff / SR);
    *state += a * (in - *state);
    return *state;
}

static float mtof(float midi) { return 440.0f * powf(2.0f, (midi - 69.0f) / 12.0f); }

/* ---------- sound effects, rendered at startup ---------- */

typedef struct {
    float *data;
    int len;
} Buffer;

static Buffer sfx[SFX_COUNT][VARIANTS];
static float SFX_GAIN[SFX_COUNT];

static void set_gains(void)
{
    static const struct { Sfx s; float g; } g[] = {
        { SFX_STEP_STONE, 0.22f }, { SFX_STEP_GRASS, 0.16f }, { SFX_STEP_WATER, 0.3f }, { SFX_JUMP, 0.2f },
        { SFX_LAND, 0.45f }, { SFX_SWING, 0.35f }, { SFX_SWING_HEAVY, 0.45f }, { SFX_HIT, 0.7f },
        { SFX_HIT_HEAVY, 0.85f }, { SFX_BLOCK, 0.55f }, { SFX_RAT_SQUEAK, 0.35f }, { SFX_RAT_DIE, 0.45f },
        { SFX_FOX_BARK, 0.5f }, { SFX_FOX_DIE, 0.5f }, { SFX_BONES, 0.45f }, { SFX_SKELETON_DIE, 0.6f },
        { SFX_GOBLIN, 0.45f }, { SFX_GOBLIN_DIE, 0.5f }, { SFX_SLIME, 0.45f }, { SFX_SLIME_SPLIT, 0.55f },
        { SFX_HURT, 0.6f }, { SFX_DIE, 0.7f }, { SFX_DOOR_OPEN, 0.55f }, { SFX_DOOR_CLOSE, 0.65f },
        { SFX_DOOR_LOCKED, 0.5f }, { SFX_UNLOCK, 0.5f }, { SFX_CHEST, 0.55f }, { SFX_TOMB, 0.7f },
        { SFX_PICKUP, 0.35f }, { SFX_TORCH_ON, 0.5f }, { SFX_TORCH_OFF, 0.4f }, { SFX_LANTERN, 0.4f },
        { SFX_EAT, 0.5f }, { SFX_DRINK, 0.5f }, { SFX_DUCK, 0.6f }, { SFX_WATCH, 0.5f },
        { SFX_THROW, 0.35f }, { SFX_EXPLOSION, 1.0f }, { SFX_BOUNCE, 0.35f }, { SFX_CAST_FIRE, 0.6f },
        { SFX_CAST_FROST, 0.55f }, { SFX_CAST_LIGHTNING, 0.75f }, { SFX_CAST_HEAL, 0.45f },
        { SFX_CAST_WISP, 0.45f }, { SFX_BLINK, 0.55f }, { SFX_MAGIC_HIT, 0.6f }, { SFX_NO_MANA, 0.3f },
        { SFX_THUNDER, 1.0f }, { SFX_CLICK, 0.18f },
        { SFX_SPLASH, 0.7f }, { SFX_SWIM, 0.35f }, { SFX_GASP, 0.5f }, { SFX_BUBBLES, 0.35f }, { SFX_DROWN, 0.6f },
        { SFX_STEP_SAND, 0.18f }, { SFX_STEP_WOOD, 0.3f }, { SFX_GULL, 0.35f }, { SFX_WALRUS_WHISTLE, 0.55f },
        { SFX_WALRUS_GRUNT, 0.55f }, { SFX_DOLPHIN, 0.4f }, { SFX_OAR, 0.45f }, { SFX_BOAT_CREAK, 0.4f },
        { SFX_CAST_LINE, 0.45f }, { SFX_REEL, 0.25f }, { SFX_BITE, 0.5f }, { SFX_CATCH, 0.45f }, { SFX_LINE_SNAP, 0.5f },
        { SFX_JELLY_STING, 0.55f }, { SFX_SHARK_BITE, 0.8f }, { SFX_CRAB, 0.4f }, { SFX_CONCH, 0.8f }, { SFX_WHIRLPOOL, 0.8f },
        { SFX_SHELLS, 0.4f }, { SFX_BUY, 0.5f }, { SFX_QUEST, 0.45f }, { SFX_QUEST_DONE, 0.55f }, { SFX_BELL, 0.9f },
        { SFX_CHIME, 0.5f }, { SFX_LEVER, 0.55f }, { SFX_PLATE, 0.5f }, { SFX_GATE, 0.8f }, { SFX_PAGE, 0.3f },
        { SFX_EQUIP, 0.45f }, { SFX_DIG, 0.5f }, { SFX_DETECTOR, 0.25f }, { SFX_UKULELE, 0.55f }, { SFX_GNOME, 0.45f },
        { SFX_SHOP_BELL, 0.4f }, { SFX_RUMBLE, 0.9f }, { SFX_ERUPTION, 1.0f }, { SFX_LAVA_BOMB, 0.8f }, { SFX_HISS, 0.4f },
        { SFX_COUGH, 0.5f }, { SFX_HAILSTONE, 0.3f }, { SFX_GUST, 0.5f }, { SFX_REBIRTH, 0.6f },
        { SFX_BAT, 0.35f }, { SFX_IMP, 0.5f }, { SFX_WRAITH, 0.5f }, { SFX_EEL, 0.55f }, { SFX_KING_ROAR, 1.0f },
        { SFX_DROWNED, 0.5f }, { SFX_CAST_TIDE, 0.7f }, { SFX_CAST_METEOR, 0.7f }, { SFX_CAST_SPIKES, 0.6f },
        { SFX_CAST_SHADOW, 0.5f }, { SFX_CAST_GALE, 0.6f }, { SFX_CAST_STARS, 0.5f }, { SFX_CAST_GILLS, 0.5f },
    };
    for (size_t i = 0; i < sizeof g / sizeof g[0]; i++)
        SFX_GAIN[g[i].s] = g[i].g;
}

static float *new_buffer(Buffer *b, float seconds)
{
    b->len = (int)(seconds * SR);
    b->data = calloc(b->len, sizeof *b->data);
    return b->data;
}

static void normalize(Buffer *b)
{
    float peak = 1e-6f;
    for (int i = 0; i < b->len; i++)
        peak = fmaxf(peak, fabsf(b->data[i]));
    /* short fades at both ends so nothing clicks */
    for (int i = 0; i < b->len; i++) {
        float fade = fminf(1.0f, fminf(i / 48.0f, (b->len - i) / 240.0f));
        b->data[i] = b->data[i] / peak * fade;
    }
}

/* exponential decay envelope with a short attack */
static float env(float t, float attack, float decay)
{
    if (t < attack)
        return t / attack;
    return expf(-(t - attack) / decay);
}

/* a burst of filtered noise: footsteps, swooshes, crunches */
static void gen_noise_hit(Buffer *b, float len, float cutoff, float q, float attack, float decay, bool bandpass)
{
    float *d = new_buffer(b, len);
    Svf f = {0};
    for (int i = 0; i < b->len; i++) {
        float t = (float)i / SR;
        svf(&f, rnd(), cutoff, q);
        d[i] = (bandpass ? f.bp : f.lp) * env(t, attack, decay);
    }
}

/* sine with a pitch glide: thumps, chirps, squeaks */
static void add_glide(float *d, int len, float f0, float f1, float glide_time, float amp,
                      float attack, float decay, float vibrato)
{
    float phase = 0;
    for (int i = 0; i < len; i++) {
        float t = (float)i / SR;
        float k = fminf(t / glide_time, 1.0f);
        float f = f0 + (f1 - f0) * k;
        f *= 1.0f + vibrato * sinf(t * 2.0f * PI_F * 28.0f);
        phase += f / SR;
        d[i] += sinf(phase * 2.0f * PI_F) * amp * env(t, attack, decay);
    }
}

/* wood or stone groaning: a buzzy tone with a jittery, sticking pitch */
static void add_creak(float *d, int len, float base, float formant, float amp, float wobble)
{
    Svf f = {0};
    float phase = 0, slip = 0;
    for (int i = 0; i < len; i++) {
        float t = (float)i / SR;
        if ((i & 255) == 0)
            slip = rnd() * wobble;
        float freq = base * (1.0f + 0.25f * sinf(t * 3.1f) + slip);
        phase += freq / SR;
        float saw = 2.0f * (phase - floorf(phase)) - 1.0f;
        svf(&f, saw, formant, 0.25f);
        float shape = sinf(PI_F * fminf(t / ((float)len / SR), 1.0f));
        d[i] += f.bp * amp * shape;
    }
}

/* a struck bell or metal plate: inharmonic partials */
static void add_bell(float *d, int len, float base, const float *ratios, int n, float amp, float decay)
{
    for (int p = 0; p < n; p++) {
        float f = base * ratios[p];
        for (int i = 0; i < len; i++) {
            float t = (float)i / SR;
            d[i] += sinf(2.0f * PI_F * f * t) * amp / (p + 1) * expf(-t / (decay / (1.0f + p * 0.4f)));
        }
    }
}

/* a buzzy voice through a vowel formant: grunts, barks, cackles */
static void add_voice(float *d, int len, float f0, float f1, float formant, float noise, float attack, float decay)
{
    Svf f = {0};
    float phase = 0;
    for (int i = 0; i < len; i++) {
        float t = (float)i / SR;
        float fr = f0 + (f1 - f0) * (float)i / len;
        phase += fr / SR;
        float saw = 2.0f * (phase - floorf(phase)) - 1.0f + rnd() * noise;
        svf(&f, saw, formant, 0.35f);
        d[i] += f.bp * env(t, attack, decay);
    }
}

/* band-passed noise that sweeps: whooshes */
static void add_whoosh(float *d, int len, float f0, float f1, float q, float amp)
{
    Svf f = {0};
    for (int i = 0; i < len; i++) {
        float t = (float)i / len;
        svf(&f, rnd(), f0 + (f1 - f0) * t, q);
        d[i] += f.bp * amp * sinf(PI_F * t);
    }
}

/* a plucked string (Karplus-Strong) mixed into a buffer: ukulele, guitar */
static void add_ks(float *d, int len, float freq, float amp, float decay)
{
    int n = (int)(SR / freq);
    if (n < 2) n = 2;
    float *ring = calloc(n, sizeof *ring);
    for (int i = 0; i < n; i++)
        ring[i] = rnd();
    float rho = powf(0.001f, 1.0f / (decay * freq));
    int idx = 0;
    for (int i = 0; i < len; i++) {
        int next = (idx + 1) % n;
        float v = ring[idx];
        ring[idx] = rho * 0.5f * (ring[idx] + ring[next]);
        idx = next;
        d[i] += v * amp;
    }
    free(ring);
}

/* noise swelling up and falling away through a moving low-pass: waves, wind, rumbles */
static void add_swell(float *d, int len, float c0, float c1, float amp, float peak)
{
    float lp1 = 0, lp2 = 0;
    for (int i = 0; i < len; i++) {
        float t = (float)i / len;
        float e = t < peak ? t / peak : (1.0f - t) / (1.0f - peak);
        float cut = c0 + (c1 - c0) * e;
        d[i] += onepole(&lp2, onepole(&lp1, rnd(), cut), cut) * amp * e;
    }
}

static const float PLATE[5] = { 1.0f, 2.38f, 3.42f, 5.01f, 6.7f };
static const float BELL[4] = { 1.0f, 2.0f, 3.01f, 4.2f };
static const float CLICKS[3] = { 1.0f, 2.7f, 5.1f };

static void gen_sfx(void)
{
    for (int v = 0; v < VARIANTS; v++) {
        float r = rnd01();          /* per-variant randomness */
        Buffer *b;
        float *d;

        /* footsteps: stone, grass, and sloshing through water */
        gen_noise_hit(&sfx[SFX_STEP_STONE][v], 0.12f, 700 + r * 500, 0.6f, 0.002f, 0.025f, false);
        add_glide(sfx[SFX_STEP_STONE][v].data, sfx[SFX_STEP_STONE][v].len, 120 + r * 40, 70, 0.05f, 0.5f, 0.002f, 0.03f, 0);
        gen_noise_hit(&sfx[SFX_STEP_GRASS][v], 0.16f, 2500 + r * 1500, 0.9f, 0.01f, 0.04f, true);
        b = &sfx[SFX_STEP_WATER][v];
        d = new_buffer(b, 0.4f);
        {
            Svf f = {0};
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                svf(&f, rnd(), 900 + 1800 * expf(-t * 8) + 400 * sinf(t * 60), 0.3f);
                d[i] = f.bp * env(t, 0.02f, 0.1f);
            }
        }

        gen_noise_hit(&sfx[SFX_JUMP][v], 0.2f, 900 + r * 300, 1.2f, 0.03f, 0.06f, true);
        b = &sfx[SFX_LAND][v];
        gen_noise_hit(b, 0.3f, 400, 0.7f, 0.002f, 0.06f, false);
        add_glide(b->data, b->len, 90, 45, 0.15f, 1.2f, 0.002f, 0.09f, 0);

        /* swings: band-passed noise whose pitch rises then falls, like air parting */
        for (int heavy = 0; heavy < 2; heavy++) {
            b = &sfx[heavy ? SFX_SWING_HEAVY : SFX_SWING][v];
            float len = heavy ? 0.45f : 0.28f;
            d = new_buffer(b, len);
            Svf f = {0};
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR / len;
                float sweep = sinf(PI_F * t);
                svf(&f, rnd(), (heavy ? 250 : 500) + sweep * (heavy ? 900 : 2000) * (0.8f + r * 0.4f), 0.5f);
                d[i] = f.bp * sweep * sweep;
            }
        }

        /* hits: a body thud plus a crunch */
        for (int heavy = 0; heavy < 2; heavy++) {
            b = &sfx[heavy ? SFX_HIT_HEAVY : SFX_HIT][v];
            d = new_buffer(b, heavy ? 0.45f : 0.28f);
            add_glide(d, b->len, heavy ? 95 : 140, heavy ? 45 : 70, 0.12f, 1.0f, 0.001f, heavy ? 0.12f : 0.07f, 0);
            Svf f = {0};
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                svf(&f, rnd(), 1400 + r * 900, 0.8f);
                d[i] += f.bp * 1.4f * env(t, 0.001f, heavy ? 0.05f : 0.03f);
            }
        }

        b = &sfx[SFX_BLOCK][v];
        d = new_buffer(b, 0.8f);
        add_bell(d, b->len, 480 + r * 80, PLATE, 5, 0.7f, 0.35f);
        for (int i = 0; i < 600; i++)
            d[i] += rnd() * (1.0f - i / 600.0f);

        /* rats */
        b = &sfx[SFX_RAT_SQUEAK][v];
        d = new_buffer(b, 0.35f);
        for (int c = 0; c < 1 + (int)(r * 2.5f); c++) {
            int start = (int)(c * 0.11f * SR);
            add_glide(d + start, b->len - start, 2600 + r * 900, 3600 + r * 600, 0.05f, 0.8f, 0.005f, 0.04f, 0.04f);
        }
        b = &sfx[SFX_RAT_DIE][v];
        d = new_buffer(b, 0.6f);
        add_glide(d, b->len, 3400 + r * 400, 1100, 0.5f, 0.8f, 0.01f, 0.25f, 0.06f);

        /* foxes: raspy barks, a yelp */
        b = &sfx[SFX_FOX_BARK][v];
        d = new_buffer(b, 0.45f);
        for (int c = 0; c < 1 + (v & 1); c++)
            add_voice(d + (int)(c * 0.2f * SR), (int)(0.18f * SR), 560, 310, 1100 + r * 300, 0.35f, 0.008f, 0.06f);
        b = &sfx[SFX_FOX_DIE][v];
        d = new_buffer(b, 0.7f);
        add_voice(d, b->len, 820, 320, 1100, 0.3f, 0.01f, 0.25f);

        /* skeletons: dry bones clattering, and a collapse into a pile */
        b = &sfx[SFX_BONES][v];
        d = new_buffer(b, 0.5f);
        for (int c = 0; c < 6; c++) {
            int start = (int)((c * 0.06f + rnd01() * 0.03f) * SR);
            add_bell(d + start, b->len - start, 900 + rnd01() * 1400, CLICKS, 3, 0.4f + rnd01() * 0.3f, 0.015f);
        }
        b = &sfx[SFX_SKELETON_DIE][v];
        d = new_buffer(b, 1.2f);
        for (int c = 0; c < 18; c++) {
            int start = (int)(powf(c / 18.0f, 1.6f) * 0.9f * SR);
            add_bell(d + start, b->len - start, 600 + rnd01() * 1500, CLICKS, 3, 0.5f * (1.0f - c / 20.0f), 0.02f);
        }

        /* goblins: a nasal cackle, and a squawk */
        b = &sfx[SFX_GOBLIN][v];
        d = new_buffer(b, 0.6f);
        for (int c = 0; c < 4; c++)
            add_voice(d + (int)(c * 0.12f * SR), (int)(0.1f * SR), 380 + r * 60 + c * 20, 330, 1700 + r * 400, 0.2f, 0.005f, 0.04f);
        b = &sfx[SFX_GOBLIN_DIE][v];
        d = new_buffer(b, 0.7f);
        add_voice(d, b->len, 520, 180, 1500, 0.3f, 0.01f, 0.22f);

        /* slimes: wet squelches; splitting in two is a bigger, bubbling one */
        for (int split = 0; split < 2; split++) {
            b = &sfx[split ? SFX_SLIME_SPLIT : SFX_SLIME][v];
            float len = split ? 0.7f : 0.35f;
            d = new_buffer(b, len);
            Svf f = {0};
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                float wob = sinf(t * (split ? 38.0f : 55.0f) * 2 * PI_F) * 0.5f + 0.5f;
                svf(&f, rnd(), 250 + 900 * wob * expf(-t * 4), 0.15f);
                d[i] = f.bp * env(t, 0.01f, split ? 0.2f : 0.09f);
            }
            add_glide(d, b->len, split ? 160 : 220, 90, len, 0.4f, 0.01f, len * 0.4f, 0.05f);
        }

        /* the player: a grunt when hurt, a long groan when falling */
        b = &sfx[SFX_HURT][v];
        d = new_buffer(b, 0.35f);
        add_voice(d, b->len, 125 + r * 25, 85 + r * 15, 600, 0.25f, 0.02f, 0.12f);
        b = &sfx[SFX_DIE][v];
        d = new_buffer(b, 1.2f);
        add_voice(d, b->len, 105, 68, 600, 0.25f, 0.02f, 0.45f);

        /* doors */
        b = &sfx[SFX_DOOR_OPEN][v];
        d = new_buffer(b, 1.4f);
        add_creak(d, b->len, 70 + r * 25, 850 + r * 250, 1.0f, 0.12f);
        b = &sfx[SFX_DOOR_CLOSE][v];
        d = new_buffer(b, 1.4f);
        add_creak(d, (int)(0.8f * SR), 90 + r * 20, 900, 0.7f, 0.12f);
        add_glide(d + (int)(0.8f * SR), b->len - (int)(0.8f * SR), 75, 38, 0.2f, 1.6f, 0.002f, 0.18f, 0);
        for (int i = 0; i < 2400; i++)
            d[(int)(0.8f * SR) + i] += rnd() * 0.6f * (1.0f - i / 2400.0f);

        b = &sfx[SFX_DOOR_LOCKED][v];
        d = new_buffer(b, 0.5f);
        for (int c = 0; c < 3; c++)
            add_bell(d + (int)(c * 0.09f * SR), b->len - (int)(c * 0.09f * SR), 1300 + r * 300 + c * 120, CLICKS, 3, 0.6f, 0.02f);
        b = &sfx[SFX_UNLOCK][v];
        d = new_buffer(b, 0.6f);
        add_bell(d, b->len, 1500, CLICKS, 3, 0.5f, 0.02f);
        add_bell(d + (int)(0.15f * SR), b->len - (int)(0.15f * SR), 900, CLICKS, 3, 0.7f, 0.05f);
        add_glide(d + (int)(0.15f * SR), b->len - (int)(0.15f * SR), 160, 90, 0.1f, 0.8f, 0.001f, 0.06f, 0);

        b = &sfx[SFX_CHEST][v];
        d = new_buffer(b, 1.1f);
        add_creak(d, (int)(0.75f * SR), 150 + r * 40, 1300, 0.8f, 0.15f);
        add_glide(d + (int)(0.75f * SR), b->len - (int)(0.75f * SR), 210, 120, 0.05f, 0.9f, 0.001f, 0.06f, 0);

        /* a tomb lid grinding aside, and an eerie chord rising out of it */
        b = &sfx[SFX_TOMB][v];
        d = new_buffer(b, 3.0f);
        {
            Svf f = {0};
            float lp = 0;
            for (int i = 0; i < (int)(1.6f * SR); i++) {
                float t = (float)i / SR;
                svf(&f, rnd(), 200 + 150 * sinf(t * 23.0f), 0.2f);
                d[i] += onepole(&lp, f.bp, 900) * 2.0f * sinf(PI_F * t / 1.6f);
            }
            static const float notes[3] = { 61, 64, 68 };
            for (int n = 0; n < 3; n++)
                for (int i = (int)(0.8f * SR); i < b->len; i++) {
                    float t = (float)i / SR - 0.8f;
                    d[i] += sinf(2 * PI_F * mtof(notes[n]) * t) * 0.25f * sinf(PI_F * fminf(t / 2.2f, 1.0f)) *
                            (1.0f + 0.3f * sinf(t * 5.0f + n));
                }
        }

        b = &sfx[SFX_PICKUP][v];
        d = new_buffer(b, 1.0f);
        add_bell(d, b->len, mtof(76 + (v % 2) * 2), BELL, 4, 0.5f, 0.35f);
        add_bell(d + (int)(0.09f * SR), b->len - (int)(0.09f * SR), mtof(83 + (v % 2) * 2), BELL, 4, 0.5f, 0.5f);

        b = &sfx[SFX_TORCH_ON][v];
        d = new_buffer(b, 0.9f);
        {
            Svf f = {0};
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                svf(&f, rnd(), 300 + 2500 * fminf(t / 0.25f, 1.0f) * expf(-t * 2.0f), 0.6f);
                d[i] = (f.bp + f.lp * 0.6f) * env(t, 0.08f, 0.3f);
            }
        }
        gen_noise_hit(&sfx[SFX_TORCH_OFF][v], 0.7f, 5000, 1.2f, 0.01f, 0.2f, true);
        /* a lantern's handle and glass */
        b = &sfx[SFX_LANTERN][v];
        d = new_buffer(b, 0.5f);
        add_bell(d, b->len, 1800 + r * 400, PLATE, 5, 0.4f, 0.08f);
        add_bell(d + (int)(0.07f * SR), b->len - (int)(0.07f * SR), 2600 + r * 300, CLICKS, 3, 0.3f, 0.04f);

        b = &sfx[SFX_EAT][v];
        d = new_buffer(b, 0.9f);
        for (int c = 0; c < 3; c++) {
            Svf f = {0};
            int start = (int)((c * 0.26f + r * 0.03f) * SR);
            for (int i = start; i < b->len; i++) {
                float t = (float)(i - start) / SR;
                svf(&f, rnd(), 1800 + rnd01() * 1500, 0.9f);
                d[i] += f.bp * env(t, 0.003f, 0.05f);
            }
        }
        /* gulps */
        b = &sfx[SFX_DRINK][v];
        d = new_buffer(b, 0.9f);
        for (int c = 0; c < 3; c++)
            add_glide(d + (int)(c * 0.25f * SR), (int)(0.2f * SR), 180, 420, 0.12f, 0.8f, 0.01f, 0.06f, 0);

        b = &sfx[SFX_DUCK][v];
        d = new_buffer(b, 0.55f);
        add_glide(d, (int)(0.25f * SR), 950, 1350, 0.2f, 1.0f, 0.01f, 0.15f, 0.09f);
        add_glide(d + (int)(0.27f * SR), b->len - (int)(0.27f * SR), 1250, 1500, 0.15f, 0.8f, 0.01f, 0.12f, 0.1f);

        b = &sfx[SFX_WATCH][v];
        d = new_buffer(b, 2.2f);
        {
            static const float tick[3] = { 1.0f, 3.1f, 6.3f };
            float at = 0;
            for (int c = 0; c < 6; c++) {
                add_bell(d + (int)(at * SR), b->len - (int)(at * SR), c % 2 ? 2400 : 3000, tick, 3, 0.5f, 0.012f);
                at += 0.18f + c * 0.07f;
            }
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                d[i] += sinf(2 * PI_F * 55 * t) * 0.35f * sinf(PI_F * t / 2.2f);
            }
        }

        gen_noise_hit(&sfx[SFX_THROW][v], 0.3f, 1200 + r * 400, 0.8f, 0.05f, 0.08f, true);

        b = &sfx[SFX_EXPLOSION][v];
        d = new_buffer(b, 2.6f);
        {
            float lp1 = 0, lp2 = 0;
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                float cutoff = 150 + 5000 * expf(-t * 3.0f);
                float n = onepole(&lp2, onepole(&lp1, rnd(), cutoff), cutoff);
                d[i] = n * 3.0f * env(t, 0.003f, 0.6f) + (i < 400 ? rnd() * (1.0f - i / 400.0f) : 0.0f);
            }
            add_glide(d, b->len, 70, 30, 0.6f, 1.2f, 0.005f, 0.5f, 0);
        }

        b = &sfx[SFX_BOUNCE][v];
        d = new_buffer(b, 0.2f);
        static const float tink[3] = { 1.0f, 2.9f, 4.7f };
        add_bell(d, b->len, 700 + r * 200, tink, 3, 0.6f, 0.04f);

        /* spells */
        b = &sfx[SFX_CAST_FIRE][v];                 /* a roaring whoosh */
        d = new_buffer(b, 0.8f);
        add_whoosh(d, b->len, 400, 2000, 0.5f, 1.0f);
        add_glide(d, b->len, 90, 60, 0.4f, 0.5f, 0.02f, 0.2f, 0.05f);
        b = &sfx[SFX_CAST_FROST][v];                /* crystalline shimmer and a crack */
        d = new_buffer(b, 1.2f);
        {
            static const float ice[5] = { 1.0f, 1.5f, 2.02f, 2.97f, 4.1f };
            add_bell(d, b->len, 1400 + r * 200, ice, 5, 0.5f, 0.5f);
            for (int i = 0; i < 2000; i++)
                d[i] += rnd() * (1.0f - i / 2000.0f) * 0.8f;
            add_whoosh(d, b->len, 7000, 4000, 0.4f, 0.3f);
        }
        b = &sfx[SFX_CAST_LIGHTNING][v];            /* a hard electric crack and buzz */
        d = new_buffer(b, 0.9f);
        {
            float phase = 0;
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                phase += (120 + rnd() * 60) / SR;
                float buzz = (phase - floorf(phase)) * 2 - 1;
                d[i] = (rnd() * (i < 1500 ? 1.0f : 0.3f) + buzz * 0.6f) * env(t, 0.001f, 0.18f);
            }
        }
        b = &sfx[SFX_CAST_HEAL][v];                 /* a warm rising chord */
        d = new_buffer(b, 1.6f);
        {
            static const float notes[4] = { 72, 76, 79, 84 };
            for (int n = 0; n < 4; n++) {
                int start = (int)(n * 0.08f * SR);
                add_bell(d + start, b->len - start, mtof(notes[n]), BELL, 4, 0.4f, 0.8f);
            }
        }
        b = &sfx[SFX_CAST_WISP][v];                 /* a soft chime and an airy breath */
        d = new_buffer(b, 1.4f);
        add_bell(d, b->len, mtof(88 + v % 3), BELL, 4, 0.5f, 0.6f);
        add_whoosh(d, b->len, 2500, 3500, 0.7f, 0.25f);
        b = &sfx[SFX_BLINK][v];                     /* a whoosh that snaps shut */
        d = new_buffer(b, 0.5f);
        add_whoosh(d, b->len, 300, 6300, 0.5f, 1.0f);
        add_glide(d, b->len, 300, 2400, 0.3f, 0.4f, 0.01f, 0.2f, 0);
        b = &sfx[SFX_MAGIC_HIT][v];
        d = new_buffer(b, 0.5f);
        add_bell(d, b->len, 600 + r * 300, PLATE, 5, 0.5f, 0.12f);
        for (int i = 0; i < 3000; i++)
            d[i] += rnd() * (1.0f - i / 3000.0f) * 0.7f;
        gen_noise_hit(&sfx[SFX_NO_MANA][v], 0.3f, 500, 0.6f, 0.01f, 0.08f, true);

        /* thunder: a crack for close strikes, then a long rolling rumble */
        b = &sfx[SFX_THUNDER][v];
        d = new_buffer(b, 6.0f);
        {
            float lp1 = 0, lp2 = 0;
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                float roll = 0.6f + 0.4f * sinf(t * (2.0f + r)) * sinf(t * 0.7f + v);
                float n = onepole(&lp2, onepole(&lp1, rnd(), 90 + 400 * expf(-t * 1.5f)), 120);
                d[i] = n * 6.0f * env(t, 0.05f + r * 0.2f, 1.6f) * roll;
                if (v < 2 && i < 3000)
                    d[i] += rnd() * (1.0f - i / 3000.0f) * 0.8f;
            }
        }

        gen_noise_hit(&sfx[SFX_CLICK][v], 0.05f, 3000, 1.0f, 0.001f, 0.008f, true);

        /* ---- the sea ---- */
        b = &sfx[SFX_SPLASH][v];
        d = new_buffer(b, 1.1f);
        add_swell(d, b->len, 300, 5000, 1.4f, 0.06f);
        for (int c = 0; c < 5; c++) {
            int st = (int)((0.05f + rnd01() * 0.4f) * SR);
            add_glide(d + st, (int)(0.12f * SR), 300 + rnd01() * 300, 900 + rnd01() * 700, 0.1f, 0.25f, 0.005f, 0.04f, 0);
        }
        b = &sfx[SFX_SWIM][v];
        d = new_buffer(b, 0.6f);
        add_swell(d, b->len, 400, 2500 + r * 1500, 0.9f, 0.3f);
        add_glide(d + (int)(0.15f * SR), (int)(0.1f * SR), 400, 700, 0.08f, 0.15f, 0.005f, 0.03f, 0);
        b = &sfx[SFX_GASP][v];
        d = new_buffer(b, 0.7f);
        {
            Svf f = {0};
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                svf(&f, rnd(), 900 + 1600 * t, 1.2f);
                d[i] = f.bp * sinf(PI_F * fminf(t / 0.55f, 1.0f)) * (t < 0.55f ? 1.0f : 0.0f);
            }
        }
        b = &sfx[SFX_BUBBLES][v];
        d = new_buffer(b, 0.8f);
        for (int c = 0; c < 7; c++) {
            int st = (int)(rnd01() * 0.6f * SR);
            add_glide(d + st, b->len - st, 250 + rnd01() * 400, 700 + rnd01() * 900, 0.04f, 0.4f, 0.002f, 0.03f, 0);
        }
        b = &sfx[SFX_DROWN][v];
        d = new_buffer(b, 0.9f);
        add_voice(d, b->len, 150, 90, 500, 0.4f, 0.02f, 0.3f);
        for (int c = 0; c < 6; c++) {
            int st = (int)(rnd01() * 0.7f * SR);
            add_glide(d + st, b->len - st, 200 + rnd01() * 200, 500, 0.04f, 0.5f, 0.002f, 0.03f, 0);
        }
        gen_noise_hit(&sfx[SFX_STEP_SAND][v], 0.14f, 3500 + r * 1500, 0.7f, 0.006f, 0.035f, true);
        b = &sfx[SFX_STEP_WOOD][v];
        d = new_buffer(b, 0.2f);
        add_glide(d, b->len, 190 + r * 40, 120, 0.05f, 0.9f, 0.002f, 0.05f, 0);
        add_bell(d, b->len, 900 + r * 300, CLICKS, 3, 0.25f, 0.02f);
        b = &sfx[SFX_GULL][v];
        d = new_buffer(b, 0.9f);
        for (int c = 0; c < 2 + (v & 1); c++)
            add_voice(d + (int)(c * 0.26f * SR), (int)(0.22f * SR), 820 + r * 150 - c * 60, 560, 1900 + r * 300, 0.25f, 0.01f, 0.08f);
        /* the walrus's whistle: a sweet breathy up-glide, a flick higher, then a long
         * warbling fall ("fweee... fwoo"), each walrus in its own key */
        b = &sfx[SFX_WALRUS_WHISTLE][v];
        d = new_buffer(b, 2.2f);
        {
            float base = 820 + r * 260;
            float phase = 0;
            Svf breath = {0};
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                /* the pitch path through the phrase */
                float f;
                if (t < 0.35f) f = base * (0.8f + 0.55f * (t / 0.35f));
                else if (t < 0.62f) f = base * (1.35f - 0.25f * sinf((t - 0.35f) / 0.27f * PI_F) + 0.25f * (t - 0.35f) / 0.27f);
                else if (t < 1.2f) f = base * (1.6f - 0.15f * ((t - 0.62f) / 0.58f));
                else f = base * (1.45f - 0.7f * ((t - 1.2f) / 0.7f));
                f *= 1.0f + 0.018f * sinf(t * 2 * PI_F * 6.5f) * (t > 0.6f ? 1.0f : 0.3f);
                phase += f / SR;
                float gate = (t < 0.33f || (t > 0.37f && t < 1.9f)) ? 1.0f : 0.15f;
                float e = fminf(t / 0.04f, 1.0f) * fminf((2.0f - t) / 0.15f, 1.0f) * gate;
                if (e < 0) e = 0;
                svf(&breath, rnd(), f, 0.08f);
                d[i] = (sinf(2 * PI_F * phase) * 0.85f + 0.12f * sinf(4 * PI_F * phase) + breath.bp * 0.35f) * e;
            }
        }
        b = &sfx[SFX_WALRUS_GRUNT][v];
        d = new_buffer(b, 0.9f);
        add_voice(d, b->len, 95 + r * 20, 60, 380, 0.45f, 0.03f, 0.3f);
        b = &sfx[SFX_DOLPHIN][v];
        d = new_buffer(b, 1.0f);
        for (int c = 0; c < 12; c++)
            add_bell(d + (int)(c * 0.035f * SR), (int)(0.02f * SR), 3000 + rnd01() * 2000, CLICKS, 3, 0.4f, 0.004f);
        add_glide(d + (int)(0.45f * SR), (int)(0.4f * SR), 5000 + r * 2000, 9000, 0.3f, 0.5f, 0.02f, 0.2f, 0.03f);
        b = &sfx[SFX_OAR][v];
        d = new_buffer(b, 0.7f);
        add_swell(d, b->len, 300, 2200, 0.8f, 0.25f);
        add_creak(d, (int)(0.3f * SR), 120 + r * 30, 700, 0.35f, 0.1f);
        b = &sfx[SFX_BOAT_CREAK][v];
        d = new_buffer(b, 1.0f);
        add_creak(d, b->len, 85 + r * 30, 600 + r * 200, 0.9f, 0.14f);
        b = &sfx[SFX_CAST_LINE][v];
        d = new_buffer(b, 0.9f);
        add_whoosh(d, (int)(0.35f * SR), 800, 3500, 0.6f, 1.0f);
        for (int c = 0; c < 14; c++)
            add_bell(d + (int)((0.3f + c * 0.03f) * SR), (int)(0.02f * SR), 2800, CLICKS, 3, 0.25f, 0.005f);
        b = &sfx[SFX_REEL][v];
        d = new_buffer(b, 0.35f);
        for (int c = 0; c < 8; c++)
            add_bell(d + (int)(c * 0.04f * SR), (int)(0.03f * SR), 2400 + r * 400, CLICKS, 3, 0.35f, 0.006f);
        b = &sfx[SFX_BITE][v];
        d = new_buffer(b, 0.5f);
        add_glide(d, b->len, 700 + r * 200, 180, 0.15f, 0.9f, 0.002f, 0.08f, 0);
        add_swell(d, b->len, 400, 3000, 0.5f, 0.1f);
        b = &sfx[SFX_CATCH][v];
        d = new_buffer(b, 1.2f);
        {
            static const float notes[4] = { 72, 76, 79, 84 };
            for (int n = 0; n < 4; n++)
                add_bell(d + (int)(n * 0.1f * SR), b->len - (int)(n * 0.1f * SR), mtof(notes[n] + (v % 2) * 2), BELL, 4, 0.4f, 0.4f);
        }
        b = &sfx[SFX_LINE_SNAP][v];
        d = new_buffer(b, 0.6f);
        add_glide(d, b->len, 1800, 500, 0.3f, 0.6f, 0.001f, 0.15f, 0.02f);
        for (int i = 0; i < 800; i++)
            d[i] += rnd() * (1.0f - i / 800.0f);
        b = &sfx[SFX_JELLY_STING][v];
        d = new_buffer(b, 0.5f);
        {
            float phase = 0;
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                phase += (220 + rnd() * 90) / SR;
                d[i] = ((phase - floorf(phase)) * 2 - 1 + rnd() * 0.5f) * env(t, 0.002f, 0.1f);
            }
        }
        b = &sfx[SFX_SHARK_BITE][v];
        d = new_buffer(b, 0.7f);
        add_glide(d, b->len, 110, 45, 0.2f, 1.2f, 0.002f, 0.2f, 0);
        for (int i = 0; i < 6000; i++)
            d[i] += rnd() * 0.8f * (1.0f - i / 6000.0f);
        b = &sfx[SFX_CRAB][v];
        d = new_buffer(b, 0.4f);
        for (int c = 0; c < 4; c++)
            add_bell(d + (int)(c * 0.07f * SR), (int)(0.05f * SR), 1400 + rnd01() * 800, CLICKS, 3, 0.5f, 0.01f);
        /* the Conch of the Deep: a long, low, breathy horn */
        b = &sfx[SFX_CONCH][v];
        d = new_buffer(b, 3.2f);
        {
            Svf f = {0}, f2 = {0};
            float phase = 0;
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                float fr = 98.0f * (1.0f + 0.01f * sinf(t * 2 * PI_F * 4.5f)) * (1.0f + 0.03f * fminf(t, 0.5f));
                phase += fr / SR;
                float saw = 2 * (phase - floorf(phase)) - 1 + rnd() * 0.15f;
                svf(&f, saw, 520, 0.2f);
                svf(&f2, saw, 1100, 0.3f);
                float e = fminf(t / 0.4f, 1.0f) * fminf((3.2f - t) / 0.8f, 1.0f);
                d[i] = (f.bp + f2.bp * 0.4f) * e;
            }
        }
        b = &sfx[SFX_WHIRLPOOL][v];
        d = new_buffer(b, 4.5f);
        {
            Svf f = {0};
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                svf(&f, rnd(), 250 + 600 * (0.5f + 0.5f * sinf(t * 7.0f)) + t * 200, 0.3f);
                d[i] = f.bp * sinf(PI_F * t / 4.5f) * 1.5f;
            }
            add_glide(d, b->len, 60, 35, 4.0f, 0.6f, 0.5f, 2.5f, 0.02f);
        }

        /* ---- people and things ---- */
        b = &sfx[SFX_SHELLS][v];
        d = new_buffer(b, 0.5f);
        for (int c = 0; c < 5; c++) {
            int st = (int)(rnd01() * 0.25f * SR);
            add_bell(d + st, b->len - st, 2500 + rnd01() * 2500, CLICKS, 3, 0.4f, 0.03f);
        }
        b = &sfx[SFX_BUY][v];
        d = new_buffer(b, 1.0f);
        add_bell(d, b->len, 2100, BELL, 4, 0.6f, 0.4f);
        add_bell(d + (int)(0.12f * SR), b->len - (int)(0.12f * SR), 2800, BELL, 4, 0.6f, 0.5f);
        add_glide(d + (int)(0.25f * SR), (int)(0.2f * SR), 140, 80, 0.1f, 0.6f, 0.002f, 0.06f, 0);
        b = &sfx[SFX_QUEST][v];
        d = new_buffer(b, 1.2f);
        {
            static const float notes[3] = { 74, 79, 86 };
            for (int n = 0; n < 3; n++)
                add_bell(d + (int)(n * 0.12f * SR), b->len - (int)(n * 0.12f * SR), mtof(notes[n]), BELL, 4, 0.45f, 0.45f);
        }
        b = &sfx[SFX_QUEST_DONE][v];
        d = new_buffer(b, 2.2f);
        {
            static const float notes[6] = { 60, 64, 67, 72, 67, 72 };
            for (int n = 0; n < 6; n++) {
                int st = (int)(n * 0.14f * SR);
                add_voice(d + st, (int)(0.3f * SR), mtof(notes[n]), mtof(notes[n]), 1400, 0.05f, 0.02f, n == 5 ? 0.5f : 0.12f);
                add_bell(d + st, b->len - st, mtof(notes[n] + 12), BELL, 4, 0.25f, 0.6f);
            }
        }
        /* a great bronze bell: hum, prime, tierce, quint and nominal */
        b = &sfx[SFX_BELL][v];
        d = new_buffer(b, 5.0f);
        {
            static const float partials[6] = { 0.5f, 1.0f, 1.19f, 1.5f, 2.0f, 2.52f };
            add_bell(d, b->len, 196, partials, 6, 1.0f, 3.5f);
            for (int i = 0; i < 1200; i++)
                d[i] += rnd() * 0.3f * (1.0f - i / 1200.0f);
        }
        b = &sfx[SFX_CHIME][v];
        d = new_buffer(b, 2.0f);
        add_bell(d, b->len, 880, BELL, 4, 0.7f, 1.2f);
        b = &sfx[SFX_LEVER][v];
        d = new_buffer(b, 0.6f);
        add_creak(d, (int)(0.35f * SR), 160, 900, 0.5f, 0.1f);
        add_glide(d + (int)(0.35f * SR), (int)(0.2f * SR), 130, 70, 0.1f, 1.0f, 0.002f, 0.06f, 0);
        add_bell(d + (int)(0.35f * SR), (int)(0.2f * SR), 700, PLATE, 5, 0.3f, 0.05f);
        b = &sfx[SFX_PLATE][v];
        d = new_buffer(b, 0.6f);
        add_glide(d, b->len, 120, 70, 0.1f, 1.0f, 0.002f, 0.08f, 0);
        add_bell(d, b->len, 1320, BELL, 4, 0.3f, 0.3f);
        b = &sfx[SFX_GATE][v];
        d = new_buffer(b, 3.5f);
        {
            Svf f = {0};
            float lp = 0;
            for (int i = 0; i < (int)(3.0f * SR); i++) {
                float t = (float)i / SR;
                svf(&f, rnd(), 160 + 120 * sinf(t * 19.0f), 0.2f);
                d[i] += onepole(&lp, f.bp, 800) * 2.2f * sinf(PI_F * t / 3.0f);
            }
            add_glide(d + (int)(2.9f * SR), (int)(0.6f * SR), 70, 35, 0.3f, 1.5f, 0.002f, 0.25f, 0);
        }
        b = &sfx[SFX_PAGE][v];
        d = new_buffer(b, 0.35f);
        {
            Svf f = {0};
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                svf(&f, rnd(), 4000 + 1500 * sinf(t * 90.0f), 0.8f);
                d[i] = f.bp * sinf(PI_F * t / 0.35f) * (0.6f + 0.4f * sinf(t * 60.0f));
            }
        }
        b = &sfx[SFX_EQUIP][v];
        d = new_buffer(b, 0.6f);
        add_bell(d, b->len, 620 + r * 200, PLATE, 5, 0.6f, 0.15f);
        add_bell(d + (int)(0.08f * SR), b->len - (int)(0.08f * SR), 950 + r * 200, PLATE, 5, 0.4f, 0.1f);
        b = &sfx[SFX_DIG][v];
        d = new_buffer(b, 0.5f);
        gen_noise_hit(b, 0.5f, 1800, 0.8f, 0.02f, 0.12f, true);
        add_glide(b->data, b->len, 110, 60, 0.1f, 0.8f, 0.002f, 0.08f, 0);
        b = &sfx[SFX_DETECTOR][v];
        d = new_buffer(b, 0.1f);
        add_glide(d, b->len, 1800, 1800, 0.1f, 1.0f, 0.003f, 0.06f, 0);
        /* the ukulele: a bright little strummed ditty, C, A minor, F, G */
        b = &sfx[SFX_UKULELE][v];
        d = new_buffer(b, 2.8f);
        {
            static const float chords[4][4] = { { 67, 60, 64, 72 }, { 69, 60, 64, 69 }, { 69, 60, 65, 69 }, { 67, 62, 67, 71 } };
            for (int c = 0; c < 4; c++)
                for (int st = 0; st < 2; st++)
                    for (int k = 0; k < 4; k++) {
                        int at = (int)((c * 0.6f + st * 0.3f + k * 0.012f) * SR);
                        if (at < b->len)
                            add_ks(d + at, b->len - at, mtof(chords[(c + v) % 4][k]), st ? 0.25f : 0.35f, 0.9f);
                    }
        }
        b = &sfx[SFX_GNOME][v];
        d = new_buffer(b, 0.4f);
        add_bell(d, b->len, 1900 + r * 400, PLATE, 5, 0.6f, 0.05f);
        b = &sfx[SFX_SHOP_BELL][v];
        d = new_buffer(b, 1.0f);
        for (int c = 0; c < 4; c++)
            add_bell(d + (int)(c * 0.07f * SR), b->len - (int)(c * 0.07f * SR), 2600 + (c % 2) * 400, BELL, 4, 0.35f, 0.35f);

        /* ---- Old Ember and the weather ---- */
        b = &sfx[SFX_RUMBLE][v];
        d = new_buffer(b, 4.0f);
        add_swell(d, b->len, 40, 160, 3.0f, 0.4f);
        add_glide(d, b->len, 38 + r * 8, 32, 3.0f, 0.8f, 0.8f, 2.0f, 0.03f);
        b = &sfx[SFX_ERUPTION][v];
        d = new_buffer(b, 7.0f);
        {
            float lp1 = 0, lp2 = 0;
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                float cutoff = 90 + 4000 * expf(-t * 1.2f);
                float n = onepole(&lp2, onepole(&lp1, rnd(), cutoff), cutoff);
                d[i] = n * 4.0f * env(t, 0.01f, 2.5f) + (i < 1500 ? rnd() * (1.0f - i / 1500.0f) : 0.0f);
            }
            add_glide(d, b->len, 45, 22, 3.0f, 1.5f, 0.01f, 2.5f, 0);
        }
        b = &sfx[SFX_LAVA_BOMB][v];
        d = new_buffer(b, 1.6f);
        add_whoosh(d, (int)(0.5f * SR), 1500, 300, 0.5f, 0.6f);
        {
            float lp = 0;
            for (int i = (int)(0.5f * SR); i < b->len; i++) {
                float t = (float)i / SR - 0.5f;
                d[i] += onepole(&lp, rnd(), 900 * expf(-t * 2.0f) + 100) * 2.5f * env(t, 0.003f, 0.4f);
            }
        }
        gen_noise_hit(&sfx[SFX_HISS][v], 1.4f, 6000, 1.0f, 0.1f, 0.5f, true);
        b = &sfx[SFX_COUGH][v];
        d = new_buffer(b, 1.0f);
        for (int c = 0; c < 3; c++) {
            Svf f = {0};
            int st = (int)((c * 0.28f + rnd01() * 0.04f) * SR);
            for (int i = 0; i < (int)(0.18f * SR) && st + i < b->len; i++) {
                float t = (float)i / SR;
                svf(&f, rnd(), 700 + r * 200, 0.6f);
                d[st + i] += f.bp * env(t, 0.005f, 0.06f) * 1.5f;
            }
        }
        b = &sfx[SFX_HAILSTONE][v];
        d = new_buffer(b, 0.4f);
        for (int c = 0; c < 8; c++) {
            int st = (int)(rnd01() * 0.3f * SR);
            add_bell(d + st, b->len - st, 3000 + rnd01() * 3000, CLICKS, 3, 0.3f, 0.006f);
        }
        b = &sfx[SFX_GUST][v];
        d = new_buffer(b, 2.5f);
        add_whoosh(d, b->len, 200, 900 + r * 400, 0.6f, 1.0f);
        b = &sfx[SFX_REBIRTH][v];
        d = new_buffer(b, 4.0f);
        {
            static const float notes[5] = { 60, 67, 72, 76, 79 };
            for (int n = 0; n < 5; n++)
                add_bell(d + (int)(n * 0.3f * SR), b->len - (int)(n * 0.3f * SR), mtof(notes[n]), BELL, 4, 0.35f, 1.6f);
            for (int c = 0; c < 20; c++) {
                int st = (int)(rnd01() * 3.0f * SR);
                add_bell(d + st, b->len - st, 3000 + rnd01() * 4000, BELL, 4, 0.08f, 0.2f);
            }
        }

        /* ---- the new creatures ---- */
        b = &sfx[SFX_BAT][v];
        d = new_buffer(b, 0.3f);
        for (int c = 0; c < 3; c++)
            add_glide(d + (int)(c * 0.07f * SR), (int)(0.05f * SR), 5500 + r * 1500, 7000, 0.04f, 0.5f, 0.003f, 0.02f, 0);
        b = &sfx[SFX_IMP][v];
        d = new_buffer(b, 0.8f);
        for (int c = 0; c < 5; c++)
            add_voice(d + (int)(c * 0.11f * SR), (int)(0.09f * SR), 520 + r * 100 + c * 30, 460, 2200, 0.5f, 0.005f, 0.035f);
        for (int i = 0; i < b->len; i++)
            if (rnd01() < 0.002f)
                d[i] += rnd();
        b = &sfx[SFX_WRAITH][v];
        d = new_buffer(b, 2.2f);
        {
            float phase = 0;
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                float f = (260 + r * 60) * (1.0f + 0.25f * sinf(t * 1.4f)) * (1.0f + 0.01f * sinf(t * 30.0f));
                phase += f / SR;
                d[i] = sinf(2 * PI_F * phase) * sinf(PI_F * t / 2.2f) * (0.7f + 0.3f * sinf(t * 9.0f));
            }
        }
        b = &sfx[SFX_EEL][v];
        d = new_buffer(b, 0.6f);
        gen_noise_hit(b, 0.6f, 5000, 1.0f, 0.02f, 0.2f, true);
        add_bell(b->data + (int)(0.25f * SR), (int)(0.1f * SR), 1100, CLICKS, 3, 0.8f, 0.02f);
        b = &sfx[SFX_KING_ROAR][v];
        d = new_buffer(b, 2.5f);
        add_voice(d, b->len, 75 + r * 10, 48, 420, 0.5f, 0.1f, 1.0f);
        add_swell(d, b->len, 80, 400, 1.2f, 0.3f);
        b = &sfx[SFX_DROWNED][v];
        d = new_buffer(b, 0.7f);
        for (int c = 0; c < 4; c++)
            add_bell(d + (int)(c * 0.08f * SR), (int)(0.05f * SR), 800 + rnd01() * 900, CLICKS, 3, 0.4f, 0.015f);
        for (int c = 0; c < 4; c++) {
            int st = (int)(rnd01() * 0.5f * SR);
            add_glide(d + st, b->len - st, 200, 500, 0.05f, 0.3f, 0.002f, 0.04f, 0);
        }

        /* ---- the new spells ---- */
        b = &sfx[SFX_CAST_TIDE][v];
        d = new_buffer(b, 1.8f);
        add_swell(d, b->len, 200, 4500, 1.6f, 0.35f);
        b = &sfx[SFX_CAST_METEOR][v];
        d = new_buffer(b, 1.8f);
        add_whoosh(d, b->len, 150, 2500, 0.4f, 0.9f);
        add_glide(d, b->len, 60, 140, 1.5f, 0.6f, 0.3f, 0.8f, 0.03f);
        b = &sfx[SFX_CAST_SPIKES][v];
        d = new_buffer(b, 1.0f);
        for (int c = 0; c < 8; c++) {
            int st = (int)(c * 0.1f * SR);
            add_glide(d + st, b->len - st, 140, 60, 0.08f, 0.7f, 0.002f, 0.06f, 0);
            for (int i = 0; i < 600 && st + i < b->len; i++)
                d[st + i] += rnd() * 0.5f * (1.0f - i / 600.0f);
        }
        b = &sfx[SFX_CAST_SHADOW][v];
        d = new_buffer(b, 1.4f);
        add_whoosh(d, b->len, 2500, 150, 0.5f, 1.0f);
        add_glide(d, b->len, 220, 110, 1.2f, 0.4f, 0.3f, 0.6f, 0.02f);
        b = &sfx[SFX_CAST_GALE][v];
        d = new_buffer(b, 2.0f);
        {
            Svf f = {0};
            for (int i = 0; i < b->len; i++) {
                float t = (float)i / SR;
                svf(&f, rnd(), 500 + 900 * (0.5f + 0.5f * sinf(t * 11.0f)), 0.35f);
                d[i] = f.bp * sinf(PI_F * t / 2.0f) * 1.4f;
            }
        }
        b = &sfx[SFX_CAST_STARS][v];
        d = new_buffer(b, 1.2f);
        {
            static const float notes[5] = { 84, 88, 91, 95, 96 };
            for (int n = 0; n < 5; n++)
                add_bell(d + (int)(n * 0.06f * SR), b->len - (int)(n * 0.06f * SR), mtof(notes[n]), BELL, 4, 0.3f, 0.3f);
        }
        b = &sfx[SFX_CAST_GILLS][v];
        d = new_buffer(b, 1.5f);
        for (int c = 0; c < 14; c++) {
            int st = (int)(rnd01() * 1.2f * SR);
            add_glide(d + st, b->len - st, 300 + rnd01() * 300, 1200, 0.06f, 0.3f, 0.003f, 0.05f, 0);
        }
        add_bell(d, b->len, mtof(84), BELL, 4, 0.3f, 0.9f);

        for (int s = 0; s < SFX_COUNT; s++)
            if (sfx[s][v].data)
                normalize(&sfx[s][v]);
    }
}

/* ---------- music ---------- */

typedef struct {
    float buf[2400];        /* one period of the string */
    int len, idx;
    float rho;              /* energy kept per period: how long it rings */
    float gain, pan;
    bool on;
} Pluck;                    /* Karplus-Strong plucked string: harp, piano */

typedef struct {
    float phase[3];
    float freq, amp, target;
    Svf f1, f2;             /* choir formants */
} PadVoice;

typedef struct {
    float freq, target_freq, phase, amp, target, vib_phase, lp;
} Flute;

typedef struct {
    float freq, phase, mod_phase, t, gain, pan;
    float ratio, decay;     /* 0 = the mood's usual bell */
    bool on;
} Bell;                     /* FM bell: bells, music box, kalimba, steel drum */

/* a sustained, one-note-at-a-time lead with its own timbre: fiddle, accordion, whistle,
 * glass harmonica, brass */
typedef struct {
    float freq, target_freq, phase, phase2, amp, target, vib_phase, lp, flick;
    Svf f1, f2;
} Mono;

/* a short chord stab (the accordion's "pah") */
typedef struct {
    float freq, phase, env;
    Svf f;
} Stab;

/* one drum of the kit */
typedef struct {
    float env, phase, freq, vel;
    Svf f;
} Drum;

/* Freeverb-style reverb: parallel combs, then series allpasses, per channel */
#define COMBS 8
#define ALLPASSES 4
typedef struct {
    float *buf;
    int len, idx;
    float store;
} Delay;

typedef struct {
    Delay comb[2][COMBS], allpass[2][ALLPASSES];
} Reverb;

static const int COMB_LEN[COMBS] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
static const int ALLPASS_LEN[ALLPASSES] = { 556, 441, 341, 225 };

static void delay_init(Delay *d, int len)
{
    d->len = len * SR / 44100;
    d->buf = calloc(d->len, sizeof *d->buf);
}

static float reverb_channel(Reverb *r, int ch, float in, float feedback)
{
    const float damp = 0.25f;
    float out = 0;
    for (int i = 0; i < COMBS; i++) {
        Delay *d = &r->comb[ch][i];
        float y = d->buf[d->idx];
        d->store = y * (1 - damp) + d->store * damp;
        d->buf[d->idx] = in + d->store * feedback;
        d->idx = (d->idx + 1) % d->len;
        out += y;
    }
    for (int i = 0; i < ALLPASSES; i++) {
        Delay *d = &r->allpass[ch][i];
        float y = d->buf[d->idx];
        d->buf[d->idx] = out + y * 0.5f;
        d->idx = (d->idx + 1) % d->len;
        out = y - out;
    }
    return out;
}

typedef enum {
    LEAD_HARP, LEAD_PIANO, LEAD_FLUTE, LEAD_BELLS, LEAD_MUSICBOX,
    LEAD_GUITAR, LEAD_FIDDLE, LEAD_ACCORDION, LEAD_WHISTLE, LEAD_GLASS, LEAD_KALIMBA, LEAD_BRASS, LEAD_STEEL,
} Lead;

/* the drum patterns of each genre */
typedef enum { RHY_NONE, RHY_BOSSA, RHY_JIG, RHY_SHANTY, RHY_TAIKO, RHY_LOFI, RHY_REEL, RHY_FESTIVAL, RHY_HEARTBEAT } Rhythm;
/* what the accompaniment does */
typedef enum { COMP_NONE, COMP_STRUM, COMP_OOMPAH, COMP_ARP, COMP_WALK } Comp;

typedef struct {
    int tonic;              /* MIDI note of the scale's tonic */
    int scale[7];
    int chords[4][5];       /* four chords, up to five notes (0 = unused) */
    float bpm;
    float note_chance;      /* how busy the lead is */
    int bar_steps;          /* eighth notes per chord */
    Lead lead;
    int lead_octave;        /* shifts the melody up or down */
    float pad, bass, choir, drone;  /* how much of each layer */
    float reverb;           /* comb feedback: bigger = longer halls */
    /* the coast's moods add these (the ruins' moods leave them zero) */
    Rhythm rhythm;
    float drums;            /* how loud the kit is */
    Comp comp;
    int chords_b[4][5];     /* a second section, played every other time round (0 = none) */
    bool whales;            /* whale song drifting in the distance */
} MoodDef;

static const MoodDef MOODS[MOOD_COUNT] = {
    [MOOD_DAY] = { 62, { 0, 2, 3, 5, 7, 9, 10 },            /* D dorian: bright but wistful */
        { { 50, 53, 57, 60, 64 }, { 46, 50, 53, 57, 0 }, { 53, 57, 60, 64, 0 }, { 48, 52, 55, 62, 0 } },
        68, 0.38f, 16, LEAD_HARP, 0, 1.0f, 1.0f, 0.0f, 0.0f, 0.84f },
    [MOOD_RAIN] = { 64, { 0, 2, 3, 5, 7, 8, 10 },           /* E minor: a piano by a rainy window */
        { { 52, 55, 59, 62, 66 }, { 48, 52, 55, 59, 0 }, { 43, 47, 50, 54, 0 }, { 50, 54, 57, 61, 0 } },
        58, 0.3f, 16, LEAD_PIANO, 0, 0.8f, 0.6f, 0.0f, 0.0f, 0.86f },
    [MOOD_NIGHT] = { 57, { 0, 2, 3, 5, 7, 8, 10 },          /* A aeolian: hushed */
        { { 45, 48, 52, 55, 59 }, { 41, 45, 48, 52, 0 }, { 50, 53, 57, 60, 0 }, { 52, 55, 59, 62, 0 } },
        52, 0.22f, 16, LEAD_HARP, -12, 1.0f, 0.8f, 0.0f, 0.0f, 0.86f },
    [MOOD_CRYPT] = { 61, { 0, 1, 3, 5, 7, 8, 10 },          /* C# phrygian: stone and candle smoke */
        { { 49, 52, 56, 0, 0 }, { 50, 54, 57, 0, 0 }, { 49, 52, 56, 61, 0 }, { 47, 50, 54, 0, 0 } },
        44, 0.16f, 16, LEAD_BELLS, 0, 0.3f, 0.5f, 1.0f, 1.0f, 0.9f },
    [MOOD_PALACE] = { 67, { 0, 2, 4, 6, 7, 9, 11 },         /* G lydian: a stately court in 3/4 */
        { { 55, 59, 62, 66, 0 }, { 57, 61, 64, 67, 0 }, { 52, 55, 59, 62, 0 }, { 60, 64, 67, 71, 0 } },
        84, 0.45f, 12, LEAD_FLUTE, 0, 0.9f, 1.0f, 0.2f, 0.0f, 0.86f },
    [MOOD_GARDEN] = { 65, { 0, 2, 4, 7, 9, 12, 14 },        /* F major pentatonic: birdsong weather */
        { { 53, 57, 60, 64, 0 }, { 50, 53, 57, 60, 0 }, { 46, 50, 53, 57, 0 }, { 48, 52, 55, 58, 0 } },
        76, 0.4f, 16, LEAD_FLUTE, 0, 0.6f, 0.8f, 0.0f, 0.0f, 0.82f },
    [MOOD_LIBRARY] = { 57, { 0, 2, 3, 5, 7, 9, 10 },        /* A dorian music box: dust and old pages */
        { { 57, 60, 64, 67, 0 }, { 55, 59, 62, 0, 0 }, { 53, 57, 60, 64, 0 }, { 52, 55, 59, 62, 0 } },
        60, 0.42f, 16, LEAD_MUSICBOX, 12, 0.6f, 0.3f, 0.0f, 0.0f, 0.84f },

    /* ---- the coast ---- */
    [MOOD_BEACH] = { 65, { 0, 2, 4, 5, 7, 9, 11 },          /* bossa nova in F: nylon strings and a brushed shaker */
        { { 53, 57, 60, 64, 0 }, { 55, 58, 62, 65, 0 }, { 57, 60, 64, 67, 0 }, { 48, 52, 55, 58, 62 } },
        88, 0.45f, 16, LEAD_GUITAR, 0, 0.35f, 1.0f, 0.0f, 0.0f, 0.8f,
        RHY_BOSSA, 0.8f, COMP_STRUM,
        { { 50, 53, 57, 60, 64 }, { 55, 59, 62, 65, 0 }, { 52, 55, 59, 62, 0 }, { 57, 61, 64, 67, 0 } } },
    [MOOD_VILLAGE] = { 62, { 0, 2, 4, 5, 7, 9, 10 },        /* a D mixolydian jig: fiddle over a drone */
        { { 50, 54, 57, 62, 0 }, { 48, 52, 55, 60, 0 }, { 55, 59, 62, 0, 0 }, { 50, 54, 57, 62, 0 } },
        126, 0.75f, 12, LEAD_FIDDLE, 0, 0.2f, 0.6f, 0.0f, 0.55f, 0.8f,
        RHY_JIG, 0.9f, COMP_NONE,
        { { 55, 59, 62, 0, 0 }, { 48, 52, 55, 0, 0 }, { 57, 61, 64, 0, 0 }, { 50, 54, 57, 62, 0 } } },
    [MOOD_MARINA] = { 57, { 0, 2, 3, 5, 7, 8, 10 },         /* a sea shanty: accordion oom-pah and a tin whistle */
        { { 45, 48, 52, 57, 0 }, { 43, 47, 50, 55, 0 }, { 41, 45, 48, 53, 0 }, { 40, 44, 47, 52, 0 } },
        100, 0.55f, 12, LEAD_WHISTLE, 12, 0.0f, 1.0f, 0.0f, 0.0f, 0.8f,
        RHY_SHANTY, 0.8f, COMP_OOMPAH,
        { { 41, 45, 48, 53, 0 }, { 48, 52, 55, 60, 0 }, { 43, 47, 50, 55, 0 }, { 45, 48, 52, 57, 0 } } },
    [MOOD_OCEAN] = { 64, { 0, 2, 4, 6, 7, 9, 11 },          /* out on the open water: a slow lydian harp waltz */
        { { 52, 56, 59, 63, 0 }, { 54, 57, 61, 64, 0 }, { 56, 59, 63, 66, 0 }, { 57, 61, 64, 68, 0 } },
        70, 0.3f, 12, LEAD_HARP, 0, 1.0f, 0.7f, 0.35f, 0.0f, 0.88f,
        RHY_NONE, 0.0f, COMP_ARP, { { 0 } }, true },
    [MOOD_UNDERSEA] = { 61, { 0, 2, 4, 6, 7, 9, 11 },       /* the Drowned Court: glass bells, choir and whale song */
        { { 49, 53, 56, 60, 63 }, { 51, 54, 58, 61, 0 }, { 54, 58, 61, 65, 0 }, { 47, 51, 54, 58, 61 } },
        46, 0.28f, 16, LEAD_GLASS, 12, 0.8f, 0.5f, 1.0f, 0.35f, 0.93f,
        RHY_NONE, 0.0f, COMP_NONE, { { 0 } }, true },
    [MOOD_VOLCANO] = { 52, { 0, 1, 4, 5, 7, 8, 10 },        /* E phrygian dominant: taiko and low brass */
        { { 40, 44, 47, 0, 0 }, { 41, 45, 48, 0, 0 }, { 40, 44, 47, 52, 0 }, { 38, 41, 45, 0, 0 } },
        72, 0.3f, 16, LEAD_BRASS, -12, 0.4f, 1.0f, 0.3f, 1.0f, 0.86f,
        RHY_TAIKO, 1.0f, COMP_NONE },
    [MOOD_TAVERN] = { 67, { 0, 2, 4, 5, 7, 9, 11 },         /* the Salted Eel: a quick reel to dance to */
        { { 55, 59, 62, 67, 0 }, { 48, 52, 55, 60, 0 }, { 50, 54, 57, 62, 0 }, { 55, 59, 62, 0, 0 } },
        144, 0.85f, 16, LEAD_FIDDLE, 0, 0.0f, 0.9f, 0.0f, 0.0f, 0.78f,
        RHY_REEL, 0.8f, COMP_OOMPAH,
        { { 52, 55, 59, 64, 0 }, { 48, 52, 55, 60, 0 }, { 50, 54, 57, 62, 0 }, { 55, 59, 62, 67, 0 } } },
    [MOOD_SHOP] = { 62, { 0, 2, 3, 5, 7, 9, 10 },           /* the Curious Clam: lazy lo-fi with a kalimba */
        { { 50, 53, 57, 60, 64 }, { 55, 59, 62, 65, 69 }, { 48, 52, 55, 59, 62 }, { 57, 61, 64, 67, 0 } },
        76, 0.4f, 16, LEAD_KALIMBA, 12, 0.7f, 1.0f, 0.0f, 0.0f, 0.8f,
        RHY_LOFI, 0.75f, COMP_NONE },
    [MOOD_ANCIENT] = { 60, { 0, 2, 4, 6, 8, 10, 12 },       /* whole tones: something very old and strange */
        { { 48, 52, 56, 0, 0 }, { 50, 54, 58, 0, 0 }, { 46, 50, 54, 0, 0 }, { 52, 56, 60, 0, 0 } },
        40, 0.25f, 16, LEAD_GLASS, 0, 0.5f, 0.4f, 0.6f, 0.8f, 0.94f,
        RHY_HEARTBEAT, 0.5f, COMP_NONE },
    [MOOD_ERUPTION] = { 52, { 0, 1, 4, 5, 7, 8, 10 },       /* Old Ember wakes */
        { { 40, 44, 47, 0, 0 }, { 41, 45, 48, 0, 0 }, { 40, 43, 47, 0, 0 }, { 39, 43, 46, 0, 0 } },
        132, 0.45f, 8, LEAD_BRASS, -12, 0.4f, 1.0f, 0.6f, 1.0f, 0.84f,
        RHY_TAIKO, 1.2f, COMP_NONE },
    [MOOD_FESTIVAL] = { 60, { 0, 2, 4, 5, 7, 9, 11 },       /* the bell is home: a street band */
        { { 48, 52, 55, 60, 0 }, { 53, 57, 60, 65, 0 }, { 55, 59, 62, 67, 0 }, { 48, 52, 55, 60, 0 } },
        132, 0.7f, 16, LEAD_STEEL, 12, 0.3f, 1.0f, 0.3f, 0.0f, 0.8f,
        RHY_FESTIVAL, 0.85f, COMP_OOMPAH,
        { { 57, 60, 64, 0, 0 }, { 53, 57, 60, 0, 0 }, { 55, 59, 62, 0, 0 }, { 48, 52, 55, 60, 0 } } },
};

/* drum patterns, one bit per eighth-note step: kick, snare, hats/shaker, low drum
 * (bodhran/taiko), click (rim/clave), jingles */
typedef struct { unsigned kick, snare, hat, low, click, jingle; int steps; } Pattern;
static const Pattern PATTERNS[] = {
    [RHY_NONE] = { 0, 0, 0, 0, 0, 0, 16 },
    [RHY_BOSSA] = { 0x0909, 0, 0xFFFF, 0, 0x1449, 0, 16 },
    [RHY_JIG] = { 0, 0, 0, 0x0FFF, 0, 0, 12 },
    [RHY_SHANTY] = { 0x0041, 0x0514, 0, 0, 0, 0, 12 },
    [RHY_TAIKO] = { 0, 0, 0, 0x2521, 0x4044, 0, 16 },
    [RHY_LOFI] = { 0x0481, 0x1010, 0x5555, 0, 0, 0, 16 },
    [RHY_REEL] = { 0x0101, 0x1010, 0, 0xFFFF, 0, 0, 16 },
    [RHY_FESTIVAL] = { 0x1111, 0x1010, 0, 0, 0, 0xFFFF, 16 },
    [RHY_HEARTBEAT] = { 0x0003, 0, 0, 0, 0, 0, 16 },
};

typedef struct {
    const float *data;
    int len;
    float pos, rate;
    float gl, gr;
    bool on;
    bool speech;            /* a character talking: never stolen */
} Voice;

static struct {
    SDL_AudioStream *stream;
    bool ok;
    bool music_on;
    float music_volume, sfx_volume;

    AudioScene scene;
    vec3 listener;
    float listener_yaw;

    Voice voices[MAX_VOICES];
    Buffer clips[MAX_CLIPS];
    int clip_count;

    /* music */
    Mood mood;
    float mood_fade;        /* 1 = full volume; dips to 0 while switching */
    Pluck plucks[16];
    int next_pluck;
    PadVoice pad[5];
    Flute flute;
    Mono mono;
    Stab stabs[4];
    int next_stab;
    Drum kick, snare, hat, low, click, jingle;
    float crackle_lp;
    int section;
    float whale_t, whale_phase, whale_env, whale_f0, whale_f1, whale_len, whale_pos;
    Bell bells[10];
    int next_bell;
    float bass_phase, bass_amp, bass_freq;
    float drone_phase, drone_lp;
    double step_clock;
    int step, chord, last_degree, arp;
    float pad_lp;
    float music_gain;

    float danger, drum_phase, drum_env, drum_freq;

    /* boombox groove */
    float groove;
    double groove_clock;
    int groove_step;
    float gbass_phase, gbass_env, gbass_freq;
    float kick_env, kick_phase, snare_env, hat_env;
    Svf gbass_f, hat_f, snare_f;

    /* ambience */
    float wind_lp[2][2], wind_t;
    float bird_t, bird_phase, bird_f0, bird_f1, bird_len, bird_pos, bird_pan;
    int bird_chirps;
    float cricket_t;
    float fire_pop, fire_lp;
    float rain_lp[2], rain_hp[2], rain_drop, rain_drop_lp;
    float drip_t, drip_phase, drip_freq, drip_env, drip_pan;
    Svf water_f;
    float surf_t, surf_env, surf_len, surf_lp[2][2], surf_hiss;
    Svf howl_f;
    float hail_env;
    float tor_lp[2];
    float rumble_lp[2];
    float lava_t, lava_phase, lava_env, lava_freq;
    float harb_t, harb_env, harb_freq, harb_phase;
    float chime_t, chime_phase[3], chime_env[3], chime_freq[3];
    int chime_next;
    float bub_t, bub_phase, bub_env, bub_freq;
    float sub;                  /* smoothed: 1 = head under water */
    float night_mix, rain_mix, shelter, slow;
    float master_lp[2];

    Reverb reverb;
} A;

static void pluck(float midi, float velocity, float pan, bool bright)
{
    Pluck *p = &A.plucks[A.next_pluck];
    A.next_pluck = (A.next_pluck + 1) % 16;
    float f = mtof(midi);
    p->len = (int)(SR / f);
    if (p->len > 2400) p->len = 2400;
    if (p->len < 2) p->len = 2;
    /* the initial noise burst is low-passed: darker when played gently */
    float lp = 0;
    for (int i = 0; i < p->len; i++) {
        lp += ((bright ? 0.6f : 0.3f) + 0.4f * velocity) * (rnd() - lp);
        p->buf[i] = lp;
    }
    p->idx = 0;
    p->rho = powf(0.001f, 1.0f / ((bright ? 2.2f : 3.5f) * f));
    p->gain = velocity * 0.5f;
    p->pan = pan;
    p->on = true;
}

static void ring(float midi, float velocity, float pan)
{
    Bell *b = &A.bells[A.next_bell];
    A.next_bell = (A.next_bell + 1) % 10;
    *b = (Bell){ .freq = mtof(midi), .gain = velocity * 0.22f, .pan = pan, .on = true };
}

/* a bell of a particular kind: kalimba tines, steel drums */
static void ring_ex(float midi, float velocity, float pan, float ratio, float decay)
{
    Bell *b = &A.bells[A.next_bell];
    A.next_bell = (A.next_bell + 1) % 10;
    *b = (Bell){ .freq = mtof(midi), .gain = velocity * 0.22f, .pan = pan, .ratio = ratio, .decay = decay, .on = true };
}

static void mono_note(float note, float vel)
{
    Mono *m = &A.mono;
    m->target_freq = mtof(note);
    if (m->amp < 0.05f)
        m->freq = m->target_freq;
    m->target = 0.6f + vel * 0.4f;
    m->flick = 1.0f;        /* a little grace note up into it */
}

static void stab(float midi, float vel)
{
    Stab *s = &A.stabs[A.next_stab];
    A.next_stab = (A.next_stab + 1) % 4;
    s->freq = mtof(midi);
    s->env = vel;
}

static void play_lead(const MoodDef *m, float note, float vel, float pan)
{
    switch (m->lead) {
    case LEAD_HARP: pluck(note, vel, pan, false); break;
    case LEAD_PIANO:
        pluck(note, vel, pan, true);
        pluck(note + 0.08f, vel * 0.6f, -pan, true);     /* two slightly detuned strings */
        break;
    case LEAD_FLUTE:
        A.flute.target_freq = mtof(note);
        if (A.flute.amp < 0.05f)
            A.flute.freq = A.flute.target_freq;
        A.flute.target = 0.6f + vel * 0.4f;
        break;
    case LEAD_BELLS: ring(note, vel, pan); break;
    case LEAD_MUSICBOX: ring(note + 12, vel, pan); break;
    case LEAD_GUITAR:
        pluck(note, vel * 0.9f, pan, false);
        break;
    case LEAD_KALIMBA: ring_ex(note, vel * 1.1f, pan, 5.4f, 0.7f); break;
    case LEAD_STEEL: ring_ex(note, vel * 1.1f, pan, 1.5f, 0.9f); break;
    case LEAD_FIDDLE: case LEAD_ACCORDION: case LEAD_WHISTLE: case LEAD_GLASS: case LEAD_BRASS:
        mono_note(note, vel);
        break;
    }
}

static void hit(Drum *d, float vel, float freq)
{
    d->env = vel;
    d->phase = 0.0f;
    d->freq = freq;
    d->vel = vel;
}

/* the kit and the accompaniment, each eighth note */
static void comp_step(const MoodDef *m, int beat, const int *chord)
{
    const Pattern *pt = &PATTERNS[m->rhythm];
    if (m->rhythm != RHY_NONE) {
        int k = beat % pt->steps;
        unsigned bit = 1u << k;
        float accent = k == 0 ? 1.0f : 0.75f;
        if (pt->kick & bit) hit(&A.kick, accent, 0);
        if (pt->snare & bit) hit(&A.snare, accent * 0.8f, 0);
        if (pt->hat & bit) hit(&A.hat, (k % 2 ? 0.45f : 0.8f), 0);
        if (pt->low & bit) hit(&A.low, (k % 3 == 0 || k == 0) ? 1.0f : 0.45f, m->rhythm == RHY_TAIKO ? 58.0f : 92.0f);
        if (pt->click & bit) hit(&A.click, 0.8f, 0);
        if (pt->jingle & bit) hit(&A.jingle, k % 2 ? 0.4f : 0.7f, 0);
    }
    switch (m->comp) {
    case COMP_STRUM: {
        /* bossa guitar: chord tones on the syncopated beats */
        static const unsigned where = 0x4949;
        if (where & (1u << (beat % 16)))
            for (int i = 1; i < 5; i++)
                if (chord[i])
                    pluck((float)chord[i], 0.22f + 0.05f * i, -0.2f + i * 0.1f, false);
        if (beat % 4 == 0) {
            A.bass_freq = mtof(chord[beat % 8 == 0 ? 0 : (chord[2] ? 2 : 0)] - 12);
            A.bass_amp = 0.9f;
        }
        break;
    }
    case COMP_OOMPAH: {
        /* bass on the strong beat, accordion chord on the weak ones */
        int bar = m->bar_steps == 12 ? 6 : 8;
        int k = beat % bar;
        if (k == 0) {
            A.bass_freq = mtof(chord[(beat / bar) % 2 ? (chord[2] ? 2 : 0) : 0] - 12);
            A.bass_amp = 1.0f;
        } else if (k % 2 == 0) {
            for (int i = 1; i < 4; i++)
                if (chord[i])
                    stab((float)chord[i], 0.8f);
        }
        break;
    }
    case COMP_ARP: {
        int i = beat % 4;
        int n = chord[i] ? chord[i] : chord[0];
        pluck((float)(n + 12), 0.3f, -0.4f + i * 0.25f, false);
        break;
    }
    default:
        break;
    }
}

static void music_step(void)
{
    const MoodDef *m = &MOODS[A.mood];
    int beat = A.step % m->bar_steps;

    if (beat == 0) {
        /* new chord: re-voice the pad, drop in the bass, sometimes arpeggiate */
        A.chord = (A.chord + 1) % 4;
        if (A.chord == 0 && m->chords_b[0][0])
            A.section ^= 1;         /* the coast's tunes have a second part */
        const int (*chords)[5] = A.section && m->chords_b[0][0] ? m->chords_b : m->chords;
        for (int i = 0; i < 5; i++) {
            int n = chords[A.chord][i];
            A.pad[i].target = n ? 1.0f : 0.0f;
            if (n)
                A.pad[i].freq = mtof(n);
        }
        A.bass_freq = mtof(chords[A.chord][0] - 12);
        A.bass_amp = 1.0f;
        if (rnd01() < 0.5f && m->lead != LEAD_FLUTE && m->comp == COMP_NONE && m->lead < LEAD_FIDDLE)
            A.arp = 4;
    }
    {
        const int (*chords)[5] = A.section && m->chords_b[0][0] ? m->chords_b : m->chords;
        comp_step(m, A.step, chords[A.chord]);
    }
    /* palace waltz: the bass again on the second downbeat */
    if (m->bar_steps == 12 && beat == 6)
        A.bass_amp = 0.7f;

    if (A.flute.target > 0.0f && rnd01() < 0.35f)
        A.flute.target = 0.0f;      /* the flute breathes between phrases */
    if (A.mono.target > 0.0f && rnd01() < (m->lead == LEAD_GLASS ? 0.15f : 0.25f))
        A.mono.target = 0.0f;

    if (A.arp > 0) {
        int n = m->chords[A.chord][(4 - A.arp) % 4];
        if (n)
            play_lead(m, (float)(n + 12 + m->lead_octave), 0.45f, -0.3f + 0.2f * (4 - A.arp));
        A.arp--;
        return;
    }

    /* melody: wander step-wise through the scale, landing on chord tones on the beat */
    bool strong = beat % 4 == 0;
    if (rnd01() < m->note_chance * (strong ? 1.4f : 0.8f)) {
        int degree = A.last_degree + (int)floorf(rnd() * 2.6f);
        if (degree < 0) degree = 1;
        if (degree > 11) degree = 9;
        int note = m->tonic + 12 * (degree / 7) + m->scale[degree % 7];
        if (strong) {
            int best = note, bd = 99;
            const int (*chords)[5] = A.section && m->chords_b[0][0] ? m->chords_b : m->chords;
            for (int i = 0; i < 5; i++) {
                int c = chords[A.chord][i];
                if (!c)
                    continue;
                for (int o = 12; o <= 24; o += 12) {
                    int d = abs(c + o - note);
                    if (d < bd) { bd = d; best = c + o; }
                }
            }
            note = best;
        }
        A.last_degree = degree;
        play_lead(m, (float)(note + m->lead_octave), 0.35f + rnd01() * 0.4f, rnd() * 0.5f);
        if (m->lead == LEAD_HARP && rnd01() < 0.15f)
            pluck((float)(note - 12 + m->lead_octave), 0.25f, rnd() * 0.3f, false);
    }
}

static void music_sample(float *l, float *r, float rate)
{
    const MoodDef *m = &MOODS[A.mood];
    double eighth = SR * 60.0 / m->bpm / 2.0;
    A.step_clock += rate;
    if (A.step_clock >= eighth) {
        A.step_clock -= eighth;
        A.step++;
        music_step();
    }

    float out_l = 0, out_r = 0;

    for (int i = 0; i < 16; i++) {
        Pluck *p = &A.plucks[i];
        if (!p->on)
            continue;
        int j = p->idx, k = (p->idx + 1) % p->len;
        float v = p->buf[j];
        p->buf[j] = p->rho * 0.5f * (p->buf[j] + p->buf[k]);
        p->idx = k;
        v *= p->gain;
        out_l += v * (0.5f - 0.5f * p->pan);
        out_r += v * (0.5f + 0.5f * p->pan);
    }

    /* bells: FM with an inharmonic ratio for a bright metallic ring */
    float bell_decay = m->lead == LEAD_MUSICBOX ? 0.6f : 2.2f;
    for (int i = 0; i < 10; i++) {
        Bell *b = &A.bells[i];
        if (!b->on)
            continue;
        b->t += 1.0f / SR;
        float e = expf(-b->t / (b->decay > 0.0f ? b->decay : bell_decay));
        if (e < 0.001f) {
            b->on = false;
            continue;
        }
        b->mod_phase += b->freq * (b->ratio > 0.0f ? b->ratio : 3.5f) * rate / SR;
        b->phase += b->freq * rate / SR;
        float s = sinf(2 * PI_F * b->phase + 2.0f * e * sinf(2 * PI_F * b->mod_phase)) * e * b->gain;
        out_l += s * (0.5f - 0.5f * b->pan);
        out_r += s * (0.5f + 0.5f * b->pan);
    }

    /* flute: a sine with breath and a slow vibrato, gliding between notes */
    Flute *fl = &A.flute;
    fl->amp += (fl->target - fl->amp) * (1.0f / (SR * 0.08f));
    if (fl->amp > 1e-4f) {
        fl->freq += (fl->target_freq - fl->freq) * (1.0f / (SR * 0.04f));
        fl->vib_phase += 5.2f / SR;
        float vib = 1.0f + 0.006f * sinf(2 * PI_F * fl->vib_phase) * fminf(fl->amp * 2, 1);
        fl->phase += fl->freq * vib * rate / SR;
        fl->phase -= floorf(fl->phase);
        float breath = onepole(&fl->lp, rnd(), 2500) * 0.25f;
        float s = (sinf(2 * PI_F * fl->phase) + 0.12f * sinf(4 * PI_F * fl->phase) + breath) * fl->amp * 0.12f;
        out_l += s * 0.55f;
        out_r += s * 0.45f;
    }

    /* the sustained leads: fiddle, accordion, whistle, glass harmonica, brass */
    Mono *mo = &A.mono;
    float attack = m->lead == LEAD_GLASS ? 0.3f : m->lead == LEAD_BRASS ? 0.06f : 0.025f;
    mo->amp += (mo->target - mo->amp) * (1.0f / (SR * (mo->target > mo->amp ? attack : 0.09f)));
    if (mo->amp > 1e-4f && m->lead >= LEAD_FIDDLE && m->lead <= LEAD_BRASS) {
        mo->freq += (mo->target_freq - mo->freq) * (1.0f / (SR * 0.03f));
        mo->flick *= 1.0f - 1.0f / (SR * 0.05f);
        mo->vib_phase += (m->lead == LEAD_ACCORDION ? 6.0f : 5.3f) / SR;
        float vib = 1.0f + (m->lead == LEAD_FIDDLE ? 0.012f : m->lead == LEAD_GLASS ? 0.004f : 0.006f) * sinf(2 * PI_F * mo->vib_phase);
        float f = mo->freq * vib * (1.0f + (m->lead == LEAD_WHISTLE ? 0.06f : 0.0f) * mo->flick);
        mo->phase += f * rate / SR;
        mo->phase -= floorf(mo->phase);
        mo->phase2 += f * 1.004f * rate / SR;
        mo->phase2 -= floorf(mo->phase2);
        float sv = 0.0f;
        switch (m->lead) {
        case LEAD_FIDDLE: {
            float saw = 2 * mo->phase - 1;
            svf(&mo->f1, saw, 900, 0.3f);
            svf(&mo->f2, saw, 2700, 0.4f);
            sv = (mo->f1.bp * 1.2f + mo->f2.bp * 0.6f) * 0.9f;
            break;
        }
        case LEAD_ACCORDION: {
            float p1 = mo->phase < 0.3f ? 1.0f : -0.4f, p2 = mo->phase2 < 0.3f ? 1.0f : -0.4f;
            sv = onepole(&mo->lp, (p1 + p2) * 0.5f, 2600) * (0.8f + 0.2f * sinf(2 * PI_F * mo->vib_phase));
            break;
        }
        case LEAD_WHISTLE:
            svf(&mo->f1, rnd(), f, 0.05f);
            sv = sinf(2 * PI_F * mo->phase) + 0.08f * sinf(4 * PI_F * mo->phase) + mo->f1.bp * 0.25f;
            break;
        case LEAD_GLASS:
            sv = (sinf(2 * PI_F * mo->phase) + 0.3f * sinf(4 * PI_F * mo->phase2)) * (0.8f + 0.2f * sinf(A.wind_t * 25.0f));
            break;
        case LEAD_BRASS: {
            float saw = (2 * mo->phase - 1) + (2 * mo->phase2 - 1);
            sv = onepole(&mo->lp, saw * 0.5f, 250 + 1800 * mo->amp);
            break;
        }
        default:
            break;
        }
        float s2 = sv * mo->amp * 0.1f;
        out_l += s2 * 0.5f;
        out_r += s2 * 0.5f;
    }
    /* accordion stabs */
    for (int i = 0; i < 4; i++) {
        Stab *st = &A.stabs[i];
        if (st->env < 1e-4f)
            continue;
        st->env *= 1.0f - 1.0f / (SR * 0.22f);
        st->phase += st->freq * rate / SR;
        st->phase -= floorf(st->phase);
        svf(&st->f, (st->phase < 0.35f ? 1.0f : -0.5f), 2200, 0.5f);
        float v = st->f.lp * st->env * 0.05f;
        out_l += v * 0.45f;
        out_r += v * 0.55f;
    }

    /* the drum kit */
    if (m->rhythm != RHY_NONE) {
        float dk = 0.0f;
        Drum *d = &A.kick;
        d->env *= 1.0f - 1.0f / (SR * 0.14f);
        d->phase += (48 + 110 * d->env * d->env) / SR;
        dk += sinf(2 * PI_F * d->phase) * d->env * 0.5f;
        d = &A.snare;
        d->env *= 1.0f - 1.0f / (SR * (m->rhythm == RHY_LOFI ? 0.12f : 0.08f));
        svf(&d->f, rnd(), m->rhythm == RHY_LOFI ? 1800 : 2600, 0.6f);
        dk += (d->f.bp * 0.7f + sinf(2 * PI_F * 190 * d->env) * 0.15f) * d->env * 0.45f;
        d = &A.hat;
        d->env *= 1.0f - 1.0f / (SR * (m->rhythm == RHY_BOSSA ? 0.045f : 0.022f));
        svf(&d->f, rnd(), 7500, 0.5f);
        dk += (rnd() - d->f.lp) * d->env * 0.06f;
        d = &A.low;
        d->env *= 1.0f - 1.0f / (SR * (m->rhythm == RHY_TAIKO ? 0.45f : 0.16f));
        d->phase += d->freq * (0.75f + 0.4f * d->env) / SR;
        svf(&d->f, rnd(), 300, 0.5f);
        dk += (sinf(2 * PI_F * d->phase) + d->f.lp * 0.4f * d->env) * d->env * (m->rhythm == RHY_TAIKO ? 0.7f : 0.4f);
        d = &A.click;
        d->env *= 1.0f - 1.0f / (SR * 0.02f);
        dk += sinf(2 * PI_F * 1900 * (float)(A.step_clock / SR)) * d->env * 0.12f;
        d = &A.jingle;
        d->env *= 1.0f - 1.0f / (SR * 0.06f);
        svf(&d->f, rnd(), 8000, 0.2f);
        dk += d->f.bp * d->env * 0.1f;
        if (m->rhythm == RHY_LOFI)      /* vinyl crackle */
            dk += (rnd01() < 0.0015f ? rnd() * 0.15f : 0.0f) + onepole(&A.crackle_lp, rnd(), 900) * 0.008f;
        dk *= m->drums;
        out_l += dk;
        out_r += dk;
    }

    /* whale song, far away */
    if (m->whales) {
        A.whale_t -= 1.0f / SR;
        if (A.whale_t <= 0.0f && A.whale_env <= 1e-3f) {
            A.whale_t = 9.0f + rnd01() * 14.0f;
            A.whale_f0 = 140 + rnd01() * 120;
            A.whale_f1 = A.whale_f0 * (0.6f + rnd01() * 1.4f);
            A.whale_len = 2.5f + rnd01() * 2.5f;
            A.whale_pos = 0.0f;
            A.whale_env = 1.0f;
        }
        if (A.whale_env > 1e-3f) {
            A.whale_pos += 1.0f / SR;
            float t = A.whale_pos / A.whale_len;
            if (t >= 1.0f)
                A.whale_env = 0.0f;
            float f = A.whale_f0 + (A.whale_f1 - A.whale_f0) * (0.5f - 0.5f * cosf(t * PI_F));
            A.whale_phase += f * (1.0f + 0.01f * sinf(A.whale_pos * 30.0f)) / SR;
            float w = (sinf(2 * PI_F * A.whale_phase) + 0.3f * sinf(6 * PI_F * A.whale_phase)) * sinf(PI_F * fminf(t, 1.0f)) * 0.05f;
            out_l += w * 0.7f;
            out_r += w;
        }
    }

    /* pad (warm strings) and choir (the same notes through vowel formants) */
    float pad = 0, choir = 0;
    for (int i = 0; i < 5; i++) {
        PadVoice *v = &A.pad[i];
        v->amp += (v->target - v->amp) * (1.0f / (SR * 1.8f));
        if (v->amp < 1e-4f)
            continue;
        float saws = 0;
        for (int o = 0; o < 3; o++) {
            float det = o == 0 ? 0.997f : o == 1 ? 1.003f : 1.0f;
            v->phase[o] += v->freq * rate * det / SR;
            v->phase[o] -= floorf(v->phase[o]);
            if (o < 2) {
                float s = sinf(2 * PI_F * v->phase[o]);
                pad += (s + 0.25f * s * s * s) * v->amp;
            }
            saws += (2 * v->phase[o] - 1) * v->amp;
        }
        if (m->choir > 0.0f) {
            svf(&v->f1, saws, 700, 0.12f);         /* "ah" */
            svf(&v->f2, saws, 1150, 0.15f);
            choir += v->f1.bp + v->f2.bp * 0.6f;
        }
    }
    float pl = onepole(&A.pad_lp, pad * 0.022f * m->pad + choir * 0.012f * m->choir, 1600);
    out_l += pl;
    out_r += pl;

    /* bass: a soft sine under each chord */
    A.bass_phase += A.bass_freq * rate / SR;
    A.bass_phase -= floorf(A.bass_phase);
    A.bass_amp *= 1.0f - 1.0f / (SR * 3.0f);
    float bass = sinf(2 * PI_F * A.bass_phase) * A.bass_amp * 0.09f * m->bass;
    out_l += bass;
    out_r += bass;

    /* drone: a low, slowly breathing tone for the crypt */
    if (m->drone > 0.0f) {
        A.drone_phase += mtof((float)(m->tonic - 36)) * rate / SR;
        A.drone_phase -= floorf(A.drone_phase);
        float d = onepole(&A.drone_lp, 2 * A.drone_phase - 1, 180 + 60 * sinf(A.wind_t * 0.2f)) * 0.08f * m->drone;
        out_l += d;
        out_r += d;
    }

    *l = out_l;
    *r = out_r;
}

/* war drums while creatures hunt you: low toms on beats one and three */
static float danger_sample(void)
{
    if (A.step_clock < 1.0 && (A.step % 4 == 0) && A.danger > 0.05f) {
        A.drum_env = 1.0f;
        A.drum_freq = A.step % 8 == 0 ? 62.0f : 74.0f;
    }
    A.drum_env *= 1.0f - 1.0f / (SR * 0.25f);
    A.drum_phase += A.drum_freq * (0.7f + 0.6f * A.drum_env) / SR;
    float s = sinf(2 * PI_F * A.drum_phase) * A.drum_env + rnd() * A.drum_env * A.drum_env * 0.15f;
    return s * A.danger * 0.28f;
}

/* the boombox: bass line, kick, snare, hats. three records: a D minor funk groove,
 * a one-drop reggae skank, and four-on-the-floor disco */
static void groove_sample(float *l, float *r)
{
    static const int BASSES[3][16] = {
        { 38, 0, 38, 50, 0, 41, 0, 43, 38, 0, 45, 0, 48, 0, 45, 43 },
        { 43, 0, 0, 0, 43, 0, 46, 0, 48, 0, 0, 0, 46, 0, 43, 0 },
        { 45, 57, 45, 57, 45, 57, 45, 57, 41, 53, 41, 53, 43, 55, 43, 55 },
    };
    static const int KICKS[3][16] = {
        { 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 },
    };
    static const int SNARES[3][16] = {
        { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 },
    };
    static const float TEMPO[3] = { 104.0f, 76.0f, 120.0f };
    int rec = A.scene.groove % 3;
    const int *BASS = BASSES[rec], *KICK = KICKS[rec], *SNARE = SNARES[rec];

    double sixteenth = SR * 60.0 / TEMPO[rec] / 4.0;
    A.groove_clock += 1.0;
    if (A.groove_clock >= sixteenth) {
        A.groove_clock -= sixteenth;
        A.groove_step = (A.groove_step + 1) % 16;
        int s = A.groove_step;
        if (BASS[s]) {
            A.gbass_freq = mtof((float)BASS[s]);
            A.gbass_env = 1.0f;
        }
        if (KICK[s]) A.kick_env = 1.0f, A.kick_phase = 0;
        if (SNARE[s]) A.snare_env = 1.0f;
        A.hat_env = rec == 2 ? (s % 2 ? 0.9f : 0.2f) : rec == 1 ? (s % 4 == 2 ? 0.8f : 0.0f) : (s % 2 == 0 ? 0.7f : 0.35f);
        /* reggae: the skank, a clipped chord on the off-beats */
        if (rec == 1 && s % 4 == 2)
            for (int i = 0; i < 3; i++)
                stab((float)(67 + i * 4 - (s >= 8 ? 2 : 0)), 0.9f);
    }

    A.gbass_env *= 1.0f - 1.0f / (SR * 0.18f);
    A.gbass_phase += A.gbass_freq / SR;
    A.gbass_phase -= floorf(A.gbass_phase);
    svf(&A.gbass_f, 2 * A.gbass_phase - 1, 300 + 1800 * A.gbass_env, 0.3f);
    float bass = A.gbass_f.lp * A.gbass_env * 0.4f;

    A.kick_env *= 1.0f - 1.0f / (SR * 0.12f);
    A.kick_phase += (50 + 120 * A.kick_env * A.kick_env) / SR;
    float kick = sinf(2 * PI_F * A.kick_phase) * A.kick_env * 0.55f;

    A.snare_env *= 1.0f - 1.0f / (SR * 0.09f);
    svf(&A.snare_f, rnd(), 2500, 0.6f);
    float snare = (A.snare_f.bp * 0.8f + sinf(2 * PI_F * 185 * A.snare_env) * 0.2f) * A.snare_env * 0.5f;

    A.hat_env *= 1.0f - 1.0f / (SR * 0.02f);
    svf(&A.hat_f, rnd(), 7000, 0.4f);
    float hat = (rnd() - A.hat_f.lp) * A.hat_env * 0.08f;

    *l = bass + kick + snare * 0.9f + hat * 1.2f;
    *r = bass + kick + snare * 1.1f + hat * 0.8f;
}

/* ---------- ambience ---------- */

static void ambience_sample(float *l, float *r, float *wet)
{
    float night = A.night_mix;
    bool under = A.scene.underground;
    float open = 1.0f - A.shelter;
    *wet = 0;

    /* wind: two decorrelated noises, gently gusting; faint indoors */
    A.wind_t += 1.0f / SR;
    float gust = 0.55f + 0.45f * sinf(A.wind_t * 0.21f) * sinf(A.wind_t * 0.07f + 1.0f);
    float cutoff = 250 + 500 * gust;
    float wl = onepole(&A.wind_lp[0][1], onepole(&A.wind_lp[0][0], rnd(), cutoff), cutoff);
    float wr = onepole(&A.wind_lp[1][1], onepole(&A.wind_lp[1][0], rnd(), cutoff), cutoff);
    float wind_gain = (0.12f + 0.08f * night + 0.1f * A.rain_mix) * gust * (under ? 0.35f : 0.4f + 0.6f * open);
    *l = wl * wind_gain;
    *r = wr * wind_gain;

    /* birds on a dry day, from somewhere around you */
    if (night < 0.5f && A.rain_mix < 0.3f && !under && open > 0.5f) {
        A.bird_t -= 1.0f / SR;
        if (A.bird_t <= 0 && A.bird_chirps == 0) {
            A.bird_chirps = 2 + (int)(rnd01() * 5);
            A.bird_f0 = 2800 + rnd01() * 2200;
            A.bird_f1 = A.bird_f0 * (0.7f + rnd01() * 0.7f);
            A.bird_len = 0.05f + rnd01() * 0.07f;
            A.bird_pos = 0;
            A.bird_pan = rnd();
            A.bird_t = 2.5f + rnd01() * 6.0f;
        }
        if (A.bird_chirps > 0) {
            float t = A.bird_pos / A.bird_len;
            if (t < 1.0f) {
                float f = A.bird_f0 + (A.bird_f1 - A.bird_f0) * t;
                A.bird_phase += f / SR;
                float s = sinf(2 * PI_F * A.bird_phase) * sinf(PI_F * t) * 0.03f;
                *l += s * (0.5f - 0.5f * A.bird_pan);
                *r += s * (0.5f + 0.5f * A.bird_pan);
                *wet += s;
            }
            A.bird_pos += 1.0f / SR;
            if (A.bird_pos > A.bird_len * 1.8f) {
                A.bird_pos = 0;
                A.bird_chirps--;
            }
        }
    }

    /* crickets on dry nights */
    if (night > 0.05f && A.rain_mix < 0.5f && !under) {
        A.cricket_t += 1.0f / SR;
        for (int c = 0; c < 2; c++) {
            float period = c ? 0.73f : 0.61f;
            float local = fmodf(A.cricket_t + c * 0.3f, period);
            float gate = local < 0.12f ? (sinf(local * 2 * PI_F * 30.0f) > 0.2f ? 1.0f : 0.0f) : 0.0f;
            float tone = sinf(2 * PI_F * (c ? 4700.0f : 4300.0f) * A.cricket_t);
            float s = tone * gate * 0.012f * night * (1.0f - A.rain_mix) * open;
            *l += s * (c ? 0.3f : 0.8f);
            *r += s * (c ? 0.8f : 0.3f);
        }
    }

    /* rain: a steady hiss plus individual drops; muffled and quieter under a roof */
    if (A.rain_mix > 0.01f && !under) {
        float cut = 1200 + 5000 * open;
        for (int ch = 0; ch < 2; ch++) {
            float n = rnd();
            float hp = n - onepole(&A.rain_hp[ch], n, 400);
            float s = onepole(&A.rain_lp[ch], hp, cut) * 0.18f * A.rain_mix * (0.35f + 0.65f * open);
            if (ch == 0) *l += s; else *r += s;
        }
        if (rnd01() < A.rain_mix * 90.0f / SR)
            A.rain_drop = 0.3f + rnd01() * 0.7f;
        A.rain_drop *= 1.0f - 1.0f / (SR * 0.003f);
        float drop = onepole(&A.rain_drop_lp, rnd() * A.rain_drop, 2500 * (0.3f + 0.7f * open)) * 0.2f * A.rain_mix;
        *l += drop * (0.5f + rnd() * 0.3f);
        *r += drop * (0.5f - rnd() * 0.3f);
    }

    /* the crypt: slow water drips echoing off stone */
    if (under) {
        A.drip_t -= 1.0f / SR;
        if (A.drip_t <= 0) {
            A.drip_t = 0.8f + rnd01() * 3.0f;
            A.drip_env = 1.0f;
            A.drip_freq = 1300 + rnd01() * 1800;
            A.drip_pan = rnd() * 0.8f;
            A.drip_phase = 0;
        }
        if (A.drip_env > 1e-4f) {
            A.drip_env *= 1.0f - 1.0f / (SR * 0.05f);
            A.drip_phase += A.drip_freq * (1.0f + 0.6f * A.drip_env) / SR;
            float s = sinf(2 * PI_F * A.drip_phase) * A.drip_env * 0.05f;
            *l += s * (0.5f - 0.5f * A.drip_pan);
            *r += s * (0.5f + 0.5f * A.drip_pan);
            *wet += s * 4.0f;
        }
    }

    /* fire: random pops and a soft roar from the nearest fire */
    float fire = A.scene.fire;
    if (fire > 0.01f) {
        if (rnd01() < fire * 25.0f / SR)
            A.fire_pop = 0.3f + rnd01() * 0.7f;
        A.fire_pop *= 1.0f - 1.0f / (SR * 0.004f);
        float s = (rnd() * A.fire_pop * 0.25f + onepole(&A.fire_lp, rnd(), 400) * 0.35f) * fire;
        float pan = A.scene.fire_pan;
        *l += s * (0.55f - 0.45f * pan);
        *r += s * (0.55f + 0.45f * pan);
    }

    /* the sea on the shore: a steady wash, and every few seconds a wave rolls in and breaks */
    float surf = A.scene.surf;
    if (surf > 0.01f && !under) {
        A.surf_t -= 1.0f / SR;
        if (A.surf_t <= 0.0f) {
            A.surf_len = 3.5f + rnd01() * 2.5f;
            A.surf_t = A.surf_len * (0.9f + rnd01() * 0.8f);
            A.surf_env = 0.0001f;
        }
        float wave = 0.0f;
        if (A.surf_env > 0.0f) {
            A.surf_env += 1.0f / (SR * A.surf_len);
            float t = A.surf_env;
            if (t >= 1.0f)
                A.surf_env = 0.0f;
            /* the swell builds, crashes, then fizzes back down the sand */
            wave = t < 0.35f ? t / 0.35f * 0.6f : t < 0.42f ? 1.0f : expf(-(t - 0.42f) * 4.0f);
        }
        float cut = 300 + 3500 * wave;
        float pan = A.scene.surf_pan;
        for (int ch = 0; ch < 2; ch++) {
            float n = onepole(&A.surf_lp[ch][1], onepole(&A.surf_lp[ch][0], rnd(), cut), cut);
            float s = n * (0.25f + wave * 1.1f) * surf * 0.5f;
            if (ch == 0) *l += s * (0.55f - 0.45f * pan); else *r += s * (0.55f + 0.45f * pan);
            *wet += s * 0.3f;
        }
    }
    /* a gale howling round corners */
    if (A.scene.wind > 0.01f) {
        svf(&A.howl_f, rnd(), 380 + 600 * A.scene.wind + 200 * sinf(A.wind_t * 0.7f), 0.08f);
        float s = A.howl_f.bp * A.scene.wind * 0.35f * (0.4f + 0.6f * open);
        *l += s;
        *r += s * 0.9f;
    }
    /* hail rattling down */
    if (A.scene.hail > 0.05f && !under) {
        if (rnd01() < A.scene.hail * 400.0f / SR)
            A.hail_env = 0.3f + rnd01() * 0.7f;
        A.hail_env *= 1.0f - 1.0f / (SR * 0.002f);
        float s = rnd() * A.hail_env * 0.25f * A.scene.hail * (0.3f + 0.7f * open);
        float pan = rnd();
        *l += s * (0.5f - 0.4f * pan);
        *r += s * (0.5f + 0.4f * pan);
    }
    /* a tornado's roar */
    if (A.scene.tornado > 0.01f) {
        float n = onepole(&A.tor_lp[1], onepole(&A.tor_lp[0], rnd(), 180), 220);
        float s = n * A.scene.tornado * 2.5f * (0.8f + 0.2f * sinf(A.wind_t * 3.0f));
        float pan = A.scene.tornado_pan;
        *l += s * (0.55f - 0.45f * pan);
        *r += s * (0.55f + 0.45f * pan);
    }
    /* Old Ember's rumble, deep in the ground */
    if (A.scene.volcano > 0.01f) {
        float n = onepole(&A.rumble_lp[1], onepole(&A.rumble_lp[0], rnd(), 60), 70);
        float s = n * A.scene.volcano * 3.0f * (0.6f + 0.4f * sinf(A.wind_t * 0.5f));
        *l += s;
        *r += s;
    }
    /* lava bubbling and popping */
    if (A.scene.lava > 0.01f) {
        A.lava_t -= 1.0f / SR;
        if (A.lava_t <= 0.0f) {
            A.lava_t = 0.08f + rnd01() * 0.4f;
            A.lava_env = 1.0f;
            A.lava_freq = 70 + rnd01() * 90;
        }
        A.lava_env *= 1.0f - 1.0f / (SR * 0.08f);
        A.lava_phase += A.lava_freq * (1.0f + A.lava_env) / SR;
        float s = sinf(2 * PI_F * A.lava_phase) * A.lava_env * A.scene.lava * 0.25f;
        *l += s;
        *r += s;
    }
    /* the harbour: rigging clinking against masts, ropes creaking */
    if (A.scene.harbour > 0.01f) {
        A.harb_t -= 1.0f / SR;
        if (A.harb_t <= 0.0f) {
            A.harb_t = 0.6f + rnd01() * 2.5f;
            A.harb_env = 1.0f;
            A.harb_freq = rnd01() < 0.6f ? 1800 + rnd01() * 1400 : 140 + rnd01() * 60;
        }
        A.harb_env *= 1.0f - 1.0f / (SR * (A.harb_freq > 1000 ? 0.05f : 0.3f));
        A.harb_phase += A.harb_freq / SR;
        float s = sinf(2 * PI_F * A.harb_phase) * A.harb_env * A.scene.harbour * (A.harb_freq > 1000 ? 0.02f : 0.03f);
        *l += s * 0.6f;
        *r += s * 0.4f;
        *wet += s;
    }
    /* Brinewick's shell wind chimes, pentatonic and random */
    if (A.scene.chimes > 0.01f && !under) {
        A.chime_t -= 1.0f / SR;
        if (A.chime_t <= 0.0f) {
            A.chime_t = (0.4f + rnd01() * 2.0f) / (0.3f + A.scene.chimes);
            static const float notes[5] = { 84, 86, 88, 91, 93 };
            int k = A.chime_next = (A.chime_next + 1) % 3;
            A.chime_freq[k] = mtof(notes[(int)(rnd01() * 5) % 5]);
            A.chime_env[k] = 0.6f + rnd01() * 0.4f;
        }
        for (int k = 0; k < 3; k++) {
            if (A.chime_env[k] < 1e-4f)
                continue;
            A.chime_env[k] *= 1.0f - 1.0f / (SR * 1.2f);
            A.chime_phase[k] += A.chime_freq[k] / SR;
            float s = (sinf(2 * PI_F * A.chime_phase[k]) + 0.3f * sinf(2 * PI_F * A.chime_phase[k] * 2.76f)) * A.chime_env[k] * 0.012f * A.scene.chimes;
            *l += s * (k == 1 ? 0.8f : 0.4f);
            *r += s * (k == 1 ? 0.4f : 0.8f);
            *wet += s * 2.0f;
        }
    }
    /* under water: bubbles rising past your ears */
    if (A.sub > 0.3f) {
        A.bub_t -= 1.0f / SR;
        if (A.bub_t <= 0.0f) {
            A.bub_t = 0.05f + rnd01() * 0.6f;
            A.bub_env = 1.0f;
            A.bub_freq = 300 + rnd01() * 500;
        }
        A.bub_env *= 1.0f - 1.0f / (SR * 0.04f);
        A.bub_phase += A.bub_freq * (1.0f + (1.0f - A.bub_env) * 1.5f) / SR;
        float s = sinf(2 * PI_F * A.bub_phase) * A.bub_env * 0.05f * A.sub;
        *l += s;
        *r += s;
    }

    /* fountain: a bubbling, band-passed babble */
    float water = A.scene.water;
    if (water > 0.01f) {
        svf(&A.water_f, rnd(), 900 + 700 * sinf(A.wind_t * 13.0f) * sinf(A.wind_t * 7.1f), 0.4f);
        float s = A.water_f.bp * 0.25f * water;
        float pan = A.scene.water_pan;
        *l += s * (0.55f - 0.45f * pan);
        *r += s * (0.55f + 0.45f * pan);
    }
}

/* ---------- mixer ---------- */

static void SDLCALL callback(void *user, SDL_AudioStream *stream, int additional, int total)
{
    (void)user;
    (void)total;
    int frames = additional / (int)(2 * sizeof(float));
    static float buf[4096 * 2];

    while (frames > 0) {
        int n = frames > 4096 ? 4096 : frames;
        for (int i = 0; i < n; i++) {
            /* smooth the controls so nothing jumps */
            A.night_mix += ((A.scene.mood == MOOD_NIGHT ? 1.0f : 0.0f) - A.night_mix) * (1.0f / (SR * 3.0f));
            A.rain_mix += (A.scene.rain - A.rain_mix) * (1.0f / (SR * 2.0f));
            A.shelter += (A.scene.sheltered - A.shelter) * (1.0f / (SR * 0.4f));
            A.danger += (A.scene.danger - A.danger) * (1.0f / (SR * 1.5f));
            A.groove += ((A.scene.boombox ? 1.0f : 0.0f) - A.groove) * (1.0f / (SR * 0.5f));
            A.slow += ((A.scene.slowmo ? 1.0f : 0.0f) - A.slow) * (1.0f / (SR * 0.4f));
            A.sub += ((A.scene.submerged ? 1.0f : 0.0f) - A.sub) * (1.0f / (SR * 0.15f));

            /* switching moods: fade out, change, fade back in */
            if (A.scene.mood != A.mood) {
                A.mood_fade -= 1.0f / (SR * 1.5f);
                if (A.mood_fade <= 0.0f) {
                    A.mood = A.scene.mood;
                    A.mood_fade = 0.0f;
                    A.step = MOODS[A.mood].bar_steps - 1;
                    A.flute.target = 0;
                }
            } else if (A.mood_fade < 1.0f) {
                A.mood_fade = fminf(1.0f, A.mood_fade + 1.0f / (SR * 2.5f));
            }
            A.music_gain += ((A.music_on ? 1.0f - A.groove : 0.0f) - A.music_gain) * (1.0f / (SR * 1.0f));
            float rate = 1.0f - 0.45f * A.slow;
            float mg = A.music_gain * A.mood_fade * 1.6f * A.music_volume;

            float l = 0, r = 0, send_l = 0, send_r = 0;

            float ml, mr;
            music_sample(&ml, &mr, rate);
            ml *= mg;
            mr *= mg;
            float drum = danger_sample() * (A.music_on ? A.music_volume : 0.0f) * (1.0f - A.groove);
            l += ml + drum;
            r += mr + drum;
            send_l += ml * 0.9f + drum * 0.3f;
            send_r += mr * 0.9f + drum * 0.3f;

            if (A.groove > 0.001f) {
                float gl, gr;
                groove_sample(&gl, &gr);
                l += gl * A.groove * 0.6f * A.sfx_volume;
                r += gr * A.groove * 0.6f * A.sfx_volume;
            }

            float al, ar, awet;
            ambience_sample(&al, &ar, &awet);
            l += al * A.sfx_volume;
            r += ar * A.sfx_volume;
            send_l += awet * A.sfx_volume;
            send_r += awet * A.sfx_volume;

            /* sound effects and voices */
            for (int v = 0; v < MAX_VOICES; v++) {
                Voice *vo = &A.voices[v];
                if (!vo->on)
                    continue;
                int p = (int)vo->pos;
                if (p >= vo->len - 1) {
                    vo->on = false;
                    continue;
                }
                float f = vo->pos - p;
                float s = (vo->data[p] * (1 - f) + vo->data[p + 1] * f) * A.sfx_volume;
                vo->pos += vo->rate * (vo->speech ? 1.0f : rate);
                l += s * vo->gl;
                r += s * vo->gr;
                send_l += s * vo->gl * 0.25f;
                send_r += s * vo->gr * 0.25f;
            }

            /* one shared reverb; the crypt and the palace ring longer */
            float fb = MOODS[A.mood].reverb;
            float in = (send_l + send_r) * 0.5f * 0.015f;
            l += reverb_channel(&A.reverb, 0, in, fb) * 0.9f;
            r += reverb_channel(&A.reverb, 1, in, fb) * 0.9f;

            float cutoff = fminf(18000.0f - 16500.0f * A.slow, 18000.0f - 17300.0f * A.sub);
            l = onepole(&A.master_lp[0], l, cutoff);
            r = onepole(&A.master_lp[1], r, cutoff);

            buf[i * 2] = tanhf(l * 0.9f);
            buf[i * 2 + 1] = tanhf(r * 0.9f);
        }
        SDL_PutAudioStreamData(stream, buf, n * 2 * sizeof(float));
        frames -= n;
    }
}

/* ---------- public API ---------- */

bool audio_init(void)
{
    memset(&A, 0, sizeof A);
    A.music_volume = A.sfx_volume = 1.0f;
    A.mood_fade = 1.0f;
    A.scene.mood = MOOD_RAIN;
    A.mood = MOOD_RAIN;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("no audio: %s", SDL_GetError());
        return false;
    }
    set_gains();
    gen_sfx();
    for (int ch = 0; ch < 2; ch++) {
        int spread = ch ? 23 : 0;
        for (int i = 0; i < COMBS; i++)
            delay_init(&A.reverb.comb[ch][i], COMB_LEN[i] + spread);
        for (int i = 0; i < ALLPASSES; i++)
            delay_init(&A.reverb.allpass[ch][i], ALLPASS_LEN[i] + spread);
    }
    A.music_on = true;
    A.bird_t = 2.0f;
    A.step = 15;
    A.chord = 3;
    A.last_degree = 4;

    SDL_AudioSpec spec = { SDL_AUDIO_F32, 2, SR };
    A.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, callback, NULL);
    if (!A.stream) {
        SDL_Log("no audio device: %s", SDL_GetError());
        return false;
    }
    SDL_ResumeAudioStreamDevice(A.stream);
    A.ok = true;
    return true;
}

void audio_shutdown(void)
{
    if (A.stream)
        SDL_DestroyAudioStream(A.stream);
    A.stream = NULL;
    A.ok = false;
    for (int s = 0; s < SFX_COUNT; s++)
        for (int v = 0; v < VARIANTS; v++)
            free(sfx[s][v].data);
    for (int c = 0; c < A.clip_count; c++)
        free(A.clips[c].data);
    for (int ch = 0; ch < 2; ch++) {
        for (int i = 0; i < COMBS; i++)
            free(A.reverb.comb[ch][i].buf);
        for (int i = 0; i < ALLPASSES; i++)
            free(A.reverb.allpass[ch][i].buf);
    }
}

#define LOCK() if (!A.ok) return; SDL_LockAudioStream(A.stream)
#define UNLOCK() SDL_UnlockAudioStream(A.stream)

void audio_set_listener(vec3 pos, float yaw)
{
    LOCK();
    glm_vec3_copy(pos, A.listener);
    A.listener_yaw = yaw;
    UNLOCK();
}

void audio_set_scene(const AudioScene *scene)
{
    LOCK();
    A.scene = *scene;
    UNLOCK();
}

void audio_set_volumes(float music, float effects)
{
    LOCK();
    A.music_volume = music;
    A.sfx_volume = effects;
    UNLOCK();
}

void audio_toggle_music(void)
{
    LOCK();
    A.music_on = !A.music_on;
    UNLOCK();
}

bool audio_music_on(void)
{
    return A.ok && A.music_on;
}

static Voice *free_voice(void)
{
    /* take a free voice, or steal the effect furthest along (never a voice line) */
    int best = -1;
    float furthest = -1;
    for (int v = 0; v < MAX_VOICES; v++) {
        if (!A.voices[v].on)
            return &A.voices[v];
        float done = A.voices[v].pos / A.voices[v].len;
        if (!A.voices[v].speech && done > furthest) {
            furthest = done;
            best = v;
        }
    }
    return best >= 0 ? &A.voices[best] : NULL;
}

/* pan and fade for a sound at `pos` (only called with the stream locked) */
static bool spatial(vec3 pos, float volume, float *gl, float *gr)
{
    vec3 d;
    glm_vec3_sub(pos, A.listener, d);
    float dist = glm_vec3_norm(d);
    if (dist > 60.0f)
        return false;
    float att = 1.0f / (1.0f + dist * 0.18f + dist * dist * 0.004f);
    vec3 right = { cosf(A.listener_yaw), 0, -sinf(A.listener_yaw) };
    float pan = dist > 0.3f ? glm_vec3_dot(d, right) / dist : 0.0f;
    *gl = volume * att * cosf((pan + 1) * PI_F / 4);
    *gr = volume * att * sinf((pan + 1) * PI_F / 4);
    return true;
}

static void start_voice_pitch(Sfx s, float gl, float gr, float pitch)
{
    const Buffer *b = &sfx[s][(int)(rnd01() * VARIANTS) % VARIANTS];
    Voice *vo = free_voice();
    if (!b->data || !vo)
        return;
    float rate = pitch > 0.0f ? pitch : 0.94f + rnd01() * 0.12f;
    *vo = (Voice){ .data = b->data, .len = b->len, .rate = rate,
                   .gl = gl * SFX_GAIN[s], .gr = gr * SFX_GAIN[s], .on = true };
}

static void start_voice(Sfx s, float gl, float gr)
{
    start_voice_pitch(s, gl, gr, 0.0f);
}

void audio_play(Sfx s, float volume)
{
    LOCK();
    start_voice(s, volume * 0.7f, volume * 0.7f);
    UNLOCK();
}

void audio_play_at(Sfx s, vec3 pos, float volume)
{
    LOCK();
    float gl, gr;
    if (spatial(pos, volume, &gl, &gr))
        start_voice(s, gl, gr);
    UNLOCK();
}

void audio_play_pitch(Sfx s, float volume, float pitch)
{
    LOCK();
    start_voice_pitch(s, volume * 0.7f, volume * 0.7f, pitch);
    UNLOCK();
}

void audio_play_at_pitch(Sfx s, vec3 pos, float volume, float pitch)
{
    LOCK();
    float gl, gr;
    if (spatial(pos, volume, &gl, &gr))
        start_voice_pitch(s, gl, gr, pitch);
    UNLOCK();
}

/* ---------- voice clips ---------- */

int audio_load_clip(const char *path)
{
    if (A.clip_count >= MAX_CLIPS)
        return -1;
    SDL_AudioSpec spec;
    Uint8 *wav;
    Uint32 len;
    if (!SDL_LoadWAV(path, &spec, &wav, &len))
        return -1;
    /* convert to mono float at our rate once, up front */
    SDL_AudioSpec want = { SDL_AUDIO_F32, 1, SR };
    Uint8 *out;
    int out_len;
    bool ok = SDL_ConvertAudioSamples(&spec, wav, (int)len, &want, &out, &out_len);
    SDL_free(wav);
    if (!ok)
        return -1;
    Buffer *b = &A.clips[A.clip_count];
    b->len = out_len / (int)sizeof(float);
    b->data = malloc(out_len);
    memcpy(b->data, out, out_len);
    SDL_free(out);
    return A.clip_count++;
}

float audio_clip_length(int clip)
{
    if (clip < 0 || clip >= A.clip_count)
        return 0.0f;
    return A.clips[clip].len / (float)SR;
}

void audio_speak(int clip, vec3 pos)
{
    if (clip < 0 || clip >= A.clip_count)
        return;
    LOCK();
    for (int v = 0; v < MAX_VOICES; v++)
        if (A.voices[v].speech)
            A.voices[v].on = false;
    float gl, gr;
    if (!spatial(pos, 1.0f, &gl, &gr))
        gl = gr = 0.3f;
    /* voices stay clear even a few steps away, but keep their direction */
    float boost = 1.6f / fmaxf(gl + gr, 0.2f);
    Voice *vo = free_voice();
    if (vo)
        *vo = (Voice){ .data = A.clips[clip].data, .len = A.clips[clip].len, .rate = 1.0f,
                       .gl = gl * boost, .gr = gr * boost, .on = true, .speech = true };
    UNLOCK();
}

void audio_stop_speaking(void)
{
    LOCK();
    for (int v = 0; v < MAX_VOICES; v++)
        if (A.voices[v].speech)
            A.voices[v].on = false;
    UNLOCK();
}

bool audio_speaking(void)
{
    if (!A.ok)
        return false;
    bool on = false;
    SDL_LockAudioStream(A.stream);
    for (int v = 0; v < MAX_VOICES; v++)
        if (A.voices[v].on && A.voices[v].speech)
            on = true;
    SDL_UnlockAudioStream(A.stream);
    return on;
}
