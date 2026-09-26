#include "game.h"
#include "meshgen.h"

#include <stdlib.h>
#include <string.h>

/* Spells (learned from tomes found in the crypt's tombs), magic projectiles, the
 * light wisp, and the tombs themselves. */

/* ---------- tombs ---------- */

void tombs_build_models(Game *g)
{
    /* a stone sarcophagus with glowing runes carved around it, and its lid */
    Material stone, runes, gold;
    material_from_dir(&stone, "assets/textures/sandstone_blocks_08");
    material_color(&runes, 0.05f, 0.08f, 0.12f, 0.4f, 0.0f);
    glm_vec3_copy((vec3){0.4f, 1.6f, 4.0f}, runes.emissive);
    material_color(&gold, 0.9f, 0.62f, 0.22f, 0.3f, 1.0f);

    MeshBuilder base[2], lid[3];
    for (int i = 0; i < 2; i++)
        mb_init(&base[i]);
    for (int i = 0; i < 3; i++)
        mb_init(&lid[i]);

    /* local space: long along z, the head end at -z */
    mat4 xf;
    glm_translate_make(xf, (vec3){0, 0.08f, 0});
    mb_box(&base[0], xf, (vec3){0.62f, 0.08f, 1.18f}, 1.5f);          /* plinth */
    glm_translate_make(xf, (vec3){0, 0.5f, 0});
    mb_box(&base[0], xf, (vec3){0.55f, 0.36f, 1.1f}, 1.5f);           /* chest */
    for (int side = -1; side <= 1; side += 2) {
        for (int k = 0; k < 5; k++) {
            /* a row of rune marks on each long side */
            glm_translate_make(xf, (vec3){side * 0.555f, 0.5f + (k % 2) * 0.08f, -0.8f + k * 0.4f});
            glm_rotate(xf, GLM_PI_4f * (k % 3), (vec3){1, 0, 0});
            mb_box(&base[1], xf, (vec3){0.006f, 0.07f, 0.03f}, 1.0f);
        }
    }
    glm_translate_make(xf, (vec3){0, 0.03f, 0});
    mb_box(&lid[0], xf, (vec3){0.62f, 0.07f, 1.18f}, 1.5f);           /* slab */
    glm_translate_make(xf, (vec3){0, 0.13f, 0.1f});
    mb_box(&lid[0], xf, (vec3){0.3f, 0.06f, 0.75f}, 1.5f);            /* a carved figure's shape */
    glm_translate_make(xf, (vec3){0, 0.14f, -0.72f});
    mb_capsule(&lid[0], xf, 0.16f, 0.0f, 14);                          /* its head */
    glm_translate_make(xf, (vec3){0, 0.101f, -0.1f});
    mb_box(&lid[1], xf, (vec3){0.04f, 0.002f, 0.4f}, 1.0f);            /* a glowing sword rune */
    glm_translate_make(xf, (vec3){0, 0.101f, -0.3f});
    mb_box(&lid[1], xf, (vec3){0.16f, 0.002f, 0.03f}, 1.0f);
    for (int side = -1; side <= 1; side += 2) {
        glm_translate_make(xf, (vec3){side * 0.6f, 0.03f, 0});
        mb_box(&lid[2], xf, (vec3){0.025f, 0.075f, 1.19f}, 1.0f);      /* gilded edges */
    }

    Material bm[2] = { stone, runes }, lm[3] = { stone, runes, gold };
    model_from_builders(&g->tomb_model, base, bm, 2);
    model_from_builders(&g->tomb_lid_model, lid, lm, 3);
    for (int i = 0; i < 2; i++)
        mb_free(&base[i]);
    for (int i = 0; i < 3; i++)
        mb_free(&lid[i]);
}

static const char *spell_word(ItemId id)
{
    switch (id) {
    case ITEM_TOME_GILLS: return "Gills";
    case ITEM_TOME_TIDE: return "Tides";
    case ITEM_TOME_METEOR: return "Meteors";
    case ITEM_TOME_SPIKES: return "Thorns";
    case ITEM_TOME_SHADOW: return "Shadows";
    case ITEM_TOME_GALE: return "Gales";
    case ITEM_TOME_MISSILES: return "Seeking Stars";
    case ITEM_TOME_FIREBALL: return "Fireball";
    case ITEM_TOME_FROST: return "Frost";
    case ITEM_TOME_LIGHTNING: return "Storms";
    case ITEM_TOME_HEAL: return "Mending";
    case ITEM_TOME_WISP: return "the Wisp";
    case ITEM_TOME_BLINK: return "Blinking";
    default: return "?";
    }
}

