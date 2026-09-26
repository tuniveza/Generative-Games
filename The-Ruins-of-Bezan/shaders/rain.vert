#version 450 core

/* raindrops as thin streaks, stretched along how they fall */

layout(location = 0) in vec4 a_pos_len;    /* xyz, streak length */

layout(location = 0) uniform vec3 u_fall;   /* direction of fall (normalized) */
layout(location = 1) uniform float u_width; /* half-width of a streak: thin rain, fat hail */

out float v_along;
out float v_across;

void main()
{
    vec2 corner = vec2(gl_VertexID & 1, gl_VertexID >> 1);
    vec3 to_cam = normalize(u_camera_pos.xyz - a_pos_len.xyz);
    vec3 side = normalize(cross(u_fall, to_cam)) * u_width;
    float len = a_pos_len.w * (u_width > 0.01 ? 0.12 : 1.0);    /* hailstones are short */
    vec3 world = a_pos_len.xyz - u_fall * len * corner.y + side * (corner.x * 2.0 - 1.0);
    v_along = corner.y;
    v_across = corner.x * 2.0 - 1.0;
    gl_Position = u_view_proj * vec4(world, 1.0);
}
