#include "camera.h"

#define ORBIT_SPEED 0.005f      /* radians per pixel */
#define PAN_SPEED   0.0015f     /* world units per pixel, per unit of distance */
#define ZOOM_STEP   0.9f        /* distance multiplier per wheel notch */

#define LOOK_SPEED  0.0025f     /* radians per pixel */
#define FLY_SPEED   15.0f       /* units per second */
#define SPRINT      3.0f        /* speed multiplier while shift is held */

#define MAX_PITCH   glm_rad(89.0f)

void camera_init(Camera *c, vec3 target, float distance)
{
    *c = (Camera){0};
    c->mode = CAM_ORBIT;
    c->sensitivity = 1.0f;
    glm_vec3_copy(target, c->target);
    c->distance = distance;
}

/* direction the first-person camera looks in */
static void fp_front(const Camera *c, vec3 front)
{
    front[0] = -cosf(c->fp_pitch) * sinf(c->fp_yaw);
    front[1] =  sinf(c->fp_pitch);
    front[2] = -cosf(c->fp_pitch) * cosf(c->fp_yaw);
}

static void orbit_eye(const Camera *c, vec3 eye)
{
    /* spherical -> cartesian, relative to the target */
    vec3 offset = {
        c->distance * cosf(c->pitch) * sinf(c->yaw),
        c->distance * sinf(c->pitch),
        c->distance * cosf(c->pitch) * cosf(c->yaw),
    };
    glm_vec3_add((float *)c->target, offset, eye);
}

void camera_toggle(Camera *c)
{
    if (c->mode == CAM_ORBIT) {
        /* start first person exactly where the orbit camera is, looking the same way */
        orbit_eye(c, c->position);
        c->fp_yaw = c->yaw;
        c->fp_pitch = -c->pitch;
        c->mode = CAM_FIRST_PERSON;
    } else {
        c->mode = CAM_ORBIT;    /* orbit camera is still where you left it */
    }
}

void camera_eye(const Camera *c, vec3 eye)
{
    if (c->mode == CAM_ORBIT)
        orbit_eye(c, eye);
    else
        glm_vec3_copy((float *)c->position, eye);
}

void camera_view(const Camera *c, mat4 view)
{
    vec3 up = {0.0f, 1.0f, 0.0f};
    if (c->mode == CAM_ORBIT) {
        vec3 eye;
        orbit_eye(c, eye);
        glm_lookat(eye, (float *)c->target, up, view);
    } else {
        vec3 front;
        fp_front(c, front);
        glm_look((float *)c->position, front, up, view);
    }
}

static void orbit_event(Camera *c, const SDL_Event *e)
{
    if (e->type == SDL_EVENT_MOUSE_MOTION) {
        if (e->motion.state & SDL_BUTTON_LMASK) {
            c->yaw   -= e->motion.xrel * ORBIT_SPEED;
            c->pitch += e->motion.yrel * ORBIT_SPEED;
            c->pitch = glm_clamp(c->pitch, -MAX_PITCH, MAX_PITCH);
        } else if (e->motion.state & (SDL_BUTTON_RMASK | SDL_BUTTON_MMASK)) {
            /* move the target along the camera's own right and up directions */
            mat4 view;
            camera_view(c, view);
            vec3 right = { view[0][0], view[1][0], view[2][0] };
            vec3 up    = { view[0][1], view[1][1], view[2][1] };
            float s = PAN_SPEED * c->distance;
            glm_vec3_muladds(right, -e->motion.xrel * s, c->target);
            glm_vec3_muladds(up,     e->motion.yrel * s, c->target);
        }
    } else if (e->type == SDL_EVENT_MOUSE_WHEEL) {
        c->distance *= powf(ZOOM_STEP, e->wheel.y);
        c->distance = glm_clamp(c->distance, 0.05f, 100.0f);
    }
}

static void fp_event(Camera *c, const SDL_Event *e)
{
    if (e->type == SDL_EVENT_MOUSE_MOTION) {
        /* the mouse is captured in this mode, so every movement turns the view */
        c->fp_yaw   -= e->motion.xrel * LOOK_SPEED * c->sensitivity;
        c->fp_pitch -= e->motion.yrel * LOOK_SPEED * c->sensitivity * (c->invert_y ? -1.0f : 1.0f);
        c->fp_pitch = glm_clamp(c->fp_pitch, -MAX_PITCH, MAX_PITCH);
    } else if (e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat && e->key.key == SDLK_F9) {
        c->flying = !c->flying;     /* debug: fly through walls */
    }
}

void camera_event(Camera *c, const SDL_Event *e)
{
    if (c->mode == CAM_ORBIT)
        orbit_event(c, e);
    else
        fp_event(c, e);
}

void camera_update(Camera *c, float dt)
{
    if (c->mode != CAM_FIRST_PERSON || !c->flying)
        return;         /* walking is the game's job (collisions, gravity) */

    const bool *keys = SDL_GetKeyboardState(NULL);

    /* flying moves where you look */
    vec3 forward, right;
    fp_front(c, forward);
    right[0] =  cosf(c->fp_yaw);
    right[1] =  0.0f;
    right[2] = -sinf(c->fp_yaw);

    vec3 move = {0.0f, 0.0f, 0.0f};
    if (keys[SDL_SCANCODE_W]) glm_vec3_add(move, forward, move);
    if (keys[SDL_SCANCODE_S]) glm_vec3_sub(move, forward, move);
    if (keys[SDL_SCANCODE_D]) glm_vec3_add(move, right, move);
    if (keys[SDL_SCANCODE_A]) glm_vec3_sub(move, right, move);
    if (keys[SDL_SCANCODE_SPACE]) move[1] += 1.0f;
    if (keys[SDL_SCANCODE_LCTRL]) move[1] -= 1.0f;

    /* normalize so diagonal movement isn't faster */
    if (glm_vec3_norm2(move) > 0.0f) {
        glm_vec3_normalize(move);
        float speed = FLY_SPEED;
        if (keys[SDL_SCANCODE_LSHIFT])
            speed *= SPRINT;
        glm_vec3_muladds(move, speed * dt, c->position);
    }
}