void tomb_open(Game *g, Tomb *t)
{
    if (t->opened)
        return;
    t->opened = true;
    audio_play_at(SFX_TOMB, t->pos, 1.0f);
    vec3 at = { t->pos[0], t->pos[1] + 1.0f, t->pos[2] };
    fx_magic(&g->ps, at, (vec3){1.0f, 3.0f, 6.0f}, 0.8f, 60);

    if (t->gift != ITEM_NONE) {
        game_give(g, t->gift, ITEMS[t->gift].kind == KIND_SPELL ? 1 : 2);
        if (ITEMS[t->gift].kind == KIND_SPELL)
            message(g, COL_MAGIC, "Cold light rises from the tomb. You learn the spell of %s!", spell_word(t->gift));
        else
            message(g, COL_MAGIC, "Among the old bones: %s x2.", ITEMS[t->gift].name);
    }
    if (t->great) {
        game_give(g, ITEM_MANA_POTION, 3);
        message(g, COL_GOLD, "The great tomb held a king's ransom of blue vials.");
    }

    /* the dead nearby don't like being disturbed */
    int woke = 0;
    for (int i = 0; i < MAX_CREATURES; i++) {
        Creature *c = &g->creatures[i];
        if (c->used && c->type == CR_SKELETON && c->state == ST_DORMANT &&
            glm_vec3_distance(c->pos, t->pos) < 14.0f) {
            c->state = ST_AWAKEN;
            c->state_t = -frand() * 0.8f;
            woke++;
        }
    }
    if (woke)
        message(g, COL_RED, "Bones stir in the dark...");
}

/* ---------- projectiles ---------- */

void bolt_fire(Game *g, BoltKind kind, vec3 from, vec3 vel, bool hostile)
{
    for (int i = 0; i < MAX_BOLTS; i++) {
        Bolt *b = &g->bolts[i];
        if (b->used)
            continue;
        *b = (Bolt){ .used = true, .kind = kind, .life = kind == BOLT_METEOR ? 6.0f : 3.0f, .hostile = hostile, .target = -1 };
        glm_vec3_copy(from, b->pos);
        glm_vec3_copy(vel, b->vel);
        return;
    }
}

static void bolt_impact(Game *g, Bolt *b)
{
    b->used = false;
    if (b->kind == BOLT_EMBER) {
        /* a coal bursting where it lands */
        fx_flame(&g->ps, b->pos, 3.0f, 0.1f);
        fx_sparks(&g->ps, b->pos, 16);
        audio_play_at(SFX_LAVA_BOMB, b->pos, 0.5f);
        if (!g->dead && glm_vec3_distance(b->pos, (vec3){g->pos[0], g->pos[1] + 0.8f, g->pos[2]}) < 2.6f)
            hurt_player(g, b->pos, 9.0f);
        return;
    }
    if (b->kind == BOLT_METEOR) {
        explode(g, b->pos, ITEMS[ITEM_TOME_METEOR].damage, ITEMS[ITEM_TOME_METEOR].range, false);
        for (int i = 0; i < MAX_CREATURES; i++) {
            Creature *c = &g->creatures[i];
            if (c->used && c->state != ST_DEAD && glm_vec3_distance(c->pos, b->pos) < 5.0f)
                c->burn_t = 4.0f;
        }
        return;
    }
    if (b->kind == BOLT_HARPOON) {
        /* the harpoon stays where it struck: go and pick it up */
        vec3 at = { b->pos[0], ground_at(g, b->pos[0], b->pos[2], b->pos[1] + 0.3f) + 0.05f, b->pos[2] };
        spawn_pickup(g, ITEM_HARPOON, 1, at);
        audio_play_at(SFX_HIT, b->pos, 0.8f);
        return;
    }
    if (b->kind == BOLT_FIREBALL) {
        explode(g, b->pos, ITEMS[ITEM_TOME_FIREBALL].damage, ITEMS[ITEM_TOME_FIREBALL].range, false);
        for (int i = 0; i < MAX_CREATURES; i++) {
            Creature *c = &g->creatures[i];
            if (c->used && c->state != ST_DEAD && glm_vec3_distance(c->pos, b->pos) < 3.5f)
                c->burn_t = 3.0f;       /* set alight */
        }
    } else {
        fx_magic(&g->ps, b->pos, (vec3){4.0f, 1.0f, 6.0f}, 0.2f, 30);
        audio_play_at(SFX_MAGIC_HIT, b->pos, 0.9f);
    }
}

