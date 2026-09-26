#version 450 core

/* the sea: chalky turquoise over white sand, deepening to teal and blue out past the
 * marina. surf runs up the beach, the sun glints, rain pocks it. from underneath you
 * see the sky through a bright round window, and the murk everywhere else */

in vec3 v_world_pos;
in vec3 v_wave_n;
in float v_crest;

layout(location = 1) uniform vec4 u_params;     /* camera x, z, storm, time */
layout(location = 2) uniform vec4 u_whirl;      /* x, z, radius, strength */

out vec4 f_col;

void main()
{
    vec3 p = v_world_pos;
    float ground = terrain_height_at(p.xz);
    if (ground > u_sea.x + 0.35)
        discard;                    /* dry land, and the rooms dug under it (the crypt) */
    float depth = u_sea.x - ground;
    float t = u_params.w, storm = u_params.z;

    /* small ripples on top of the swell, and rain pocking it */
    vec2 tilt = vec2(sin(p.x * 0.9 + t * 1.6) + 0.7 * sin(p.z * 1.3 - t * 1.2),
                     cos(p.z * 1.1 + t * 1.3) + 0.6 * cos(p.x * 1.7 - t * 1.1)) * 0.035;
    tilt += (vec2(noise2(p.xz * 0.7 + t * 0.35), noise2(p.xz * 0.7 - t * 0.3 + 5.0)) - 0.5) * (0.1 + storm * 0.2);
    tilt += ripples(p.xz, t) * u_weather.w * 0.5;
    vec3 n = normalize(v_wave_n + vec3(tilt.x, 0.0, tilt.y));
    vec3 v = normalize(u_camera_pos.xyz - p);

    if (u_camera_pos.y < p.y) {
        /* looking up from under the water: the sky only shows through a circle
         * overhead (Snell's window); outside it the surface mirrors the murk */
        float up = -v.y;
        float window = smoothstep(0.62, 0.7, up);
        vec3 sky = sky_color(refract(-v, -n, 1.33)) * 1.2 + u_sun_color.rgb * pow(max(dot(-v, u_sun_dir.xyz), 0.0), 60.0) * 2.0;
        vec3 color = mix(sea_murk() * 1.6, sky, window);
        f_col = vec4(apply_fog(color, p), 1.0);
        return;
    }

    /* the water itself: milky, chalky turquoise in the shallows, deepening to teal */
    vec3 shallow = vec3(0.28, 0.78, 0.72);
    vec3 mid = vec3(0.05, 0.46, 0.52);
    vec3 deep = vec3(0.012, 0.13, 0.22);
    vec3 water = mix(shallow, mid, smoothstep(0.3, 7.0, depth));
    water = mix(water, deep, smoothstep(7.0, 38.0, depth));
    float shadow = sun_shadow(p, n);
    float exposure = sky_exposure(p, vec3(0, 1, 0));
    vec3 body = water * (u_sun_color.rgb * max(u_sun_dir.y, 0.0) * 0.22 * shadow + u_sky_color.rgb * 1.15 + 0.015);

    float fres = 0.02 + 0.98 * pow(1.0 - max(dot(n, v), 0.0), 5.0);
    vec3 color = mix(body, sky_color(reflect(-v, n)) * mix(0.3, 1.0, exposure), fres);
    color += brdf(vec3(0.0), 0.0, 0.035, n, v, u_sun_dir.xyz, u_sun_color.rgb) * shadow;
    int count = int(u_misc.y);
    for (int i = 0; i < count; i++) {
        vec3 to_light = u_light_pos[i].xyz - p;
        float d = length(to_light);
        if (d < u_light_pos[i].w)
            color += brdf(vec3(0.0), 0.0, 0.05, n, v, to_light / d, u_light_color[i].rgb / (d * d + 1.0));
    }

    /* foam: along the waterline, in lines of surf running up the beach, and on the
     * crests when the sea is rough */
    float grain = noise2(p.xz * 1.7 + t * 0.2) * 0.6 + noise2(p.xz * 5.0 - t * 0.3) * 0.4;
    float edge = 1.0 - smoothstep(0.0, 0.7, depth);
    float surf = smoothstep(0.6, 1.0, sin(depth * 4.2 - t * 1.5 + noise2(p.xz * 0.25) * 5.0)) * (1.0 - smoothstep(0.2, 2.4, depth));
    float crest = smoothstep(0.12, 0.35, v_crest) * storm;
    float foam = clamp(edge * 0.9 + surf * 0.75 + crest, 0.0, 1.0) * smoothstep(0.25, 0.65, grain + edge * 0.3);

    /* a whirlpool's spiral arms */
    float r = length(p.xz - u_whirl.xy);
    if (u_whirl.w > 0.0 && r < u_whirl.z * 1.3) {
        float a = atan(p.z - u_whirl.y, p.x - u_whirl.x);
        float arms = smoothstep(0.7, 1.0, sin(a * 3.0 + r * 0.6 - t * 5.0));
        foam = max(foam, arms * u_whirl.w * (1.0 - r / (u_whirl.z * 1.3)));
    }

    vec3 white = vec3(0.92, 0.96, 0.96) * (u_sun_color.rgb * max(u_sun_dir.y, 0.0) * 0.25 * shadow + u_sky_color.rgb * 1.3 + 0.04);
    color = mix(color, white, foam);

    /* see-through where it's shallow, so the sand shows */
    float alpha = mix(0.3, 0.97, smoothstep(0.0, 4.5, depth));
    alpha = max(alpha, max(fres, foam));
    f_col = vec4(apply_fog(color, p), alpha);
}
