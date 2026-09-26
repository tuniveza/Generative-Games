/* inserted after #version in every shader by shader_load() (shader.c).
 * per-frame data shared by everything: camera, sun, sky, weather and torch lights. */

#define MAX_LIGHTS 32
#define PI 3.14159265359
#define FLOOR_Y -0.5
/* landmarks beyond the ruins; must match world.h */
#define VOLCANO_XZ vec2(230.0, -230.0)
#define VOLCANO_R 140.0

layout(std140, binding = 0) uniform Frame {
    mat4 u_view_proj;
    mat4 u_inv_view_proj;
    mat4 u_shadow_matrix;       /* world -> shadow map uv + depth */
    vec4 u_camera_pos;          /* xyz */
    vec4 u_sun_dir;             /* xyz = towards the sun, w = shadow strength 0..1 */
    vec4 u_sun_color;           /* rgb, already multiplied by intensity */
    vec4 u_sky_color;           /* ambient light from above */
    vec4 u_ground_color;        /* ambient light from below */
    vec4 u_fog;                 /* rgb = fog color, a = density */
    vec4 u_misc;                /* x = time (s), y = light count, z = open sky above the camera 0..1 */
    vec4 u_zenith;              /* sky color straight up */
    vec4 u_weather;             /* x = wetness, y = cloud cover, z = lightning flash, w = rain */
    vec4 u_cover_grid;          /* x0, z0, 1 / cell size, 1 = grid present */
    vec4 u_sea;                 /* x = sea level, y = camera underwater, z = volcanic ash, w = in the undersea palace */
    vec4 u_height_grid;         /* terrain heights: x0, z0, 1 / spacing, samples per side */
    vec4 u_light_pos[MAX_LIGHTS];     /* xyz, w = radius */
    vec4 u_light_color[MAX_LIGHTS];   /* rgb intensity */
};

layout(binding = 8) uniform sampler2DShadow u_shadow_map;
layout(binding = 10) uniform sampler2D u_cover;     /* height of whatever is overhead */
layout(binding = 11) uniform sampler2D u_heights;   /* the terrain's height */

/* ground height under (x, z), from the terrain's height texture */
float terrain_height_at(vec2 xz)
{
    if (u_height_grid.w < 1.0)
        return 0.0;
    vec2 g = (xz - u_height_grid.xy) * u_height_grid.z;
    return texture(u_heights, (g + 0.5) / u_height_grid.w).r;
}

/* how far under the open sea a point is (0 on land, in the crypt, above the waves) */
float sea_depth(vec3 p)
{
    if (terrain_height_at(p.xz) > u_sea.x + 0.3)
        return 0.0;
    return max(u_sea.x - p.y, 0.0);
}

/* the dancing light net that sunlight makes on the sea floor (after joltz0r's
 * "water turbulence") */
float caustics(vec2 p, float t)
{
    vec2 q = mod(p * 0.9, 6.28318) - 250.0;
    vec2 i = q;
    float c = 1.0;
    const float inten = 0.005;
    for (int n = 0; n < 4; n++) {
        float tt = t * (1.0 - 3.5 / float(n + 1));
        i = q + vec2(cos(tt - i.x) + sin(tt + i.y), sin(tt - i.y) + cos(tt + i.x));
        c += 1.0 / length(vec2(q.x / (sin(i.x + tt) / inten), q.y / (cos(i.y + tt) / inten)));
    }
    c /= 4.0;
    c = 1.17 - pow(c, 1.4);
    return clamp(pow(abs(c), 8.0), 0.0, 2.0);
}

/* 1 = open sky above this point, 0 = under a roof, a floor, or underground.
 * level_sky_exposure (level.c) is the same test on the CPU */
float sky_exposure(vec3 world_pos, vec3 n)
{
    if (u_cover_grid.w < 0.5)
        return 1.0;
    /* look a little outward so wall faces check the open air in front of them */
    vec3 p = world_pos + vec3(n.x, 0.0, n.z) * 0.45;
    vec2 uv = (p.xz - u_cover_grid.xy) * u_cover_grid.z + 0.5;
    ivec2 size = textureSize(u_cover, 0);
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x >= float(size.x) || uv.y >= float(size.y))
        return 1.0;         /* beyond the world's edge: open sky */
    float top = texelFetch(u_cover, ivec2(uv), 0).r;
    float open = smoothstep(0.35, 0.05, top - world_pos.y);
    /* deep underground, even the stairwell light fades */
    return open * smoothstep(FLOOR_Y - 3.0, FLOOR_Y - 1.0, world_pos.y);
}

