#include "meshgen.h"
#include "texture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mb_init(MeshBuilder *mb)
{
    memset(mb, 0, sizeof *mb);
}

void mb_free(MeshBuilder *mb)
{
    free(mb->verts);
    free(mb->joints);
    free(mb->idx);
    memset(mb, 0, sizeof *mb);
}

static int add_vert(MeshBuilder *mb, vec3 p, vec3 n, float u, float v, vec3 t, float w)
{
    if (mb->vert_count == mb->vert_cap) {
        mb->vert_cap = mb->vert_cap ? mb->vert_cap * 2 : 256;
        mb->verts = realloc(mb->verts, mb->vert_cap * 12 * sizeof *mb->verts);
        mb->joints = realloc(mb->joints, mb->vert_cap * sizeof *mb->joints);
    }
    float *d = &mb->verts[mb->vert_count * 12];
    d[0] = p[0]; d[1] = p[1]; d[2] = p[2];
    d[3] = n[0]; d[4] = n[1]; d[5] = n[2];
    d[6] = u;    d[7] = v;
    d[8] = t[0]; d[9] = t[1]; d[10] = t[2]; d[11] = w;
    mb->joints[mb->vert_count] = (GLushort)mb->joint;
    return mb->vert_count++;
}

static void add_tri(MeshBuilder *mb, int a, int b, int c)
{
    if (mb->idx_count + 3 > mb->idx_cap) {
        mb->idx_cap = mb->idx_cap ? mb->idx_cap * 2 : 512;
        mb->idx = realloc(mb->idx, mb->idx_cap * sizeof *mb->idx);
    }
    mb->idx[mb->idx_count++] = a;
    mb->idx[mb->idx_count++] = b;
    mb->idx[mb->idx_count++] = c;
}

void mb_box(MeshBuilder *mb, mat4 xf, vec3 half, float tile)
{
    /* per face: normal, u direction, and the direction that is "up" in the texture */
    static const float faces[6][3][3] = {
        { { 1, 0, 0}, { 0, 0,-1}, {0, 1, 0} },
        { {-1, 0, 0}, { 0, 0, 1}, {0, 1, 0} },
        { { 0, 0, 1}, { 1, 0, 0}, {0, 1, 0} },
        { { 0, 0,-1}, {-1, 0, 0}, {0, 1, 0} },
        { { 0, 1, 0}, { 1, 0, 0}, {0, 0,-1} },
        { { 0,-1, 0}, { 1, 0, 0}, {0, 0, 1} },
    };
    mat3 rot;
    glm_mat4_pick3(xf, rot);

    for (int f = 0; f < 6; f++) {
        vec3 n, u, up;
        glm_vec3_copy((float *)faces[f][0], n);
        glm_vec3_copy((float *)faces[f][1], u);
        glm_vec3_copy((float *)faces[f][2], up);

        /* world-space directions of this face */
        vec3 wn, wu, wup;
        glm_mat3_mulv(rot, n, wn);
        glm_mat3_mulv(rot, u, wu);
        glm_mat3_mulv(rot, up, wup);
        glm_vec3_normalize(wn);
        glm_vec3_normalize(wu);
        glm_vec3_normalize(wup);
        vec3 c;
        glm_vec3_cross(wn, wu, c);
        float w = glm_vec3_dot(c, wup) >= 0.0f ? 1.0f : -1.0f;

        int base = mb->vert_count;
        for (int i = 0; i < 4; i++) {
            float su = (i == 1 || i == 2) ? 1.0f : -1.0f;
            float sv = (i >= 2) ? 1.0f : -1.0f;
            vec3 p;
            for (int k = 0; k < 3; k++)
                p[k] = (n[k] + u[k] * su + up[k] * sv) * half[k];
            vec3 wp;
            glm_mat4_mulv3(xf, p, 1.0f, wp);
            add_vert(mb, wp, wn, glm_vec3_dot(wp, wu) / tile, -glm_vec3_dot(wp, wup) / tile, wu, w);
        }
        add_tri(mb, base, base + 1, base + 2);
        add_tri(mb, base, base + 2, base + 3);
    }
}

