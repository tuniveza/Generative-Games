#include "game.h"
#include "ui.h"

#include <string.h>

/* Fishing: click with a rod to cast into deep water. Wait for the float to bob under,
 * click to strike, then hold the mouse button to lift the green zone and keep it over the
 * fish as it darts about; let go and the zone sinks. Keep the fish in the zone until the
 * meter fills.
 *
 * What bites depends on the rod (tier 1..4: better rods have a wider zone and forgive
 * more), the bait (the best you carry goes on the hook), how deep the water is, the
 * time of day, the weather, and where you are. Besides fish there's junk, bottles with
 * notes in them, shells, four rare treasures that change how you swim, and, for the
 * patient and well-equipped, the Conch of the Deep. */

typedef struct {
    ItemId id;
    float weight;
    int rod;            /* least rod tier that can land it */
    int bait;           /* least bait tier it'll take */
    float depth;        /* least water depth */
    int when;           /* 0 any time, 1 night only, 2 storms only */
    float speed;        /* how wildly it darts on the line */
} Catch;

static const Catch CATCHES[] = {
    { ITEM_FISH_SARDINE, 30, 1, 0, 0.8f, 0, 0.6f },
    { ITEM_FISH_MACKEREL, 22, 1, 1, 1.5f, 0, 0.9f },
    { ITEM_FISH_SNAPPER, 12, 2, 2, 3.0f, 0, 1.2f },
    { ITEM_FISH_PUFFER, 8, 1, 1, 2.0f, 0, 0.8f },
    { ITEM_FISH_EEL, 7, 2, 1, 2.5f, 1, 1.4f },
    { ITEM_FISH_LANTERN, 9, 2, 3, 8.0f, 1, 1.3f },
    { ITEM_FISH_GHOST, 6, 3, 2, 4.0f, 1, 1.6f },
    { ITEM_FISH_GOLDEN, 2.5f, 3, 2, 4.0f, 0, 2.0f },
    { ITEM_OLD_BOOT, 10, 1, 0, 0.5f, 0, 0.2f },
    { ITEM_SEAWEED, 10, 1, 0, 0.5f, 0, 0.2f },
    { ITEM_BOTTLE, 4, 1, 0, 1.0f, 0, 0.3f },
    { ITEM_SELKIE_SCALE, 1.2f, 3, 2, 10.0f, 0, 1.8f },
    { ITEM_GILL_PEARL, 1.2f, 2, 3, 3.0f, 0, 1.5f },
    { ITEM_DOLPHIN_CHARM, 3.0f, 2, 1, 5.0f, 2, 1.7f },
    { ITEM_ANGLER_LAMP, 1.5f, 3, 3, 10.0f, 1, 1.8f },
};

bool fishing_busy(const Game *g)
{
    return g->fish.state != FISH_IDLE && g->fish.state != FISH_LANDED;
}

void fishing_cancel(Game *g)
{
    if (g->fish.state != FISH_IDLE && g->fish.state != FISH_LANDED)
        audio_play(SFX_REEL, 0.6f);
    g->fish.state = FISH_IDLE;
}

static int rod_tier(const Game *g)
{
    ItemId held = g->slots[g->selected].id;
    return held != ITEM_NONE && ITEMS[held].kind == KIND_ROD ? ITEMS[held].tier : 0;
}

/* the best bait you carry */
static ItemId best_bait(const Game *g)
{
    static const ItemId order[4] = { ITEM_BAIT_EMBER, ITEM_BAIT_GLOWWORM, ITEM_BAIT_SHRIMP, ITEM_BAIT_WORM };
    for (int i = 0; i < 4; i++)
        if (has_item(g, order[i]))
            return order[i];
    return ITEM_NONE;
}

/* the rod's tip, roughly, wherever you're looking from */
static void rod_tip(const Game *g, vec3 out)
{
    vec3 dir, right = { cosf(g->cam.fp_yaw), 0.0f, -sinf(g->cam.fp_yaw) };
    look_dir(g, dir);
    glm_vec3_copy((float *)g->head, out);
    glm_vec3_muladds(dir, 1.6f, out);
    glm_vec3_muladds(right, 0.25f, out);
    out[1] += 0.55f;
}

