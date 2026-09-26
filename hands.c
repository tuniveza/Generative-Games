#include "hands.h"
#include "meshgen.h"

#include <string.h>

/* hand space: wrist at the origin, fingers along -Z, palm facing -X (inwards for
 * the right hand), thumb up (+Y). the left hand is the same model mirrored. */

enum { B_UPPER, B_FOREARM, B_PALM, B_FINGER0 = 3, B_THUMB0 = 15, BONE_COUNT = 18 };

#define FOREARM_LEN 0.34f       /* long enough that the elbow is at the edge of the view */
#define KNUCKLE_Z  -0.092f

static const float FINGER_Y[4] = { 0.029f, 0.009f, -0.011f, -0.029f };
static const float FINGER_LEN[4][3] = {
    { 0.042f, 0.026f, 0.020f },
    { 0.046f, 0.029f, 0.021f },
    { 0.043f, 0.027f, 0.020f },
    { 0.034f, 0.021f, 0.018f },
};
static const float FINGER_R[3] = { 0.0098f, 0.0088f, 0.0078f };
static const float THUMB_LEN[3] = { 0.036f, 0.029f, 0.024f };
static const float THUMB_R[3] = { 0.0125f, 0.0112f, 0.0098f };

static int finger_bone(int f, int k) { return B_FINGER0 + f * 3 + k; }

void hands_init(Hands *h)
{
    memset(h, 0, sizeof *h);

    int parents[BONE_COUNT];
    mat4 rest[BONE_COUNT];
    mat4 world[BONE_COUNT];

    /* skeleton, each bone relative to its parent: upper arm (at the elbow), forearm, palm... */
    parents[B_UPPER] = -1;
    glm_translate_make(rest[B_UPPER], (vec3){0, -0.004f, FOREARM_LEN});
    parents[B_FOREARM] = B_UPPER;
    glm_mat4_identity(rest[B_FOREARM]);
    parents[B_PALM] = B_FOREARM;
    glm_translate_make(rest[B_PALM], (vec3){0, 0.004f, -FOREARM_LEN});
    for (int f = 0; f < 4; f++) {
        for (int k = 0; k < 3; k++) {
            int b = finger_bone(f, k);
            parents[b] = k ? b - 1 : B_PALM;
            if (k == 0) {
                /* fingers fan out a little */
                glm_translate_make(rest[b], (vec3){-0.002f, FINGER_Y[f], KNUCKLE_Z + (f == 1 ? -0.004f : 0.0f)});
                glm_rotate(rest[b], (f - 1.5f) * -0.05f, (vec3){1, 0, 0});
            } else {
                glm_translate_make(rest[b], (vec3){0, 0, -FINGER_LEN[f][k - 1]});
            }
        }
    }
    /* thumb: from the heel of the palm, pointing forward, up and inwards */
    parents[B_THUMB0] = B_PALM;
    glm_translate_make(rest[B_THUMB0], (vec3){-0.012f, 0.03f, -0.028f});
    glm_rotate(rest[B_THUMB0], 0.55f, (vec3){1, 0, 0});
    glm_rotate(rest[B_THUMB0], 0.45f, (vec3){0, 1, 0});
    for (int k = 1; k < 3; k++) {
        parents[B_THUMB0 + k] = B_THUMB0 + k - 1;
        glm_translate_make(rest[B_THUMB0 + k], (vec3){0, 0, -THUMB_LEN[k - 1]});
    }

    for (int b = 0; b < BONE_COUNT; b++) {
        if (parents[b] >= 0)
            glm_mat4_mul(world[parents[b]], rest[b], world[b]);
        else
            glm_mat4_copy(rest[b], world[b]);
    }

    /* geometry: capsules around each bone, built at the rest pose */
    MeshBuilder skin, sleeve;
    mb_init(&skin);
    mb_init(&sleeve);
    mat4 xf;

    skin.joint = B_FOREARM;
    mb_capsule(&skin, world[B_FOREARM], 0.031f, FOREARM_LEN - 0.02f, 14);
    sleeve.joint = B_FOREARM;
    glm_translate_make(xf, (vec3){0, 0, 0.02f});
    glm_mat4_mul(world[B_FOREARM], xf, xf);
    mb_capsule(&sleeve, xf, 0.036f, FOREARM_LEN - 0.12f, 16);
    /* upper arm: from the elbow back, down and out, off the edge of the screen */
    sleeve.joint = B_UPPER;
    {
        versor q;
        vec3 from = { 0, 0, -1 }, to = { 0.15f, -0.85f, 0.5f };
        glm_vec3_normalize(to);
        glm_quat_from_vecs(from, to, q);
        mat4 rot;
        glm_quat_mat4(q, rot);
        glm_mat4_mul(world[B_UPPER], rot, xf);
        mb_capsule(&sleeve, xf, 0.044f, 0.22f, 16);
    }
    /* leather cuff at the wrist */
    glm_translate_make(xf, (vec3){0, 0, 0.055f});
    glm_scale(xf, (vec3){1.0f, 1.12f, 1.0f});
    mb_capsule(&sleeve, xf, 0.036f, 0.035f, 16);

    skin.joint = B_PALM;
    glm_translate_make(xf, (vec3){0.002f, 0, -0.022f});
    glm_scale(xf, (vec3){0.017f, 0.041f, 0.024f});
    mb_capsule(&skin, xf, 1.0f, 2.2f, 16);
    /* heel of the thumb */
    glm_translate_make(xf, (vec3){-0.007f, 0.022f, -0.03f});
    glm_scale(xf, (vec3){0.016f, 0.02f, 0.022f});
    mb_capsule(&skin, xf, 1.0f, 1.0f, 12);

    for (int f = 0; f < 4; f++) {
        float scale = f == 3 ? 0.88f : 1.0f;
        for (int k = 0; k < 3; k++) {
            skin.joint = finger_bone(f, k);
            mb_capsule(&skin, world[finger_bone(f, k)], FINGER_R[k] * scale,
                       FINGER_LEN[f][k] - (k == 2 ? FINGER_R[k] * 0.6f : 0.0f), 10);
        }
    }
    for (int k = 0; k < 3; k++) {
        skin.joint = B_THUMB0 + k;
        mb_capsule(&skin, world[B_THUMB0 + k], THUMB_R[k], THUMB_LEN[k] - (k == 2 ? THUMB_R[k] * 0.6f : 0.0f), 10);
    }

    MeshBuilder mbs[2] = { skin, sleeve };
    Material mats[2];
    material_color(&mats[0], 0.42f, 0.24f, 0.16f, 0.5f, 0.0f);      /* sun-tanned skin */
    material_color(&mats[1], 0.07f, 0.045f, 0.03f, 0.75f, 0.0f);    /* worn leather */
    model_from_builders_skinned(&h->model, mbs, mats, 2, BONE_COUNT, parents, rest);
    mb_free(&skin);
    mb_free(&sleeve);

    for (int i = 0; i < 2; i++)
        pose_init(&h->pose[i], &h->model);
}

