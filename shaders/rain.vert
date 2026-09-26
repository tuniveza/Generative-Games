#version 450 core

/* raindrops as thin streaks, stretched along how they fall */

layout(location = 0) in vec4 a_pos_len;    /* xyz, streak length */

layout(location = 0) uniform vec3 u_fall;   /* direction of fall (normalized) */

out float v_along;
out float v_across;

void main()
{
    vec2 corner = vec2(gl_VertexID & 1, gl_VertexID >> 1);
    vec3 to_cam = normalize(u_camera_pos.xyz - a_pos_len.xyz);
    vec3 side = normalize(cross(u_fall, to_cam)) * 0.006;
    vec3 world = a_pos_len.xyz - u_fall * a_pos_len.w * corner.y + side * (corner.x * 2.0 - 1.0);
    v_along = corner.y;
    v_across = corner.x * 2.0 - 1.0;
    gl_Position = u_view_proj * vec4(world, 1.0);
}
