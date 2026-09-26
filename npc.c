#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The palace's residents. Their lines live in assets/dialogue.txt and their voices in
 * assets/voices/<id>_<n>.wav (rendered by tools/make_voices.sh). Line 0 is a greeting,
 * said once when you first come close; after that each "talk" tells the next story. */

typedef struct {
    const char *id;
    const char *file;
    const char *keep[3];    /* accessories to leave visible; every other prop is hidden */
    bool helmet;
} Cast;

static const Cast CAST[7] = {
    [1] = { "king",    "Knight",       { NULL }, false },
    [2] = { "mage",    "Mage",         { "2H_Staff" }, true },
    [3] = { "guard",   "Barbarian",    { "1H_Axe", "Mug" }, true },
    [4] = { "jester",  "Rogue",        { "Throwable" }, true },
    [5] = { "pilgrim", "Rogue_Hooded", { NULL }, true },
    [6] = { "herald",  "Knight",       { "1H_Sword", "Round_Shield" }, true },
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
        if (strstr(name, "Helmet"))
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
    char buf[512];
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
        if (nf < 4 || strcmp(fields[0], n->id) != 0 || n->line_count >= MAX_LINES)
            continue;
        snprintf(n->name, sizeof n->name, "%s", fields[2]);
        snprintf(n->lines[n->line_count], sizeof n->lines[0], "%s", fields[3]);
        char path[128];
        snprintf(path, sizeof path, "assets/voices/%s_%d.wav", n->id, n->line_count);
        n->clips[n->line_count] = audio_load_clip(path);
        n->line_count++;
    }
    fclose(f);
}

void npc_add(Game *g, int number, vec3 pos)
{
    if (number < 1 || number > 6 || g->npc_count >= MAX_NPCS)
        return;
    int i = g->npc_count++;
    Npc *n = &g->npcs[i];
    memset(n, 0, sizeof *n);
    const Cast *c = &CAST[number];
    snprintf(n->id, sizeof n->id, "%s", c->id);

    char path[128];
    snprintf(path, sizeof path, "assets/kaykit/%s.glb", c->file);
    if (!model_load(&g->npc_models[i], path)) {
        g->npc_count--;
        return;
    }
    dress(&g->npc_models[i], c);
    Transform fix = { .scale = { 0.8f, 0.8f, 0.8f } };      /* KayKit people, a touch smaller */
    model_adjust(&g->npc_models[i], &fix, false);
    Rig *r = &g->npc_rigs[i];
    r->idle = model_find_anim(&g->npc_models[i], "Idle");
    r->talk = model_find_anim(&g->npc_models[i], "Interact");
    r->cheer = model_find_anim(&g->npc_models[i], "Cheer");
    n->model = i;
    n->xf = TRANSFORM_AT(pos, 0.0f);

    /* face the middle of the hall (or, outside, the path up to the door) */
    vec3 hall = { 6.0f, 0.0f, -86.0f };     /* the middle of the great hall */
    n->home_yaw = number == 6 ? 0.0f : atan2f(hall[0] - pos[0], hall[2] - pos[2]);
    if (number == 1)
        n->home_yaw = 0.0f;     /* the king faces down the hall from his throne */
    n->yaw = n->home_yaw;
    n->anim_t = frand() * 5.0f;
    pose_init(&n->pose, &g->npc_models[i]);
    load_lines(n);

    level_add_box(&g->level, (vec3){pos[0] - 0.35f, pos[1], pos[2] - 0.35f},
                  (vec3){pos[0] + 0.35f, pos[1] + 1.8f, pos[2] + 0.35f});
}

void npc_talk(Game *g, Npc *n)
{
    if (!n->line_count)
        return;
    /* the greeting first, then the stories in turn */
    int line = n->greeted ? n->next_line : 0;
    if (!n->greeted) {
        n->greeted = true;
        n->next_line = 1;
    } else {
        n->next_line = n->next_line + 1 < n->line_count ? n->next_line + 1 : (n->line_count > 1 ? 1 : 0);
    }
    vec3 mouth = { n->pos[0], n->pos[1] + 1.6f, n->pos[2] };
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

void npcs_update(Game *g, float dt)
{
    for (int i = 0; i < g->npc_count; i++) {
        Npc *n = &g->npcs[i];
        n->anim_t += dt;
        n->talk_t = fmaxf(0.0f, n->talk_t - dt);
        float d = glm_vec3_distance(n->pos, g->pos);

        /* say hello, once, when you first come close */
        if (!n->greeted && d < 4.0f && !g->dead && !audio_speaking())
            npc_talk(g, n);

        /* look at you while you're near, otherwise back to where they were facing */
        float want = d < 6.0f ? atan2f(g->pos[0] - n->pos[0], g->pos[2] - n->pos[2]) : n->home_yaw;
        n->yaw += wrap_angle(want - n->yaw) * (1.0f - expf(-dt * 4.0f));

        if (d > 50.0f)
            continue;
        const Model *m = &g->npc_models[n->model];
        const Rig *r = &g->npc_rigs[n->model];
        pose_reset(&n->pose, m);
        anim_apply(m, r->idle, n->anim_t, true, 1.0f, &n->pose);
        if (n->talk_t > 0.0f) {
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
        if (glm_vec3_distance(n->pos, g->eye) > 60.0f)
            continue;
        mat4 xf;
        transform_matrix(&n->xf, xf);
        model_draw(&g->npc_models[n->model], &n->pose, prog, vp, xf, &dp);
    }
}
