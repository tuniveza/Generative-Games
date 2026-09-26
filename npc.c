#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* People: the palace's residents, and the coast's. Their lines live in assets/dialogue.txt
 * and their voices in assets/voices/<id>_<n>.wav (rendered by tools/make_voices.sh).
 * Line 0 is a greeting, said once when you first come close; after that each "talk"
 * tells the next story. Lines tagged "id:tag" are for the quests (see quest.c).
 *
 * Everyone is a KayKit adventurer, dressed differently: which accessories they keep,
 * whether they wear their hat, a dye for their clothes, what they carry. */

/* clothes dyed a new color: saturated, non-skin pixels get their hue turned */
static void turn_hue(unsigned char *px, int w, int h, float turn, float sat_mul, float light_mul)
{
    for (int i = 0; i < w * h; i++) {
        unsigned char *p = &px[i * 4];
        float r = p[0] / 255.0f, g = p[1] / 255.0f, b = p[2] / 255.0f;
        float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b));
        float sat = mx > 0 ? (mx - mn) / mx : 0;
        bool skin = r >= g && g >= b && mx > 0.55f && sat > 0.15f && sat < 0.6f && (r - g) < 0.3f;
        if (skin || sat < 0.12f)
            continue;
        /* rotate around the grey axis */
        float c = cosf(turn), s = sinf(turn), k = (1.0f - c) / 3.0f, q = sqrtf(1.0f / 3.0f) * s;
        float nr = r * (c + k) + g * (k - q) + b * (k + q);
        float ng = r * (k + q) + g * (c + k) + b * (k - q);
        float nb = r * (k - q) + g * (k + q) + b * (c + k);
        float grey = (nr + ng + nb) / 3.0f;
        nr = (grey + (nr - grey) * sat_mul) * light_mul;
        ng = (grey + (ng - grey) * sat_mul) * light_mul;
        nb = (grey + (nb - grey) * sat_mul) * light_mul;
        p[0] = (unsigned char)(glm_clamp(nr, 0, 1) * 255);
        p[1] = (unsigned char)(glm_clamp(ng, 0, 1) * 255);
        p[2] = (unsigned char)(glm_clamp(nb, 0, 1) * 255);
    }
}
static void dye_teal(unsigned char *px, int w, int h) { turn_hue(px, w, h, 2.4f, 1.0f, 1.0f); }
static void dye_coral(unsigned char *px, int w, int h) { turn_hue(px, w, h, -0.7f, 1.1f, 1.05f); }
static void dye_gold(unsigned char *px, int w, int h) { turn_hue(px, w, h, 1.1f, 0.9f, 1.1f); }
/* the Unmoored: everything pale and cold, like something seen through deep water */
static void spectral(unsigned char *px, int w, int h)
{
    for (int i = 0; i < w * h; i++) {
        unsigned char *p = &px[i * 4];
        float l = (p[0] * 0.3f + p[1] * 0.59f + p[2] * 0.11f) / 255.0f;
        p[0] = (unsigned char)(fminf(1.0f, l * 0.55f + 0.12f) * 255);
        p[1] = (unsigned char)(fminf(1.0f, l * 0.9f + 0.2f) * 255);
        p[2] = (unsigned char)(fminf(1.0f, l * 1.0f + 0.3f) * 255);
    }
}

typedef struct {
    const char *id;
    const char *file;
    const char *keep[3];    /* accessories to leave visible; every other prop is hidden */
    bool helmet;
    void (*dye)(unsigned char *, int, int);
    NpcBehave behave;
    float size;             /* 1 = grown-up */
    ItemId carries;         /* something from the item list in the right hand */
} Cast;

