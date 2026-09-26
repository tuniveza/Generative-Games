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
layout(location = 5) uniform float u_underwater;    /* 0..1 under the sea: wobble and tint */
layout(location = 6) uniform float u_ash;           /* 0..1 the volcano's ash cloud */
layout(location = 7) uniform float u_toxic;         /* 0..1 choking on fumes */
layout(location = 8) uniform float u_time;

out vec4 f_col;

/* ACES filmic curve (Narkowicz fit): rich contrast, highlights roll off smoothly */
vec3 aces(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
    vec2 uv = v_uv;
    if (u_underwater > 0.0)     /* the world wavers through the water */
        uv += vec2(sin(uv.y * 38.0 + u_time * 2.1), cos(uv.x * 31.0 + u_time * 1.7)) * 0.0022 * u_underwater;
    if (u_toxic > 0.0)          /* fumes: the view swims */
        uv += vec2(sin(uv.y * 9.0 + u_time * 3.0), cos(uv.x * 7.0 + u_time * 2.4)) * 0.006 * u_toxic;
    vec3 hdr = texture(u_hdr, uv).rgb + texture(u_bloom, uv).rgb * u_bloom_strength;
    vec3 color = aces(hdr * u_exposure);

    float edge = length(v_uv - 0.5) * 1.4;
    color *= 1.0 - 0.35 * edge * edge;                         /* vignette */

    if (u_slowmo > 0.0) {
        float grey = dot(color, vec3(0.299, 0.587, 0.114));
        color = mix(color, grey * vec3(1.1, 0.95, 0.75), u_slowmo * 0.7);
    }
    color = mix(color, vec3(0.6, 0.0, 0.0), u_damage * smoothstep(0.3, 1.0, edge) * 0.8);
    if (u_underwater > 0.0)
        color = mix(color, color * vec3(0.55, 1.0, 1.05), u_underwater * 0.5);
    if (u_ash > 0.0) {
        float grey = dot(color, vec3(0.299, 0.587, 0.114));
        color = mix(color, grey * vec3(1.05, 0.85, 0.7), u_ash * 0.55);
    }
    if (u_toxic > 0.0)
        color = mix(color, color * vec3(0.8, 1.1, 0.45), u_toxic * 0.6 * smoothstep(0.1, 0.9, edge + 0.3));
    color = mix(color, vec3(0.15, 0.0, 0.0), u_dead * 0.8);

    f_col = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