static void bolts_update(Game *g, float dt)
{
    for (int i = 0; i < MAX_BOLTS; i++) {
        Bolt *b = &g->bolts[i];
        if (!b->used)
            continue;
        vec3 next;
        glm_vec3_copy(b->pos, next);
        glm_vec3_muladds(b->vel, dt, next);
        b->life -= dt;

        /* thrown things fall; seeking stars turn toward their prey */
        if (b->kind == BOLT_EMBER || b->kind == BOLT_HARPOON)
            b->vel[1] -= 9.8f * dt;
        if (b->kind == BOLT_STAR && b->target >= 0) {
            Creature *c = &g->creatures[b->target];
            if (c->used && c->state != ST_DEAD) {
                vec3 to = { c->pos[0], c->pos[1] + creature_height(c) * 0.5f, c->pos[2] };
                glm_vec3_sub(to, b->pos, to);
                glm_vec3_normalize(to);
                glm_vec3_scale(to, 16.0f, to);
                glm_vec3_lerp(b->vel, to, 1.0f - expf(-dt * 5.0f), b->vel);
            }
        }
        /* trail */
        if (b->kind == BOLT_FIREBALL)
            fx_flame(&g->ps, b->pos, 0.9f, dt);
        else if (b->kind == BOLT_EMBER)
            fx_flame(&g->ps, b->pos, 0.5f, dt);
        else if (b->kind == BOLT_METEOR)
            fx_flame(&g->ps, b->pos, 4.0f, dt);
        else if (b->kind == BOLT_STAR)
            fx_magic(&g->ps, b->pos, (vec3){4.0f, 3.0f, 6.5f}, 0.05f, 3);
        else if (b->kind != BOLT_HARPOON)
            fx_magic(&g->ps, b->pos, (vec3){3.0f, 0.8f, 5.0f}, 0.08f, 2);

        bool hit = b->life <= 0.0f || !level_line_clear(&g->level, b->pos, next) ||
                   next[1] < ground_at(g, next[0], next[2], next[1] + 0.5f);
        if (b->kind == BOLT_METEOR && next[1] > b->pos[1] - 0.001f && !hit)
            hit = false;
        if (!hit && b->hostile) {
            vec3 body = { g->pos[0], g->pos[1] + 1.0f, g->pos[2] };
            if (!g->dead && glm_vec3_distance(next, body) < 0.8f) {
                hurt_player(g, b->pos, b->kind == BOLT_EMBER ? 6.0f : 10.0f);
                hit = true;
            }
        }
        if (!hit && !b->hostile && b->kind != BOLT_METEOR) {
            for (int k = 0; k < MAX_CREATURES; k++) {
                Creature *c = &g->creatures[k];
                if (!c->used || c->state == ST_DEAD)
                    continue;
                vec3 mid = { c->pos[0], c->pos[1] + creature_height(c) * 0.5f, c->pos[2] };
                if (glm_vec3_distance(next, mid) < creature_radius(c) + 0.5f) {
                    hit = true;
                    if (b->kind == BOLT_STAR || b->kind == BOLT_HARPOON) {
                        float dmg = b->kind == BOLT_STAR ? ITEMS[ITEM_TOME_MISSILES].damage : ITEMS[ITEM_HARPOON].damage * (creature_aquatic(c) ? 2.0f : 1.0f);
                        vec3 push;
                        glm_vec3_normalize_to(b->vel, push);
                        glm_vec3_scale(push, 3.0f, push);
                        damage_creature(g, c, dmg, push);
                    }
                    break;
                }
            }
        }
        glm_vec3_copy(next, b->pos);
        if (hit)
            bolt_impact(g, b);
    }
}

/* ---------- casting ---------- */

