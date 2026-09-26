#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <cglm/cglm.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio.h"
#include "game.h"
#include "shader.h"

#include <stb_image_write.h>
#include <string.h>

#define WIN_W 1280
#define WIN_H 720

static void GLAD_API_PTR gl_debug_cb(GLenum source, GLenum type, GLuint id, GLenum severity,
                                 GLsizei length, const GLchar *msg, const void *user)
{
    (void)source; (void)type; (void)id; (void)length; (void)user;
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
        return;
    fprintf(stderr, "GL: %s\n", msg);
}

/* --shot: render a scripted moment to a PNG without showing a window (for testing) */
typedef struct {
    const char *shot;
    int frames;
    bool night, give_all, inventory, help, orbit;
    int select;
    int action_at;          /* frame to click the left mouse button, 0 = never */
    int interact_at;        /* frame to press E */
    int hold;               /* item id to put in hand */
    int left;               /* what the left hand carries */
    bool cam_set;
    float cam[5];           /* x y z yaw pitch (degrees) */
    bool fly, third;        /* hover where --cam puts you; see yourself from behind */
    bool block;             /* hold the right mouse button (shield up) */
    int weather;            /* -1 = as it comes */
    float volcano;          /* seconds into Old Ember's cycle, < 0 = from the start */
    int wear[4];            /* item ids to put on */
} Options;

extern float avatar_test_turn;

static void parse_options(Options *o, int argc, char **argv)
{
    memset(o, 0, sizeof *o);
    o->frames = 90;
    o->select = -1;
    o->weather = -1;
    o->volcano = -1.0f;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shot") && i + 1 < argc) o->shot = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) o->frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--night")) o->night = true;
        else if (!strcmp(argv[i], "--give-all")) o->give_all = true;
        else if (!strcmp(argv[i], "--inventory")) o->inventory = true;
        else if (!strcmp(argv[i], "--help-panel")) o->help = true;
        else if (!strcmp(argv[i], "--orbit")) o->orbit = true;
        else if (!strcmp(argv[i], "--select") && i + 1 < argc) o->select = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--action-at") && i + 1 < argc) o->action_at = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--interact-at") && i + 1 < argc) o->interact_at = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--hold") && i + 1 < argc) o->hold = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--left") && i + 1 < argc) o->left = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fly")) o->fly = true;
        else if (!strcmp(argv[i], "--third")) o->third = true;
        else if (!strcmp(argv[i], "--block")) o->block = true;
        else if (!strcmp(argv[i], "--face")) avatar_test_turn = 3.14159f;
        else if (!strcmp(argv[i], "--weather") && i + 1 < argc) o->weather = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--volcano") && i + 1 < argc) o->volcano = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--wear") && i + 1 < argc) {
            for (int k = 0; k < 4; k++)
                if (!o->wear[k]) {
                    o->wear[k] = atoi(argv[++i]);
                    break;
                }
        }
        else if (!strcmp(argv[i], "--cam") && i + 5 < argc) {
            o->cam_set = true;
            for (int k = 0; k < 5; k++)
                o->cam[k] = (float)atof(argv[++i]);
        }
    }
}

static void apply_options(Game *game, const Options *o)
{
    if (o->night)
        game_set_night(game, true);
    if (o->give_all)
        for (int id = ITEM_DAGGER; id < ITEM_COUNT; id++)
            game_give(game, id, ITEMS[id].stacks ? 3 : 1);
    if (o->select >= 0)
        game_select(game, o->select);
    if (o->inventory)
        game->inv_open = true;
    if (o->help)
        game->menu = MENU_OPTIONS;
    if (o->hold > 0 && o->hold < ITEM_COUNT) {
        game_give(game, o->hold, ITEMS[o->hold].stacks ? 3 : 1);
        for (int i = 0; i < INV_SLOTS; i++)
            if (game->slots[i].id == (ItemId)o->hold) {
                Slot tmp = game->slots[1];
                game->slots[1] = game->slots[i];
                game->slots[i] = tmp;
            }
        game_select(game, 1);
    }
    if (o->left == 2)
        game_give(game, ITEM_LANTERN, 1);
    game->left = (LeftHand)o->left;
    if (o->cam_set) {
        glm_vec3_copy((vec3){o->cam[0], o->cam[1], o->cam[2]}, game->pos);
        game->cam.fp_yaw = glm_rad(o->cam[3]);
        game->cam.fp_pitch = glm_rad(o->cam[4]);
    }
    if (o->fly)
        game->cam.flying = true;
    game->rmb = o->block;
    game->third_person = o->third;
    if (o->weather >= 0 && o->weather < WEATHER_COUNT) {
        weather_set(&game->weather, (WeatherType)o->weather);
        weather_snap(&game->weather);
        game->weather.auto_change = false;
    }
    if (o->volcano >= 0.0f)
        game->volcano.t = o->volcano;
    for (int k = 0; k < 4; k++)
        if (o->wear[k] > 0 && o->wear[k] < ITEM_COUNT) {
            game_give(game, o->wear[k], 1);
            equip_item(game, o->wear[k]);
        }
    if (o->orbit) {
        game->cam.mode = CAM_ORBIT;
        glm_vec3_copy(game->pos, game->cam.target);
        game->cam.target[1] += 1.2f;
        game->cam.yaw = game->cam.fp_yaw;
        game->cam.pitch = 0.35f;
        game->cam.distance = 5.0f;
    }
}