/* one flat quad or triangle (corners in order), its normal from the winding */
static void flat_face(MeshBuilder *mb, mat4 xf, vec3 *pts, int n, float tile)
{
    vec3 w[4];
    for (int i = 0; i < n; i++)
        glm_mat4_mulv3(xf, pts[i], 1.0f, w[i]);
    vec3 e1, e2, nrm, u;
    glm_vec3_sub(w[1], w[0], e1);
    glm_vec3_sub(w[n - 1], w[0], e2);
    glm_vec3_cross(e1, e2, nrm);
    glm_vec3_normalize(nrm);
    glm_vec3_normalize_to(e1, u);
    vec3 up;
    glm_vec3_cross(nrm, u, up);
    int base = mb->vert_count;
    for (int i = 0; i < n; i++)
        add_vert(mb, w[i], nrm, glm_vec3_dot(w[i], u) / tile, -glm_vec3_dot(w[i], up) / tile, u, 1.0f);
    add_tri(mb, base, base + 1, base + 2);
    if (n == 4)
        add_tri(mb, base, base + 2, base + 3);
}

void mb_wedge(MeshBuilder *mb, mat4 xf, vec3 half, float tile)
{
    float x = half[0], y = half[1], z = half[2];
    vec3 front[3] = { { -x, -y, z }, { x, -y, z }, { 0, y, z } };
    vec3 back[3] = { { x, -y, -z }, { -x, -y, -z }, { 0, y, -z } };
    vec3 right[4] = { { x, -y, z }, { x, -y, -z }, { 0, y, -z }, { 0, y, z } };
    vec3 left[4] = { { -x, -y, -z }, { -x, -y, z }, { 0, y, z }, { 0, y, -z } };
    vec3 bottom[4] = { { -x, -y, -z }, { x, -y, -z }, { x, -y, z }, { -x, -y, z } };
    flat_face(mb, xf, front, 3, tile);
    flat_face(mb, xf, back, 3, tile);
    flat_face(mb, xf, right, 4, tile);
    flat_face(mb, xf, left, 4, tile);
    flat_face(mb, xf, bottom, 4, tile);
}

void mb_cylinder(MeshBuilder *mb, mat4 xf, float r_bottom, float r_top, float height,
                 int segments, float tile)
{
    mat3 rot;
    glm_mat4_pick3(xf, rot);
    float slope = (r_bottom - r_top) / height;
    float circ = 2.0f * GLM_PIf * (r_bottom + r_top) * 0.5f;

    int base = mb->vert_count;
    for (int i = 0; i <= segments; i++) {
        float a = (float)i / segments * 2.0f * GLM_PIf;
        float ca = cosf(a), sa = sinf(a);
        for (int j = 0; j < 2; j++) {
            float r = j ? r_top : r_bottom;
            vec3 p = { ca * r, j ? height : 0.0f, sa * r };
            vec3 n = { ca, slope, sa };
            vec3 t = { -sa, 0.0f, ca };
            vec3 wp, wn, wt;
            glm_mat4_mulv3(xf, p, 1.0f, wp);
            glm_mat3_mulv(rot, n, wn);
            glm_vec3_normalize(wn);
            glm_mat3_mulv(rot, t, wt);
            glm_vec3_normalize(wt);
            /* u wraps around, v runs down from the top (texture upright) */
            add_vert(mb, wp, wn, (float)i / segments * circ / tile, -p[1] / tile, wt, -1.0f);
        }
    }
    for (int i = 0; i < segments; i++) {
        int a = base + i * 2;
        add_tri(mb, a, a + 1, a + 3);
        add_tri(mb, a, a + 3, a + 2);
    }

    /* caps */
    for (int j = 0; j < 2; j++) {
        float r = j ? r_top : r_bottom;
        if (r <= 0.0f)
            continue;
        vec3 n = { 0.0f, j ? 1.0f : -1.0f, 0.0f }, t = { 1, 0, 0 }, wn, wt;
        glm_mat3_mulv(rot, n, wn);
        glm_mat3_mulv(rot, t, wt);
        int center;
        vec3 cp = { 0.0f, j ? height : 0.0f, 0.0f }, wcp;
        glm_mat4_mulv3(xf, cp, 1.0f, wcp);
        center = add_vert(mb, wcp, wn, 0.5f, 0.5f, wt, 1.0f);
        int first = mb->vert_count;
        for (int i = 0; i <= segments; i++) {
            float a = (float)i / segments * 2.0f * GLM_PIf;
            vec3 p = { cosf(a) * r, cp[1], sinf(a) * r }, wp;
            glm_mat4_mulv3(xf, p, 1.0f, wp);
            add_vert(mb, wp, wn, p[0] / tile, p[2] / tile, wt, 1.0f);
        }
        for (int i = 0; i < segments; i++) {
            if (j)
                add_tri(mb, center, first + i + 1, first + i);
            else
                add_tri(mb, center, first + i, first + i + 1);
        }
    }
}