static Creature *nearest(Game *g, vec3 from, vec3 dir, float range, float min_dot, Creature **skip, int nskip)
{
    Creature *best = NULL;
    float bd = range;
    for (int i = 0; i < MAX_CREATURES; i++) {
        Creature *c = &g->creatures[i];
        if (!c->used || c->state == ST_DEAD)
            continue;
        bool skipped = false;
        for (int k = 0; k < nskip; k++)
            if (skip[k] == c)
                skipped = true;
        if (skipped)
            continue;
        vec3 mid = { c->pos[0], c->pos[1] + creature_height(c) * 0.5f, c->pos[2] };
        vec3 d;
        glm_vec3_sub(mid, from, d);
        float dist = glm_vec3_norm(d);
        if (dist > bd)
            continue;
        if (dir && glm_vec3_dot(d, dir) / fmaxf(dist, 1e-3f) < min_dot)
            continue;
        if (!level_line_clear(&g->level, from, mid))
            continue;
        bd = dist;
        best = c;
    }
    return best;
}

void magic_cast(Game *g, Spell spell)
{
    vec3 palm, dir;
    hands_palm_pos(&g->hands, palm);
    look_dir(g, dir);

    switch (spell) {
    case SPELL_FIREBALL: {
        vec3 from, vel;
        glm_vec3_copy(g->eye, from);
        glm_vec3_muladds(dir, 0.6f, from);
        glm_vec3_scale(dir, 18.0f, vel);
        bolt_fire(g, BOLT_FIREBALL, from, vel, false);
        audio_play(SFX_CAST_FIRE, 1.0f);
        break;
    }
    case SPELL_FROST: {
        fx_frost_ring(&g->ps, g->pos, ITEMS[ITEM_TOME_FROST].range);
        audio_play(SFX_CAST_FROST, 1.0f);
        int frozen = 0;
        for (int i = 0; i < MAX_CREATURES; i++) {
            Creature *c = &g->creatures[i];
            if (!c->used || c->state == ST_DEAD)
                continue;
            float d = glm_vec3_distance(c->pos, g->pos);
            if (d < ITEMS[ITEM_TOME_FROST].range) {
                vec3 push;
                glm_vec3_sub(c->pos, g->pos, push);
                push[1] = 0;
                glm_vec3_normalize(push);
                glm_vec3_scale(push, 3.0f, push);
                damage_creature(g, c, ITEMS[ITEM_TOME_FROST].damage, push);
                if (c->used && c->state != ST_DEAD) {
                    c->frozen_t = 4.0f;
                    frozen++;
                }
            }
        }
        if (frozen)
            message(g, COL_MAGIC, "Frost locks %d creature%s in ice.", frozen, frozen > 1 ? "s" : "");
        break;
    }
    case SPELL_LIGHTNING: {
        /* strike the nearest foe in front, then leap to two more */
        Creature *hit[3] = { 0 };
        vec3 from;
        glm_vec3_copy(palm, from);
        hit[0] = nearest(g, g->eye, dir, ITEMS[ITEM_TOME_LIGHTNING].range, 0.6f, NULL, 0);
        if (!hit[0]) {
            /* nothing to strike: the bolt cracks into the air ahead */
            vec3 end;
            glm_vec3_copy(g->eye, end);
            glm_vec3_muladds(dir, 12.0f, end);
            fx_bolt(&g->ps, from, end, (vec3){5.0f, 6.0f, 12.0f});
        }
        for (int k = 0; k < 3 && hit[k]; k++) {
            Creature *c = hit[k];
            vec3 mid = { c->pos[0], c->pos[1] + creature_height(c) * 0.6f, c->pos[2] };
            fx_bolt(&g->ps, from, mid, (vec3){5.0f, 6.0f, 12.0f});
            vec3 push;
            glm_vec3_sub(mid, from, push);
            push[1] = 0;
            glm_vec3_normalize(push);
            glm_vec3_scale(push, 2.0f, push);
            damage_creature(g, c, ITEMS[ITEM_TOME_LIGHTNING].damage * (k ? 0.75f : 1.0f), push);
            glm_vec3_copy(mid, from);
            if (k < 2)
                hit[k + 1] = nearest(g, mid, NULL, 9.0f, 0.0f, hit, k + 1);
        }
        g->flash_t = 0.25f;
        glm_vec3_copy(palm, g->flash_pos);
        audio_play(SFX_CAST_LIGHTNING, 1.0f);
        break;
    }
    case SPELL_HEAL:
        g->hp = fminf(g->max_hp, g->hp + ITEMS[ITEM_TOME_HEAL].heal);
        fx_magic(&g->ps, (vec3){g->pos[0], g->pos[1] + 1.0f, g->pos[2]}, (vec3){1.5f, 5.0f, 1.2f}, 0.7f, 50);
        audio_play(SFX_CAST_HEAL, 1.0f);
        message(g, COL_MAGIC, "Warm light closes your wounds.");
        break;
    case SPELL_WISP:
        g->wisp = true;
        g->wisp_t = 60.0f;
        glm_vec3_copy(palm, g->wisp_pos);
        audio_play(SFX_CAST_WISP, 1.0f);
        message(g, COL_MAGIC, "A wisp of light blinks into being and follows you.");
        break;
    case SPELL_BLINK: {
        /* step forward along the ground until something is in the way */
        vec3 flat, p, last;
        flat_forward(g, flat);
        glm_vec3_copy(g->pos, p);
        glm_vec3_copy(p, last);
        fx_magic(&g->ps, (vec3){p[0], p[1] + 1.0f, p[2]}, (vec3){5.0f, 1.5f, 6.0f}, 0.5f, 40);
        for (float d = 0.25f; d <= ITEMS[ITEM_TOME_BLINK].range; d += 0.25f) {
            vec3 q;
            glm_vec3_copy(g->pos, q);
            glm_vec3_muladds(flat, d, q);
            q[1] = ground_at(g, q[0], q[2], p[1] + 0.6f);
            if (q[1] < p[1] - 3.0f)
                q[1] = p[1];            /* don't drop down holes */
            vec3 test;
            glm_vec3_copy(q, test);
            if (level_collide(&g->level, test, 0.35f, 1.75f))
                break;
            glm_vec3_copy(q, last);
            glm_vec3_copy(q, p);
        }
        glm_vec3_copy(last, g->pos);
        g->vel[1] = 0;
        fx_magic(&g->ps, (vec3){last[0], last[1] + 1.0f, last[2]}, (vec3){5.0f, 1.5f, 6.0f}, 0.5f, 40);
        audio_play(SFX_BLINK, 1.0f);
        break;
    }
    case SPELL_GILLS:
        g->gills_t = 60.0f;
        g->stamina = g->max_stamina;
        g->breath = g->max_breath;
        fx_magic(&g->ps, g->head, (vec3){0.6f, 3.5f, 4.5f}, 0.5f, 50);
        audio_play(SFX_CAST_GILLS, 1.0f);
        message(g, COL_MAGIC, "Cool water floods your lungs, and it's fine. It's lovely, even. (a minute)");
        break;
    case SPELL_TIDE: {
        /* a wave rolling out ahead: knocks everything over, douses fire */
        vec3 flat;
        flat_forward(g, flat);
        for (int k = 0; k < 120; k++) {
            Particle p = {0};
            float side = (frand() - 0.5f) * 2.0f;
            glm_vec3_copy((vec3){g->pos[0] + flat[0] + flat[2] * side, g->pos[1] + 0.3f + frand() * 1.2f, g->pos[2] + flat[2] - flat[0] * side}, p.pos);
            glm_vec3_copy((vec3){flat[0] * (8.0f + frand() * 6.0f) + flat[2] * side * 3.0f, 1.0f + frand() * 2.0f,
                                 flat[2] * (8.0f + frand() * 6.0f) - flat[0] * side * 3.0f}, p.vel);
            glm_vec4_copy((vec4){0.6f, 0.95f, 1.0f, 0.7f}, p.color0);
            glm_vec4_copy((vec4){0.8f, 1.0f, 1.0f, 0.0f}, p.color1);
            p.size0 = 0.25f;
            p.size1 = 0.6f;
            p.max_life = p.life = 0.9f;
            p.gravity = 6.0f;
            p.drag = 0.8f;
            particles_emit(&g->ps, &p);
        }
        audio_play(SFX_CAST_TIDE, 1.0f);
        for (int i = 0; i < MAX_CREATURES; i++) {
            Creature *c = &g->creatures[i];
            if (!c->used || c->state == ST_DEAD)
                continue;
            vec3 d;
            glm_vec3_sub(c->pos, g->pos, d);
            d[1] = 0;
            float len = glm_vec3_norm(d);
            if (len > ITEMS[ITEM_TOME_TIDE].range || (len > 1.0f && glm_vec3_dot(d, flat) / len < 0.4f))
                continue;
            vec3 push;
            glm_vec3_scale(flat, ITEMS[ITEM_TOME_TIDE].knockback, push);
            c->burn_t = 0.0f;
            damage_creature(g, c, ITEMS[ITEM_TOME_TIDE].damage * (c->type == CR_IMP ? 3.0f : 1.0f), push);
        }
        break;
    }
    case SPELL_METEOR: {
        /* where you're looking, up to 45 paces off, a stone falls */
        vec3 at;
        glm_vec3_copy(g->head, at);
        for (float d = 1.0f; d < 45.0f; d += 0.5f) {
            vec3 q;
            glm_vec3_copy(g->head, q);
            glm_vec3_muladds(dir, d, q);
            if (q[1] < ground_at(g, q[0], q[2], q[1] + 0.5f) || !level_line_clear(&g->level, g->head, q))
                break;
            glm_vec3_copy(q, at);
        }
        vec3 from = { at[0] + 12.0f, at[1] + 50.0f, at[2] - 8.0f }, vel;
        glm_vec3_sub(at, from, vel);
        glm_vec3_scale(vel, 1.0f / 1.6f, vel);
        bolt_fire(g, BOLT_METEOR, from, vel, false);
        audio_play(SFX_CAST_METEOR, 1.0f);
        message(g, COL_MAGIC, "The sky splits, and something burning falls out of it.");
        break;
    }
    case SPELL_SPIKES:
    case SPELL_GALE:
        for (int i = 0; i < MAX_EFFECTS; i++) {
            SpellEffect *e = &g->effects[i];
            if (e->used)
                continue;
            *e = (SpellEffect){ .used = true, .kind = spell == SPELL_GALE ? 0 : 1, .life = spell == SPELL_GALE ? 5.0f : 1.2f };
            flat_forward(g, e->dir);
            glm_vec3_copy(g->pos, e->pos);
            if (spell == SPELL_GALE) {
                glm_vec3_muladds(e->dir, 8.0f, e->pos);
                e->pos[1] = ground_at(g, e->pos[0], e->pos[2], g->pos[1] + 1.0f);
            }
            break;
        }
        audio_play(spell == SPELL_GALE ? SFX_CAST_GALE : SFX_CAST_SPIKES, 1.0f);
        break;
    case SPELL_SHADOW:
        g->shadow_t = 10.0f;
        fx_magic(&g->ps, (vec3){g->pos[0], g->pos[1] + 1.0f, g->pos[2]}, (vec3){0.6f, 0.3f, 1.2f}, 0.8f, 60);
        audio_play(SFX_CAST_SHADOW, 1.0f);
        message(g, COL_MAGIC, "You fold yourself into the shadows. For ten seconds, nothing can find you.");
        break;
    case SPELL_MISSILES: {
        Creature *skip[3] = { 0 };
        for (int k = 0; k < 3; k++) {
            Creature *c = nearest(g, g->head, NULL, ITEMS[ITEM_TOME_MISSILES].range, 0.0f, skip, k);
            skip[k] = c;
            vec3 vel;
            glm_vec3_scale(dir, 10.0f, vel);
            vel[0] += (k - 1) * 3.0f;
            vel[1] += 3.0f;
            bolt_fire(g, BOLT_STAR, palm, vel, false);
            for (int b = MAX_BOLTS - 1; b >= 0; b--)
                if (g->bolts[b].used && g->bolts[b].kind == BOLT_STAR && g->bolts[b].target < 0) {
                    g->bolts[b].target = c ? (int)(c - g->creatures) : -1;
                    break;
                }
        }
        audio_play(SFX_CAST_STARS, 1.0f);
        break;
    }
    default:
        break;
    }
}

