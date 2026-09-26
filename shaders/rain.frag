#version 450 core

in float v_along;
in float v_across;

out vec4 f_col;

void main()
{
    float a = (1.0 - abs(v_across)) * sin(v_along * PI) * 0.35;
    vec3 c = u_sky_color.rgb * 1.6 + vec3(0.08) + u_weather.z * 2.0;
    f_col = vec4(c, a);
}
