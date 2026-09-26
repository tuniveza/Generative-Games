#ifndef CAMERA_H
#define CAMERA_H

#include <SDL3/SDL.h>
#include <cglm/cglm.h>
#include <stdbool.h>

typedef enum {
    CAM_ORBIT,          /* 3rd person: left drag = orbit, right/middle drag = pan, wheel = zoom */
    CAM_FIRST_PERSON,   /* mouse = look, WASD = move, shift = sprint,
                           F9 = toggle flying, space / ctrl = up / down while flying */
} CameraMode;

typedef struct {
    CameraMode mode;

    /* orbit: circles around `target` at `distance` */
    vec3 target;
    float yaw;              /* radians, around the Y axis; 0 = looking down -Z */
    float pitch;            /* radians, up/down, clamped just short of +-90 degrees */
    float distance;

    /* first person: stands at `position` looking along (fp_yaw, fp_pitch) */
    vec3 position;
    float fp_yaw;
    float fp_pitch;
    bool flying;            /* false = walking: main keeps you on the ground */

    float sensitivity;      /* mouse look multiplier, 1 = default */
    bool invert_y;
} Camera;

void camera_init(Camera *c, vec3 target, float distance);

/* switches mode; entering first person starts from where the orbit camera is */
void camera_toggle(Camera *c);

void camera_event(Camera *c, const SDL_Event *e);

/* per-frame movement from held keys while flying (first person, F9) */
void camera_update(Camera *c, float dt);

void camera_eye(const Camera *c, vec3 eye);
void camera_view(const Camera *c, mat4 view);

#endif
