#version 450 core

in vec2 v_uv;
in vec4 v_color;
flat in int v_mode;

layout(binding = 0) uniform sampler2D u_image;
layout(binding = 1) uniform sampler2D u_font;

out vec4 f_col;

void main()
{
    if (v_mode == 0) {                      /* solid color */
        f_col = v_color;
    } else if (v_mode == 1) {               /* text: atlas holds coverage */
        f_col = vec4(v_color.rgb, v_color.a * texture(u_font, v_uv).r);
    } else {
        vec4 c = texture(u_image, v_uv);
        if (v_mode == 3)                    /* sRGB texture came back linear: re-encode */
            c.rgb = pow(c.rgb, vec3(1.0 / 2.2));
        f_col = c * v_color;
    }
}
