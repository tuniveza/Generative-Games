#ifndef HANDS_H
#define HANDS_H

#include "items.h"
#include "model.h"

/* what the right hand is doing this frame */
typedef enum {
    ACT_IDLE,
    ACT_SWING,      /* attacking with the held weapon (or fists) */
    ACT_USE,        /* eating / drinking / using a gadget */
    ACT_THROW,
    ACT_BLOCK,      /* shield raised, or binoculars up */
    ACT_CAST,       /* pushing a spell out of the open palm */
} HandAction;

typedef enum { LEFT_EMPTY, LEFT_TORCH, LEFT_LANTERN } LeftHand;

/* everything the hands react to; filled in by the game every frame */
typedef struct {
    ItemId held;            /* right hand; ITEM_NONE = bare fist */
    HandAction action;
    float action_t;         /* 0..1 progress of the action */
    SwingStyle swing;
    LeftHand left;          /* what the left hand carries */
    float reach_t;          /* 0..1 left hand reaching out to open/take something */
    float speed;            /* horizontal speed (m/s) for walk bob */
    bool grounded;
    float look_dx, look_dy; /* mouse movement this frame, for sway */
    float hurt;             /* 0..1 flinch */
    float land;             /* 0..1 dip after landing */
} HandInput;

typedef struct {
    Model model;            /* one skinned hand + forearm */
    Pose pose[2];           /* right, left */
    mat4 world[2];          /* hand -> world, for attaching items */
    float bob_phase;
    float sway_x, sway_y;   /* smoothed look sway */
    float breath;
    float curl[2][5];       /* smoothed finger curls */
    float time;
} Hands;

void hands_init(Hands *h);
void hands_free(Hands *h);

/* animates both hands; inv_view places them relative to the camera */
void hands_update(Hands *h, const HandInput *in, mat4 inv_view, float dt);

/* draws hands and held things (call after renderer_begin_viewmodel) */
void hands_draw(Hands *h, const HandInput *in, GLuint prog, mat4 view_proj);

/* world position of the top of the left-hand torch (for its flame and light) */
void hands_torch_tip(const Hands *h, vec3 out);
/* where the left hand's lantern flame is */
void hands_lantern_pos(const Hands *h, vec3 out);
/* center of the right palm, where spells gather */
void hands_palm_pos(const Hands *h, vec3 out);
/* world position of the right hand (for effects coming from the held item) */
void hands_right_pos(const Hands *h, vec3 out);

#endif
