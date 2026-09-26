#version 450 core

/* glTF metallic-roughness materials, lit by the sun, torches and sky (common.glsl) */

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_tangent;

layout(location = 2)  uniform vec4  u_base_color;
layout(location = 3)  uniform float u_metallic;
layout(location = 4)  uniform float u_roughness;
layout(location = 5)  uniform float u_normal_scale;
layout(location = 6)  uniform float u_occlusion_strength;
layout(location = 7)  uniform vec3  u_emissive;
layout(location = 8)  uniform float u_alpha_cutoff;   /* < 0 = no alpha test */
layout(location = 9)  uniform int   u_flags;
layout(location = 11) uniform vec4  u_tint;           /* rgb glow added on top (hit flash) */

layout(binding = 0) uniform sampler2D u_base_tex;
layout(binding = 1) uniform sampler2D u_mr_tex;
layout(binding = 2) uniform sampler2D u_normal_tex;
layout(binding = 3) uniform sampler2D u_occlusion_tex;
layout(binding = 4) uniform sampler2D u_emissive_tex;

const int F_BASE_TEX      = 1 << 0;
const int F_MR_TEX        = 1 << 1;
const int F_NORMAL_TEX    = 1 << 2;
const int F_OCCLUSION_TEX = 1 << 3;
const int F_EMISSIVE_TEX  = 1 << 4;
const int F_NORMALS       = 1 << 5;
const int F_TANGENTS      = 1 << 6;

out vec4 f_col;

bool has(int flag) { return (u_flags & flag) != 0; }

vec3 get_normal()
{
    /* surface normal: from the mesh, or from screen-space derivatives if it has none */
    vec3 ng = has(F_NORMALS) ? normalize(v_normal)
                             : normalize(cross(dFdx(v_world_pos), dFdy(v_world_pos)));
    if (!gl_FrontFacing)
        ng = -ng;                      /* seeing the back side (double-sided surfaces) */

    if (!has(F_NORMAL_TEX))
        return ng;

    /* tangent frame: from the mesh, or derived from how the uvs change across the pixel */
    vec3 t, b;
    if (has(F_TANGENTS)) {
        t = normalize(v_tangent.xyz - ng * dot(ng, v_tangent.xyz));
        b = cross(ng, t) * v_tangent.w;
    } else {
        vec3 uv_dx = dFdx(vec3(v_uv, 0.0));
        vec3 uv_dy = dFdy(vec3(v_uv, 0.0));
        vec3 t_ = (uv_dy.t * dFdx(v_world_pos) - uv_dx.t * dFdy(v_world_pos)) /
                  (uv_dx.s * uv_dy.t - uv_dy.s * uv_dx.t + 1e-8);
        t = normalize(t_ - ng * dot(ng, t_));
        b = cross(ng, t);
    }
    if (!gl_FrontFacing) {
        t = -t;
        b = -b;
    }

    vec3 n = texture(u_normal_tex, v_uv).xyz * 2.0 - 1.0;
    n.xy *= u_normal_scale;
    return normalize(mat3(t, b, ng) * n);
}

void main()
{
    vec4 base = u_base_color;
    if (has(F_BASE_TEX))
        base *= texture(u_base_tex, v_uv);      /* sRGB texture: sampled as linear */

    if (u_alpha_cutoff >= 0.0 && base.a < u_alpha_cutoff)
        discard;

    float metallic = u_metallic;
    float roughness = u_roughness;
    if (has(F_MR_TEX)) {
        vec4 mr = texture(u_mr_tex, v_uv);
        roughness *= mr.g;
        metallic *= mr.b;
    }

    float ao = 1.0;
    if (has(F_OCCLUSION_TEX))
        ao = 1.0 + u_occlusion_strength * (texture(u_occlusion_tex, v_uv).r - 1.0);

    vec3 n = get_normal();
    vec3 ng = has(F_NORMALS) ? normalize(v_normal) : n;
    vec3 albedo = base.rgb;
    if (u_tint.a < 0.5)             /* tint.a = 1: first-person hands, which stay dry */
        apply_wet(albedo, roughness, n, ng, v_world_pos);
    vec3 color = shade(albedo, clamp(metallic, 0.0, 1.0), roughness, ao, n, v_world_pos);

    vec3 emissive = u_emissive;
    if (has(F_EMISSIVE_TEX))
        emissive *= texture(u_emissive_tex, v_uv).rgb;
    color += emissive + u_tint.rgb;

    f_col = vec4(apply_fog(color, v_world_pos), base.a);
}