void hands_free(Hands *h)
{
    pose_free(&h->pose[0]);
    pose_free(&h->pose[1]);
    model_free(&h->model);
}

/* ---------- animation helpers ---------- */

typedef struct {
    vec3 pos;       /* wrist position in view space */
    vec3 rot;       /* pitch (x), yaw (y), roll (z) in radians */
} Placement;

static float smooth01(float t) { t = glm_clamp(t, 0, 1); return t * t * (3 - 2 * t); }

static void blend(Placement *out, const Placement *a, const Placement *b, float t)
{
    glm_vec3_lerp((float *)a->pos, (float *)b->pos, t, out->pos);
    glm_vec3_lerp((float *)a->rot, (float *)b->rot, t, out->rot);
}

/* keyframed offsets (added to the idle placement) for each attack style */
typedef struct {
    float t;
    Placement p;
} Key;

static void sample_keys(const Key *keys, int n, float t, Placement *out)
{
    if (t <= keys[0].t) { *out = keys[0].p; return; }
    for (int i = 0; i < n - 1; i++) {
        if (t <= keys[i + 1].t) {
            float f = smooth01((t - keys[i].t) / (keys[i + 1].t - keys[i].t));
            blend(out, &keys[i].p, &keys[i + 1].p, f);
            return;
        }
    }
    *out = keys[n - 1].p;
}

#define K(t, x, y, z, rx, ry, rz) { t, { { x, y, z }, { rx, ry, rz } } }

