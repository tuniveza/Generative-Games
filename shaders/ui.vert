#version 450 core

layout(location = 0) in vec4 a_pos_uv;
layout(location = 1) in vec4 a_color;
layout(location = 2) in float a_mode;

layout(location = 0) uniform vec2 u_screen;

out vec2 v_uv;
out vec4 v_color;
flat out int v_mode;

void main()
{
    v_uv = a_pos_uv.zw;
    v_color = a_color;
    v_mode = int(a_mode + 0.5);
    /* pixels (top-left origin) -> clip space */
    vec2 p = a_pos_uv.xy / u_screen * 2.0 - 1.0;
    gl_Position = vec4(p.x, -p.y, 0.0, 1.0);
}