void fishing_start(Game *g)
{
    Fishing *f = &g->fish;
    switch (f->state) {
    case FISH_IDLE:
    case FISH_LANDED: {
        if (g->swimming) {
            message(g, COL_TEXT, "Not while you're swimming.");
            return;
        }
        /* aim where you're looking, farther the higher you aim */
        vec3 fwd;
        flat_forward(g, fwd);
        float reach = glm_clamp(9.0f + g->cam.fp_pitch * 12.0f, 4.0f, 16.0f);
        vec3 target = { g->pos[0] + fwd[0] * reach, 0, g->pos[2] + fwd[2] * reach };
        float surface, depth;
        if (!sea_at(g, target[0], target[2], &surface, &depth) || depth < 0.8f) {
            message(g, COL_TEXT, "Cast out over deep water: the sea, the harbour, the cove.");
            return;
        }
        f->bait = best_bait(g);
        if (f->bait == ITEM_NONE)
            message(g, COL_TEXT, "No bait: you cast a bare hook. Only junk and the foolish will bite. (Coralie sells bait.)");
        else
            take_items(g, f->bait, 1);
        rod_tip(g, f->bobber);
        target[1] = surface;
        /* a lob that lands on target */
        float flight = 0.9f;
        for (int k = 0; k < 3; k++)
            f->bob_vel[k] = (target[k] - f->bobber[k]) / flight;
        f->bob_vel[1] += 0.5f * 9.8f * flight;
        f->state = FISH_CAST;
        f->t = 0.0f;
        audio_play(SFX_CAST_LINE, 1.0f);
        break;
    }
    case FISH_CAST:
    case FISH_WAIT:
        f->state = FISH_IDLE;       /* reel it back in */
        audio_play(SFX_REEL, 0.8f);
        break;
    case FISH_BITE: {
        /* strike! pick what's on the line */
        float surface, depth;
        if (!sea_at(g, f->bobber[0], f->bobber[2], &surface, &depth))
            depth = 1.0f;
        int tier = rod_tier(g);
        int bait_tier = f->bait != ITEM_NONE ? ITEMS[f->bait].tier : 0;
        bool night = g->night, storm = storm_level(g) > 0.5f;
        Area area = level_area(&g->level, f->bobber);
        float luck = (has_item(g, ITEM_ELEPHANT) ? 1.6f : 1.0f) * (g->equip[EQ_HEAD] == ITEM_FISHER_HAT ? 1.2f : 1.0f);
        f->hooked = ITEM_NONE;
        float speed = 0.8f;
        /* the Conch of the Deep: the whole point of the Leviathan rod */
        if (tier >= 4 && f->bait == ITEM_BAIT_EMBER && depth > 11.0f && night && !has_item(g, ITEM_CONCH) &&
            g->quest.stage[Q_MAIN] >= 6 && frand() < 0.4f) {
            f->hooked = ITEM_CONCH;
            speed = 2.2f;
        } else {
            float total = 0.0f, weights[32];
            int n = sizeof CATCHES / sizeof CATCHES[0];
            for (int i = 0; i < n; i++) {
                const Catch *c = &CATCHES[i];
                float w = c->weight;
                if (tier < c->rod || bait_tier < c->bait || depth < c->depth || (c->when == 1 && !night) || (c->when == 2 && !storm))
                    w = 0.0f;
                if (c->id == ITEM_FISH_EEL && area == AREA_MARINA)
                    w *= 2.0f;
                if (c->id == ITEM_GILL_PEARL && area != AREA_MARINA)
                    w *= 0.3f;
                if (c->weight < 4.0f && c->id != ITEM_BOTTLE)
                    w *= luck * (1.0f + tier * 0.25f);
                if (f->bait == ITEM_NONE && ITEMS[c->id].kind == KIND_FISH && c->id != ITEM_FISH_SARDINE)
                    w *= 0.15f;
                weights[i] = w;
                total += w;
            }
            float r = frand() * total;
            for (int i = 0; i < n && f->hooked == ITEM_NONE; i++) {
                if (r < weights[i]) {
                    f->hooked = CATCHES[i].id;
                    speed = CATCHES[i].speed;
                }
                r -= weights[i];
            }
            if (f->hooked == ITEM_NONE)
                f->hooked = ITEM_FISH_SARDINE;
        }
        f->state = FISH_REEL;
        f->zone = 0.3f;
        f->zone_vel = 0.0f;
        f->fish_y = 0.5f;
        f->fish_target = frand();
        f->fish_speed = speed;
        f->meter = 0.3f;
        f->t = 0.0f;
        audio_play(SFX_REEL, 1.0f);
        break;
    }
    case FISH_REEL:
        break;      /* holding the button is what matters now */
    }
}

static void splash_small(Game *g, vec3 at, int n)
{
    for (int i = 0; i < n; i++) {
        Particle p = {0};
        glm_vec3_copy(at, p.pos);
        glm_vec3_copy((vec3){(frand() - 0.5f) * 1.2f, 1.0f + frand() * 1.5f, (frand() - 0.5f) * 1.2f}, p.vel);
        glm_vec4_copy((vec4){0.8f, 0.95f, 0.95f, 0.6f}, p.color0);
        glm_vec4_copy((vec4){0.8f, 0.95f, 0.95f, 0.0f}, p.color1);
        p.size0 = 0.05f;
        p.size1 = 0.03f;
        p.max_life = p.life = 0.5f;
        p.gravity = 9.0f;
        particles_emit(&g->ps, &p);
    }
}