static const Key SWING_KEYS[5][5] = {
    [SWING_PUNCH] = {
        K(0.00f, 0, 0, 0, 0, 0, 0),
        K(0.15f, 0.02f, -0.02f, 0.05f, 0.1f, 0, 0),
        K(0.35f, -0.08f, 0.04f, -0.26f, -0.1f, 0.25f, 0),
        K(0.60f, -0.03f, 0.01f, -0.1f, 0, 0.1f, 0),
        K(1.00f, 0, 0, 0, 0, 0, 0),
    },
    [SWING_STAB] = {
        K(0.00f, 0, 0, 0, 0, 0, 0),
        K(0.20f, 0.03f, 0.0f, 0.06f, 0.35f, 0, 0),
        K(0.40f, -0.06f, 0.05f, -0.3f, -0.9f, 0.2f, 0),
        K(0.65f, -0.02f, 0.02f, -0.12f, -0.5f, 0.1f, 0),
        K(1.00f, 0, 0, 0, 0, 0, 0),
    },
    [SWING_SLASH] = {
        K(0.00f, 0, 0, 0, 0, 0, 0),
        K(0.28f, 0.1f, 0.12f, 0.05f, 0.35f, -0.3f, -0.7f),
        K(0.50f, -0.2f, -0.08f, -0.14f, -0.7f, 0.8f, 0.9f),
        K(0.70f, -0.16f, -0.1f, -0.08f, -0.5f, 0.6f, 0.6f),
        K(1.00f, 0, 0, 0, 0, 0, 0),
    },
    [SWING_OVERHEAD] = {
        K(0.00f, 0, 0, 0, 0, 0, 0),
        K(0.35f, 0.0f, 0.2f, 0.08f, 0.9f, 0, -0.1f),
        K(0.55f, -0.06f, -0.16f, -0.2f, -1.1f, 0.1f, 0.1f),
        K(0.75f, -0.05f, -0.15f, -0.15f, -0.9f, 0.1f, 0.1f),
        K(1.00f, 0, 0, 0, 0, 0, 0),
    },
    [SWING_BASH] = {
        K(0.00f, 0, 0, 0, 0, 0, 0),
        K(0.20f, -0.05f, 0.03f, 0.06f, 0, 0.4f, 0),
        K(0.40f, -0.14f, 0.05f, -0.24f, 0, 0.9f, 0),
        K(0.65f, -0.1f, 0.03f, -0.1f, 0, 0.6f, 0),
        K(1.00f, 0, 0, 0, 0, 0, 0),
    },
};

static const Key USE_KEYS[4] = {
    K(0.00f, 0, 0, 0, 0, 0, 0),
    K(0.35f, -0.16f, 0.1f, 0.12f, 0.9f, 0.5f, 0.3f),    /* up to the mouth */
    K(0.75f, -0.16f, 0.11f, 0.12f, 1.0f, 0.5f, 0.3f),
    K(1.00f, 0, 0, 0, 0, 0, 0),
};

static const Key CAST_KEYS[4] = {
    K(0.00f, 0, 0, 0, 0, 0, 0),
    K(0.25f, 0.02f, 0.04f, 0.08f, 0.5f, 0, -0.3f),      /* draw back, palm up */
    K(0.45f, -0.03f, 0.05f, -0.12f, -0.6f, 0.3f, 0.9f), /* thrust out, palm forward */
    K(1.00f, 0, 0, 0, 0, 0, 0),
};

static const Key THROW_KEYS[4] = {
    K(0.00f, 0, 0, 0, 0, 0, 0),
    K(0.35f, 0.06f, 0.16f, 0.14f, 1.2f, -0.2f, 0),      /* wind up behind the head */
    K(0.55f, -0.05f, 0.02f, -0.3f, -0.6f, 0.2f, 0),     /* let go */
    K(1.00f, 0, 0, 0, 0, 0, 0),
};

static void set_curl(Pose *p, const float curl[5], float spread)
{
    for (int f = 0; f < 4; f++) {
        for (int k = 0; k < 3; k++) {
            /* each joint bends a bit less than the one before */
            static const float amount[3] = { 1.45f, 1.7f, 1.1f };
            versor q;
            glm_quatv(q, curl[f] * amount[k], (vec3){0, 1, 0});
            if (k == 0) {
                versor s;
                glm_quatv(s, (f - 1.5f) * -spread, (vec3){1, 0, 0});
                glm_quat_mul(s, q, q);
            }
            int b = finger_bone(f, k);
            /* rest rotation of the knuckle keeps its fan angle */
            glm_quat_identity(p->r[b]);
            glm_quat_mul(p->r[b], q, p->r[b]);
        }
    }
    for (int k = 0; k < 3; k++) {
        versor q;
        glm_quatv(q, curl[4] * (k == 0 ? 0.5f : 0.9f), (vec3){0.3f, 1, 0});
        int b = B_THUMB0 + k;
        if (k == 0) {
            /* keep the base's rest orientation, add the bend on top */
            versor rest;
            glm_quatv(rest, 0.45f, (vec3){0, 1, 0});
            versor rx;
            glm_quatv(rx, 0.55f, (vec3){1, 0, 0});
            glm_quat_mul(rx, rest, rest);
            glm_quat_mul(rest, q, p->r[b]);
        } else {
            glm_vec4_copy(q, p->r[b]);
        }
    }
}