static const Cast CAST[NPC_KINDS] = {
    [NPC_KING]    = { "king",    "Knight",       { NULL }, false },
    [NPC_MAGE]    = { "mage",    "Mage",         { "2H_Staff" }, true },
    [NPC_GUARD]   = { "guard",   "Barbarian",    { "1H_Axe", "Mug" }, true },
    [NPC_JESTER]  = { "jester",  "Rogue",        { "Throwable" }, true },
    [NPC_PILGRIM] = { "pilgrim", "Rogue_Hooded", { NULL }, true },
    [NPC_HERALD]  = { "herald",  "Knight",       { "1H_Sword", "Round_Shield" }, true },
    [NPC_ELDER]   = { "elder",   "Mage",         { "2H_Staff" }, false, dye_teal, BEHAVE_STAND, 0.95f },
    [NPC_SHOPKEEPER] = { "coralie", "Rogue",     { NULL }, false, dye_coral, BEHAVE_SHOP, 1.0f },
    [NPC_HARBOURMASTER] = { "quill", "Knight",   { NULL }, false, dye_teal, BEHAVE_STAND, 1.0f },
    [NPC_FISHER]  = { "brine",   "Barbarian",    { NULL }, true, dye_teal, BEHAVE_STAND, 1.0f, ITEM_ROD_DRIFTWOOD },
    [NPC_ANCIENT] = { "unmoored", "Mage",        { "Spellbook_open" }, true, spectral, BEHAVE_STAND, 1.15f },
    [NPC_TAVERN]  = { "tamsin",  "Barbarian",    { "Mug" }, false, dye_coral, BEHAVE_SHOP, 1.0f },
    [NPC_LAMPLIGHTER] = { "pell", "Rogue_Hooded", { NULL }, true, dye_gold, BEHAVE_WANDER, 1.0f, ITEM_LANTERN },
    [NPC_CHILD]   = { "wren",    "Rogue",        { NULL }, false, dye_gold, BEHAVE_SWIM, 0.62f },
    [NPC_NETMENDER] = { "hollis", "Barbarian",   { NULL }, true, NULL, BEHAVE_WANDER, 1.0f },
    [NPC_CURATOR] = { "juniper", "Mage",         { "Spellbook_open" }, false, dye_coral, BEHAVE_STAND, 1.0f },
    [NPC_SWIMMER] = { "sunny",   "Rogue",        { NULL }, false, dye_teal, BEHAVE_SWIM, 1.0f },
    [NPC_SMITH]   = { "bram",    "Barbarian",    { "1H_Axe" }, false, dye_gold, BEHAVE_SHOP, 1.05f },
};

/* body parts are never hidden; anything else attached to the rig is a prop */
static bool is_body(const char *name)
{
    static const char *parts[] = { "ArmLeft", "ArmRight", "Body", "Head", "LegLeft", "LegRight", "Cape", "Hat" };
    for (size_t i = 0; i < sizeof parts / sizeof parts[0]; i++)
        if (strstr(name, parts[i]))
            return true;
    return false;
}

static void dress(Model *m, const Cast *c)
{
    for (int i = 0; i < m->node_count; i++) {
        const char *name = m->nodes[i].name;
        if (m->nodes[i].mesh < 0)
            continue;
        bool keep = is_body(name);
        if (strstr(name, "Helmet") || strstr(name, "Hat"))
            keep = c->helmet;
        for (int k = 0; k < 3 && c->keep[k]; k++)
            if (strcmp(name, c->keep[k]) == 0)
                keep = true;
        if (!keep)
            model_hide_node(m, name);
    }
}

static void load_lines(Npc *n)
{
    FILE *f = fopen("assets/dialogue.txt", "r");
    if (!f)
        return;
    char buf[640];
    while (fgets(buf, sizeof buf, f)) {
        if (buf[0] == '#' || buf[0] == '\n')
            continue;
        buf[strcspn(buf, "\r\n")] = 0;
        char *fields[4];
        int nf = 0;
        char *p = buf;
        while (nf < 4 && p) {
            fields[nf++] = p;
            p = nf < 4 ? strchr(p, '|') : NULL;
            if (p)
                *p++ = 0;
        }
        if (nf < 4)
            continue;
        /* "elder" or "elder:tag" */
        char who[48];
        snprintf(who, sizeof who, "%s", fields[0]);
        char *tag = strchr(who, ':');
        if (tag)
            *tag++ = 0;
        if (strcmp(who, n->id) != 0 || n->line_count >= MAX_LINES)
            continue;
        snprintf(n->name, sizeof n->name, "%s", fields[2]);
        int k = n->line_count;
        snprintf(n->lines[k], sizeof n->lines[0], "%s", fields[3]);
        snprintf(n->tags[k], sizeof n->tags[0], "%s", tag ? tag : "");
        /* voices are numbered per speaker, in file order (see tools/make_voices.sh) */
        char path[160];
        if (tag)
            snprintf(path, sizeof path, "assets/voices/%s_%s.wav", n->id, tag);
        else {
            int plain = 0;
            for (int j = 0; j < k; j++)
                plain += n->tags[j][0] == 0;
            snprintf(path, sizeof path, "assets/voices/%s_%d.wav", n->id, plain);
        }
        n->clips[k] = audio_load_clip(path);
        n->line_count++;
    }
    fclose(f);
}