void mb_capsule(MeshBuilder *mb, mat4 xf, float r, float length, int segments)
{
    /* a sphere split at the equator, with the two halves pulled apart along -Z */
    mat3 rot;
    glm_mat4_pick3(xf, rot);
    int rings = segments / 2;
    if (rings < 2)
        rings = 2;

    int base = mb->vert_count;
    int cols = segments + 1;
    int rows = 0;
    for (int ring = 0; ring <= rings * 2 + 1; ring++) {
        /* back half: latitude +90 -> 0 around the origin;
         * front half: 0 -> -90 around (0, 0, -length). the equator ring appears in both */
        bool front = ring > rings;
        int k = front ? ring - rings - 1 : ring;
        float lat = front ? -(float)k / rings * GLM_PI_2f : GLM_PI_2f - (float)k / rings * GLM_PI_2f;
        float z_off = front ? -length : 0.0f;
        for (int i = 0; i < cols; i++) {
            float lon = (float)i / segments * 2.0f * GLM_PIf;
            vec3 n = { cosf(lat) * cosf(lon), cosf(lat) * sinf(lon), sinf(lat) };
            vec3 p = { n[0] * r, n[1] * r, n[2] * r + z_off };
            vec3 t = { -sinf(lon), cosf(lon), 0.0f };
            vec3 wp, wn, wt;
            glm_mat4_mulv3(xf, p, 1.0f, wp);
            glm_mat3_mulv(rot, n, wn);
            glm_vec3_normalize(wn);
            glm_mat3_mulv(rot, t, wt);
            glm_vec3_normalize(wt);
            add_vert(mb, wp, wn, (float)i / segments, (float)ring / (rings * 2 + 1), wt, 1.0f);
        }
        rows++;
    }
    for (int y = 0; y < rows - 1; y++) {
        for (int i = 0; i < segments; i++) {
            int a = base + y * cols + i, b = a + cols;
            add_tri(mb, a, b + 1, a + 1);
            add_tri(mb, a, b, b + 1);
        }
    }
}

/* ---------- materials ---------- */

typedef struct {
    char dir[256];
    GLuint albedo, normal, arm;
} TexSet;

#define MAX_TEXSETS 64
static TexSet cache[MAX_TEXSETS];
static int cache_count;