static void placement_matrix(const Placement *p, mat4 out)
{
    glm_translate_make(out, (float *)p->pos);
    glm_rotate(out, p->rot[1], (vec3){0, 1, 0});
    glm_rotate(out, p->rot[0], (vec3){1, 0, 0});
    glm_rotate(out, p->rot[2], (vec3){0, 0, 1});
}

void hands_update(Hands *h, const HandInput *in, mat4 inv_view, float dt)
{
    h->time += dt;

    /* walking bob, faster when running, none in the air */
    float move = in->grounded ? glm_clamp(in->speed / 4.5f, 0, 1.6f) : 0.0f;
    h->bob_phase += dt * (4.0f + in->speed * 1.6f) * (move > 0.05f ? 1.0f : 0.0f);
    float bob_x = cosf(h->bob_phase) * 0.012f * move;
    float bob_y = -fabsf(sinf(h->bob_phase)) * 0.016f * move;

    /* hands lag behind quick looks, then catch up */
    float k = 1.0f - expf(-dt * 10.0f);
    h->sway_x += (glm_clamp(-in->look_dx * 0.0009f, -0.05f, 0.05f) - h->sway_x) * k;
    h->sway_y += (glm_clamp(in->look_dy * 0.0009f, -0.04f, 0.04f) - h->sway_y) * k;
    float breath = sinf(h->time * 1.6f) * 0.004f;

    vec3 common = { bob_x + h->sway_x, bob_y + h->sway_y + breath - in->land * 0.05f, 0 };
    if (in->hurt > 0.0f) {
        common[0] += sinf(h->time * 60.0f) * 0.01f * in->hurt;
        common[1] -= 0.03f * in->hurt;
    }

    /* ---- right hand ---- */
    bool spell = in->held != ITEM_NONE && ITEMS[in->held].kind == KIND_SPELL;
    bool fist = in->held == ITEM_NONE;
    bool weapon = in->held != ITEM_NONE && !spell &&
                  (ITEMS[in->held].kind == KIND_WEAPON || ITEMS[in->held].kind == KIND_THROWN ||
                   in->held == ITEM_TORCH);
    Placement rp = { { 0.18f, -0.165f, -0.36f }, { 0.15f, 0.12f, 0.0f } };
    if (fist)
        glm_vec3_copy((vec3){0.17f, -0.19f, -0.34f}, rp.pos);
    if (spell) {
        /* an open hand, palm up, ready to cast */
        glm_vec3_copy((vec3){0.17f, -0.2f, -0.38f}, rp.pos);
        glm_vec3_copy((vec3){0.2f, 0.25f, 1.3f}, rp.rot);
    } else if (!weapon && !fist) {
        /* small things held out on the palm, turned up */
        glm_vec3_copy((vec3){0.15f, -0.17f, -0.34f}, rp.pos);
        glm_vec3_copy((vec3){0.1f, 0.3f, 1.2f}, rp.rot);
    }
    if (in->held == ITEM_SHIELD)
        glm_vec3_copy((vec3){0.18f, -0.26f, -0.38f}, rp.pos);

    Placement off = {0};
    if (in->action == ACT_SWING)
        sample_keys(SWING_KEYS[in->swing], 5, in->action_t, &off);
    else if (in->action == ACT_USE)
        sample_keys(USE_KEYS, 4, in->action_t, &off);
    else if (in->action == ACT_THROW)
        sample_keys(THROW_KEYS, 4, in->action_t, &off);
    else if (in->action == ACT_CAST)
        sample_keys(CAST_KEYS, 4, in->action_t, &off);
    else if (in->action == ACT_BLOCK) {
        /* shield up across the body, or binoculars to the eyes */
        if (in->held == ITEM_SHIELD)
            off = (Placement){ { -0.16f, 0.12f, 0.05f }, { 0.1f, 0.9f, 0.0f } };
        else
            off = (Placement){ { -0.17f, 0.2f, 0.18f }, { 0.0f, 0.1f, -1.2f } };
    }
    glm_vec3_add(rp.pos, off.pos, rp.pos);
    glm_vec3_add(rp.rot, off.rot, rp.rot);
    glm_vec3_add(rp.pos, common, rp.pos);

    float target[5];
    float c = fist ? (in->action == ACT_SWING ? 1.0f : 0.7f) : weapon ? 0.95f : spell ? 0.12f : 0.5f;
    for (int f = 0; f < 4; f++)
        target[f] = c + (fist ? 0.0f : f * 0.02f);
    target[4] = fist ? 0.75f : weapon ? 0.8f : spell ? 0.05f : 0.3f;
    float ck = 1.0f - expf(-dt * 14.0f);
    for (int f = 0; f < 5; f++)
        h->curl[0][f] += (target[f] - h->curl[0][f]) * ck;

    /* ---- left hand: low and relaxed, torch up, or reaching out ---- */
    Placement lp = { { -0.22f, -0.3f, -0.34f }, { 0.25f, -0.2f, 0.0f } };
    float ltarget[5] = { 0.35f, 0.4f, 0.45f, 0.5f, 0.3f };
    if (in->left == LEFT_TORCH) {
        lp = (Placement){ { -0.27f, -0.23f, -0.42f }, { 0.1f, -0.05f, 0.25f } };
        for (int f = 0; f < 5; f++)
            ltarget[f] = 0.95f;
    } else if (in->left == LEFT_LANTERN) {
        /* held out by its handle, a little to the side */
        lp = (Placement){ { -0.25f, -0.14f, -0.46f }, { 0.35f, -0.1f, 1.45f } };
        for (int f = 0; f < 5; f++)
            ltarget[f] = 0.9f;
    }
    if (in->reach_t > 0.0f) {
        /* out and grab, then back */
        float r = sinf(in->reach_t * GLM_PIf);
        Placement reach = { { -0.07f, -0.1f, -0.55f }, { -0.1f, -0.2f, -0.6f } };
        blend(&lp, &lp, &reach, smooth01(r * 1.4f));
        for (int f = 0; f < 5; f++)
            ltarget[f] = in->reach_t < 0.45f ? 0.05f : 0.75f;
    }
    glm_vec3_add(lp.pos, (vec3){-common[0] * 0.8f, common[1], 0}, lp.pos);
    for (int f = 0; f < 5; f++)
        h->curl[1][f] += (ltarget[f] - h->curl[1][f]) * ck;

    /* ---- pose the skeletons and place them in the world ---- */
    Placement places[2] = { rp, lp };
    for (int i = 0; i < 2; i++) {
        Pose *p = &h->pose[i];
        pose_reset(p, &h->model);
        set_curl(p, h->curl[i], i == 0 && !fist && !weapon ? (spell ? 0.2f : 0.12f) : 0.04f);

        /* the left hand is the right-hand model mirrored across its own x axis,
         * so its palm faces +X (inwards) */
        mat4 local;
        placement_matrix(&places[i], local);
        if (i == 1)
            glm_scale(local, (vec3){-1, 1, 1});
        glm_mat4_mul(inv_view, local, h->world[i]);
        pose_update(&h->model, p);
    }
}

