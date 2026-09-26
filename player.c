#include "game.h"
#include "shapes.h"

#include <stdlib.h>
#include <string.h>

/* You: walking, swimming (and diving, and tiring), rowing boats, the camera over your
 * shoulder, and your body seen from outside, wearing what you've put on.
 *
 * The body is KayKit's rogue. Armour is borrowed from the other adventurers, who share
 * the rogue's skeleton: the knight's plate and helm, the barbarian's furs, the mage's
 * robes. Each is its own model posed like the rogue every frame, drawing only the
 * pieces you're wearing. */

#define SWIM_DEPTH   1.3f       /* water this deep (over your feet) and you swim */
#define FLOAT_DEPTH  1.42f      /* where your feet hang while treading water */
#define BASE_STAMINA 45.0f      /* seconds of swimming before you tire */
#define BASE_BREATH  15.0f      /* seconds under water before you start to drown */

static float smooth_k(float rate, float dt) { return 1.0f - expf(-dt * rate); }

float avatar_test_turn;     /* --face (main.c): turn the body round to look at its front */

/* ---------- water ---------- */

static bool wears(const Game *g, ItemId id)
{
    for (int i = 0; i < EQ_COUNT; i++)
        if (g->equip[i] == id)
            return true;
    return false;
}

float armor_total(const Game *g)
{
    float a = 0.0f;
    for (int i = 0; i < EQ_COUNT; i++)
        if (g->equip[i] != ITEM_NONE)
            a += ITEMS[g->equip[i]].armor;
    return fminf(a, 0.8f);
}

static void refresh_limits(Game *g)
{
    g->max_stamina = BASE_STAMINA + (g->dolphin ? 40.0f : 0.0f);
    g->max_breath = BASE_BREATH * (g->gill_pearl ? 2.0f : 1.0f);
}

/* ---------- walking (as it always was, plus sand, planks, wind and wading) ---------- */

static void splash_at(Game *g, vec3 at, int n, float up)
{
    for (int i = 0; i < n; i++) {
        Particle p = {0};
        glm_vec3_copy((vec3){at[0] + (frand() - 0.5f) * 0.8f, at[1], at[2] + (frand() - 0.5f) * 0.8f}, p.pos);
        glm_vec3_copy((vec3){(frand() - 0.5f) * 1.5f, up * (0.6f + frand()), (frand() - 0.5f) * 1.5f}, p.vel);
        glm_vec4_copy((vec4){0.75f, 0.9f, 0.9f, 0.55f}, p.color0);
        glm_vec4_copy((vec4){0.75f, 0.9f, 0.9f, 0.0f}, p.color1);
        p.size0 = 0.07f;
        p.size1 = 0.03f;
        p.max_life = p.life = 0.5f + frand() * 0.3f;
        p.gravity = 9.0f;
        particles_emit(&g->ps, &p);
    }
}

