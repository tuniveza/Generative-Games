#include "game.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The pause menu (Esc / Start), the options that are saved to settings.cfg, and the
 * gamepad's sticks. */

#define SETTINGS_FILE "settings.cfg"

/* ---------- settings ---------- */

void settings_load(Settings *s)
{
    *s = (Settings){ .mouse_sens = 1.0f, .fov = 75.0f, .music_volume = 0.8f, .sfx_volume = 1.0f,
                     .auto_weather = true };
    FILE *f = fopen(SETTINGS_FILE, "r");
    if (!f)
        return;
    char key[64];
    float v;
    while (fscanf(f, " %63[^=]=%f", key, &v) == 2) {
        if (!strcmp(key, "mouse_sensitivity")) s->mouse_sens = glm_clamp(v, 0.1f, 5.0f);
        else if (!strcmp(key, "invert_y")) s->invert_y = v != 0.0f;
        else if (!strcmp(key, "fov")) s->fov = glm_clamp(v, 55.0f, 110.0f);
        else if (!strcmp(key, "music_volume")) s->music_volume = glm_clamp(v, 0.0f, 1.0f);
        else if (!strcmp(key, "effects_volume")) s->sfx_volume = glm_clamp(v, 0.0f, 1.0f);
        else if (!strcmp(key, "changing_weather")) s->auto_weather = v != 0.0f;
    }
    fclose(f);
}

void settings_save(const Settings *s)
{
    FILE *f = fopen(SETTINGS_FILE, "w");
    if (!f)
        return;
    fprintf(f, "mouse_sensitivity=%.2f\ninvert_y=%d\nfov=%.0f\nmusic_volume=%.2f\neffects_volume=%.2f\nchanging_weather=%d\n",
            s->mouse_sens, s->invert_y, s->fov, s->music_volume, s->sfx_volume, s->auto_weather);
    fclose(f);
}

void settings_apply(Game *g)
{
    g->cam.sensitivity = g->settings.mouse_sens;
    g->cam.invert_y = g->settings.invert_y;
    g->weather.auto_change = g->settings.auto_weather;
    audio_set_volumes(g->settings.music_volume, g->settings.sfx_volume);
}

/* ---------- the menu ---------- */

typedef enum { ITEM_BUTTON, ITEM_SLIDER, ITEM_TOGGLE } MenuItemKind;

typedef struct {
    const char *label;
    MenuItemKind kind;
    float *value;
    float min, max;
    bool *flag;
    int action;             /* buttons: what they do */
} MenuItem;

enum { ACT_RESUME, ACT_OPTIONS, ACT_CONTROLS, ACT_QUIT, ACT_BACK };

static int items_for(Game *g, MenuItem *out)
{
    Settings *s = &g->settings;
    int n = 0;
    switch (g->menu) {
    case MENU_MAIN:
        out[n++] = (MenuItem){ "Resume", ITEM_BUTTON, .action = ACT_RESUME };
        out[n++] = (MenuItem){ "Options", ITEM_BUTTON, .action = ACT_OPTIONS };
        out[n++] = (MenuItem){ "Controls", ITEM_BUTTON, .action = ACT_CONTROLS };
        out[n++] = (MenuItem){ "Quit to desktop", ITEM_BUTTON, .action = ACT_QUIT };
        break;
    case MENU_OPTIONS:
        out[n++] = (MenuItem){ "Mouse sensitivity", ITEM_SLIDER, .value = &s->mouse_sens, .min = 0.2f, .max = 3.0f };
        out[n++] = (MenuItem){ "Invert mouse Y", ITEM_TOGGLE, .flag = &s->invert_y };
        out[n++] = (MenuItem){ "Field of view", ITEM_SLIDER, .value = &s->fov, .min = 60.0f, .max = 100.0f };
        out[n++] = (MenuItem){ "Music volume", ITEM_SLIDER, .value = &s->music_volume, .min = 0.0f, .max = 1.0f };
        out[n++] = (MenuItem){ "Effects volume", ITEM_SLIDER, .value = &s->sfx_volume, .min = 0.0f, .max = 1.0f };
        out[n++] = (MenuItem){ "Changing weather", ITEM_TOGGLE, .flag = &s->auto_weather };
        out[n++] = (MenuItem){ "Back", ITEM_BUTTON, .action = ACT_BACK };
        break;
    case MENU_CONTROLS:
        out[n++] = (MenuItem){ "Back", ITEM_BUTTON, .action = ACT_BACK };
        break;
    default:
        break;
    }
    return n;
}

