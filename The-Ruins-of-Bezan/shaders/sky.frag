#version 450 core

/* sky gradient, a sun disc, and a cloud layer that thickens with the weather */

in vec2 v_uv;

out vec4 f_col;

float fbm(vec2 p)
{
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) {
        s += a * noise2(p);
        p *= 2.03;
        a *= 0.5;
    }
    return s;
}

void main()
{
    /* direction through this pixel, from the inverse camera matrix */
    vec4 far = u_inv_view_proj * vec4(v_uv * 2.0 - 1.0, 1.0, 1.0);
    vec3 dir = normalize(far.xyz / far.w - u_camera_pos.xyz);

    vec3 color = sky_color(dir);
    if (dir.y < 0.0)    /* below the horizon: the far haze (over the sea), darkening only well below */
        color = mix(u_fog.rgb, u_ground_color.rgb * 0.5, smoothstep(0.05, 0.4, -dir.y));

    /* sun: bright disc plus a wide soft glow, hidden by thick cloud */
    float cloud = u_weather.y;
    float s = max(dot(dir, u_sun_dir.xyz), 0.0);
    color += u_sun_color.rgb * (pow(s, 2000.0) * 8.0 + pow(s, 12.0) * 0.08) * (1.0 - cloud * 0.95);

    /* clouds: drifting noise projected on a dome */
    if (dir.y > 0.0) {
        vec2 p = dir.xz / (dir.y + 0.12) * 1.3 + vec2(u_misc.x * 0.012, u_misc.x * 0.004);
        float c = fbm(p);
        float amount = smoothstep(0.62 - cloud * 0.45, 0.95 - cloud * 0.3, c);
        vec3 cloud_col = mix(u_fog.rgb * 1.15, u_fog.rgb * 0.55, cloud) + u_weather.z * vec3(2.0, 2.1, 2.5);
        color = mix(color, cloud_col, amount * smoothstep(0.0, 0.15, dir.y) * (0.5 + cloud * 0.5));
    }

    if (u_sea.y > 0.5)
        color = sea_murk();         /* under the sea there's no sky, only water */
    f_col = vec4(color, 1.0);
}