/* grip transform for things held in the right fist, relative to the palm bone */
static void grip_matrix(ItemId id, bool weapon, mat4 out)
{
    if (id == ITEM_TORCH) {
        /* held out and tipped well forward, so the flame is ahead, not in your face */
        glm_translate_make(out, (vec3){-0.028f, 0.0f, -0.07f});
        glm_rotate(out, -0.85f, (vec3){1, 0, 0});
        return;
    }
    if (weapon) {
        /* handle through the fist, tipped forward */
        glm_translate_make(out, (vec3){-0.028f, 0.0f, -0.07f});
        glm_rotate(out, -0.35f, (vec3){1, 0, 0});
        if (id == ITEM_SHIELD) {
            glm_translate_make(out, (vec3){-0.09f, 0.02f, -0.06f});
            glm_rotate(out, GLM_PI_2f, (vec3){0, 1, 0});
            glm_rotate(out, 0.2f, (vec3){1, 0, 0});
        }
    } else {
        /* resting on the open palm */
        glm_translate_make(out, (vec3){-0.05f, 0.0f, -0.07f});
        glm_rotate(out, -GLM_PI_2f, (vec3){0, 0, 1});
    }
}

/* the lantern hangs from the left fist by its handle */
static void lantern_matrix(const Hands *h, mat4 out)
{
    const Model *m = &ITEMS[ITEM_LANTERN].model;
    vec3 c;
    glm_vec3_center((float *)m->min, (float *)m->max, c);
    float s = ITEMS[ITEM_LANTERN].hold_size / fmaxf(m->max[1] - m->min[1], 1e-3f);
    mat4 xf;
    glm_mat4_mul((vec4 *)h->world[1], (vec4 *)h->pose[1].world[B_PALM], xf);
    /* just the fist's position; the lantern itself hangs straight down */
    vec3 fist;
    glm_mat4_mulv3(xf, (vec3){-0.03f, 0.0f, -0.07f}, 1.0f, fist);
    glm_translate_make(out, fist);
    glm_rotate(out, h->sway_x * 4.0f, (vec3){0, 0, 1});     /* swings a little */
    glm_scale(out, (vec3){s, s, s});
    glm_translate(out, (vec3){-c[0], -m->max[1], -c[2]});
}

