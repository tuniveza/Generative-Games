#version 450 core

/* a slime's jelly: the model's vertices, rippling and sloshing over time */

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec4 a_tangent;

layout(location = 0)  uniform mat4 u_mvp;
layout(location = 1)  uniform mat4 u_model;
layout(location = 10) uniform bool u_skinned;       /* set by model_draw; slimes never are */
layout(location = 12) uniform mat3 u_normal_matrix;
layout(location = 13) uniform vec4 u_slime;     /* x = its own time offset, y = how hard it's sloshing */

out vec3 v_world_pos;
out vec3 v_normal;
out vec3 v_local;

void main()
{
    float t = u_misc.x * 3.2 + u_slime.x;
    /* waves running over the surface, stronger toward the top, a sag at the bottom */
    float ripple = sin(a_pos.y * 9.0 + t * 1.6) * 0.025 + sin(a_pos.x * 13.0 - t * 1.3 + a_pos.z * 7.0) * 0.018
                 + sin(a_pos.z * 11.0 + t * 2.1) * 0.015;
    ripple *= (0.6 + u_slime.y) * smoothstep(0.0, 0.5, a_pos.y) * (u_skinned ? 0.0 : 1.0);
    vec3 p = a_pos + a_normal * ripple;
    p.xz *= 1.0 + 0.06 * smoothstep(0.35, 0.0, a_pos.y);        /* it spreads where it meets the ground */
    v_local = a_pos;
    v_world_pos = vec3(u_model * vec4(p, 1.0));
    v_normal = normalize(u_normal_matrix * (a_normal + vec3(cos(a_pos.y * 9.0 + t * 1.6), 0.0, cos(a_pos.x * 13.0 - t * 1.3)) * ripple * 3.0));
    gl_Position = u_mvp * vec4(p, 1.0);
}