static void read_input(Game *g, vec3 fwd, vec3 right, vec3 wish, bool *sprint, bool *jump, bool *down)
{
    const bool *keys = SDL_GetKeyboardState(NULL);
    bool control = !g->dead && !g->inv_open && g->menu == MENU_NONE && g->shop_npc < 0 && !g->journal_open;
    flat_forward(g, fwd);
    glm_vec3_copy((vec3){-fwd[2], 0, fwd[0]}, right);
    glm_vec3_zero(wish);
    *sprint = keys[SDL_SCANCODE_LSHIFT];
    *jump = *down = false;
    if (!control)
        return;
    if (keys[SDL_SCANCODE_W]) glm_vec3_add(wish, fwd, wish);
    if (keys[SDL_SCANCODE_S]) glm_vec3_sub(wish, fwd, wish);
    if (keys[SDL_SCANCODE_D]) glm_vec3_add(wish, right, wish);
    if (keys[SDL_SCANCODE_A]) glm_vec3_sub(wish, right, wish);
    *jump = keys[SDL_SCANCODE_SPACE];
    *down = keys[SDL_SCANCODE_C] || keys[SDL_SCANCODE_LCTRL];
    if (g->pad) {
        float lx = SDL_GetGamepadAxis(g->pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
        float ly = SDL_GetGamepadAxis(g->pad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
        if (fabsf(lx) > 0.15f) glm_vec3_muladds(right, lx, wish);
        if (fabsf(ly) > 0.15f) glm_vec3_muladds(fwd, -ly, wish);
        *sprint |= SDL_GetGamepadButton(g->pad, SDL_GAMEPAD_BUTTON_LEFT_STICK);
        *jump |= SDL_GetGamepadButton(g->pad, SDL_GAMEPAD_BUTTON_SOUTH);
        *down |= g->crouching;
    }
}

static void walk(Game *g, float dt)
{
    vec3 fwd, right, wish;
    bool sprint, jump, down;
    read_input(g, fwd, right, wish, &sprint, &jump, &down);
    bool control = !g->dead && !g->inv_open && g->menu == MENU_NONE;
    if (control) {
        const bool *keys = SDL_GetKeyboardState(NULL);
        if (keys[SDL_SCANCODE_C] || keys[SDL_SCANCODE_LCTRL])
            g->crouching = true;
        else if (!g->pad)
            g->crouching = false;
    }
    g->crouch_amount += ((g->crouching ? 1.0f : 0.0f) - g->crouch_amount) * smooth_k(12.0f, dt);

    float speed = sprint && !g->crouching ? 7.0f : 4.2f;
    if (g->crouching) speed *= 0.5f;
    if (g->action == ACT_BLOCK) speed *= 0.5f;
    if (wears(g, ITEM_KNIGHT_PLATE)) speed *= 0.92f;
    if (wears(g, ITEM_DIVING_HELM)) speed *= 0.85f;
    if (g->toxic > 0.3f) speed *= 1.0f - (g->toxic - 0.3f) * 0.6f;
    bool wading = level_cell_at(&g->level, g->pos[0], g->pos[2]) == 'w' && g->pos[1] < WATER_Y;
    float surface, depth;
    g->wading_sea = sea_at(g, g->pos[0], g->pos[2], &surface, &depth) && surface - g->pos[1] > 0.3f;
    if (wading || (g->wading_sea && !wears(g, ITEM_RUBBER_BOOTS)))
        speed *= 0.6f;
    float len = glm_vec3_norm(wish);
    if (len > 1e-3f)
        glm_vec3_scale(wish, speed * fminf(len, 1.0f) / len, wish);

    /* a gale leans on you */
    if (g->weather.wind > 0.3f && level_sky_exposure(&g->level, g->head) > 0.5f)
        glm_vec3_muladds(g->weather.wind_dir, g->weather.gust * (g->crouching ? 0.8f : 2.2f), wish);

    float k = smooth_k(g->on_ground ? 12.0f : 2.0f, dt);
    g->vel[0] += (wish[0] - g->vel[0]) * k;
    g->vel[2] += (wish[2] - g->vel[2]) * k;

    /* a tornado picks you up and throws you */
    vec3 twist;
    weather_twister_force(&g->weather, g->pos, twist);
    if (glm_vec3_norm2(twist) > 0.01f) {
        glm_vec3_muladds(twist, dt, g->vel);
        if (twist[1] > 1.0f)
            g->on_ground = false;
    }

    if (jump && g->on_ground && !g->crouching) {
        g->vel[1] = 6.2f;
        g->on_ground = false;
        audio_play(SFX_JUMP, 0.6f);
    }
    g->vel[1] -= GRAVITY * dt;

    g->pos[0] += g->vel[0] * dt;
    g->pos[2] += g->vel[2] * dt;
    level_collide(&g->level, g->pos, PLAYER_R, PLAYER_H - g->crouch_amount * 0.6f);

    float feet = g->pos[1];
    g->pos[1] += g->vel[1] * dt;
    float ground = ground_at(g, g->pos[0], g->pos[2], feet);
    if (g->pos[1] <= ground) {
        if (!g->on_ground && g->vel[1] < -6.0f) {
            g->land_t = glm_clamp(-g->vel[1] / 14.0f, 0.3f, 1.0f);
            audio_play(wading || g->wading_sea ? SFX_STEP_WATER : SFX_LAND, g->land_t);
            if (g->vel[1] < -15.0f)
                hurt_player(g, (vec3){g->pos[0], g->pos[1] - 1, g->pos[2]}, (-g->vel[1] - 15.0f) * 4.0f);
        }
        g->pos[1] = ground;
        g->vel[1] = 0.0f;
        g->on_ground = true;
    } else {
        g->on_ground = g->pos[1] - ground < 0.05f;
    }

    /* footsteps: stone, grass, sand, planks, or splashing through water */
    if (g->on_ground) {
        float moved = sqrtf(g->vel[0] * g->vel[0] + g->vel[2] * g->vel[2]) * dt;
        g->step_dist += moved;
        if (g->step_dist > (g->crouching ? 1.4f : 1.9f)) {
            g->step_dist = 0.0f;
            float vol = g->crouching ? 0.35f : sprint ? 1.0f : 0.7f;
            if (wading || g->wading_sea) {
                audio_play(SFX_STEP_WATER, vol);
                splash_at(g, (vec3){g->pos[0], g->wading_sea ? surface : WATER_Y, g->pos[2]}, 6, 1.4f);
            } else {
                bool box = level_ground(&g->level, g->pos[0], g->pos[2], g->pos[1], STEP) > -999.0f;
                Area area = level_area(&g->level, g->pos);
                Sfx s = SFX_STEP_GRASS;
                if (box)
                    s = (area == AREA_MARINA && g->pos[2] > MARINA_Z0 + 1.5f) || (area == AREA_BEACH) ? SFX_STEP_WOOD : SFX_STEP_STONE;
                else if (g->pos[1] < -0.8f || area == AREA_BEACH)
                    s = SFX_STEP_SAND;
                audio_play(s, vol);
            }
        }
        if ((wading || g->wading_sea) && moved > 0.0f && frand() < dt * 12.0f)
            splash_at(g, (vec3){g->pos[0], g->wading_sea ? surface : WATER_Y, g->pos[2]}, 1, 1.0f);
    }
}

/* ---------- swimming ---------- */

/* in the water with a ledge in front of you: climb out onto it */
static bool climb_out(Game *g, float surface)
{
    vec3 fwd;
    flat_forward(g, fwd);
    for (float reach = 0.6f; reach <= 1.4f; reach += 0.4f) {
        vec3 p = { g->pos[0] + fwd[0] * reach, 0, g->pos[2] + fwd[2] * reach };
        float top = ground_at(g, p[0], p[2], surface + 2.8f);
        if (top > surface - 0.3f && top < surface + 2.8f) {
            vec3 test = { p[0], top, p[2] };
            if (level_collide(&g->level, test, PLAYER_R * 0.8f, PLAYER_H * 0.9f))
                continue;
            glm_vec3_copy(test, g->pos);
            glm_vec3_zero(g->vel);
            g->swimming = false;
            g->on_ground = true;
            audio_play(SFX_SPLASH, 0.5f);
            return true;
        }
    }
    return false;
}

static void swim(Game *g, float dt, float surface, float depth)
{
    vec3 fwd, right, wish;
    bool sprint, jump, down;
    read_input(g, fwd, right, wish, &sprint, &jump, &down);
    bool gills = g->gills_t > 0.0f || g->first_tide;
    bool tired = g->stamina <= 0.0f && !gills;

    float speed = (sprint && !tired ? 3.7f : 2.5f) * (g->selkie ? 1.5f : 1.0f) * (g->gills_t > 0.0f ? 1.25f : 1.0f);
    if (wears(g, ITEM_KNIGHT_PLATE)) speed *= 0.7f;
    if (tired) speed *= 0.5f;

    /* looking down while swimming forward dives; C / Ctrl dives straight down, space comes up */
    float len = glm_vec3_norm(wish);
    vec3 move = { 0, 0, 0 };
    if (len > 1e-3f) {
        glm_vec3_scale(wish, fminf(len, 1.0f) / len, move);
        bool forward = glm_vec3_dot(move, fwd) > 0.5f;
        float pitch = g->cam.fp_pitch;
        if (forward && (pitch < -0.35f || g->underwater)) {
            move[0] *= cosf(pitch);
            move[2] *= cosf(pitch);
            move[1] = sinf(pitch);
        }
    }
    if (down)
        move[1] -= 0.9f;
    if (jump)
        move[1] += 0.9f;
    glm_vec3_scale(move, speed, move);

    /* treading water keeps your head up, unless you're diving or too tired */
    float rest_y = surface - FLOAT_DEPTH;
    bool diving = down || move[1] < -0.3f;
    if (!diving && move[1] <= 0.3f) {
        float buoy = tired ? -0.35f : (rest_y - g->pos[1]) * 2.5f;
        move[1] += glm_clamp(buoy, -1.5f, 2.5f);
    }
    float k = smooth_k(4.0f, dt);
    for (int i = 0; i < 3; i++)
        g->vel[i] += (move[i] - g->vel[i]) * k;

    vec3 twist;
    weather_twister_force(&g->weather, g->pos, twist);
    glm_vec3_muladds(twist, dt * 0.5f, g->vel);

    glm_vec3_muladds(g->vel, dt, g->pos);
    level_collide(&g->level, g->pos, PLAYER_R, PLAYER_H * 0.6f);
    float floor_y = surface - depth;
    if (g->pos[1] < floor_y) {
        g->pos[1] = floor_y;
        g->vel[1] = fmaxf(g->vel[1], 0.0f);
    }
    /* you can't swim up out of the sea */
    if (g->pos[1] > rest_y + 0.25f && !(g->dolphin && sprint)) {
        g->pos[1] = rest_y + 0.25f;
        g->vel[1] = fminf(g->vel[1], 0.0f);
    }

    /* strokes: a splash on each, bubbles when under */
    float moving = sqrtf(g->vel[0] * g->vel[0] + g->vel[2] * g->vel[2]);
    g->swim_phase += dt * (1.2f + moving * 0.9f);
    if (moving > 0.6f && fmodf(g->swim_phase, 1.6f) < dt * (1.2f + moving * 0.9f)) {
        audio_play(g->underwater ? SFX_BUBBLES : SFX_SWIM, g->underwater ? 0.35f : 0.55f);
        if (!g->underwater)
            splash_at(g, (vec3){g->pos[0], surface, g->pos[2]}, 5, 1.2f);
    }
    if (g->underwater && frand() < dt * 3.0f) {
        Particle p = {0};
        glm_vec3_copy(g->head, p.pos);
        glm_vec3_copy((vec3){(frand() - 0.5f) * 0.3f, 1.2f + frand(), (frand() - 0.5f) * 0.3f}, p.vel);
        glm_vec4_copy((vec4){0.7f, 0.9f, 1.0f, 0.6f}, p.color0);
        glm_vec4_copy((vec4){0.7f, 0.9f, 1.0f, 0.0f}, p.color1);
        p.size0 = 0.03f;
        p.size1 = 0.05f;
        p.max_life = p.life = 1.2f;
        p.gravity = -1.0f;
        particles_emit(&g->ps, &p);
    }

    /* stamina: swimming tires you (a boat never does) */
    if (!gills) {
        float drain = (sprint && moving > 0.5f ? 2.5f : 1.0f) * (g->dolphin ? 0.4f : 1.0f);
        float before = g->stamina;
        g->stamina = fmaxf(0.0f, g->stamina - drain * dt);
        if (before > 10.0f && g->stamina <= 10.0f)
            message(g, COL_RED, "Your arms are growing heavy. Find land, or a boat.");
        if (before > 0.0f && g->stamina <= 0.0f)
            message(g, COL_RED, "You're exhausted! You can barely keep your head up.");
    } else {
        g->stamina = fminf(g->max_stamina, g->stamina + dt * 4.0f);
    }

    if (jump && climb_out(g, surface))
        return;
}

/* ---------- boats ---------- */

void boat_add(Game *g, vec3 pos, float yaw)
{
    if (g->boat_count >= MAX_BOATS)
        return;
    Boat *b = &g->boats[g->boat_count++];
    *b = (Boat){ .used = true };
    b->xf = TRANSFORM_AT(pos, yaw);
}

int boat_near(Game *g, float reach)
{
    int best = -1;
    float bd = reach;
    for (int i = 0; i < g->boat_count; i++) {
        Boat *b = &g->boats[i];
        float dx = b->pos[0] - g->pos[0], dz = b->pos[2] - g->pos[2];
        float d = sqrtf(dx * dx + dz * dz);
        if (d < bd && fabsf(b->pos[1] - g->pos[1]) < 4.0f) {
            bd = d;
            best = i;
        }
    }
    return best;
}

void boat_board(Game *g, int i)
{
    if (i < 0 || i >= g->boat_count || g->boats[i].occupied)
        return;
    fishing_cancel(g);
    g->boat_in = i;
    g->boats[i].occupied = true;
    g->swimming = g->underwater = false;
    glm_vec3_zero(g->vel);
    g->stamina = g->max_stamina;
    audio_play(SFX_BOAT_CREAK, 0.9f);
    message(g, COL_TEXT, "You climb into the boat. (W/S row, A/D turn, E to step out)");
}

void boat_leave(Game *g)
{
    if (g->boat_in < 0)
        return;
    Boat *b = &g->boats[g->boat_in];
    b->occupied = false;
    g->boat_in = -1;
    /* step onto the nearest dry footing within a few paces, or into the water */
    for (float r = 1.2f; r <= 4.0f; r += 0.4f) {
        for (int k = 0; k < 16; k++) {
            float a = k * GLM_PIf / 8.0f;
            float x = b->pos[0] + cosf(a) * r, z = b->pos[2] + sinf(a) * r;
            float surface, depth;
            float top = ground_at(g, x, z, b->pos[1] + 3.0f);
            bool sea = sea_at(g, x, z, &surface, &depth);
            if ((!sea || top > surface - 0.4f) && top < b->pos[1] + 3.0f) {
                vec3 p = { x, top, z };
                if (level_collide(&g->level, p, PLAYER_R, PLAYER_H))
                    continue;
                glm_vec3_copy(p, g->pos);
                glm_vec3_zero(g->vel);
                audio_play(SFX_STEP_WOOD, 0.7f);
                return;
            }
        }
    }
    g->pos[0] += cosf(b->yaw) * 1.3f;
    g->pos[2] -= sinf(b->yaw) * 1.3f;
    g->pos[1] = b->pos[1] - 1.2f;
    audio_play(SFX_SPLASH, 1.0f);
    message(g, COL_TEXT, "You slip over the side into the water.");
}

static void row(Game *g, float dt)
{
    Boat *b = &g->boats[g->boat_in];
    const bool *keys = SDL_GetKeyboardState(NULL);
    bool control = !g->dead && !g->inv_open && g->menu == MENU_NONE && g->shop_npc < 0;
    float thrust = 0.0f, turn = 0.0f;
    if (control) {
        if (keys[SDL_SCANCODE_W]) thrust += 1.0f;
        if (keys[SDL_SCANCODE_S]) thrust -= 0.6f;
        if (keys[SDL_SCANCODE_A]) turn += 1.0f;
        if (keys[SDL_SCANCODE_D]) turn -= 1.0f;
        if (g->pad) {
            float lx = SDL_GetGamepadAxis(g->pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
            float ly = SDL_GetGamepadAxis(g->pad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
            if (fabsf(ly) > 0.15f) thrust -= ly;
            if (fabsf(lx) > 0.15f) turn -= lx;
        }
    }
    bool sprint = keys[SDL_SCANCODE_LSHIFT];
    float power = sprint ? 6.5f : 4.5f;
    vec3 fwd = { sinf(b->yaw), 0.0f, cosf(b->yaw) };
    vec3 want;
    glm_vec3_scale(fwd, thrust * power, want);
    b->vel[0] += (want[0] - b->vel[0]) * smooth_k(0.8f, dt);
    b->vel[2] += (want[2] - b->vel[2]) * smooth_k(0.8f, dt);
    b->spin += (turn * 1.1f - b->spin) * smooth_k(2.0f, dt);
    b->yaw = wrap_angle(b->yaw + b->spin * dt);
    if (fabsf(thrust) > 0.1f || fabsf(turn) > 0.1f) {
        float before = b->row_t;
        b->row_t += dt * (sprint ? 1.3f : 0.9f);
        if (floorf(before) != floorf(b->row_t)) {
            audio_play_at(SFX_OAR, b->pos, 0.8f);
            vec3 side = { cosf(b->yaw) * 1.6f, 0, -sinf(b->yaw) * 1.6f };
            vec3 l = { b->pos[0] + side[0], b->pos[1] + 0.1f, b->pos[2] + side[2] };
            vec3 r = { b->pos[0] - side[0], b->pos[1] + 0.1f, b->pos[2] - side[2] };
            splash_at(g, l, 5, 1.0f);
            splash_at(g, r, 5, 1.0f);
        }
    }

    /* move, but not up onto the sand or through the piers */
    vec3 before;
    glm_vec3_copy(b->pos, before);
    b->pos[0] += b->vel[0] * dt;
    b->pos[2] += b->vel[2] * dt;
    float surface, depth;
    vec3 bow = { b->pos[0] + fwd[0] * 1.6f, 0, b->pos[2] + fwd[2] * 1.6f };
    bool ok = sea_at(g, b->pos[0], b->pos[2], &surface, &depth) && depth > 0.5f;
    float s2, d2;
    if (thrust > 0.0f && (!sea_at(g, bow[0], bow[2], &s2, &d2) || d2 < 0.45f))
        ok = false;
    vec3 test = { b->pos[0], surface - 0.6f, b->pos[2] };
    if (level_collide(&g->level, test, 1.2f, 1.2f)) {
        b->pos[0] = test[0];
        b->pos[2] = test[2];
        glm_vec3_scale(b->vel, 0.5f, b->vel);
    }
    if (!ok) {
        glm_vec3_copy(before, b->pos);
        glm_vec3_scale(b->vel, -0.2f, b->vel);
    }
    g->pos[0] = b->pos[0] - fwd[0] * 0.3f;
    g->pos[2] = b->pos[2] - fwd[2] * 0.3f;
    g->pos[1] = b->pos[1] - 0.4f;
    glm_vec3_copy(b->vel, g->vel);
    g->on_ground = true;
}

void boats_update(Game *g, float dt)
{
    float storm = storm_level(g);
    for (int i = 0; i < g->boat_count; i++) {
        Boat *b = &g->boats[i];
        float surface, depth;
        if (!sea_at(g, b->pos[0], b->pos[2], &surface, &depth))
            surface = SEA_Y;
        if (!b->occupied) {
            glm_vec3_scale(b->vel, expf(-dt * 0.6f), b->vel);
            glm_vec3_muladds(b->vel, dt, b->pos);
        }
        /* ride the swell: pitch and roll from the waves under bow, stern and both sides */
        vec3 fwd = { sinf(b->yaw), 0.0f, cosf(b->yaw) }, side = { fwd[2], 0.0f, -fwd[0] };
        float t = g->time;
        float h_bow = ocean_height(b->pos[0] + fwd[0] * 1.5f, b->pos[2] + fwd[2] * 1.5f, t, storm, depth);
        float h_stern = ocean_height(b->pos[0] - fwd[0] * 1.5f, b->pos[2] - fwd[2] * 1.5f, t, storm, depth);
        float h_port = ocean_height(b->pos[0] + side[0] * 0.7f, b->pos[2] + side[2] * 0.7f, t, storm, depth);
        float h_star = ocean_height(b->pos[0] - side[0] * 0.7f, b->pos[2] - side[2] * 0.7f, t, storm, depth);
        float y = (h_bow + h_stern + h_port + h_star) * 0.25f - 0.18f;
        b->pos[1] += (y - b->pos[1]) * smooth_k(6.0f, dt);
        b->pitch += (-atan2f(h_bow - h_stern, 3.0f) - b->pitch) * smooth_k(4.0f, dt);
        b->roll += (atan2f(h_port - h_star, 1.4f) + (b->occupied ? b->spin * 0.05f : 0.0f) - b->roll) * smooth_k(4.0f, dt);
        /* the whirlpool drags boats in */
        if (g->ocean.whirl[3] > 0.0f) {
            vec3 d = { g->ocean.whirl[0] - b->pos[0], 0.0f, g->ocean.whirl[1] - b->pos[2] };
            float r = glm_vec3_norm(d);
            if (r < g->ocean.whirl[2] * 1.5f && r > 0.5f) {
                vec3 around = { -d[2] / r, 0.0f, d[0] / r };
                glm_vec3_muladds(d, dt * 0.25f * g->ocean.whirl[3] / r * 4.0f, b->pos);
                glm_vec3_muladds(around, dt * 4.0f * g->ocean.whirl[3], b->pos);
            }
        }
    }
}

void boats_draw(Game *g, GLuint prog, mat4 vp, bool depth)
{
    DrawParams dp = { .depth_only = depth };
    for (int i = 0; i < g->boat_count; i++) {
        Boat *b = &g->boats[i];
        if (glm_vec3_distance(b->pos, g->eye) > (depth ? 60.0f : 220.0f))
            continue;
        mat4 xf;
        transform_matrix(&b->xf, xf);
        model_draw(&g->boat_model, NULL, prog, vp, xf, &dp);
        /* the oars: shipped along the sides, or pulling when someone rows */
        for (int s = -1; s <= 1; s += 2) {
            mat4 o;
            glm_mat4_copy(xf, o);
            glm_translate(o, (vec3){s * 0.62f, 0.65f, 0.25f});
            if (b->occupied) {
                float a = b->row_t * 2.0f * GLM_PIf;
                glm_rotate_y(o, s * (0.2f + sinf(a) * 0.55f), o);
                glm_rotate_z(o, s * (1.35f - 0.25f * cosf(a)), o);
            } else {
                glm_rotate_y(o, s * 1.45f, o);
                glm_rotate_z(o, s * 1.52f, o);
            }
            glm_translate(o, (vec3){0, -0.7f, 0});
            model_draw(&g->oar_model, NULL, prog, vp, o, &dp);
        }
    }
}

/* ---------- moving, whichever way ---------- */

void player_move(Game *g, float dt)
{
    refresh_limits(g);
    if (g->cam.mode != CAM_FIRST_PERSON)
        return;
    if (g->cam.flying) {
        /* noclip: the camera moves itself */
        glm_vec3_copy((vec3){g->pos[0], g->pos[1] + EYE_HEIGHT, g->pos[2]}, g->cam.position);
        camera_update(&g->cam, dt);
        glm_vec3_copy(g->cam.position, g->pos);
        g->pos[1] -= EYE_HEIGHT;
        glm_vec3_zero(g->vel);
        g->swimming = false;
        return;
    }
    if (g->boat_in >= 0) {
        row(g, dt);
        g->swimming = g->underwater = false;
        g->breath = fminf(g->max_breath, g->breath + dt * 4.0f);
        return;
    }

    float surface, depth;
    bool sea = sea_at(g, g->pos[0], g->pos[2], &surface, &depth);
    g->surface_y = sea ? surface : -1000.0f;
    float under = sea ? surface - g->pos[1] : 0.0f;
    bool was = g->swimming;
    /* start swimming once the water's chest deep and there's depth to swim in */
    if (sea && depth > SWIM_DEPTH && under > (was ? 1.0f : 1.25f))
        g->swimming = true;
    else if (!sea || under < 0.95f || depth < 1.1f)
        g->swimming = false;
    if (g->swimming && !was) {
        if (g->vel[1] < -3.0f) {
            audio_play(SFX_SPLASH, fminf(1.0f, -g->vel[1] / 10.0f));
            splash_at(g, (vec3){g->pos[0], surface, g->pos[2]}, 30, 3.0f);
        }
        g->vel[1] *= 0.3f;
        fishing_cancel(g);
    }

    if (g->swimming)
        swim(g, dt, surface, depth);
    else
        walk(g, dt);

    /* breath: only counts while your head's under */
    float head_y = g->pos[1] + EYE_HEIGHT - g->crouch_amount * 0.6f;
    bool was_under = g->underwater;
    g->underwater = sea && head_y < surface - 0.05f;
    bool gills = g->gills_t > 0.0f || g->first_tide;
    if (g->underwater && !gills) {
        g->breath -= dt * (wears(g, ITEM_DIVING_HELM) ? 0.25f : 1.0f);
        if (g->breath <= 0.0f) {
            g->breath = 0.0f;
            if (frand() < dt * 1.5f) {
                hurt_player(g, g->pos, 6.0f);
                audio_play(SFX_DROWN, 0.8f);
            }
        }
    } else {
        if (was_under && g->breath < g->max_breath * 0.4f)
            audio_play(SFX_GASP, 0.9f);
        g->breath = fminf(g->max_breath, g->breath + dt * (g->swimming ? 3.0f : 6.0f));
    }
    /* stamina comes back on land, in the shallows, or treading water in a boat */
    if (!g->swimming)
        g->stamina = fminf(g->max_stamina, g->stamina + dt * 6.0f);
    if (g->swimming && g->stamina <= 0.0f && !gills && g->underwater && frand() < dt)
        hurt_player(g, g->pos, 4.0f);
}

/* ---------- the camera over your shoulder ---------- */

void player_third_camera(Game *g, float dt)
{
    (void)dt;
    vec3 dir, right;
    look_dir(g, dir);
    glm_vec3_copy((vec3){cosf(g->cam.fp_yaw), 0.0f, -sinf(g->cam.fp_yaw)}, right);
    vec3 target = { g->pos[0], g->pos[1] + 1.55f - g->crouch_amount * 0.5f, g->pos[2] };
    if (g->boat_in >= 0)
        target[1] += 0.2f;
    float dist = g->cam_dist;
    vec3 want;
    glm_vec3_copy(target, want);
    glm_vec3_muladds(dir, -dist, want);
    glm_vec3_muladds(right, 0.55f, want);
    want[1] += 0.3f;
    /* pull in if a wall is in the way */
    if (!level_line_clear(&g->level, target, want)) {
        float lo = 0.0f, hi = 1.0f;
        for (int i = 0; i < 8; i++) {
            float mid = (lo + hi) * 0.5f;
            vec3 p;
            glm_vec3_lerp(target, want, mid, p);
            if (level_line_clear(&g->level, target, p))
                lo = mid;
            else
                hi = mid;
        }
        glm_vec3_lerp(target, want, lo * 0.92f, want);
    }
    float ground = ground_at(g, want[0], want[2], want[1] + 0.5f);
    if (want[1] < ground + 0.3f)
        want[1] = ground + 0.3f;
    glm_vec3_copy(want, g->eye);
}

/* ---------- your body, seen from outside ---------- */

static const char *AV_FILES[AV_COUNT] = { "Rogue", "Knight", "Barbarian", "Mage" };

/* which piece of whose outfit you're wearing */
typedef struct {
    ItemId item;
    int model;
    const char *nodes[5];
} Piece;

static const Piece PIECES[] = {
    { ITEM_IRON_HELM, AV_KNIGHT, { "Knight_Helmet" } },
    { ITEM_KNIGHT_PLATE, AV_KNIGHT, { "Knight_Body", "Knight_ArmLeft", "Knight_ArmRight", "Knight_LegLeft", "Knight_LegRight" } },
    { ITEM_RED_CAPE, AV_KNIGHT, { "Knight_Cape" } },
    { ITEM_FUR_HAT, AV_BARBARIAN, { "Barbarian_Hat" } },
    { ITEM_BARBARIAN_FURS, AV_BARBARIAN, { "Barbarian_Body", "Barbarian_ArmLeft", "Barbarian_ArmRight", "Barbarian_LegLeft", "Barbarian_LegRight" } },
    { ITEM_FUR_CLOAK, AV_BARBARIAN, { "Barbarian_Cape" } },
    { ITEM_WIZARD_HAT, AV_MAGE, { "Mage_Hat" } },
    { ITEM_MAGISTER_ROBES, AV_MAGE, { "Mage_Body", "Mage_ArmLeft", "Mage_ArmRight", "Mage_LegLeft", "Mage_LegRight" } },
    { ITEM_STARRY_CAPE, AV_MAGE, { "Mage_Cape" } },
};

static int find_anim(const Model *m, const char *name)
{
    return model_find_anim(m, name);
}

enum {
    AN_IDLE, AN_WALK, AN_RUN, AN_BACK, AN_JUMP, AN_CHOP, AN_STAB, AN_SLICE, AN_OVERHEAD, AN_PUNCH,
    AN_BASH, AN_BLOCK, AN_CAST, AN_THROW, AN_USE, AN_HIT, AN_DEATH, AN_SIT, AN_CHEER, AN_INTERACT,
    AN_COUNT
};
static int AN[AN_COUNT];

void player_load(Game *g)
{
    Avatar *av = &g->avatar;
    for (int i = 0; i < AV_COUNT; i++) {
        char path[128];
        snprintf(path, sizeof path, "assets/kaykit/%s.glb", AV_FILES[i]);
        load_or_die(&av->models[i], path, NULL);
        Transform fix = { .scale = { 0.8f, 0.8f, 0.8f } };
        model_adjust(&av->models[i], &fix, false);
        pose_init(&av->poses[i], &av->models[i]);
        av->skip[i] = calloc(av->models[i].node_count, 1);
        av->bone_map[i] = malloc(av->models[i].node_count * sizeof(int));
        for (int n = 0; n < av->models[i].node_count; n++)
            av->bone_map[i][n] = model_find_node(&av->models[0], av->models[i].nodes[n].name);
    }
    const Model *m = &av->models[AV_ROGUE];
    static const char *names[AN_COUNT] = {
        "Idle", "Walking_A", "Running_A", "Walking_Backwards", "Jump_Idle", "1H_Melee_Attack_Chop",
        "1H_Melee_Attack_Stab", "1H_Melee_Attack_Slice_Diagonal", "2H_Melee_Attack_Chop",
        "Unarmed_Melee_Attack_Punch_A", "Block_Attack", "Blocking", "Spellcast_Shoot", "Throw", "Use_Item",
        "Hit_A", "Death_A", "Sit_Floor_Idle", "Cheer", "Interact",
    };
    for (int i = 0; i < AN_COUNT; i++)
        AN[i] = find_anim(m, names[i]);
    av->rig.hand_r = model_find_node(m, "handslot.r");
    av->rig.hand_l = model_find_node(m, "handslot.l");
    av->head = model_find_node(m, "head");

    shape_boat(&g->boat_model);
    /* an oar: a long shaft with a flat blade, pivoting at its middle */
    {
        MeshBuilder mb[2];
        Material mats[2];
        mb_init(&mb[0]);
        mb_init(&mb[1]);
        material_color(&mats[0], 0.55f, 0.4f, 0.25f, 0.7f, 0.0f);
        material_color(&mats[1], 0.1f, 0.42f, 0.4f, 0.6f, 0.0f);
        mat4 xf = GLM_MAT4_IDENTITY_INIT;
        glm_translate(xf, (vec3){0, -0.6f, 0});
        mb_cylinder(&mb[0], xf, 0.025f, 0.022f, 2.2f, 8, 1.0f);
        glm_translate_make(xf, (vec3){0, -0.45f, 0});
        mb_box(&mb[1], xf, (vec3){0.08f, 0.2f, 0.012f}, 1.0f);
        model_from_builders(&g->oar_model, mb, mats, 2);
        mb_free(&mb[0]);
        mb_free(&mb[1]);
    }
    g->boat_in = -1;
    g->cam_dist = 3.4f;
    refresh_limits(g);
    g->stamina = g->max_stamina;
    g->breath = g->max_breath;
}

void player_free(Game *g)
{
    Avatar *av = &g->avatar;
    for (int i = 0; i < AV_COUNT; i++) {
        pose_free(&av->poses[i]);
        model_free(&av->models[i]);
        free(av->skip[i]);
        free(av->bone_map[i]);
    }
    model_free(&g->boat_model);
    model_free(&g->oar_model);
}

void equip_item(Game *g, ItemId id)
{
    const ItemDef *it = &ITEMS[id];
    if (it->kind != KIND_ARMOR)
        return;
    if (g->equip[it->slot] == id) {
        g->equip[it->slot] = ITEM_NONE;
        message(g, COL_TEXT, "You take off the %s.", it->name);
    } else {
        g->equip[it->slot] = id;
        message(g, COL_TEXT, "You put on the %s.", it->name);
    }
    audio_play(SFX_EQUIP, 1.0f);
}

/* which of each model's pieces show, for what you're wearing */
static void dress(Game *g)
{
    Avatar *av = &g->avatar;
    for (int i = 0; i < AV_COUNT; i++) {
        const Model *m = &av->models[i];
        for (int n = 0; n < m->node_count; n++)
            av->skip[i][n] = 1;
        if (i != AV_ROGUE)
            continue;
        /* your own clothes, unless armour covers them */
        bool body = g->equip[EQ_BODY] != ITEM_NONE;
        static const char *own[] = { "Rogue_Head", "Rogue_Body", "Rogue_ArmLeft", "Rogue_ArmRight", "Rogue_LegLeft", "Rogue_LegRight", "Rogue_Cape" };
        for (size_t k = 0; k < sizeof own / sizeof own[0]; k++) {
            int n = model_find_node(m, own[k]);
            if (n < 0)
                continue;
            bool covered = (k >= 1 && k <= 5 && body) || (k == 6 && g->equip[EQ_BACK] != ITEM_NONE);
            av->skip[i][n] = covered;
        }
    }
    for (size_t p = 0; p < sizeof PIECES / sizeof PIECES[0]; p++) {
        const Piece *pc = &PIECES[p];
        if (g->equip[ITEMS[pc->item].slot] != pc->item)
            continue;
        for (int k = 0; k < 5 && pc->nodes[k]; k++) {
            int n = model_find_node(&av->models[pc->model], pc->nodes[k]);
            if (n >= 0)
                av->skip[pc->model][n] = 0;
        }
    }
}

/* the arms and legs of a swimming stroke, laid over whatever pose is there */
static void stroke(Avatar *av, float phase, float amount)
{
    const Model *m = &av->models[AV_ROGUE];
    Pose *p = &av->poses[AV_ROGUE];
    static const char *bones[4] = { "upperarm.l", "upperarm.r", "upperleg.l", "upperleg.r" };
    for (int i = 0; i < 4; i++) {
        int n = model_find_node(m, bones[i]);
        if (n < 0)
            continue;
        float a = i < 2 ? sinf(phase * 2.0f + (i ? GLM_PIf : 0.0f)) * 1.3f * amount
                        : sinf(phase * 5.0f + (i == 3 ? GLM_PIf : 0.0f)) * 0.35f * amount;
        versor q;
        glm_quatv(q, a, (vec3){1, 0, 0});
        glm_quat_mul(p->r[n], q, p->r[n]);
    }
}

void avatar_update(Game *g, float dt)
{
    Avatar *av = &g->avatar;
    const Model *m = &av->models[AV_ROGUE];
    Pose *p = &av->poses[AV_ROGUE];
    av->anim_t += dt;
    dress(g);

    /* which way the body faces: where you walk, or where you aim */
    float speed = sqrtf(g->vel[0] * g->vel[0] + g->vel[2] * g->vel[2]);
    float want = g->cam.fp_yaw + GLM_PIf + avatar_test_turn;
    bool acting = g->action != ACT_IDLE || fishing_busy(g);
    if (g->boat_in >= 0)
        want = g->boats[g->boat_in].yaw;
    else if (g->third_person && speed > 0.5f && !acting)
        want = atan2f(g->vel[0], g->vel[2]);
    av->yaw += wrap_angle(want - av->yaw) * smooth_k(10.0f, dt);

    int anim = AN[AN_IDLE];
    float t = av->anim_t, rate = 1.0f;
    bool loop = true;
    if (g->dead) {
        anim = AN[AN_DEATH];
        t = g->dead_t;
        loop = false;
    } else if (g->boat_in >= 0) {
        anim = AN[AN_SIT];
    } else if (g->swimming) {
        anim = AN[AN_IDLE];
    } else if (g->action == ACT_SWING) {
        static const int by_style[5] = { AN_PUNCH, AN_STAB, AN_SLICE, AN_OVERHEAD, AN_BASH };
        ItemId held = g->slots[g->selected].id;
        anim = AN[by_style[held != ITEM_NONE ? ITEMS[held].swing : SWING_PUNCH]];
        t = g->action_t * 0.9f;
        loop = false;
    } else if (g->action == ACT_CAST) {
        anim = AN[AN_CAST];
        t = g->action_t * 1.1f;
        loop = false;
    } else if (g->action == ACT_THROW || g->fish.state == FISH_CAST) {
        anim = AN[AN_THROW];
        t = g->action == ACT_THROW ? g->action_t : g->fish.t;
        loop = false;
    } else if (g->action == ACT_USE) {
        anim = AN[AN_USE];
        t = g->action_t * 1.4f;
        loop = false;
    } else if (g->action == ACT_BLOCK) {
        anim = AN[AN_BLOCK];
    } else if (!g->on_ground) {
        anim = AN[AN_JUMP];
    } else if (speed > 0.4f) {
        vec3 facing = { sinf(av->yaw), 0, cosf(av->yaw) };
        float along = (g->vel[0] * facing[0] + g->vel[2] * facing[2]) / speed;
        anim = along < -0.5f ? AN[AN_BACK] : speed > 5.2f ? AN[AN_RUN] : AN[AN_WALK];
        rate = anim == AN[AN_RUN] ? speed / 6.0f : speed / 3.0f;
        rate = glm_clamp(rate, 0.6f, 1.5f);
    }
    if (anim != av->anim) {
        av->last_anim = av->anim;
        av->anim = anim;
        av->blend = 0.0f;
        if (!loop)
            av->anim_t = 0.0f;
    }
    av->blend = fminf(1.0f, av->blend + dt / 0.18f);

    pose_reset(p, m);
    if (av->blend < 1.0f)
        anim_apply(m, av->last_anim, av->anim_t * rate, true, 1.0f, p);
    anim_apply(m, anim, loop ? t * rate : t, loop, av->blend < 1.0f ? av->blend : 1.0f, p);
    if (g->hurt_t > 0.0f && !g->dead)
        anim_apply(m, AN[AN_HIT], 0.45f - g->hurt_t, false, 0.5f, p);
    if (g->swimming)
        stroke(av, g->swim_phase * 2.0f, glm_clamp(speed / 2.0f, 0.3f, 1.0f));
    if (g->boat_in >= 0)
        stroke(av, g->boats[g->boat_in].row_t * GLM_PIf, 0.4f);
    pose_update(m, p);

    /* the other outfits follow the rogue's skeleton bone for bone */
    for (int i = 1; i < AV_COUNT; i++) {
        Pose *q = &av->poses[i];
        for (int n = 0; n < av->models[i].node_count; n++) {
            int j = av->bone_map[i][n];
            if (j < 0)
                continue;
            glm_vec3_copy(p->t[j], q->t[n]);
            glm_vec4_copy(p->r[j], q->r[n]);
            glm_vec3_copy(p->s[j], q->s[n]);
        }
        pose_update(&av->models[i], q);
    }
}

/* where the body stands: your feet, facing the way it faces; swimming, it lies forward */
static void body_matrix(Game *g, mat4 out)
{
    Avatar *av = &g->avatar;
    Transform t = TRANSFORM_AT(g->pos, av->yaw);
    if (g->swimming) {
        float speed = sqrtf(g->vel[0] * g->vel[0] + g->vel[2] * g->vel[2]);
        t.pitch = glm_clamp(speed / 2.5f, 0.25f, 1.0f) * 1.25f;
        t.pos[1] += 0.9f;
    }
    if (g->crouching && !g->swimming)
        t.pos[1] -= g->crouch_amount * 0.25f;
    if (g->boat_in >= 0)
        t.pos[1] += 0.55f;
    transform_matrix(&t, out);
}

/* something from the item list held in a KayKit hand slot */
static void draw_in_hand(Game *g, ItemId id, int bone, GLuint prog, mat4 vp, mat4 body, const DrawParams *dp)
{
    if (id == ITEM_NONE || !ITEMS[id].loaded || bone < 0)
        return;
    Avatar *av = &g->avatar;
    mat4 xf;
    glm_mat4_mul(body, av->poses[AV_ROGUE].world[bone], xf);
    glm_rotate_y(xf, GLM_PIf, xf);
    glm_scale_uni(xf, 1.25f / 0.8f);        /* back to meters, and a touch bigger to suit the big hands */
    if (id == ITEM_SHIELD) {
        glm_rotate_y(xf, GLM_PI_2f, xf);
        glm_translate(xf, (vec3){0.0f, -0.1f, -0.08f});
    }
    glm_mat4_mul(xf, (vec4 *)ITEMS[id].hold, xf);
    model_draw(&ITEMS[id].model, NULL, prog, vp, xf, dp);
}

/* hats, helmets and the gas mask ride on the head bone */
static void draw_on_head(Game *g, const Model *m, float size, vec3 offset, GLuint prog, mat4 vp, mat4 body, const DrawParams *dp)
{
    Avatar *av = &g->avatar;
    mat4 xf, fit;
    glm_mat4_mul(body, av->poses[AV_ROGUE].world[av->head], xf);
    glm_translate(xf, offset);
    model_fit(m, size, fit);
    glm_mat4_mul(xf, fit, xf);
    model_draw(m, NULL, prog, vp, xf, dp);
}

void avatar_draw(Game *g, GLuint prog, mat4 vp, bool depth)
{
    Avatar *av = &g->avatar;
    DrawParams dp = { .depth_only = depth };
    if (!depth && g->shadow_t > 0.0f)
        glm_vec4_copy((vec4){0.02f, 0.0f, 0.05f, 0.0f}, dp.tint);
    mat4 place, body;
    body_matrix(g, place);
    model_matrix(&av->models[AV_ROGUE], place, body);
    for (int i = 0; i < AV_COUNT; i++) {
        dp.skip = av->skip[i];
        model_draw(&av->models[i], &av->poses[i], prog, vp, place, &dp);
    }
    dp.skip = NULL;

    /* what you hold: the selected item in the right hand, lights and shields in the left */
    ItemId held = g->slots[g->selected].id;
    if (held == ITEM_TORCH || held == ITEM_LANTERN)
        held = ITEM_NONE;
    if (held != ITEM_NONE && ITEMS[held].kind != KIND_SPELL && ITEMS[held].kind != KIND_ARMOR)
        draw_in_hand(g, held, av->rig.hand_r, prog, vp, body, &dp);
    if (g->left == LEFT_TORCH)
        draw_in_hand(g, ITEM_TORCH, av->rig.hand_l, prog, vp, body, &dp);
    else if (g->left == LEFT_LANTERN)
        draw_in_hand(g, ITEM_LANTERN, av->rig.hand_l, prog, vp, body, &dp);
    else if (g->left == LEFT_SHIELD)
        draw_in_hand(g, ITEM_SHIELD, av->rig.hand_l, prog, vp, body, &dp);

    /* worn on the head */
    if (g->equip[EQ_HEAD] == ITEM_FISHER_HAT)
        draw_on_head(g, &ITEMS[ITEM_FISHER_HAT].model, 0.95f, (vec3){0, 0.95f, 0.0f}, prog, vp, body, &dp);
    if (g->equip[EQ_HEAD] == ITEM_DIVING_HELM)
        draw_on_head(g, &ITEMS[ITEM_DIVING_HELM].model, 1.35f, (vec3){0, 0.45f, 0.02f}, prog, vp, body, &dp);
    if (g->mask_on)
        draw_on_head(g, &ITEMS[ITEM_GAS_MASK].model, 1.05f, (vec3){0, 0.1f, 0.42f}, prog, vp, body, &dp);
}
