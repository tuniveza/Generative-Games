#version 450 core

in vec2 v_uv;
in vec3 v_world;

layout(location = 1) uniform vec4 u_twister;

out vec4 f_col;

void main()
{
    float t = v_uv.y, time = u_misc.x;
    /* streaks of dust and cloud spiralling up */
    vec2 q = vec2(v_uv.x * 10.0 - time * 1.9 + t * 7.0, t * 14.0 - time * 1.2);
    float swirl = noise2(q) * 0.6 + noise2(q * 2.7 + 3.0) * 0.3 + noise2(q * 7.0) * 0.1;
    float alpha = (0.45 + smoothstep(0.3, 0.8, swirl) * 0.5);
    /* it reaches down from the cloud as it forms, and fades into the cloud up top */
    float reach = 1.0 - u_twister.w * 1.05;
    alpha *= smoothstep(reach, reach + 0.12, t) * smoothstep(1.0, 0.75, t);
    alpha *= 0.9;
    vec3 dust = mix(vec3(0.07, 0.065, 0.06), vec3(0.2, 0.18, 0.15), swirl);
    vec3 col = dust * (u_sky_color.rgb * 1.0 + u_sun_color.rgb * 0.04 + 0.02) + u_weather.z * vec3(0.8, 0.85, 1.0);
    /* less haze on it than on the land: it looms */
    f_col = vec4(mix(col, apply_fog(col, v_world), 0.5), alpha);
}