/* 1 = lit by the sun, 0 = in shadow. soft edges from 16 filtered taps */
float sun_shadow(vec3 world_pos, vec3 n)
{
    if (u_sun_dir.w <= 0.0)
        return 1.0;

    /* push the lookup point off the surface a little to avoid self-shadowing acne */
    float n_dot_l = dot(n, u_sun_dir.xyz);
    vec3 offset = n * (0.03 + 0.05 * (1.0 - abs(n_dot_l)));
    vec4 sp = u_shadow_matrix * vec4(world_pos + offset, 1.0);
    if (sp.x < 0.0 || sp.x > 1.0 || sp.y < 0.0 || sp.y > 1.0 || sp.z > 1.0)
        return 1.0;

    vec2 texel = 1.0 / vec2(textureSize(u_shadow_map, 0));
    float sum = 0.0;
    for (int y = -2; y <= 1; y++)
        for (int x = -2; x <= 1; x++)
            sum += texture(u_shadow_map, vec3(sp.xy + (vec2(x, y) + 0.5) * texel * 1.5, sp.z - 0.0005));
    float lit = sum / 16.0;
    return mix(1.0, lit, u_sun_dir.w);
}

/* ---- rain: wet surfaces, puddles and ripples ---- */

float hash12(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

float noise2(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), u.x),
               mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), u.x), u.y);
}

/* expanding rings from raindrops landing in a puddle: returns a normal tilt (xz) */
vec2 ripples(vec2 p, float t)
{
    vec2 tilt = vec2(0.0);
    for (int layer = 0; layer < 2; layer++) {
        vec2 q = p * (layer == 0 ? 2.3 : 3.1) + float(layer) * 7.7;
        vec2 cell = floor(q);
        for (int j = -1; j <= 1; j++) {
            for (int i = -1; i <= 1; i++) {
                vec2 c = cell + vec2(i, j);
                float h = hash12(c);
                vec2 center = c + vec2(hash12(c + 3.1), hash12(c + 5.7));
                float phase = fract(t * 0.9 + h);
                vec2 d = q - center;
                float r = length(d);
                float ring = sin((r - phase * 1.2) * 28.0) * smoothstep(0.25, 0.0, abs(r - phase * 1.2)) * (1.0 - phase);
                tilt += (r > 1e-3 ? d / r : vec2(0.0)) * ring * 0.35;
            }
        }
    }
    return tilt;
}

/* how much of this spot is puddle (flat, exposed ground in the rain); `wet` from apply_wet */
float puddle(vec3 world_pos, vec3 ng, float wet)
{
    if (wet <= 0.0 || ng.y < 0.9)
        return 0.0;
    float n = noise2(world_pos.xz * 0.35) * 0.65 + noise2(world_pos.xz * 1.3) * 0.35;
    return smoothstep(0.55, 0.7, n) * wet;
}

/* darkens and polishes surfaces in the rain, and turns hollows into rippling puddles */
void apply_wet(inout vec3 albedo, inout float roughness, inout vec3 n, vec3 ng, vec3 world_pos)
{
    float wet = u_weather.x * sky_exposure(world_pos, ng);
    if (wet <= 0.0)
        return;
    albedo *= mix(1.0, 0.55, wet);
    roughness = mix(roughness, roughness * 0.35, wet);
    float pud = puddle(world_pos, ng, wet);
    if (pud > 0.0) {
        roughness = mix(roughness, 0.03, pud);
        albedo *= mix(1.0, 0.7, pud);
        vec2 t = ripples(world_pos.xz, u_misc.x) * u_weather.w;
        n = normalize(mix(n, normalize(vec3(t.x, 1.0, t.y)), pud));
    }
}

/* ---- Cook-Torrance BRDF pieces ---- */

float ggx_distribution(float n_dot_h, float alpha)
{
    float a2 = alpha * alpha;
    float d = n_dot_h * n_dot_h * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

float smith_geometry(float n_dot_v, float n_dot_l, float roughness)
{
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return (n_dot_v / (n_dot_v * (1.0 - k) + k)) * (n_dot_l / (n_dot_l * (1.0 - k) + k));
}

vec3 fresnel_schlick(float cos_theta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - cos_theta, 5.0);
}

/* light arriving from direction l with color `radiance` */
vec3 brdf(vec3 albedo, float metallic, float roughness, vec3 n, vec3 v, vec3 l, vec3 radiance)
{
    vec3 h = normalize(v + l);
    float n_dot_l = max(dot(n, l), 0.0);
    if (n_dot_l <= 0.0)
        return vec3(0.0);
    float n_dot_v = max(dot(n, v), 1e-4);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);

    vec3 F = fresnel_schlick(max(dot(v, h), 0.0), f0);
    float D = ggx_distribution(max(dot(n, h), 0.0), roughness * roughness);
    float G = smith_geometry(n_dot_v, n_dot_l, roughness);
    vec3 specular = D * G * F / (4.0 * n_dot_v * n_dot_l + 1e-4);
    vec3 diffuse = (1.0 - F) * (1.0 - metallic) * albedo / PI;
    return (diffuse + specular) * radiance * n_dot_l;
}

