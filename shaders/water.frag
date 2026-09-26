#version 450 core

/* water: tinted and see-through, reflecting the sky and lights, rippled by wind and rain */

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_tangent;

out vec4 f_col;

void main()
{
    vec3 p = v_world_pos;
    float t = u_misc.x;
    /* gentle swell from a few crossing waves */
    vec2 tilt = vec2(sin(p.x * 1.7 + t * 1.3) + sin(p.z * 2.3 - t * 1.1),
                     cos(p.z * 1.9 + t * 0.9) + cos(p.x * 2.9 + t * 1.6)) * 0.035;
    float exposure = sky_exposure(p, vec3(0, 1, 0));
    tilt += ripples(p.xz, t) * (0.2 + u_weather.w * exposure);
    vec3 n = normalize(vec3(tilt.x, 1.0, tilt.y));

    vec3 v = normalize(u_camera_pos.xyz - p);
    float n_dot_v = max(dot(n, v), 0.0);
    float fres = 0.02 + 0.98 * pow(1.0 - n_dot_v, 5.0);

    /* what's below (dark, murky green) and what's reflected */
    vec3 deep = vec3(0.012, 0.03, 0.028) * (u_sky_color.rgb + 0.2) * mix(0.25, 1.0, exposure);
    vec3 refl = sky_color(reflect(-v, n)) * mix(0.1, 1.0, exposure);

    vec3 color = mix(deep, refl, fres);
    color += brdf(vec3(0.0), 0.0, 0.05, n, v, u_sun_dir.xyz, u_sun_color.rgb) * sun_shadow(p, n);
    int count = int(u_misc.y);
    for (int i = 0; i < count; i++) {
        vec3 to_light = u_light_pos[i].xyz - p;
        float d = length(to_light);
        if (d < u_light_pos[i].w) {
            float atten = 1.0 / (d * d + 1.0);
            color += brdf(vec3(0.0), 0.0, 0.05, n, v, to_light / d, u_light_color[i].rgb * atten);
        }
    }

    f_col = vec4(apply_fog(color, p), mix(0.7, 0.95, fres));
}