void material_from_dir(Material *mat, const char *dir)
{
    TexSet *set = NULL;
    for (int i = 0; i < cache_count; i++)
        if (strcmp(cache[i].dir, dir) == 0)
            set = &cache[i];
    if (!set && cache_count == MAX_TEXSETS) {
        fprintf(stderr, "too many texture folders (max %d), can't add %s\n", MAX_TEXSETS, dir);
        exit(1);
    }
    if (!set) {
        set = &cache[cache_count++];
        snprintf(set->dir, sizeof set->dir, "%s", dir);
        char path[300];
        snprintf(path, sizeof path, "%s/albedo.jpg", dir);
        set->albedo = texture_load(path, true);
        snprintf(path, sizeof path, "%s/normal.jpg", dir);
        set->normal = texture_load(path, false);
        snprintf(path, sizeof path, "%s/arm.jpg", dir);
        set->arm = texture_load(path, false);
    }

    material_color(mat, 1, 1, 1, 1, 1);
    /* Poly Haven "arm" = ambient occlusion (r), roughness (g), metalness (b):
     * the same channels glTF uses, so one texture serves both slots */
    mat->base_tex = set->albedo;
    mat->normal_tex = set->normal;
    mat->mr_tex = set->arm;
    mat->occlusion_tex = set->arm;
}

void material_color(Material *mat, float r, float g, float b, float roughness, float metallic)
{
    memset(mat, 0, sizeof *mat);
    glm_vec4_copy((vec4){r, g, b, 1.0f}, mat->base_color);
    mat->roughness = roughness;
    mat->metallic = metallic;
    mat->normal_scale = 1.0f;
    mat->occlusion_strength = 1.0f;
    mat->alpha_cutoff = 0.5f;
    mat->alpha_mode = ALPHA_OPAQUE;
}

/* ---------- models ---------- */

static void build(Model *m, MeshBuilder *mbs, const Material *mats, int count, bool skinned)
{
    m->prim_count = count;
    m->prims = calloc(count, sizeof *m->prims);
    m->mesh_count = 1;
    m->meshes = calloc(1, sizeof *m->meshes);
    m->meshes[0].count = count;
    m->textures = calloc(1, sizeof *m->textures);   /* materials' textures are shared, not owned */
    m->anims = calloc(1, sizeof *m->anims);

    glm_vec3_fill(m->min,  FLT_MAX);
    glm_vec3_fill(m->max, -FLT_MAX);

    for (int i = 0; i < count; i++) {
        MeshBuilder *mb = &mbs[i];
        Primitive *p = &m->prims[i];
        p->mat = mats[i];
        if (!mb->idx_count)
            continue;

        glCreateVertexArrays(1, &p->vao);
        p->vbo[0] = upload_buffer(mb->verts, mb->vert_count * 12 * sizeof(float));
        GLsizei stride = 12 * sizeof(float);
        int sizes[4] = { 3, 3, 2, 4 }, offs[4] = { 0, 3, 6, 8 };
        for (int a = 0; a < 4; a++) {
            glVertexArrayVertexBuffer(p->vao, a, p->vbo[0], offs[a] * sizeof(float), stride);
            glEnableVertexArrayAttrib(p->vao, a);
            glVertexArrayAttribFormat(p->vao, a, sizes[a], GL_FLOAT, GL_FALSE, 0);
            glVertexArrayAttribBinding(p->vao, a, a);
        }
        p->ebo = upload_buffer(mb->idx, mb->idx_count * sizeof *mb->idx);
        glVertexArrayElementBuffer(p->vao, p->ebo);
        p->count = mb->idx_count;
        p->has_normals = p->has_tangents = true;

        if (skinned) {
            /* rigid skinning: each vertex follows exactly one bone */
            GLushort *j4 = calloc(mb->vert_count * 4, sizeof *j4);
            float *w4 = calloc(mb->vert_count * 4, sizeof *w4);
            for (int v = 0; v < mb->vert_count; v++) {
                j4[v * 4] = mb->joints[v];
                w4[v * 4] = 1.0f;
            }
            p->vbo[4] = upload_buffer(j4, mb->vert_count * 4 * sizeof *j4);
            glVertexArrayVertexBuffer(p->vao, 4, p->vbo[4], 0, 4 * sizeof *j4);
            glEnableVertexArrayAttrib(p->vao, 4);
            glVertexArrayAttribIFormat(p->vao, 4, 4, GL_UNSIGNED_SHORT, 0);
            glVertexArrayAttribBinding(p->vao, 4, 4);
            p->vbo[5] = upload_buffer(w4, mb->vert_count * 4 * sizeof *w4);
            glVertexArrayVertexBuffer(p->vao, 5, p->vbo[5], 0, 4 * sizeof *w4);
            glEnableVertexArrayAttrib(p->vao, 5);
            glVertexArrayAttribFormat(p->vao, 5, 4, GL_FLOAT, GL_FALSE, 0);
            glVertexArrayAttribBinding(p->vao, 5, 5);
            p->skinned = true;
            free(j4);
            free(w4);
        }

        glm_vec3_fill(p->bmin,  FLT_MAX);
        glm_vec3_fill(p->bmax, -FLT_MAX);
        for (int v = 0; v < mb->vert_count; v++) {
            glm_vec3_minv(p->bmin, &mb->verts[v * 12], p->bmin);
            glm_vec3_maxv(p->bmax, &mb->verts[v * 12], p->bmax);
        }
        glm_vec3_minv(m->min, p->bmin, m->min);
        glm_vec3_maxv(m->max, p->bmax, m->max);
    }
}

