#version 450 core

/* bloom step 2: grow back up, blurring with a 3x3 tent and adding onto the bigger level */

in vec2 v_uv;

layout(binding = 0) uniform sampler2D u_src;
layout(location = 0) uniform vec2 u_texel;

out vec4 f_col;

void main()
{
    vec3 sum = vec3(0.0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            sum += texture(u_src, v_uv + vec2(x, y) * u_texel).rgb * ((2 - abs(x)) * (2 - abs(y)));
    f_col = vec4(sum / 16.0, 1.0);
}