static void effects_update(Game *g, float dt)
{
    for (int i = 0; i < MAX_EFFECTS; i++) {
        SpellEffect *e = &g->effects[i];
        if (!e->used)
            continue;
        float before = e->t;
        e->t += dt;
        if (e->kind == 0) {
            /* the whirlwind wanders ahead, lifting and flinging whatever it catches */
            glm_vec3_muladds(e->dir, dt * 1.5f, e->pos);
            e->pos[1] = ground_at(g, e->pos[0], e->pos[2], e->pos[1] + 1.0f);
            for (int k = 0; k < 6; k++) {
                Particle p = {0};
                float a = frand() * 6.2832f, r = 0.5f + frand() * 2.5f;
                glm_vec3_copy((vec3){e->pos[0] + cosf(a) * r, e->pos[1] + frand() * 4.0f, e->pos[2] + sinf(a) * r}, p.pos);
                glm_vec3_copy((vec3){-sinf(a) * 9.0f, 3.0f + frand() * 3.0f, cosf(a) * 9.0f}, p.vel);
                glm_vec4_copy((vec4){0.6f, 0.9f, 0.7f, 0.4f}, p.color0);
                glm_vec4_copy((vec4){0.7f, 0.8f, 0.7f, 0.0f}, p.color1);
                p.size0 = 0.3f;
                p.size1 = 1.2f;
                p.max_life = p.life = 0.8f;
                p.drag = 0.3f;
                particles_emit(&g->ps, &p);
            }
            for (int c = 0; c < MAX_CREATURES; c++) {
                Creature *cr = &g->creatures[c];
                if (!cr->used || cr->state == ST_DEAD || glm_vec3_distance(cr->pos, e->pos) > 5.0f)
                    continue;
                vec3 around = { -(cr->pos[2] - e->pos[2]), 0, cr->pos[0] - e->pos[0] };
                glm_vec3_muladds(around, dt * 3.0f, cr->knock);
                cr->pos[1] += dt * 2.0f;
                cr->hp -= ITEMS[ITEM_TOME_GALE].damage * dt * 0.4f;
                cr->hit_flash = 0.05f;
                if (cr->hp <= 0.0f)
                    damage_creature(g, cr, 1.0f, (vec3){0, 0, 0});
            }
        } else {
            /* stone spikes bursting up in a line, one after another */
            int from = (int)(before / e->life * 14.0f), to = (int)(e->t / e->life * 14.0f);
            for (int k = from; k < to && k < 14; k++) {
                vec3 at;
                glm_vec3_copy(e->pos, at);
                glm_vec3_muladds(e->dir, 1.5f + k * 0.9f, at);
                at[1] = ground_at(g, at[0], at[2], e->pos[1] + 1.0f);
                fx_dust(&g->ps, at, 3);
                audio_play_at(SFX_HIT_HEAVY, at, 0.4f);
                for (int c = 0; c < MAX_CREATURES; c++) {
                    Creature *cr = &g->creatures[c];
                    if (cr->used && cr->state != ST_DEAD && glm_vec3_distance(cr->pos, at) < 1.6f)
                        damage_creature(g, cr, ITEMS[ITEM_TOME_SPIKES].damage, (vec3){0, 0, 0}), cr->knock[1] += 4.0f;
                }
            }
        }
        if (e->t >= e->life + (e->kind == 1 ? 1.5f : 0.0f))
            e->used = false;
    }
}