void npc_add(Game *g, int number, vec3 pos, float yaw)
{
    if (number < 1 || number >= NPC_KINDS || g->npc_count >= MAX_NPCS)
        return;
    int i = g->npc_count++;
    Npc *n = &g->npcs[i];
    memset(n, 0, sizeof *n);
    const Cast *c = &CAST[number];
    snprintf(n->id, sizeof n->id, "%s", c->id);
    n->kind = number;

    char path[128];
    snprintf(path, sizeof path, "assets/kaykit/%s.glb", c->file);
    ModelOptions opts = { .recolor = c->dye };
    if (!model_load_ex(&g->npc_models[i], path, &opts)) {
        g->npc_count--;
        return;
    }
    dress(&g->npc_models[i], c);
    float size = 0.8f * (c->size > 0.0f ? c->size : 1.0f);  /* KayKit people, a touch smaller */
    Transform fix = { .scale = { size, size, size } };
    model_adjust(&g->npc_models[i], &fix, false);
    Rig *r = &g->npc_rigs[i];
    r->idle = model_find_anim(&g->npc_models[i], "Idle");
    r->walk = model_find_anim(&g->npc_models[i], "Walking_A");
    r->talk = model_find_anim(&g->npc_models[i], "Interact");
    r->cheer = model_find_anim(&g->npc_models[i], "Cheer");
    r->hand_r = model_find_node(&g->npc_models[i], "handslot.r");
    n->model = i;
    n->xf = TRANSFORM_AT(pos, 0.0f);
    glm_vec3_copy(pos, n->home);
    glm_vec3_copy(pos, n->target);
    n->behave = c->behave;
    n->fade = 1.0f;

    if (number <= NPC_HERALD) {
        /* the palace: face the middle of the hall (or, outside, the path up to the door) */
        vec3 hall = { 6.0f, 0.0f, -86.0f };
        n->home_yaw = number == 6 ? 0.0f : atan2f(hall[0] - pos[0], hall[2] - pos[2]);
        if (number == 1)
            n->home_yaw = 0.0f;     /* the king faces down the hall from his throne */
    } else {
        n->home_yaw = yaw;
    }
    if (n->behave == BEHAVE_SWIM) {
        /* lengths along the shore */
        glm_vec3_copy((vec3){pos[0] + 24.0f, pos[1], pos[2] + 4.0f}, n->target);
        n->swimming = true;
    }
    n->yaw = n->home_yaw;
    n->anim_t = frand() * 5.0f;
    pose_init(&n->pose, &g->npc_models[i]);
    load_lines(n);

    if (n->behave == BEHAVE_STAND || n->behave == BEHAVE_SHOP)
        level_add_box(&g->level, (vec3){pos[0] - 0.35f, pos[1], pos[2] - 0.35f},
                      (vec3){pos[0] + 0.35f, pos[1] + 1.8f, pos[2] + 0.35f});
}

Npc *npc_find(Game *g, int kind)
{
    for (int i = 0; i < g->npc_count; i++)
        if (g->npcs[i].kind == kind)
            return &g->npcs[i];
    return NULL;
}

bool npc_has_line(const Npc *n, const char *tag)
{
    for (int i = 0; i < n->line_count; i++)
        if (strcmp(n->tags[i], tag) == 0)
            return true;
    return false;
}

/* the Unmoored only shows himself on clear nights, or to someone looking hard (the
 * magnifying glass), and even then only when you're close */
bool npc_visible(const Game *g, const Npc *n)
{
    if (n->gone_t > 0.0f)
        return false;
    if (n->kind != NPC_ANCIENT)
        return true;
    bool looking = g->slots[g->selected].id == ITEM_MAGNIFIER;
    bool quiet = g->night && g->weather.rain < 0.3f;
    return (quiet || looking) && glm_vec3_distance((float *)n->pos, (float *)g->pos) < (looking ? 40.0f : 16.0f);
}

