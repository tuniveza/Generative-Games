#version 450 core

in vec2 v_uv;
in vec4 v_color;

out vec4 f_col;

void main()
{
    /* soft round blob */
    float d = dot(v_uv, v_uv);
    if (d > 1.0)
        discard;
    float a = (1.0 - d) * (1.0 - d);
    f_col = vec4(v_color.rgb, v_color.a * a);
}