static Model spike_model;

void spells_draw(Game *g, GLuint prog, mat4 vp, bool depth)
{
    if (!spike_model.prim_count) {
        MeshBuilder mb;
        mb_init(&mb);
        mat4 xf = GLM_MAT4_IDENTITY_INIT;
        mb_cylinder(&mb, xf, 0.35f, 0.0f, 1.8f, 6, 1.0f);
        Material stone;
        material_from_dir(&stone, "assets/textures/rock_face");
        model_from_builders(&spike_model, &mb, &stone, 1);
        mb_free(&mb);
    }
    DrawParams dp = { .depth_only = depth };
    for (int i = 0; i < MAX_EFFECTS; i++) {
        SpellEffect *e = &g->effects[i];
        if (!e->used || e->kind != 1)
            continue;
        int shown = (int)(fminf(e->t / e->life, 1.0f) * 14.0f);
        float fade = e->t > e->life ? 1.0f - (e->t - e->life) / 1.5f : 1.0f;
        for (int k = 0; k < shown; k++) {
            vec3 at;
            glm_vec3_copy(e->pos, at);
            glm_vec3_muladds(e->dir, 1.5f + k * 0.9f, at);
            at[1] = ground_at(g, at[0], at[2], e->pos[1] + 1.0f) - (1.0f - fade) * 1.8f;
            mat4 xf;
            glm_translate_make(xf, at);
            glm_rotate_y(xf, k * 1.3f, xf);
            glm_rotate_z(xf, sinf(k * 2.1f) * 0.25f, xf);
            model_draw(&spike_model, NULL, prog, vp, xf, &dp);
        }
    }
    /* a meteor is a glowing stone */
    for (int i = 0; i < MAX_BOLTS; i++) {
        Bolt *b = &g->bolts[i];
        if (!b->used || (b->kind != BOLT_METEOR && b->kind != BOLT_HARPOON))
            continue;
        mat4 xf, fit;
        glm_translate_make(xf, b->pos);
        if (b->kind == BOLT_METEOR) {
            glm_scale_uni(xf, 1.4f);
            DrawParams mp = dp;
            if (!depth)
                glm_vec4_copy((vec4){3.0f, 0.8f, 0.1f, 0.0f}, mp.tint);
            model_draw(&g->prop_models[PM_PUMICE], NULL, prog, vp, xf, &mp);
        } else {
            vec3 d;
            glm_vec3_normalize_to(b->vel, d);
            versor q;
            glm_quat_from_vecs((vec3){0, 1, 0}, d, q);
            glm_quat_rotate(xf, q, xf);
            glm_mat4_mul(xf, ITEMS[ITEM_HARPOON].hold, xf);
            model_draw(&ITEMS[ITEM_HARPOON].model, NULL, prog, vp, xf, &dp);
            (void)fit;
        }
    }
}

