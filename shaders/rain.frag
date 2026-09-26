#version 450 core

in float v_along;
in float v_across;

layout(location = 2) uniform float u_hail;  /* 1 = white hailstones instead of rain */

out vec4 f_col;

void main()
{
    float a = (1.0 - abs(v_across)) * sin(v_along * PI) * mix(0.35, 0.8, u_hail);
    vec3 c = u_sky_color.rgb * mix(1.6, 3.0, u_hail) + vec3(0.08 + u_hail * 0.25) + u_weather.z * 2.0;
    f_col = vec4(c, a);
}