int main(int argc, char **argv)
{
    Options opt;
    parse_options(&opt, argc, argv);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window *win = SDL_CreateWindow("The Ruins of Bsg", WIN_W, WIN_H,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                       (opt.shot ? SDL_WINDOW_HIDDEN : 0));
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
        return 1;
    }

    int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
    if (!version) {
        fprintf(stderr, "failed to load OpenGL\n");
        return 1;
    }
    printf("OpenGL %d.%d - %s\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version),
           glGetString(GL_RENDERER));

    SDL_GL_SetSwapInterval(opt.shot ? 0 : 1);

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_debug_cb, NULL);
    glEnable(GL_DEPTH_TEST);

    /* positions only; color comes from the u_color uniform */
    static const float verts[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f,
    };

    /* DSA (4.5): no binding needed to set up buffers and vertex format */
    GLuint vbo;
    glCreateBuffers(1, &vbo);
    glNamedBufferStorage(vbo, sizeof verts, verts, 0);

    GLuint vao;
    glCreateVertexArrays(1, &vao);
    glVertexArrayVertexBuffer(vao, 0, vbo, 0, 3 * sizeof(float));
    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(vao, 0, 0);

    GLuint prog = shader_load("shaders/basic.vert", "shaders/basic.frag");

    /* the game: ruins, creatures, items, hands. the triangle stands in its shrine,
     * at the world origin, between the broken pillars */
    int w, h;
    SDL_GetWindowSizeInPixels(win, &w, &h);
    if (!opt.shot)
        audio_init();           /* synthesizes the sounds; the game stays silent without a device */
    Game *game = malloc(sizeof *game);
    game_init(game, w, h);
    apply_options(game, &opt);
    if (!opt.shot)
        SDL_SetWindowRelativeMouseMode(win, game->cam.mode == CAM_FIRST_PERSON && !game->inv_open);

    Uint64 last_ns = SDL_GetTicksNS();
    int frame = 0;

    bool running = true;
    // before the while loop
     float x = 0.0f;
    float dir = 1.0f;          // +1 = moving right, -1 = moving left
    float speed = 0.08f;
    float limit = 10.0f; 

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (!game_event(game, &e, win))
                running = false;
            if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
                game_resize(game, e.window.data1, e.window.data2);
        }

        float t = SDL_GetTicks() / 1000.0f;

        Uint64 now_ns = SDL_GetTicksNS();
        float dt = (now_ns - last_ns) / 1e9f;   /* seconds since last frame */
        last_ns = now_ns;
        if (opt.shot)
            dt = 1.0f / 60.0f;                  /* scripted runs step at a steady pace */
        frame++;
        if (opt.action_at && frame == opt.action_at)
            game_start_action(game);
        if (opt.interact_at && frame == opt.interact_at) {
            SDL_Event press;
            SDL_zero(press);        /* a union: zero all of it, not just .type */
            press.type = SDL_EVENT_KEY_DOWN;
            press.key.key = SDLK_E;
            press.key.down = true;
            game_event(game, &press, win);
        }

        game_update(game, dt, win);
        game_render_world(game);

        mat4 model, mvp;
        //THIS IS MOVING THE CAMERA; THE TRIANGLE HASN'T MOVED.
        //(the camera now lives in the game: game->view / game->proj)
    
            glm_rotate_make(model, t, (vec3){1.0f, 1.0f, 1.0f
            
            });
            
            x += dir * speed;
            if (x >  limit) { x =  limit; dir = -1.0f; }   // hit the right edge → go left
            if (x < -limit) { x = -limit; dir =  1.0f; }   // hit the left edge  → go right
 
            
     

            
            glm_translate(model, (vec3){x, x, 0.0f});
            glm_scale(model, (vec3){8.0f, 8.0f, 1.0f});
         
        glm_mat4_mul(game->r.frame.view_proj, model, mvp);

        glUseProgram(prog);
        glProgramUniformMatrix4fv(prog, 0, 1, GL_FALSE, (const float *)mvp);
        /* cycle the color over time */
        glProgramUniform3f(prog, 1,
                           0.0f + 0.0f * sinf(t),
                            0.0f + 0.0f * sinf(t + 2.094f),
                           0.0f + 0.0f * sinf(t + 4.189f));
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        game_render_overlay(game);
        renderer_present(&game->r);
        SDL_GL_SwapWindow(win);

        if (opt.shot && frame >= opt.frames) {
            unsigned char *px = malloc((size_t)game->width * game->height * 4);
            renderer_read_pixels(&game->r, px);
            stbi_flip_vertically_on_write(1);
            stbi_write_png(opt.shot, game->width, game->height, 4, px, game->width * 4);
            free(px);
            running = false;
        }
    }

    audio_shutdown();
    game_free(game);
    free(game);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