void magic_update(Game *g, float dt)
{
    effects_update(g, dt);
    bolts_update(g, dt);

    /* the wisp floats beside your shoulder, bobbing, lighting the way */
    if (g->wisp) {
        g->wisp_t -= dt;
        vec3 fwd, want;
        flat_forward(g, fwd);
        glm_vec3_copy(g->pos, want);
        want[0] += fwd[0] * 1.2f + fwd[2] * 0.9f;
        want[2] += fwd[2] * 1.2f - fwd[0] * 0.9f;
        want[1] += 2.3f + sinf(g->time * 2.1f) * 0.15f;
        glm_vec3_lerp(g->wisp_pos, want, 1.0f - expf(-dt * 3.0f), g->wisp_pos);
        if (frand() < dt * 30.0f)
            fx_magic(&g->ps, g->wisp_pos, (vec3){3.0f, 3.5f, 5.0f}, 0.08f, 1);
        if (g->wisp_t <= 0.0f) {
            g->wisp = false;
            message(g, COL_MAGIC, "The wisp fades away.");
        }
    }

    /* tombs: lids slide, runes glow brighter as you come near */
    for (int i = 0; i < g->tomb_count; i++) {
        Tomb *t = &g->tombs[i];
        if (t->opened && t->open < 1.0f)
            t->open = fminf(1.0f, t->open + dt * 0.6f);
        float d = glm_vec3_distance(t->pos, g->pos);
        float want = t->opened ? 0.15f : glm_clamp(1.4f - d / 10.0f, 0.3f, 1.0f);
        t->glow += (want - t->glow) * (1.0f - expf(-dt * 3.0f));
        if (!t->opened && frand() < dt * 3.0f * t->glow)
            fx_magic(&g->ps, (vec3){t->pos[0], t->pos[1] + 1.0f, t->pos[2]}, (vec3){0.6f, 1.8f, 4.0f}, 0.6f, 1);
    }
}

void magic_draw(Game *g, GLuint prog, mat4 vp, bool depth)
{
    DrawParams dp = { .depth_only = depth };
    for (int i = 0; i < g->tomb_count; i++) {
        Tomb *t = &g->tombs[i];
        if (glm_vec3_distance(t->pos, g->eye) > 60.0f)
            continue;
        mat4 xf;
        transform_matrix(&t->xf, xf);
        DrawParams tp = dp;
        float pulse = t->glow * (0.75f + 0.25f * sinf(g->time * 2.5f + i));
        if (!depth)
            glm_vec4_copy((vec4){0.0f, 0.02f * pulse, 0.05f * pulse, 0.0f}, tp.tint);
        model_draw(&g->tomb_model, NULL, prog, vp, xf, &tp);

        /* the lid slides off to one side and tips down against the tomb */
        mat4 lid;
        float o = t->open;
        glm_translate(xf, (vec3){o * 0.9f, 0.86f - o * o * 0.45f, 0.0f});
        glm_rotate(xf, -o * o * 0.5f, (vec3){0, 0, 1});
        glm_mat4_copy(xf, lid);
        model_draw(&g->tomb_lid_model, NULL, prog, vp, lid, &tp);
    }
}