void fishing_update(Game *g, float dt)
{
    Fishing *f = &g->fish;
    if (f->state == FISH_IDLE)
        return;
    if (f->state == FISH_LANDED) {
        if ((f->landed_t -= dt) <= 0.0f)
            f->state = FISH_IDLE;
        return;
    }
    /* put the rod away, or wander off, and the line comes in */
    if (rod_tier(g) == 0 || g->dead || g->swimming || glm_vec3_distance(g->pos, f->bobber) > 28.0f) {
        f->state = FISH_IDLE;
        return;
    }
    f->t += dt;
    float surface, depth;
    bool sea = sea_at(g, f->bobber[0], f->bobber[2], &surface, &depth);
    switch (f->state) {
    case FISH_CAST:
        f->bob_vel[1] -= 9.8f * dt;
        glm_vec3_muladds(f->bob_vel, dt, f->bobber);
        if (sea && f->bobber[1] <= surface) {
            f->bobber[1] = surface;
            f->state = FISH_WAIT;
            int tier = rod_tier(g);
            int bait = f->bait != ITEM_NONE ? ITEMS[f->bait].tier : 0;
            f->wait = (4.0f + frand() * 10.0f) * (1.2f - bait * 0.15f) * (1.1f - tier * 0.08f);
            audio_play_at(SFX_BITE, f->bobber, 0.5f);
            splash_small(g, f->bobber, 8);
        } else if (!sea && f->bobber[1] < ground_at(g, f->bobber[0], f->bobber[2], f->bobber[1] + 1.0f)) {
            message(g, COL_TEXT, "The hook thuds onto dry ground.");
            f->state = FISH_IDLE;
        }
        break;
    case FISH_WAIT:
        f->bobber[1] = sea ? surface + sinf(g->time * 2.0f) * 0.02f : f->bobber[1];
        if ((f->wait -= dt) <= 0.0f) {
            f->state = FISH_BITE;
            f->t = 0.0f;
            audio_play_at(SFX_BITE, f->bobber, 1.0f);
            splash_small(g, f->bobber, 14);
            message(g, COL_FUN, "Something's biting! Click!");
        }
        break;
    case FISH_BITE:
        f->bobber[1] = surface - 0.08f - sinf(f->t * 25.0f) * 0.04f;
        if (f->t > 1.1f + rod_tier(g) * 0.15f) {
            message(g, COL_TEXT, "Too slow. It nibbled the bait and was gone.");
            f->state = FISH_WAIT;
            f->wait = 4.0f + frand() * 8.0f;
            f->bobber[1] = surface;
        }
        break;
    case FISH_REEL: {
        int tier = rod_tier(g);
        float zone_h = 0.16f + tier * 0.045f;
        /* the zone rises while you hold, sinks when you let go, bouncing off the ends */
        f->zone_vel += (g->lmb ? 2.6f : -2.2f) * dt;
        f->zone_vel *= expf(-dt * 1.5f);
        f->zone += f->zone_vel * dt;
        if (f->zone < 0.0f) { f->zone = 0.0f; f->zone_vel *= -0.3f; }
        if (f->zone > 1.0f - zone_h) { f->zone = 1.0f - zone_h; f->zone_vel *= -0.3f; }
        /* the fish darts between spots */
        if (fabsf(f->fish_y - f->fish_target) < 0.03f || frand() < dt * f->fish_speed * 0.6f)
            f->fish_target = glm_clamp(f->fish_y + (frand() - 0.5f) * 0.8f * f->fish_speed, 0.02f, 0.98f);
        f->fish_y += glm_clamp(f->fish_target - f->fish_y, -dt * 0.5f * f->fish_speed, dt * 0.5f * f->fish_speed);
        bool inside = f->fish_y >= f->zone && f->fish_y <= f->zone + zone_h;
        f->meter += (inside ? 0.28f : -(0.24f - tier * 0.035f)) * dt;
        if (frand() < dt * 3.0f)
            audio_play(SFX_REEL, inside ? 0.5f : 0.25f);
        f->bobber[1] = surface - 0.1f + sinf(g->time * 20.0f) * 0.05f;
        if (frand() < dt * 6.0f)
            splash_small(g, f->bobber, 2);
        if (f->meter >= 1.0f) {
            f->state = FISH_LANDED;
            f->landed = f->hooked;
            f->landed_t = 2.5f;
            game_give(g, f->hooked, 1);
            audio_play(SFX_CATCH, 1.0f);
            splash_small(g, f->bobber, 20);
            const ItemDef *it = &ITEMS[f->hooked];
            if (f->hooked == ITEM_CONCH)
                message(g, COL_GOLD, "The line nearly pulls you in... and up comes a great spiral shell, humming: the Conch of the Deep!");
            else if (it->kind == KIND_FISH)
                message(g, COL_GOLD, "You land a %s!", it->name);
            else
                message(g, COL_FUN, "You reel in... %s.", it->name);
            quest_fish_caught(g, f->hooked);
        } else if (f->meter <= 0.0f) {
            f->state = FISH_IDLE;
            audio_play(SFX_LINE_SNAP, 1.0f);
            message(g, COL_TEXT, "The line goes slack. It got away.");
        }
        break;
    }
    default:
        break;
    }
}

