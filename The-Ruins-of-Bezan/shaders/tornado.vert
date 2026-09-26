#version 450 core

/* the tornado's funnel: rings of vertices, bent by the wind and turning */

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;         /* x = around, y = up (0 at the foot) */

layout(location = 0) uniform mat4 u_vp;
layout(location = 1) uniform vec4 u_twister;   /* foot position, power 0..1 */

out vec2 v_uv;
out vec3 v_world;

void main()
{
    float t = a_uv.y;
    float time = u_misc.x;
    /* the whole column snakes, more so higher up */
    vec3 p = a_pos;
    p.xz *= 0.7 + 0.3 * u_twister.w;
    p.x += sin(time * 0.6 + t * 3.1) * t * t * 14.0;
    p.z += cos(time * 0.45 + t * 2.3) * t * t * 10.0;
    /* ragged edges */
    p.xz *= 1.0 + 0.12 * sin(a_uv.x * 6.2832 * 3.0 + time * 4.0 + t * 9.0);
    v_world = u_twister.xyz + p;
    v_uv = a_uv;
    gl_Position = u_vp * vec4(v_world, 1.0);
}
