#version 450 core

/* bloom step 1: shrink the image, keeping only bright parts on the first step
 * (13-tap filter from "Next Generation Post Processing in Call of Duty") */

in vec2 v_uv;

layout(binding = 0) uniform sampler2D u_src;
layout(location = 0) uniform vec2 u_texel;      /* 1 / source size */
layout(location = 1) uniform bool u_first;

out vec4 f_col;

vec3 tap(float x, float y) { return texture(u_src, v_uv + vec2(x, y) * u_texel).rgb; }

void main()
{
    vec3 a = tap(-2, 2), b = tap(0, 2), c = tap(2, 2);
    vec3 d = tap(-2, 0), e = tap(0, 0), f = tap(2, 0);
    vec3 g = tap(-2, -2), h = tap(0, -2), i = tap(2, -2);
    vec3 j = tap(-1, 1), k = tap(1, 1), l = tap(-1, -1), m = tap(1, -1);

    vec3 color = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625
               + (j + k + l + m) * 0.125;

    if (u_first) {
        /* soft threshold: only light brighter than ~1 blooms */
        float brightness = max(color.r, max(color.g, color.b));
        color *= smoothstep(0.8, 2.0, brightness);
    }
    f_col = vec4(color, 1.0);
}