/* layout, shared by drawing and the mouse */
#define PANEL_W 640.0f
#define ROW_H 54.0f

static void panel_rect(Game *g, int count, float *x, float *y, float *h)
{
    *h = g->menu == MENU_CONTROLS ? 560.0f : 120.0f + count * ROW_H;
    *x = (g->width - (g->menu == MENU_CONTROLS ? 980.0f : PANEL_W)) * 0.5f;
    *y = (g->height - *h) * 0.5f;
}

static void row_rect(Game *g, int count, int i, float *x, float *y, float *w)
{
    float px, py, ph;
    panel_rect(g, count, &px, &py, &ph);
    if (g->menu == MENU_CONTROLS) {
        *x = px + 40;
        *y = py + ph - 80;
        *w = 200;
        return;
    }
    *x = px + 40;
    *y = py + 90 + i * ROW_H;
    *w = PANEL_W - 80;
}

static int hit_row(Game *g, float mx, float my)
{
    MenuItem items[16];
    int n = items_for(g, items);
    for (int i = 0; i < n; i++) {
        float x, y, w;
        row_rect(g, n, i, &x, &y, &w);
        if (mx >= x && mx <= x + w && my >= y && my <= y + ROW_H - 8)
            return i;
    }
    return -1;
}

void menu_open(Game *g, SDL_Window *win)
{
    g->menu = MENU_MAIN;
    g->menu_hover = 0;
    g->menu_drag = -1;
    g->inv_open = false;
    SDL_SetWindowRelativeMouseMode(win, false);
    audio_play(SFX_CLICK, 1.0f);
}

void menu_close(Game *g, SDL_Window *win)
{
    g->menu = MENU_NONE;
    settings_save(&g->settings);
    SDL_SetWindowRelativeMouseMode(win, g->cam.mode == CAM_FIRST_PERSON);
}

/* returns false to quit */
static bool activate(Game *g, MenuItem *it, SDL_Window *win)
{
    audio_play(SFX_CLICK, 1.0f);
    if (it->kind == ITEM_TOGGLE) {
        *it->flag = !*it->flag;
        settings_apply(g);
        return true;
    }
    if (it->kind != ITEM_BUTTON)
        return true;
    switch (it->action) {
    case ACT_RESUME: menu_close(g, win); break;
    case ACT_OPTIONS: g->menu = MENU_OPTIONS; g->menu_hover = 0; break;
    case ACT_CONTROLS: g->menu = MENU_CONTROLS; g->menu_hover = 0; break;
    case ACT_BACK:
        settings_save(&g->settings);
        g->menu = MENU_MAIN;
        g->menu_hover = 0;
        break;
    case ACT_QUIT:
        settings_save(&g->settings);
        return false;
    }
    return true;
}

static void slide(Game *g, MenuItem *it, float mx)
{
    float x, y, w;
    MenuItem items[16];
    int n = items_for(g, items);
    row_rect(g, n, g->menu_drag, &x, &y, &w);
    float sx = x + w * 0.45f, sw = w * 0.45f;
    float t = glm_clamp((mx - sx) / sw, 0.0f, 1.0f);
    *it->value = it->min + (it->max - it->min) * t;
    settings_apply(g);
}

static void nudge(Game *g, MenuItem *it, float dir)
{
    if (it->kind == ITEM_SLIDER) {
        *it->value = glm_clamp(*it->value + (it->max - it->min) * 0.05f * dir, it->min, it->max);
        settings_apply(g);
        audio_play(SFX_CLICK, 0.6f);
    } else if (it->kind == ITEM_TOGGLE) {
        *it->flag = dir > 0;
        settings_apply(g);
        audio_play(SFX_CLICK, 0.6f);
    }
}