static void init_node(ModelNode *n, int parent, int mesh, int skin)
{
    memset(n, 0, sizeof *n);
    n->parent = parent;
    n->mesh = mesh;
    n->skin = skin;
    glm_quat_identity(n->r);
    glm_vec3_one(n->s);
}

void model_from_builders(Model *m, MeshBuilder *mbs, const Material *mats, int count)
{
    memset(m, 0, sizeof *m);
    build(m, mbs, mats, count, false);
    m->node_count = 1;
    m->nodes = malloc(sizeof *m->nodes);
    init_node(&m->nodes[0], -1, 0, -1);
    m->skins = calloc(1, sizeof *m->skins);
    model_prepare(m);
}

void model_from_builders_skinned(Model *m, MeshBuilder *mbs, const Material *mats, int count,
                                 int bone_count, const int *parents, const mat4 *rest_local)
{
    memset(m, 0, sizeof *m);
    build(m, mbs, mats, count, true);

    /* bones first, then one node holding the skinned mesh */
    m->node_count = bone_count + 1;
    m->nodes = malloc(m->node_count * sizeof *m->nodes);
    for (int b = 0; b < bone_count; b++) {
        ModelNode *n = &m->nodes[b];
        init_node(n, parents[b], -1, -1);
        snprintf(n->name, sizeof n->name, "bone%d", b);
        mat4 rot;
        glm_decompose((vec4 *)rest_local[b], (vec4){0}, rot, n->s);
        glm_vec3_copy((float *)rest_local[b][3], n->t);
        glm_mat4_quat(rot, n->r);
    }
    init_node(&m->nodes[bone_count], -1, 0, 0);

    /* the builders' vertices were placed at the bones' rest positions (model space) */
    m->skin_count = 1;
    m->skins = calloc(1, sizeof *m->skins);
    Skin *s = &m->skins[0];
    s->joint_count = bone_count;
    s->joints = malloc(bone_count * sizeof *s->joints);
    s->inverse_bind = malloc(bone_count * sizeof *s->inverse_bind);

    mat4 *world = malloc(bone_count * sizeof *world);
    for (int b = 0; b < bone_count; b++) {
        if (parents[b] >= 0)
            glm_mat4_mul(world[parents[b]], (vec4 *)rest_local[b], world[b]);
        else
            glm_mat4_copy((vec4 *)rest_local[b], world[b]);
        glm_mat4_inv(world[b], s->inverse_bind[b]);
        s->joints[b] = b;
    }
    free(world);
    model_prepare(m);
}
