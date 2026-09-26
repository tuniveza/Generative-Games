#version 450 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;

layout(location = 0) uniform mat4 u_mvp;    /* terrain is already in world space */

out vec3 v_world_pos;
out vec3 v_normal;

void main()
{
    v_world_pos = a_pos;
    v_normal = a_normal;
    gl_Position = u_mvp * vec4(a_pos, 1.0);
}