bool menu_event(Game *g, const SDL_Event *e, SDL_Window *win)
{
    MenuItem items[16];
    int n = items_for(g, items);

    switch (e->type) {
    case SDL_EVENT_MOUSE_MOTION: {
        int h = hit_row(g, g->mouse_x, g->mouse_y);
        if (h >= 0)
            g->menu_hover = h;
        if (g->menu_drag >= 0 && g->menu_drag < n)
            slide(g, &items[g->menu_drag], g->mouse_x);
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        int h = hit_row(g, g->mouse_x, g->mouse_y);
        if (h < 0)
            break;
        g->menu_hover = h;
        if (items[h].kind == ITEM_SLIDER) {
            g->menu_drag = h;
            slide(g, &items[h], g->mouse_x);
        } else {
            return activate(g, &items[h], win);
        }
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP:
        g->menu_drag = -1;
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
        bool pad = e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        int b = pad ? e->gbutton.button : -1;
        SDL_Keycode k = pad ? 0 : e->key.key;
        if (k == SDLK_ESCAPE || b == SDL_GAMEPAD_BUTTON_EAST || b == SDL_GAMEPAD_BUTTON_START) {
            if (g->menu == MENU_MAIN)
                menu_close(g, win);
            else {
                settings_save(&g->settings);
                g->menu = MENU_MAIN;
                g->menu_hover = 0;
            }
        } else if (k == SDLK_UP || k == SDLK_W || b == SDL_GAMEPAD_BUTTON_DPAD_UP) {
            g->menu_hover = (g->menu_hover + n - 1) % n;
        } else if (k == SDLK_DOWN || k == SDLK_S || b == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
            g->menu_hover = (g->menu_hover + 1) % n;
        } else if (k == SDLK_LEFT || k == SDLK_A || b == SDL_GAMEPAD_BUTTON_DPAD_LEFT) {
            nudge(g, &items[g->menu_hover], -1.0f);
        } else if (k == SDLK_RIGHT || k == SDLK_D || b == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
            nudge(g, &items[g->menu_hover], 1.0f);
        } else if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_E || b == SDL_GAMEPAD_BUTTON_SOUTH) {
            if (g->menu_hover < n)
                return activate(g, &items[g->menu_hover], win);
        }
        break;
    }
    }
    return true;
}

static const float COL_PANEL[4] = { 0.04f, 0.035f, 0.03f, 0.9f };