static void speak(Game *g, Npc *n, int line)
{
    vec3 mouth = { n->pos[0], n->pos[1] + (n->swimming ? 0.2f : 1.6f), n->pos[2] };
    audio_speak(n->clips[line], mouth);
    float len = audio_clip_length(n->clips[line]);
    if (len <= 0.0f)
        len = 1.5f + strlen(n->lines[line]) * 0.06f;
    n->talk_t = len;
    for (int k = 0; k < g->npc_count; k++)
        if (&g->npcs[k] != n)
            g->npcs[k].talk_t = 0.0f;       /* only one voice at a time */
    snprintf(g->subtitle_name, sizeof g->subtitle_name, "%s", n->name);
    snprintf(g->subtitle, sizeof g->subtitle, "%s", n->lines[line]);
    g->subtitle_t = len + 1.0f;
}

void npc_say(Game *g, Npc *n, const char *tag)
{
    for (int i = 0; i < n->line_count; i++)
        if (strcmp(n->tags[i], tag) == 0) {
            speak(g, n, i);
            n->greeted = true;
            return;
        }
    npc_talk(g, n);
}

void npc_talk(Game *g, Npc *n)
{
    /* everyday lines only: greeting first, then the stories in turn */
    int plain[MAX_LINES], count = 0;
    for (int i = 0; i < n->line_count; i++)
        if (!n->tags[i][0])
            plain[count++] = i;
    if (!count)
        return;
    int line;
    if (!n->greeted) {
        n->greeted = true;
        n->next_line = count > 1 ? 1 : 0;
        line = plain[0];
    } else {
        line = plain[n->next_line];
        n->next_line = n->next_line + 1 < count ? n->next_line + 1 : (count > 1 ? 1 : 0);
    }
    speak(g, n, line);
}

void npcs_flee_eruption(Game *g)
{
    for (int i = 0; i < g->npc_count; i++) {
        Npc *n = &g->npcs[i];
        if (n->kind <= NPC_HERALD || n->gone_t > 0.0f)
            continue;       /* the palace's stone walls keep its people safe */
        n->gone_t = 1.0f;   /* back when the volcano's rebirth clears it */
        fx_dust(&g->ps, (vec3){n->pos[0], n->pos[1] + 1.0f, n->pos[2]}, 10);
    }
}

/* strolling about, or swimming lengths */
static void npc_move(Game *g, Npc *n, float dt, float d)
{
    if (n->talk_t > 0.0f || d < 3.0f)
        return;             /* stops to talk */
    vec3 to;
    glm_vec3_sub(n->target, n->pos, to);
    to[1] = 0.0f;
    float len = glm_vec3_norm(to);
    if (n->behave == BEHAVE_WANDER) {
        if (len < 0.4f) {
            n->wander_t -= dt;
            if (n->wander_t <= 0.0f) {
                float a = frand() * 6.2832f, r = 2.0f + frand() * 9.0f;
                glm_vec3_copy((vec3){n->home[0] + cosf(a) * r, n->home[1], n->home[2] + sinf(a) * r}, n->target);
                n->wander_t = 3.0f + frand() * 6.0f;
            }
            return;
        }
        float speed = 1.3f;
        vec3 before;
        glm_vec3_copy(n->pos, before);
        glm_vec3_muladds(to, speed * dt / len, n->pos);
        level_collide(&g->level, n->pos, 0.35f, 1.7f);
        if (glm_vec3_distance(before, n->pos) < speed * dt * 0.3f)
            glm_vec3_copy(n->pos, n->target);       /* blocked: pick somewhere else */
        n->pos[1] = ground_at(g, n->pos[0], n->pos[2], n->pos[1]);
        n->yaw += wrap_angle(atan2f(to[0], to[2]) - n->yaw) * (1.0f - expf(-dt * 6.0f));
    } else if (n->behave == BEHAVE_SWIM) {
        if (len < 1.0f) {
            /* turn round and swim back */
            vec3 t;
            glm_vec3_copy(n->home, t);
            glm_vec3_copy(n->target, n->home);
            glm_vec3_copy(t, n->target);
            return;
        }
        glm_vec3_muladds(to, 1.4f * dt / len, n->pos);
        n->yaw += wrap_angle(atan2f(to[0], to[2]) - n->yaw) * (1.0f - expf(-dt * 3.0f));
        float surface, depth;
        if (sea_at(g, n->pos[0], n->pos[2], &surface, &depth))
            n->pos[1] = surface - 0.35f;
    }
}