/* the sky as seen along a direction (also what shiny surfaces reflect) */
vec3 sky_color(vec3 dir)
{
    float up = max(dir.y, 0.0);
    vec3 c = mix(u_fog.rgb, u_zenith.rgb, pow(up, 0.5));
    return c + vec3(0.5, 0.55, 0.7) * u_weather.z * 3.0;     /* lightning */
}

/* full lighting for a surface point: sun (with shadow), torches, sky ambient and reflection */
vec3 shade(vec3 albedo, float metallic, float roughness, float ao, vec3 n, vec3 world_pos)
{
    roughness = clamp(roughness, 0.03, 1.0);
    vec3 v = normalize(u_camera_pos.xyz - world_pos);
    float exposure = sky_exposure(world_pos, n);

    vec3 color = brdf(albedo, metallic, roughness, n, v, u_sun_dir.xyz, u_sun_color.rgb)
               * sun_shadow(world_pos, n);

    int count = int(u_misc.y);
    for (int i = 0; i < count; i++) {
        vec3 to_light = u_light_pos[i].xyz - world_pos;
        float d = length(to_light);
        float radius = u_light_pos[i].w;
        if (d >= radius)
            continue;
        /* inverse square falloff, smoothly reaching zero at the radius */
        float window = clamp(1.0 - pow(d / radius, 4.0), 0.0, 1.0);
        float atten = window * window / (d * d + 1.0);
        color += brdf(albedo, metallic, roughness, n, v, to_light / d, u_light_color[i].rgb * atten);
    }

    /* ambient: sky from above, bounce from below; much less of it indoors */
    float n_dot_v = max(dot(n, v), 1e-4);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 f_amb = f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - n_dot_v, 5.0);
    vec3 ambient_light = mix(u_ground_color.rgb, u_sky_color.rgb, n.y * 0.5 + 0.5)
                       + vec3(0.4, 0.45, 0.6) * u_weather.z * 2.0 * exposure;
    ambient_light *= mix(0.2, 1.0, exposure);
    color += ambient_light * (1.0 - f_amb) * (1.0 - metallic) * albedo * ao;

    /* reflections of the sky on smooth things (wet stone, puddles, marble, metal) */
    vec3 r = reflect(-v, n);
    float smooth_ = (1.0 - roughness) * (1.0 - roughness);
    color += sky_color(r) * f_amb * smooth_ * mix(0.15, 1.0, exposure) * ao;

    /* under the sea: the water drinks the red first, and the sun draws moving nets of
     * light on whatever faces up */
    float depth = sea_depth(world_pos);
    if (depth > 0.0) {
        vec3 absorb = exp(-depth * vec3(0.16, 0.045, 0.035) * (1.0 - 0.75 * u_sea.w));
        float lit = max(n.y, 0.0) * sun_shadow(world_pos, n) * exposure;
        vec3 net = u_sun_color.rgb * albedo * caustics(world_pos.xz * 0.35, u_misc.x * 0.6) * lit * 0.35;
        color = color * absorb + net * exp(-depth * 0.05);
        /* the Drowned Court glows softly in its own right, as if the sea itself were lit */
        color += albedo * vec3(0.1, 0.3, 0.32) * u_sea.w * ao * (0.8 + 0.2 * n.y);
    }
    return color;
}

/* distance haze towards the fog color; much thinner under a roof, where the rain's
 * mist doesn't reach */
/* the green-blue murk you see through when under the sea (also the sky's color there) */
vec3 sea_murk()
{
    float depth = max(u_sea.x - u_camera_pos.y, 0.0);
    vec3 light = u_sun_color.rgb * 0.05 + u_sky_color.rgb * 0.5 + 0.02;
    return vec3(0.05, 0.3, 0.33) * light * exp(-depth * vec3(0.12, 0.04, 0.03)) + vec3(0.01, 0.07, 0.09) * u_sea.w;
}

vec3 apply_fog(vec3 color, vec3 world_pos)
{
    float dist = length(u_camera_pos.xyz - world_pos);
    if (u_sea.y > 0.5) {
        /* under water (or in the palace's air, which is still deep in it) */
        float density = mix(0.055, 0.02, u_sea.w);
        return mix(color, sea_murk(), 1.0 - exp(-dist * density));
    }
    float inside = 1.0 - u_misc.z;     /* sky_exposure at the camera, worked out once on the CPU */
    /* haze hangs low: tall things far off (the volcano) stand out against the sky */
    float high = smoothstep(8.0, 110.0, world_pos.y);
    float d = dist * u_fog.a * mix(1.0, 0.25, inside) * mix(1.0, 0.45, high);
    /* underground (the crypt, under the land) there's no sky haze, just darkness */
    bool buried = world_pos.y < FLOOR_Y - 1.0 && terrain_height_at(world_pos.xz) > world_pos.y + 0.5;
    vec3 fog = buried ? vec3(0.004, 0.004, 0.006) : u_fog.rgb;
    return mix(color, fog, 1.0 - exp(-d * d));
}