void menu_draw(Game *g)
{
    float dim[4] = { 0, 0, 0, 0.55f };
    ui_rect(0, 0, (float)g->width, (float)g->height, dim);

    MenuItem items[16];
    int n = items_for(g, items);
    float px, py, ph;
    panel_rect(g, n, &px, &py, &ph);
    float pw = g->menu == MENU_CONTROLS ? 980.0f : PANEL_W;
    ui_rect(px, py, pw, ph, COL_PANEL);
    ui_frame(px, py, pw, ph, 2, COL_EDGE);
    const char *title = g->menu == MENU_OPTIONS ? "Options" : g->menu == MENU_CONTROLS ? "Controls" : "Paused";
    ui_text(FONT_LARGE, px + (pw - ui_text_width(FONT_LARGE, title)) * 0.5f, py + 14, COL_GOLD, title);

    if (g->menu == MENU_CONTROLS) {
        static const char *keys[][2] = {
            { "WASD", "move" }, { "Shift", "sprint" }, { "Space", "jump" }, { "C / Ctrl", "crouch (quieter)" },
            { "Mouse", "look" }, { "Left click", "attack / cast / use" }, { "Right click", "block / zoom" },
            { "E", "open, take, light, talk" }, { "T", "light in your off hand" }, { "G", "drop the held item" },
            { "1-9 / wheel", "choose item" }, { "Tab / I", "satchel" }, { "V", "third-person view" },
            { "N", "day / night" }, { "F2", "change the weather" }, { "M", "music on / off" },
            { "H", "these controls" }, { "F12", "screenshot" }, { "Esc", "pause" },
        };
        static const char *pad[][2] = {
            { "Left stick", "move (click: sprint)" }, { "Right stick", "look" }, { "A", "jump" },
            { "B", "crouch" }, { "X", "open, take, talk" }, { "Y", "satchel" },
            { "RT", "attack / cast / use" }, { "LT", "block / zoom" }, { "LB / RB", "choose item" },
            { "D-pad up", "off-hand light" }, { "D-pad down", "drop item" }, { "Start", "pause" },
        };
        float y = py + 90;
        for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++, y += 24) {
            ui_text(FONT_SMALL, px + 40, y, COL_GOLD, keys[i][0]);
            ui_text(FONT_SMALL, px + 190, y, COL_TEXT, keys[i][1]);
        }
        y = py + 90;
        ui_text(FONT_SMALL, px + 540, y - 30, COL_FUN, "Gamepad");
        for (size_t i = 0; i < sizeof pad / sizeof pad[0]; i++, y += 24) {
            ui_text(FONT_SMALL, px + 540, y, COL_GOLD, pad[i][0]);
            ui_text(FONT_SMALL, px + 690, y, COL_TEXT, pad[i][1]);
        }
    }

    for (int i = 0; i < n; i++) {
        float x, y, w;
        row_rect(g, n, i, &x, &y, &w);
        bool hot = i == g->menu_hover;
        float bg[4] = { 0.3f, 0.22f, 0.1f, hot ? 0.55f : 0.0f };
        ui_rect(x, y, w, ROW_H - 8, bg);
        if (hot)
            ui_frame(x, y, w, ROW_H - 8, 1, COL_GOLD);
        const MenuItem *it = &items[i];
        ui_text(FONT_MEDIUM, x + 16, y + 8, hot ? COL_GOLD : COL_TEXT, it->label);
        if (it->kind == ITEM_SLIDER) {
            float sx = x + w * 0.45f, sw = w * 0.45f, sy = y + ROW_H * 0.5f - 7;
            float t = (*it->value - it->min) / (it->max - it->min);
            float track[4] = { 0.15f, 0.12f, 0.1f, 1 }, fill[4] = { 0.75f, 0.55f, 0.25f, 1 };
            ui_rect(sx, sy, sw, 6, track);
            ui_rect(sx, sy, sw * t, 6, fill);
            ui_rect(sx + sw * t - 5, sy - 6, 10, 18, COL_GOLD);
            char v[32];
            if (it->max > 10.0f)
                snprintf(v, sizeof v, "%.0f", *it->value);
            else if (it->max > 1.5f)
                snprintf(v, sizeof v, "%.1fx", *it->value);
            else
                snprintf(v, sizeof v, "%d%%", (int)(*it->value * 100 + 0.5f));
            ui_text(FONT_SMALL, sx + sw + 12, y + 12, COL_TEXT, v);
        } else if (it->kind == ITEM_TOGGLE) {
            const char *v = *it->flag ? "On" : "Off";
            ui_text(FONT_MEDIUM, x + w - 16 - ui_text_width(FONT_MEDIUM, v), y + 8, *it->flag ? COL_GOLD : COL_TEXT, v);
        }
    }
}

/* ---------- gamepad sticks (buttons are events, in game.c) ---------- */

static float deadzone(Sint16 v)
{
    float f = v / 32767.0f;
    if (fabsf(f) < 0.15f)
        return 0.0f;
    return (f - (f > 0 ? 0.15f : -0.15f)) / 0.85f;
}

void gamepad_update(Game *g, float dt)
{
    if (!g->pad || g->menu != MENU_NONE || g->inv_open)
        return;
    float lx = deadzone(SDL_GetGamepadAxis(g->pad, SDL_GAMEPAD_AXIS_RIGHTX));
    float ly = deadzone(SDL_GetGamepadAxis(g->pad, SDL_GAMEPAD_AXIS_RIGHTY));
    /* squared response: fine aim near the middle, fast turns at the edge */
    float sx = lx * fabsf(lx), sy = ly * fabsf(ly);
    float speed = 3.2f * g->settings.mouse_sens;
    if (g->cam.mode == CAM_FIRST_PERSON && !g->dead) {
        g->cam.fp_yaw -= sx * speed * dt;
        g->cam.fp_pitch -= sy * speed * dt * (g->settings.invert_y ? -1.0f : 1.0f);
        g->cam.fp_pitch = glm_clamp(g->cam.fp_pitch, glm_rad(-89.0f), glm_rad(89.0f));
        g->look_dx += sx * 400.0f * dt;
        g->look_dy += sy * 400.0f * dt;
    }

    /* triggers act like the mouse buttons (only when they change, so the mouse still works) */
    bool rt = SDL_GetGamepadAxis(g->pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 12000;
    bool lt = SDL_GetGamepadAxis(g->pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 12000;
    if (rt && !g->pad_rt)
        game_start_action(g);
    if (rt != g->pad_rt)
        g->lmb = rt;
    if (lt != g->pad_lt)
        g->rmb = lt;
    g->pad_rt = rt;
    g->pad_lt = lt;
}