void npcs_update(Game *g, float dt)
{
    for (int i = 0; i < g->npc_count; i++) {
        Npc *n = &g->npcs[i];
        n->anim_t += dt;
        n->talk_t = fmaxf(0.0f, n->talk_t - dt);
        if (n->gone_t > 0.0f)
            continue;
        float d = glm_vec3_distance(n->pos, g->pos);

        /* say hello, once, when you first come close */
        if (!n->greeted && d < 4.0f && !g->dead && !audio_speaking() && npc_visible(g, n))
            npc_talk(g, n);

        npc_move(g, n, dt, d);
        bool walking = n->behave == BEHAVE_WANDER && n->talk_t <= 0.0f && d >= 3.0f &&
                       glm_vec3_distance(n->pos, n->target) > 0.4f;

        /* look at you while you're near, otherwise back to where they were facing */
        if (!walking && !(n->behave == BEHAVE_SWIM && d >= 3.0f)) {
            float want = d < 6.0f ? atan2f(g->pos[0] - n->pos[0], g->pos[2] - n->pos[2]) : n->home_yaw;
            n->yaw += wrap_angle(want - n->yaw) * (1.0f - expf(-dt * 4.0f));
        }

        if (d > 70.0f)
            continue;
        const Model *m = &g->npc_models[n->model];
        const Rig *r = &g->npc_rigs[n->model];
        pose_reset(&n->pose, m);
        if (n->dancing || n->cheering)
            anim_apply(m, r->cheer, n->anim_t * 1.2f, true, 1.0f, &n->pose);
        else if (walking || n->swimming)
            anim_apply(m, r->walk, n->anim_t * (n->swimming ? 0.7f : 1.0f), true, 1.0f, &n->pose);
        else
            anim_apply(m, r->idle, n->anim_t, true, 1.0f, &n->pose);
        if (n->talk_t > 0.0f && !n->swimming) {
            /* gestures while talking: the "interact" motion, softened and repeated */
            float len = r->talk >= 0 ? m->anims[r->talk].duration : 1.0f;
            anim_apply(m, r->talk, fmodf(n->anim_t, len * 1.6f), false, 0.55f, &n->pose);
        }
        pose_update(m, &n->pose);
    }
}

void npcs_draw(Game *g, GLuint prog, mat4 vp, bool depth)
{
    DrawParams dp = { .depth_only = depth };
    for (int i = 0; i < g->npc_count; i++) {
        Npc *n = &g->npcs[i];
        if (glm_vec3_distance(n->pos, g->eye) > 70.0f || !npc_visible(g, n))
            continue;
        Transform t = n->xf;
        if (n->swimming) {
            t.pitch = 1.2f;         /* lying along the water */
            t.pos[1] -= 0.4f;
        }
        if (n->dancing)
            t.pos[1] += fabsf(sinf(n->anim_t * 6.0f)) * 0.15f;
        mat4 xf;
        transform_matrix(&t, xf);
        DrawParams np = dp;
        if (n->kind == NPC_ANCIENT && !depth) {
            float flicker = 0.7f + 0.3f * sinf(g->time * 3.1f) * sinf(g->time * 1.3f);
            glm_vec4_copy((vec4){0.05f * flicker, 0.18f * flicker, 0.25f * flicker, 0.0f}, np.tint);
        }
        if (n->kind == NPC_ANCIENT && depth)
            continue;               /* he casts no shadow */
        model_draw(&g->npc_models[n->model], &n->pose, prog, vp, xf, &np);

        /* what they carry */
        ItemId carries = CAST[n->kind].carries;
        int hand = g->npc_rigs[n->model].hand_r;
        if (carries != ITEM_NONE && ITEMS[carries].loaded && hand >= 0) {
            mat4 body, hx;
            model_matrix(&g->npc_models[n->model], xf, body);
            glm_mat4_mul(body, n->pose.world[hand], hx);
            glm_rotate_y(hx, GLM_PIf, hx);
            glm_scale_uni(hx, 1.25f / 0.8f);
            glm_mat4_mul(hx, (vec4 *)ITEMS[carries].hold, hx);
            model_draw(&ITEMS[carries].model, NULL, prog, vp, hx, &dp);
        }
    }
}
