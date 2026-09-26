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

    /* the body: light passing through is tinted deep green; thin in the middle, where you
     * see the core glowing through, thick and dark at the edges */
    vec3 deep = u_base_color.rgb * u_base_color.rgb;
    float exposure = sky_exposure(v_world_pos, n);
    vec3 light = u_sky_color.rgb * mix(0.35, 1.3, exposure) + u_sun_color.rgb * 0.08 * sun_shadow(v_world_pos, n) + 0.01;
    vec3 color = deep * light * mix(1.1, 0.35, fres);
    color += u_emissive * (0.25 + 1.2 * n_dot_v * n_dot_v);

    /* a wet skin: a mirror that brightens toward the rim, sharp glints of sun and sky */
    vec3 r = reflect(-v, n);
    float mirror = 0.05 + 0.95 * pow(1.0 - n_dot_v, 5.0);
    color += sky_color(r) * mirror * 2.0 * mix(0.25, 1.0, exposure);
    color += brdf(vec3(0.0), 0.0, 0.035, n, v, u_sun_dir.xyz, u_sun_color.rgb) * sun_shadow(v_world_pos, n) * 2.0;
    float key = pow(max(dot(r, normalize(vec3(-0.35, 1.0, 0.25))), 0.0), 90.0);
    float rim = pow(max(dot(r, normalize(vec3(0.6, 0.5, -0.4))), 0.0), 40.0);
    color += (u_sky_color.rgb * 5.0 + 0.25) * (key + rim * 0.4) * mix(0.3, 1.0, exposure);
    int count = int(u_misc.y);
    for (int i = 0; i < count; i++) {
        vec3 to_light = u_light_pos[i].xyz - v_world_pos;
        float d = length(to_light);
        float radius = u_light_pos[i].w;
        if (d >= radius)
            continue;
        float window = clamp(1.0 - pow(d / radius, 4.0), 0.0, 1.0);
        color += brdf(vec3(0.0), 0.0, 0.04, n, v, to_light / d, u_light_color[i].rgb * window * window / (d * d + 1.0)) * 2.0;
        color += deep * u_light_color[i].rgb * window * window / (d * d + 1.0) * 0.3;    /* torchlight glowing through */
    }
    /* little bubbles drifting up inside */
    float bubbles = smoothstep(0.975, 1.0, noise2(v_local.xz * 14.0 + vec2(0.0, u_misc.x * 0.8)) * noise2(v_local.xy * 19.0 - u_misc.x * 0.5) * 1.8);
    color += vec3(0.6, 1.0, 0.7) * bubbles * 0.5 * n_dot_v;
    color += u_tint.rgb;

    float alpha = mix(0.42, 0.97, fres) * u_base_color.a;
    f_col = vec4(apply_fog(color, v_world_pos), alpha);
}