void hands_draw(Hands *h, const HandInput *in, GLuint prog, mat4 view_proj)
{
    /* tint.a = 1 tells the shader these are the viewer's own hands (kept dry in the rain) */
    DrawParams dp = { .tint = { 0, 0, 0, 1 } };

    /* right hand */
    model_draw(&h->model, &h->pose[0], prog, view_proj, h->world[0], &dp);

    /* left hand is mirrored, which flips which side of each triangle faces out */
    glFrontFace(GL_CW);
    model_draw(&h->model, &h->pose[1], prog, view_proj, h->world[1], &dp);
    glFrontFace(GL_CCW);

    if (in->held != ITEM_NONE && ITEMS[in->held].loaded && ITEMS[in->held].kind != KIND_SPELL) {
        const ItemDef *it = &ITEMS[in->held];
        bool weapon = it->kind == KIND_WEAPON || it->kind == KIND_THROWN || in->held == ITEM_TORCH;
        mat4 grip, xf;
        grip_matrix(in->held, weapon, grip);
        glm_mat4_mul(h->world[0], h->pose[0].world[B_PALM], xf);
        glm_mat4_mul(xf, grip, xf);
        glm_mat4_mul(xf, (vec4 *)it->hold, xf);
        model_draw(&it->model, NULL, prog, view_proj, xf, &dp);
    }
    if (in->left == LEFT_TORCH) {
        const ItemDef *it = &ITEMS[ITEM_TORCH];
        mat4 grip, xf;
        grip_matrix(ITEM_TORCH, true, grip);
        glm_mat4_mul(h->world[1], h->pose[1].world[B_PALM], xf);
        glm_mat4_mul(xf, grip, xf);
        glm_mat4_mul(xf, (vec4 *)it->hold, xf);
        glFrontFace(GL_CW);
        model_draw(&it->model, NULL, prog, view_proj, xf, &dp);
        glFrontFace(GL_CCW);
    } else if (in->left == LEFT_LANTERN) {
        mat4 xf;
        lantern_matrix(h, xf);
        glFrontFace(GL_CW);
        model_draw(&ITEMS[ITEM_LANTERN].model, NULL, prog, view_proj, xf, &dp);
        glFrontFace(GL_CCW);
    }
}

void hands_torch_tip(const Hands *h, vec3 out)
{
    const ItemDef *it = &ITEMS[ITEM_TORCH];
    mat4 grip, xf;
    grip_matrix(ITEM_TORCH, true, grip);
    glm_mat4_mul((vec4 *)h->world[1], (vec4 *)h->pose[1].world[B_PALM], xf);
    glm_mat4_mul(xf, grip, xf);
    glm_mat4_mul(xf, (vec4 *)it->hold, xf);
    /* the rag head is at the top of the torch model (y ~ 0.5) */
    glm_mat4_mulv3(xf, (vec3){0, 0.5f, 0}, 1.0f, out);
}

void hands_right_pos(const Hands *h, vec3 out)
{
    glm_vec3_copy((float *)h->world[0][3], out);
}

void hands_lantern_pos(const Hands *h, vec3 out)
{
    mat4 xf;
    lantern_matrix(h, xf);
    const Model *m = &ITEMS[ITEM_LANTERN].model;
    vec3 c;
    glm_vec3_center((float *)m->min, (float *)m->max, c);
    glm_mat4_mulv3(xf, c, 1.0f, out);
}

void hands_palm_pos(const Hands *h, vec3 out)
{
    mat4 xf;
    glm_mat4_mul((vec4 *)h->world[0], (vec4 *)h->pose[0].world[B_PALM], xf);
    glm_mat4_mulv3(xf, (vec3){-0.04f, 0.0f, -0.05f}, 1.0f, out);
}
