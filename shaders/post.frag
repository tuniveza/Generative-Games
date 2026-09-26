#version 450 core

/* final image: scene + bloom, exposure, tone mapping, gamma, and screen effects */

in vec2 v_uv;

layout(binding = 0) uniform sampler2D u_hdr;
layout(binding = 1) uniform sampler2D u_bloom;
layout(location = 0) uniform float u_exposure;
layout(location = 1) uniform float u_bloom_strength;
layout(location = 2) uniform float u_damage;        /* 0..1 red flash at the edges */
layout(location = 3) uniform float u_slowmo;        /* 0..1 sepia time-slow effect */
layout(location = 4) uniform float u_dead;          /* 0..1 fade to dark red */

out vec4 f_col;

/* ACES filmic curve (Narkowicz fit): rich contrast, highlights roll off smoothly */
vec3 aces(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
    vec3 hdr = texture(u_hdr, v_uv).rgb + texture(u_bloom, v_uv).rgb * u_bloom_strength;
    vec3 color = aces(hdr * u_exposure);

    float edge = length(v_uv - 0.5) * 1.4;
    color *= 1.0 - 0.35 * edge * edge;                         /* vignette */

    if (u_slowmo > 0.0) {
        float grey = dot(color, vec3(0.299, 0.587, 0.114));
        color = mix(color, grey * vec3(1.1, 0.95, 0.75), u_slowmo * 0.7);
    }
    color = mix(color, vec3(0.6, 0.0, 0.0), u_damage * smoothstep(0.3, 1.0, edge) * 0.8);
    color = mix(color, vec3(0.15, 0.0, 0.0), u_dead * 0.8);

    f_col = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