void fishing_draw(Game *g, GLuint prog, mat4 vp)
{
    Fishing *f = &g->fish;
    if (f->state == FISH_IDLE || f->state == FISH_LANDED || g->menu != MENU_NONE)
        return;
    mat4 xf;
    glm_translate_make(xf, f->bobber);
    DrawParams dp = {0};
    model_draw(&g->bobber_model, NULL, prog, vp, xf, &dp);
    /* the line: a sagging string of faint dots from the rod's tip to the float */
    vec3 tip;
    rod_tip(g, tip);
    for (int i = 0; i <= 24; i++) {
        float t = i / 24.0f;
        Particle p = {0};
        glm_vec3_lerp(tip, f->bobber, t, p.pos);
        p.pos[1] -= sinf(t * GLM_PIf) * (f->state == FISH_REEL ? 0.05f : 0.5f);
        glm_vec4_copy((vec4){0.9f, 0.9f, 0.85f, 0.5f}, p.color0);
        glm_vec4_copy((vec4){0.9f, 0.9f, 0.85f, 0.5f}, p.color1);
        p.size0 = p.size1 = 0.006f;
        p.max_life = p.life = 0.001f;
        particles_emit(&g->ps, &p);
    }
}

void fishing_hud(Game *g)
{
    Fishing *f = &g->fish;
    float w = (float)g->width, h = (float)g->height;
    if (f->state == FISH_BITE) {
        const char *t = "!";
        ui_text_shadow(FONT_LARGE, (w - ui_text_width(FONT_LARGE, t)) * 0.5f, h * 0.36f, COL_GOLD, t);
    }
    if (f->state == FISH_REEL) {
        /* the reeling bar: a tall water-colored well, the green zone, the fish, the meter */
        float bx = w * 0.72f, by = h * 0.2f, bw = 46, bh = h * 0.5f;
        float well[4] = { 0.05f, 0.18f, 0.22f, 0.85f };
        ui_rect(bx, by, bw, bh, well);
        ui_frame(bx, by, bw, bh, 2, COL_EDGE);
        int tier = rod_tier(g);
        float zone_h = 0.16f + tier * 0.045f;
        float green[4] = { 0.3f, 0.9f, 0.4f, 0.55f };
        ui_rect(bx + 3, by + bh * (1.0f - f->zone - zone_h), bw - 6, bh * zone_h, green);
        float fishc[4] = { 1.0f, 0.75f, 0.3f, 1.0f };
        float fy = by + bh * (1.0f - f->fish_y);
        ui_rect(bx + 8, fy - 7, bw - 16, 14, fishc);
        float back[4] = { 0.05f, 0.03f, 0.02f, 0.8f }, meter[4] = { 0.95f, 0.8f, 0.3f, 0.95f };
        ui_rect(bx + bw + 10, by, 14, bh, back);
        ui_rect(bx + bw + 12, by + bh * (1.0f - f->meter), 10, bh * f->meter, meter);
        const char *hint = "Hold to lift the green, keep the fish in it";
        ui_text_shadow(FONT_SMALL, bx + bw * 0.5f - ui_text_width(FONT_SMALL, hint) * 0.5f, by + bh + 12, COL_TEXT, hint);
    }
    if (f->state == FISH_LANDED && f->landed != ITEM_NONE && ITEMS[f->landed].icon) {
        float a[4] = { 1, 1, 1, fminf(1.0f, f->landed_t) };
        ui_image(ITEMS[f->landed].icon, w * 0.5f - 64, h * 0.25f, 128, 128, a, true);
    }
    if (f->state == FISH_WAIT) {
        const char *hint = "Waiting for a bite...  (click to reel in)";
        float dim[4] = { 0.95f, 0.9f, 0.8f, 0.6f };
        ui_text_shadow(FONT_SMALL, (w - ui_text_width(FONT_SMALL, hint)) * 0.5f, h * 0.62f, dim, hint);
    }
}
