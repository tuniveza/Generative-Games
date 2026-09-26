#version 450 core

/* the ocean grid: offsets from the camera, raised by the swell. the waves must match
 * WAVES in ocean.c, which bobs boats and swimmers on the same water */

layout(location = 0) in vec2 a_offset;

layout(location = 0) uniform mat4 u_vp;
layout(location = 1) uniform vec4 u_params;     /* camera x, z (in whole grid steps), storm 0..1, time */
layout(location = 2) uniform vec4 u_whirl;      /* x, z, radius, strength: a whirlpool */

out vec3 v_world_pos;
out vec3 v_wave_n;
out float v_crest;

const vec4 WAVES[4] = vec4[4](
    vec4( 0.20, -1.00, 31.0, 0.20),
    vec4( 0.25, -0.97, 18.0, 0.14),
    vec4(-0.50, -0.86,  9.5, 0.08),
    vec4( 0.80, -0.60,  5.3, 0.045)
);

void main()
{
    vec2 xz = u_params.xy + a_offset;
    float depth = u_sea.x - terrain_height_at(xz);
    float t = u_params.w;
    /* calm in the shallows, bigger in a storm, flat out at the horizon */
    float scale = (0.7 + u_params.z * 2.6) * clamp(depth / 3.0, 0.15, 1.0);
    scale *= 1.0 - smoothstep(300.0, 900.0, length(a_offset));

    float h = 0.0, crest = 0.0;
    vec2 slope = vec2(0.0);
    for (int i = 0; i < 4; i++) {
        vec2 d = normalize(WAVES[i].xy);
        float k = 2.0 * PI / WAVES[i].z;
        float c = sqrt(9.81 / k);
        float phase = k * (dot(WAVES[i].xy, xz) - c * t);
        h += WAVES[i].w * sin(phase);
        slope += d * WAVES[i].w * k * cos(phase);
        crest += max(sin(phase), 0.0) * WAVES[i].w;
    }
    h *= scale;
    slope *= scale;

    /* a whirlpool sucks the surface down into a turning funnel */
    float r = length(xz - u_whirl.xy);
    if (u_whirl.w > 0.0 && r < u_whirl.z) {
        float f = 1.0 - r / u_whirl.z;
        h -= u_whirl.w * f * f * 7.0;
        slope += normalize(xz - u_whirl.xy + 1e-4) * u_whirl.w * f * 14.0 / u_whirl.z;
    }

    v_world_pos = vec3(xz.x, u_sea.x + h, xz.y);
    v_wave_n = normalize(vec3(-slope.x, 1.0, -slope.y));
    v_crest = crest * scale;
    gl_Position = u_vp * vec4(v_world_pos, 1.0);
}
