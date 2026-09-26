#ifndef WEATHER_H
#define WEATHER_H

#include <glad/gl.h>
#include <cglm/cglm.h>
#include <stdbool.h>

#include "level.h"
#include "particles.h"
#include "renderer.h"
#include "terrain.h"

typedef enum { WEATHER_RAIN, WEATHER_STORM, WEATHER_FOG, WEATHER_CLEAR, WEATHER_COUNT } WeatherType;

typedef struct {
    float x, y, z, len;     /* one raindrop: position and streak length */
} Drop;

typedef struct {
    WeatherType type;
    bool auto_change;       /* drift to a new weather every few minutes */
    float change_t;

    /* smoothed toward the current type's targets */
    float rain, cloud, fog, wet;

    /* lightning (storms) */
    float flash;            /* 0..1 */
    float next_bolt;
    float thunder_delay;    /* > 0: thunder is on its way */
    float thunder_volume;
    bool thunder_now;       /* set for one frame: play the thunder */
    vec3 bolt_dir;

    Drop *drops;
    int drop_count;
    vec3 fall;              /* direction raindrops fall */
    GLuint vao, vbo, prog;
} Weather;

void weather_init(Weather *w);
void weather_free(Weather *w);
void weather_set(Weather *w, WeatherType type);
const char *weather_name(WeatherType type);

/* moves the rain around the camera; splashes land on roofs, floors and puddles */
void weather_update(Weather *w, float dt, vec3 camera, const Level *lv, const Terrain *t, Particles *ps);

/* the day/night environment, adjusted for clouds, fog, wet ground and lightning */
void weather_apply(const Weather *w, const Environment *base, Environment *out);

void weather_draw(Weather *w, vec3 camera);

#endif
