#version 450 core

/* textured ground: grass and forest floor on flat land, rock on slopes and peaks */

in vec3 v_world_pos;
in vec3 v_normal;

layout(binding = 0) uniform sampler2D u_grass_albedo;
layout(binding = 1) uniform sampler2D u_grass_normal;
layout(binding = 2) uniform sampler2D u_grass_arm;
layout(binding = 3) uniform sampler2D u_rock_albedo;
layout(binding = 4) uniform sampler2D u_rock_normal;
layout(binding = 5) uniform sampler2D u_rock_arm;
layout(binding = 6) uniform sampler2D u_dirt_albedo;
layout(binding = 7) uniform sampler2D u_dirt_normal;
layout(binding = 9) uniform sampler2D u_dirt_arm;

layout(location = 20) uniform vec4 u_holes[8];     /* stairwells: no ground here (MAX_HOLES) */
layout(location = 28) uniform int u_hole_count;

out vec4 f_col;

/* cheap smooth noise to break up where grass and dirt meet */
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x),
               mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}

/* rock seen from three sides so steep slopes don't show stretched texture */
vec3 triplanar(sampler2D tex, vec3 p, vec3 w)
{
    return texture(tex, p.zy).rgb * w.x + texture(tex, p.xz).rgb * w.y + texture(tex, p.xy).rgb * w.z;
}

void main()
{
    vec3 ng = normalize(v_normal);
    vec3 p = v_world_pos;
    for (int i = 0; i < u_hole_count; i++)
        if (p.x > u_holes[i].x && p.x < u_holes[i].z && p.z > u_holes[i].y && p.z < u_holes[i].w)
            discard;

    /* flat ground: grass, with patches of forest floor */
    vec2 uv = p.xz / 3.0;
    float patches = smoothstep(0.45, 0.65, noise(p.xz * 0.05) * 0.7 + noise(p.xz * 0.3) * 0.3);
    vec3 albedo = mix(texture(u_grass_albedo, uv).rgb, texture(u_dirt_albedo, uv).rgb, patches);
    vec3 arm    = mix(texture(u_grass_arm, uv).rgb,    texture(u_dirt_arm, uv).rgb,    patches);
    vec3 tn     = mix(texture(u_grass_normal, uv).xyz, texture(u_dirt_normal, uv).xyz, patches) * 2.0 - 1.0;

    /* planar uv: u along +x, texture "up" along -z */
    vec3 t = normalize(vec3(1, 0, 0) - ng * ng.x);
    vec3 b = cross(ng, t);
    vec3 n = normalize(mat3(t, b, ng) * tn);

    /* rock where it's steep */
    vec3 w = pow(abs(ng), vec3(4.0));
    w /= w.x + w.y + w.z;
    float rock = 1.0 - smoothstep(0.72, 0.86, ng.y);
    vec3 rp = p / 6.0;
    albedo = mix(albedo, triplanar(u_rock_albedo, rp, w), rock);
    arm = mix(arm, triplanar(u_rock_arm, rp, w), rock);
    n = normalize(mix(n, ng, rock * 0.5));

    float rough = arm.g;
    apply_wet(albedo, rough, n, ng, p);
    vec3 color = shade(albedo, 0.0, rough, arm.r, n, p);
    f_col = vec4(apply_fog(color, p), 1.0);
}
