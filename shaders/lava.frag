#version 450 core

/* Old Ember's lava lake: bright molten rock under a slowly drifting, cracking crust */

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_tangent;

out vec4 f_col;

void main()
{
    vec2 p = v_world_pos.xz * 0.18;
    float t = u_misc.x * 0.07;
    float n = noise2(p + vec2(t, t * 0.6)) * 0.55 + noise2(p * 2.6 - vec2(t * 1.4, t)) * 0.3
            + noise2(p * 7.0 + t * 2.0) * 0.15;
    float crust = smoothstep(0.45, 0.62, n);
    float pulse = 0.85 + 0.15 * sin(u_misc.x * 1.7 + p.x * 3.0);
    vec3 hot = mix(vec3(14.0, 3.2, 0.35), vec3(24.0, 10.0, 2.0), smoothstep(0.35, 0.1, n)) * pulse;
    vec3 col = mix(hot, vec3(0.05, 0.025, 0.02) + hot * 0.015, crust);
    f_col = vec4(apply_fog(col, v_world_pos), 1.0);
}
