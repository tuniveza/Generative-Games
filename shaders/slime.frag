#version 450 core

/* glossy, wet, see-through jelly: thin and clear where you look straight in, thick and
 * deeply colored at the edges, mirror-bright highlights from the sun, the sky and every
 * torch, and a glow from the core inside */

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_local;

layout(location = 2)  uniform vec4 u_base_color;
layout(location = 7)  uniform vec3 u_emissive;
layout(location = 11) uniform vec4 u_tint;

out vec4 f_col;

void main()
{
    vec3 n = normalize(v_normal);
    if (!gl_FrontFacing)
        n = -n;
    vec3 v = normalize(u_camera_pos.xyz - v_world_pos);
    float n_dot_v = max(dot(n, v), 0.0);
    float fres = pow(1.0 - n_dot_v, 3.0);

    /* the body: light passing through gets tinted; the edges are the thickest */
    vec3 body = u_base_color.rgb;
    vec3 thick = body * body * 1.4;
    vec3 lit = mix(u_ground_color.rgb, u_sky_color.rgb, n.y * 0.5 + 0.5) * 1.6
             + u_sun_color.rgb * max(dot(n, u_sun_dir.xyz) * 0.5 + 0.5, 0.0) * 0.18 * sun_shadow(v_world_pos, n);
    vec3 color = mix(body * 0.55, thick, fres) * lit;
    /* glow from the core: brightest looking straight through the middle */
    color += u_emissive * (0.5 + 0.8 * n_dot_v * n_dot_v);

    /* wet highlights: very smooth, so small and sharp */
    color += brdf(vec3(0.0), 0.0, 0.06, n, v, u_sun_dir.xyz, u_sun_color.rgb) * sun_shadow(v_world_pos, n) * 1.3;
    int count = int(u_misc.y);
    for (int i = 0; i < count; i++) {
        vec3 to_light = u_light_pos[i].xyz - v_world_pos;
        float d = length(to_light);
        float radius = u_light_pos[i].w;
        if (d >= radius)
            continue;
        float window = clamp(1.0 - pow(d / radius, 4.0), 0.0, 1.0);
        color += brdf(vec3(0.0), 0.0, 0.06, n, v, to_light / d, u_light_color[i].rgb * window * window / (d * d + 1.0)) * 1.5;
    }
    /* the world mirrored in its skin */
    float exposure = sky_exposure(v_world_pos, n);
    color += sky_color(reflect(-v, n)) * (0.04 + 0.96 * pow(1.0 - n_dot_v, 5.0)) * mix(0.25, 1.0, exposure);
    /* little bubbles drifting up inside */
    float bubbles = smoothstep(0.93, 1.0, noise2(v_local.xz * 22.0 + vec2(0.0, u_misc.x * 0.8)) * noise2(v_local.xy * 19.0 - u_misc.x * 0.5) * 1.8);
    color += vec3(0.6, 1.0, 0.7) * bubbles * 0.4 * n_dot_v;
    color += u_tint.rgb;

    float alpha = mix(0.38, 0.95, fres) * u_base_color.a;
    f_col = vec4(apply_fog(color, v_world_pos), alpha);
}
