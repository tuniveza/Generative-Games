#version 450 core

/* camera-facing quads, one per particle instance */

layout(location = 0) in vec4 a_pos_size;
layout(location = 1) in vec4 a_color;

out vec2 v_uv;
out vec4 v_color;

void main()
{
    vec2 corner = vec2(gl_VertexID & 1, gl_VertexID >> 1) * 2.0 - 1.0;
    v_uv = corner;
    v_color = a_color;

    /* camera right/up from the inverse view-projection: the quad always faces us */
    vec3 right = normalize(vec3(u_inv_view_proj[0]));
    vec3 up    = normalize(vec3(u_inv_view_proj[1]));
    vec3 world = a_pos_size.xyz + (right * corner.x + up * corner.y) * a_pos_size.w;

    /* keep smoke and blood from glowing in the dark: tint by fog like everything else */
    gl_Position = u_view_proj * vec4(world, 1.0);
}
