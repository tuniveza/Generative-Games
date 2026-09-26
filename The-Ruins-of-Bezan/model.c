#include "model.h"
#include "texture.h"

#include <cgltf.h>
#include <stb_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* uniform locations, must match shaders/model.{vert,frag} and shadow.vert */
enum {
    U_MVP = 0,
    U_MODEL,
    U_BASE_COLOR,
    U_METALLIC,
    U_ROUGHNESS,
    U_NORMAL_SCALE,
    U_OCCLUSION_STRENGTH,
    U_EMISSIVE,
    U_ALPHA_CUTOFF,
    U_FLAGS,
    U_SKINNED,
    U_TINT,
    U_NORMAL_MATRIX,
};

/* bits in U_FLAGS */
enum {
    F_BASE_TEX      = 1 << 0,
    F_MR_TEX        = 1 << 1,
    F_NORMAL_TEX    = 1 << 2,
    F_OCCLUSION_TEX = 1 << 3,
    F_EMISSIVE_TEX  = 1 << 4,
    F_NORMALS       = 1 << 5,
    F_TANGENTS      = 1 << 6,
};

#define MAX_JOINTS 256
#define JOINT_BINDING 2

static GLuint joint_buffer;

void model_init_joint_buffer(void)
{
    glCreateBuffers(1, &joint_buffer);
    glNamedBufferStorage(joint_buffer, MAX_JOINTS * sizeof(mat4), NULL, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, JOINT_BINDING, joint_buffer);
}

/* ---------- textures ---------- */

/* "data:image/png;base64,iVBOR..." -> decoded bytes (free with free) */
static void *decode_data_uri(const char *uri, cgltf_size *size)
{
    const char *comma = strchr(uri, ',');
    if (!comma || comma - uri < 7 || strncmp(comma - 7, ";base64", 7) != 0)
        return NULL;

    const char *b64 = comma + 1;
    cgltf_size len = strlen(b64);
    *size = len / 4 * 3;
    for (cgltf_size i = len; i > 0 && b64[i - 1] == '='; i--)
        (*size)--;

    cgltf_options opts = {0};
    void *out = NULL;
    if (cgltf_load_buffer_base64(&opts, *size, b64, &out) != cgltf_result_success)
        return NULL;
    return out;
}

static const ModelOptions *loading_opts;    /* options of the model being loaded */

/* loads an image embedded in a buffer (.glb), embedded as base64, or as a file next to the .gltf */
static GLuint load_image(const cgltf_image *img, bool srgb, const char *gltf_path)
{
    int w, h, n;
    unsigned char *pixels = NULL;

    if (img->buffer_view) {
        const unsigned char *data = cgltf_buffer_view_data(img->buffer_view);
        pixels = stbi_load_from_memory(data, (int)img->buffer_view->size, &w, &h, &n, 4);
    } else if (img->uri && strncmp(img->uri, "data:", 5) == 0) {
        cgltf_size size;
        void *data = decode_data_uri(img->uri, &size);
        if (data) {
            pixels = stbi_load_from_memory(data, (int)size, &w, &h, &n, 4);
            free(data);
        }
    } else if (img->uri) {
        /* image uris are relative to the .gltf file's folder */
        const char *slash = strrchr(gltf_path, '/');
        int dir_len = slash ? (int)(slash - gltf_path) + 1 : 0;
        char path[1024];
        snprintf(path, sizeof path, "%.*s%s", dir_len, gltf_path, img->uri);
        cgltf_decode_uri(path + dir_len);   /* "my%20file.png" -> "my file.png" */
        pixels = stbi_load(path, &w, &h, &n, 4);
    }

    if (!pixels) {
        fprintf(stderr, "can't load image %s: %s\n",
                img->uri && strncmp(img->uri, "data:", 5) != 0 ? img->uri : "(embedded)",
                stbi_failure_reason());
        return 0;
    }

    if (srgb && loading_opts && loading_opts->recolor)
        loading_opts->recolor(pixels, w, h);
    GLuint tex = texture_from_pixels(pixels, w, h, srgb);
    stbi_image_free(pixels);
    return tex;
}

/* each image is loaded at most twice: once linear, once sRGB, depending on what uses it */
static GLuint get_texture(Model *m, const cgltf_data *data, const cgltf_texture_view *view,
                          bool srgb, const char *gltf_path)
{
    const cgltf_texture *t = view->texture;
    if (!t || !t->image)
        return 0;

    GLuint *slot = &m->textures[(t->image - data->images) * 2 + srgb];
    if (!*slot)
        *slot = load_image(t->image, srgb, gltf_path);
    return *slot;
}

static void load_material(Material *out, const cgltf_material *mat, Model *m,
                          const cgltf_data *data, const char *gltf_path)
{
    glm_vec4_one(out->base_color);
    glm_vec3_zero(out->emissive);
    out->metallic = 1.0f;
    out->roughness = 1.0f;
    out->normal_scale = 1.0f;
    out->occlusion_strength = 1.0f;
    out->alpha_cutoff = 0.5f;
    out->alpha_mode = ALPHA_OPAQUE;
    if (!mat)
        return;

    if (mat->has_pbr_metallic_roughness) {
        const cgltf_pbr_metallic_roughness *pbr = &mat->pbr_metallic_roughness;
        glm_vec4_make(pbr->base_color_factor, out->base_color);
        out->metallic = pbr->metallic_factor;
        out->roughness = pbr->roughness_factor;
        out->base_tex = get_texture(m, data, &pbr->base_color_texture, true, gltf_path);
        out->mr_tex = get_texture(m, data, &pbr->metallic_roughness_texture, false, gltf_path);
    }

    out->normal_tex = get_texture(m, data, &mat->normal_texture, false, gltf_path);
    if (mat->normal_texture.texture)
        out->normal_scale = mat->normal_texture.scale;

    out->occlusion_tex = get_texture(m, data, &mat->occlusion_texture, false, gltf_path);
    if (mat->occlusion_texture.texture)
        out->occlusion_strength = mat->occlusion_texture.scale;

    glm_vec3_make(mat->emissive_factor, out->emissive);
    if (mat->has_emissive_strength)
        glm_vec3_scale(out->emissive, mat->emissive_strength.emissive_strength, out->emissive);
    out->emissive_tex = get_texture(m, data, &mat->emissive_texture, true, gltf_path);

    out->double_sided = mat->double_sided;
    out->alpha_cutoff = mat->alpha_cutoff;
    out->alpha_mode = mat->alpha_mode == cgltf_alpha_mode_mask  ? ALPHA_MASK
                    : mat->alpha_mode == cgltf_alpha_mode_blend ? ALPHA_BLEND
                    : ALPHA_OPAQUE;
}

/* ---------- geometry ---------- */

static const cgltf_accessor *find_attr(const cgltf_primitive *p, cgltf_attribute_type type)
{
    for (cgltf_size i = 0; i < p->attributes_count; i++)
        if (p->attributes[i].type == type && p->attributes[i].index == 0)
            return p->attributes[i].data;
    return NULL;
}

GLuint upload_buffer(const void *data, size_t bytes)
{
    GLuint buf;
    glCreateBuffers(1, &buf);
    glNamedBufferStorage(buf, bytes ? bytes : 4, data, 0);     /* GL refuses empty storage */
    return buf;
}

/* copies an accessor into a tightly packed float buffer, whatever its stored format */
static GLuint upload_floats(const cgltf_accessor *acc, int components)
{
    cgltf_size n = acc->count * components;
    float *data = malloc(n * sizeof *data);
    cgltf_accessor_unpack_floats(acc, data, n);
    GLuint buf = upload_buffer(data, n * sizeof *data);
    free(data);
    return buf;
}

static void attach(GLuint vao, GLuint attrib, GLuint buf, int components)
{
    glVertexArrayVertexBuffer(vao, attrib, buf, 0, components * sizeof(float));
    glEnableVertexArrayAttrib(vao, attrib);
    glVertexArrayAttribFormat(vao, attrib, components, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(vao, attrib, attrib);
}

/* bone indices go in as integers (IFormat), not floats */
static void attach_joints(Primitive *out, const GLushort *joints, const float *weights, size_t count)
{
    out->vbo[4] = upload_buffer(joints, count * 4 * sizeof *joints);
    glVertexArrayVertexBuffer(out->vao, 4, out->vbo[4], 0, 4 * sizeof *joints);
    glEnableVertexArrayAttrib(out->vao, 4);
    glVertexArrayAttribIFormat(out->vao, 4, 4, GL_UNSIGNED_SHORT, 0);
    glVertexArrayAttribBinding(out->vao, 4, 4);

    out->vbo[5] = upload_buffer(weights, count * 4 * sizeof *weights);
    attach(out->vao, 5, out->vbo[5], 4);
    out->skinned = true;
}

static void upload_primitive(Primitive *out, const cgltf_primitive *p, Model *m,
                             const cgltf_data *data, const char *gltf_path)
{
    memset(out, 0, sizeof *out);

    const cgltf_accessor *pos = find_attr(p, cgltf_attribute_type_position);
    const cgltf_accessor *nrm = find_attr(p, cgltf_attribute_type_normal);
    const cgltf_accessor *uv  = find_attr(p, cgltf_attribute_type_texcoord);
    const cgltf_accessor *tan = find_attr(p, cgltf_attribute_type_tangent);
    const cgltf_accessor *jnt = find_attr(p, cgltf_attribute_type_joints);
    const cgltf_accessor *wgt = find_attr(p, cgltf_attribute_type_weights);
    if (!pos || p->type != cgltf_primitive_type_triangles)
        return;             /* count stays 0, so it is skipped when drawing */

    glCreateVertexArrays(1, &out->vao);

    out->vbo[0] = upload_floats(pos, 3);
    attach(out->vao, 0, out->vbo[0], 3);
    if (pos->has_min && pos->has_max) {
        glm_vec3_copy((float *)pos->min, out->bmin);
        glm_vec3_copy((float *)pos->max, out->bmax);
    }
    if (nrm) {
        out->vbo[1] = upload_floats(nrm, 3);
        attach(out->vao, 1, out->vbo[1], 3);
        out->has_normals = true;
    }
    if (uv) {
        out->vbo[2] = upload_floats(uv, 2);
        attach(out->vao, 2, out->vbo[2], 2);
    }
    if (tan && nrm) {
        /* xyz = tangent direction, w = +-1 bitangent sign */
        out->vbo[3] = upload_floats(tan, 4);
        attach(out->vao, 3, out->vbo[3], 4);
        out->has_tangents = true;
    }
    if (jnt && wgt) {
        cgltf_size n = jnt->count;
        GLushort *joints = malloc(n * 4 * sizeof *joints);
        float *weights = malloc(n * 4 * sizeof *weights);
        for (cgltf_size i = 0; i < n; i++) {
            cgltf_uint j[4] = {0};
            cgltf_accessor_read_uint(jnt, i, j, 4);
            for (int k = 0; k < 4; k++)
                joints[i * 4 + k] = (GLushort)j[k];
            cgltf_accessor_read_float(wgt, i, &weights[i * 4], 4);
        }
        attach_joints(out, joints, weights, n);
        free(joints);
        free(weights);
    }

    if (p->indices) {
        cgltf_size n = p->indices->count;
        GLuint *idx = malloc(n * sizeof *idx);
        cgltf_accessor_unpack_indices(p->indices, idx, sizeof *idx, n);
        out->ebo = upload_buffer(idx, n * sizeof *idx);
        glVertexArrayElementBuffer(out->vao, out->ebo);
        free(idx);
        out->count = (GLsizei)n;
    } else {
        out->count = (GLsizei)pos->count;
    }

    load_material(&out->mat, p->material, m, data, gltf_path);
}

/* ---------- nodes, skins, animations ---------- */

static void load_node(ModelNode *out, const cgltf_node *node, const cgltf_data *data)
{
    snprintf(out->name, sizeof out->name, "%s", node->name ? node->name : "");
    out->parent = node->parent ? (int)(node->parent - data->nodes) : -1;
    out->mesh = node->mesh ? (int)(node->mesh - data->meshes) : -1;
    out->skin = node->skin ? (int)(node->skin - data->skins) : -1;

    if (node->has_matrix) {
        mat4 mat, rot;
        glm_mat4_make(node->matrix, mat);
        glm_decompose(mat, (vec4){0}, rot, out->s);
        glm_vec3_copy(mat[3], out->t);
        glm_mat4_quat(rot, out->r);
        return;
    }
    glm_vec3_copy(node->has_translation ? (float *)node->translation : (vec3){0, 0, 0}, out->t);
    /* cgltf's arrays aren't 16-byte aligned, which cglm's vec4 SIMD code needs */
    if (node->has_rotation)
        memcpy(out->r, node->rotation, sizeof out->r);
    else
        glm_quat_identity(out->r);
    glm_vec3_copy(node->has_scale ? (float *)node->scale : (vec3){1, 1, 1}, out->s);
}

/* parents before children, so world matrices can be built in one pass */
static void build_order(Model *m)
{
    free(m->order);
    m->order = malloc(m->node_count * sizeof *m->order);
    int *depth = calloc(m->node_count, sizeof *depth);
    int max_depth = 0;
    for (int i = 0; i < m->node_count; i++) {
        for (int p = m->nodes[i].parent; p >= 0; p = m->nodes[p].parent)
            depth[i]++;
        if (depth[i] > max_depth)
            max_depth = depth[i];
    }
    int n = 0;
    for (int d = 0; d <= max_depth; d++)
        for (int i = 0; i < m->node_count; i++)
            if (depth[i] == d)
                m->order[n++] = i;
    free(depth);
}

void model_prepare(Model *m)
{
    glm_mat4_identity(m->base);
    for (int s = 0; s < m->skin_count; s++) {
        m->skins[s].offset = m->joint_total;
        m->joint_total += m->skins[s].joint_count;
    }
    build_order(m);
    pose_init(&m->rest, m);
}

static void load_skins(Model *m, const cgltf_data *data)
{
    m->skin_count = (int)data->skins_count;
    m->skins = calloc(m->skin_count ? m->skin_count : 1, sizeof *m->skins);
    for (cgltf_size i = 0; i < data->skins_count; i++) {
        const cgltf_skin *s = &data->skins[i];
        Skin *out = &m->skins[i];
        out->joint_count = (int)s->joints_count;
        out->joints = malloc(out->joint_count * sizeof *out->joints);
        out->inverse_bind = malloc(out->joint_count * sizeof *out->inverse_bind);
        for (int j = 0; j < out->joint_count; j++) {
            out->joints[j] = (int)(s->joints[j] - data->nodes);
            if (s->inverse_bind_matrices)
                cgltf_accessor_read_float(s->inverse_bind_matrices, j, (float *)out->inverse_bind[j], 16);
            else
                glm_mat4_identity(out->inverse_bind[j]);
        }
    }
}

static void load_anims(Model *m, const cgltf_data *data)
{
    m->anim_count = (int)data->animations_count;
    m->anims = calloc(m->anim_count ? m->anim_count : 1, sizeof *m->anims);
    for (cgltf_size i = 0; i < data->animations_count; i++) {
        const cgltf_animation *a = &data->animations[i];
        Animation *out = &m->anims[i];
        snprintf(out->name, sizeof out->name, "%s", a->name ? a->name : "");
        out->channels = calloc(a->channels_count ? a->channels_count : 1, sizeof *out->channels);

        for (cgltf_size c = 0; c < a->channels_count; c++) {
            const cgltf_animation_channel *ch = &a->channels[c];
            if (!ch->target_node || ch->target_path == cgltf_animation_path_type_weights)
                continue;       /* morph targets aren't supported */

            AnimChannel *o = &out->channels[out->channel_count++];
            o->node = (int)(ch->target_node - data->nodes);
            o->path = ch->target_path == cgltf_animation_path_type_translation ? PATH_TRANSLATION
                    : ch->target_path == cgltf_animation_path_type_rotation    ? PATH_ROTATION
                    : PATH_SCALE;
            o->interp = ch->sampler->interpolation == cgltf_interpolation_type_step ? INTERP_STEP
                      : ch->sampler->interpolation == cgltf_interpolation_type_cubic_spline ? INTERP_CUBIC
                      : INTERP_LINEAR;

            const cgltf_accessor *in = ch->sampler->input, *vals = ch->sampler->output;
            o->count = (int)in->count;
            o->times = malloc(in->count * sizeof *o->times);
            cgltf_accessor_unpack_floats(in, o->times, in->count);
            cgltf_size nv = vals->count * cgltf_num_components(vals->type);
            o->values = malloc(nv * sizeof *o->values);
            cgltf_accessor_unpack_floats(vals, o->values, nv);

            if (o->count && o->times[o->count - 1] > out->duration)
                out->duration = o->times[o->count - 1];
        }
    }
}

/* chain of bones along the longest axis of mesh 0, weights blended between the two
 * nearest bones. turns a static model into one we can bend from code. */
static void autorig(Model *m, const cgltf_data *data, int bones)
{
    if (!data->meshes_count || bones < 2)
        return;
    const cgltf_mesh *mesh = &data->meshes[0];

    /* bounds of all of mesh 0's positions */
    vec3 mn = { FLT_MAX, FLT_MAX, FLT_MAX }, mx = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (cgltf_size i = 0; i < mesh->primitives_count; i++) {
        const cgltf_accessor *pos = find_attr(&mesh->primitives[i], cgltf_attribute_type_position);
        if (pos && pos->has_min) {
            glm_vec3_minv(mn, (float *)pos->min, mn);
            glm_vec3_maxv(mx, (float *)pos->max, mx);
        }
    }
    vec3 ext;
    glm_vec3_sub(mx, mn, ext);
    int axis = ext[0] > ext[1] ? (ext[0] > ext[2] ? 0 : 2) : (ext[1] > ext[2] ? 1 : 2);
    float len = ext[axis];
    vec3 center;
    glm_vec3_center(mn, mx, center);

    /* new bone nodes, appended after the model's own */
    int first = m->node_count;
    m->node_count += bones;
    m->nodes = realloc(m->nodes, m->node_count * sizeof *m->nodes);

    int skin = m->skin_count++;
    m->skins = realloc(m->skins, m->skin_count * sizeof *m->skins);
    Skin *s = &m->skins[skin];
    s->joint_count = bones;
    s->joints = malloc(bones * sizeof *s->joints);
    s->inverse_bind = malloc(bones * sizeof *s->inverse_bind);

    float step = len / (bones - 1);
    for (int b = 0; b < bones; b++) {
        ModelNode *n = &m->nodes[first + b];
        memset(n, 0, sizeof *n);
        snprintf(n->name, sizeof n->name, "bone%d", b);
        n->parent = b ? first + b - 1 : -1;
        n->mesh = n->skin = -1;
        glm_quat_identity(n->r);
        glm_vec3_one(n->s);
        if (b == 0) {
            glm_vec3_copy(center, n->t);
            n->t[axis] = mn[axis];
        } else {
            n->t[axis] = step;
        }

        vec3 rest_pos;
        glm_vec3_copy(center, rest_pos);
        rest_pos[axis] = mn[axis] + step * b;
        glm_translate_make(s->inverse_bind[b], (vec3){ -rest_pos[0], -rest_pos[1], -rest_pos[2] });
        s->joints[b] = first + b;
    }

    for (int i = 0; i < m->node_count; i++)
        if (m->nodes[i].mesh == 0)
            m->nodes[i].skin = skin;

    for (cgltf_size i = 0; i < mesh->primitives_count; i++) {
        Primitive *prim = &m->prims[m->meshes[0].first + i];
        const cgltf_accessor *pos = find_attr(&mesh->primitives[i], cgltf_attribute_type_position);
        if (!pos || !prim->count)
            continue;
        GLushort *joints = malloc(pos->count * 4 * sizeof *joints);
        float *weights = malloc(pos->count * 4 * sizeof *weights);
        for (cgltf_size v = 0; v < pos->count; v++) {
            float p[3];
            cgltf_accessor_read_float(pos, v, p, 3);
            float u = glm_clamp((p[axis] - mn[axis]) / step, 0.0f, bones - 1.0f);
            int b0 = (int)u;
            if (b0 > bones - 2)
                b0 = bones - 2;
            float f = u - b0;
            GLushort jv[4] = { (GLushort)b0, (GLushort)(b0 + 1), 0, 0 };
            float wv[4] = { 1.0f - f, f, 0.0f, 0.0f };
            memcpy(&joints[v * 4], jv, sizeof jv);
            memcpy(&weights[v * 4], wv, sizeof wv);
        }
        attach_joints(prim, joints, weights, pos->count);
        free(joints);
        free(weights);
    }
}

/* ---------- poses ---------- */

void pose_init(Pose *p, const Model *m)
{
    p->node_count = m->node_count;
    p->t = malloc(m->node_count * sizeof *p->t);
    p->r = malloc(m->node_count * sizeof *p->r);
    p->s = malloc(m->node_count * sizeof *p->s);
    p->world = malloc(m->node_count * sizeof *p->world);
    p->joint_mats = malloc((m->joint_total ? m->joint_total : 1) * sizeof *p->joint_mats);
    pose_reset(p, m);
    pose_update(m, p);
}

void pose_free(Pose *p)
{
    free(p->t);
    free(p->r);
    free(p->s);
    free(p->world);
    free(p->joint_mats);
    memset(p, 0, sizeof *p);
}

void pose_reset(Pose *p, const Model *m)
{
    for (int i = 0; i < m->node_count; i++) {
        glm_vec3_copy((float *)m->nodes[i].t, p->t[i]);
        glm_vec4_copy((float *)m->nodes[i].r, p->r[i]);
        glm_vec3_copy((float *)m->nodes[i].s, p->s[i]);
    }
}

void pose_update(const Model *m, Pose *p)
{
    for (int k = 0; k < m->node_count; k++) {
        int i = m->order[k];
        mat4 local;
        glm_translate_make(local, p->t[i]);
        glm_quat_rotate(local, p->r[i], local);
        glm_scale(local, p->s[i]);

        int parent = m->nodes[i].parent;
        if (parent >= 0)
            glm_mat4_mul(p->world[parent], local, p->world[i]);
        else
            glm_mat4_copy(local, p->world[i]);
    }

    /* bone matrix = where the bone is now * (where it was at rest)^-1 */
    for (int s = 0; s < m->skin_count; s++) {
        const Skin *skin = &m->skins[s];
        for (int j = 0; j < skin->joint_count; j++)
            glm_mat4_mul(p->world[skin->joints[j]], skin->inverse_bind[j],
                         p->joint_mats[skin->offset + j]);
    }
}

/* index of the last keyframe at or before t */
static int find_key(const AnimChannel *c, float t)
{
    int lo = 0, hi = c->count - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (c->times[mid] <= t)
            lo = mid;
        else
            hi = mid - 1;
    }
    return lo;
}

static void sample_channel(const AnimChannel *c, float t, float *out)
{
    int n = c->path == PATH_ROTATION ? 4 : 3;
    int stride = c->interp == INTERP_CUBIC ? n * 3 : n;
    int value_at = c->interp == INTERP_CUBIC ? n : 0;   /* cubic: [in-tangent, value, out-tangent] */

    if (c->count == 1 || t <= c->times[0]) {
        memcpy(out, &c->values[value_at], n * sizeof *out);
        return;
    }
    if (t >= c->times[c->count - 1]) {
        memcpy(out, &c->values[(c->count - 1) * stride + value_at], n * sizeof *out);
        return;
    }

    int k = find_key(c, t);
    float t0 = c->times[k], t1 = c->times[k + 1];
    float dt = t1 - t0;
    float f = dt > 0.0f ? (t - t0) / dt : 0.0f;
    const float *a = &c->values[k * stride];
    const float *b = &c->values[(k + 1) * stride];

    if (c->interp == INTERP_STEP) {
        memcpy(out, a, n * sizeof *out);
    } else if (c->interp == INTERP_CUBIC) {
        /* hermite spline between the two keys using their tangents */
        float f2 = f * f, f3 = f2 * f;
        float h00 = 2 * f3 - 3 * f2 + 1, h10 = f3 - 2 * f2 + f;
        float h01 = -2 * f3 + 3 * f2,    h11 = f3 - f2;
        for (int i = 0; i < n; i++)
            out[i] = h00 * a[n + i] + h10 * dt * a[2 * n + i] + h01 * b[n + i] + h11 * dt * b[i];
        if (n == 4)
            glm_quat_normalize(out);
    } else if (n == 4) {
        versor qa, qb, q;          /* aligned copies for cglm */
        memcpy(qa, a, sizeof qa);
        memcpy(qb, b, sizeof qb);
        glm_quat_slerp(qa, qb, f, q);
        memcpy(out, q, sizeof q);
    } else {
        glm_vec3_lerp((float *)a, (float *)b, f, out);
    }
}

void anim_apply(const Model *m, int anim, float time, bool loop, float weight, Pose *p)
{
    if (anim < 0 || anim >= m->anim_count || weight <= 0.0f)
        return;
    const Animation *a = &m->anims[anim];
    if (a->duration > 0.0f)
        time = loop ? fmodf(time, a->duration) : glm_clamp(time, 0.0f, a->duration);
    if (time < 0.0f)
        time += a->duration;

    for (int i = 0; i < a->channel_count; i++) {
        const AnimChannel *c = &a->channels[i];
        versor v;
        sample_channel(c, time, v);
        if (c->path == PATH_ROTATION) {
            if (weight >= 1.0f)
                glm_vec4_copy(v, p->r[c->node]);
            else
                glm_quat_slerp(p->r[c->node], v, weight, p->r[c->node]);
        } else {
            float *dst = c->path == PATH_TRANSLATION ? p->t[c->node] : p->s[c->node];
            glm_vec3_lerp(dst, v, weight, dst);
        }
    }
}

/* ---------- loading ---------- */

static void grow_bounds(Model *m, const cgltf_accessor *pos, mat4 world)
{
    if (!pos->has_min || !pos->has_max)
        return;
    for (int i = 0; i < 8; i++) {
        vec3 corner = {
            (i & 1) ? pos->max[0] : pos->min[0],
            (i & 2) ? pos->max[1] : pos->min[1],
            (i & 4) ? pos->max[2] : pos->min[2],
        };
        glm_mat4_mulv3(world, corner, 1.0f, corner);
        glm_vec3_minv(m->min, corner, m->min);
        glm_vec3_maxv(m->max, corner, m->max);
    }
}

bool model_load(Model *m, const char *path)
{
    return model_load_ex(m, path, NULL);
}

bool model_load_ex(Model *m, const char *path, const ModelOptions *opts)
{
    memset(m, 0, sizeof *m);

    cgltf_options copts = {0};
    cgltf_data *data = NULL;
    cgltf_result r = cgltf_parse_file(&copts, path, &data);
    if (r == cgltf_result_success)
        r = cgltf_load_buffers(&copts, data, path);
    if (r == cgltf_result_success)
        r = cgltf_validate(data);
    if (r != cgltf_result_success) {
        fprintf(stderr, "can't load %s (cgltf error %d)\n", path, r);
        cgltf_free(data);
        return false;
    }

    loading_opts = opts;
    /* textures are loaded on demand by the materials that use them */
    m->texture_count = (int)data->images_count * 2;
    m->textures = calloc(m->texture_count ? m->texture_count : 1, sizeof *m->textures);

    for (cgltf_size i = 0; i < data->meshes_count; i++)
        m->prim_count += (int)data->meshes[i].primitives_count;
    m->prims = calloc(m->prim_count ? m->prim_count : 1, sizeof *m->prims);
    m->mesh_count = (int)data->meshes_count;
    m->meshes = calloc(m->mesh_count ? m->mesh_count : 1, sizeof *m->meshes);

    int next = 0;
    for (cgltf_size i = 0; i < data->meshes_count; i++) {
        const cgltf_mesh *mesh = &data->meshes[i];
        m->meshes[i].first = next;
        m->meshes[i].count = (int)mesh->primitives_count;
        for (cgltf_size j = 0; j < mesh->primitives_count; j++)
            upload_primitive(&m->prims[next++], &mesh->primitives[j], m, data, path);
    }
    loading_opts = NULL;

    m->node_count = (int)data->nodes_count;
    m->nodes = calloc(m->node_count ? m->node_count : 1, sizeof *m->nodes);
    for (cgltf_size i = 0; i < data->nodes_count; i++)
        load_node(&m->nodes[i], &data->nodes[i], data);

    load_skins(m, data);
    load_anims(m, data);
    if (opts && opts->autorig_bones > 0)
        autorig(m, data, opts->autorig_bones);
    model_prepare(m);

    /* bounds of the rest pose, in model space */
    glm_vec3_fill(m->min,  FLT_MAX);
    glm_vec3_fill(m->max, -FLT_MAX);
    for (cgltf_size i = 0; i < data->nodes_count; i++) {
        const cgltf_node *node = &data->nodes[i];
        if (!node->mesh)
            continue;
        mat4 ident = GLM_MAT4_IDENTITY_INIT;
        for (cgltf_size j = 0; j < node->mesh->primitives_count; j++) {
            const cgltf_accessor *pos = find_attr(&node->mesh->primitives[j], cgltf_attribute_type_position);
            /* skinned meshes ignore their node's transform */
            if (pos)
                grow_bounds(m, pos, node->skin ? ident : m->rest.world[i]);
        }
    }

    cgltf_free(data);
    return true;
}

void model_free(Model *m)
{
    for (int i = 0; i < m->prim_count; i++) {
        Primitive *p = &m->prims[i];
        glDeleteVertexArrays(1, &p->vao);
        glDeleteBuffers(6, p->vbo);
        glDeleteBuffers(1, &p->ebo);
    }
    glDeleteTextures(m->texture_count, m->textures);
    for (int s = 0; s < m->skin_count; s++) {
        free(m->skins[s].joints);
        free(m->skins[s].inverse_bind);
    }
    for (int a = 0; a < m->anim_count; a++) {
        for (int c = 0; c < m->anims[a].channel_count; c++) {
            free(m->anims[a].channels[c].times);
            free(m->anims[a].channels[c].values);
        }
        free(m->anims[a].channels);
    }
    pose_free(&m->rest);
    free(m->prims);
    free(m->meshes);
    free(m->nodes);
    free(m->order);
    free(m->skins);
    free(m->anims);
    free(m->textures);
    memset(m, 0, sizeof *m);
}

/* the box around (mn, mx) after moving it by xf */
static void bounds_through(mat4 xf, vec3 mn, vec3 mx)
{
    vec3 lo = { FLT_MAX, FLT_MAX, FLT_MAX }, hi = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (int k = 0; k < 8; k++) {
        vec3 c = { (k & 1) ? mx[0] : mn[0], (k & 2) ? mx[1] : mn[1], (k & 4) ? mx[2] : mn[2] };
        glm_mat4_mulv3(xf, c, 1.0f, c);
        glm_vec3_minv(lo, c, lo);
        glm_vec3_maxv(hi, c, hi);
    }
    glm_vec3_copy(lo, mn);
    glm_vec3_copy(hi, mx);
}

static void refit_bounds(Model *m)
{
    glm_vec3_fill(m->min,  FLT_MAX);
    glm_vec3_fill(m->max, -FLT_MAX);
    mat4 ident = GLM_MAT4_IDENTITY_INIT;
    for (int i = 0; i < m->node_count; i++) {
        const ModelNode *n = &m->nodes[i];
        if (n->mesh < 0)
            continue;
        const Mesh *mesh = &m->meshes[n->mesh];
        for (int j = 0; j < mesh->count; j++) {
            const Primitive *p = &m->prims[mesh->first + j];
            for (int k = 0; k < 8; k++) {
                vec3 c = { (k & 1) ? p->bmax[0] : p->bmin[0], (k & 2) ? p->bmax[1] : p->bmin[1],
                           (k & 4) ? p->bmax[2] : p->bmin[2] };
                glm_mat4_mulv3(n->skin >= 0 ? ident : m->rest.world[i], c, 1.0f, c);
                glm_vec3_minv(m->min, c, m->min);
                glm_vec3_maxv(m->max, c, m->max);
            }
        }
    }
    bounds_through(m->base, m->min, m->max);
}

void model_hide_node(Model *m, const char *name)
{
    int i = model_find_node(m, name);
    if (i >= 0) {
        m->nodes[i].mesh = -1;
        refit_bounds(m);
    }
}

int model_find_node(const Model *m, const char *name)
{
    for (int i = 0; i < m->node_count; i++)
        if (strcmp(m->nodes[i].name, name) == 0)
            return i;
    return -1;
}

int model_find_anim(const Model *m, const char *name)
{
    for (int i = 0; i < m->anim_count; i++)
        if (strcmp(m->anims[i].name, name) == 0)
            return i;
    return -1;
}

void transform_matrix(const Transform *t, mat4 out)
{
    glm_translate_make(out, (float *)t->pos);
    if (t->yaw != 0.0f)
        glm_rotate_y(out, t->yaw, out);
    if (t->pitch != 0.0f)
        glm_rotate_x(out, t->pitch, out);
    if (t->roll != 0.0f)
        glm_rotate_z(out, t->roll, out);
    glm_scale(out, (float *)t->scale);
}

void model_adjust(Model *m, const Transform *fix, bool ground)
{
    mat4 xf;
    transform_matrix(fix, xf);
    bounds_through(xf, m->min, m->max);
    if (ground) {
        /* lift (or drop) it so its lowest point is at y = 0 */
        mat4 lift;
        glm_translate_make(lift, (vec3){ 0.0f, -m->min[1], 0.0f });
        glm_mat4_mul(lift, xf, xf);
        m->max[1] -= m->min[1];
        m->min[1] = 0.0f;
    }
    glm_mat4_mul(xf, m->base, m->base);
}

void model_matrix(const Model *m, mat4 place, mat4 out)
{
    glm_mat4_mul(place, (vec4 *)m->base, out);
}

void model_fit(const Model *m, float size, mat4 dest)
{
    vec3 extent, center;
    glm_vec3_sub((float *)m->max, (float *)m->min, extent);
    glm_vec3_center((float *)m->min, (float *)m->max, center);

    float largest = glm_vec3_max(extent);
    float s = largest > 0.0f ? size / largest : 1.0f;

    glm_scale_make(dest, (vec3){s, s, s});
    glm_translate(dest, (vec3){-center[0], -center[1], -center[2]});
}

/* ---------- drawing ---------- */

static void set_material(GLuint prog, const Primitive *p)
{
    const Material *mat = &p->mat;
    int flags = (mat->base_tex      ? F_BASE_TEX      : 0)
              | (mat->mr_tex        ? F_MR_TEX        : 0)
              | (mat->normal_tex    ? F_NORMAL_TEX    : 0)
              | (mat->occlusion_tex ? F_OCCLUSION_TEX : 0)
              | (mat->emissive_tex  ? F_EMISSIVE_TEX  : 0)
              | (p->has_normals     ? F_NORMALS       : 0)
              | (p->has_tangents    ? F_TANGENTS      : 0);

    glProgramUniform4fv(prog, U_BASE_COLOR, 1, mat->base_color);
    glProgramUniform1f(prog, U_METALLIC, mat->metallic);
    glProgramUniform1f(prog, U_ROUGHNESS, mat->roughness);
    glProgramUniform1f(prog, U_NORMAL_SCALE, mat->normal_scale);
    glProgramUniform1f(prog, U_OCCLUSION_STRENGTH, mat->occlusion_strength);
    glProgramUniform3fv(prog, U_EMISSIVE, 1, mat->emissive);
    glProgramUniform1f(prog, U_ALPHA_CUTOFF, mat->alpha_mode == ALPHA_MASK ? mat->alpha_cutoff : -1.0f);
    glProgramUniform1i(prog, U_FLAGS, flags);

    /* texture units 0..4 = base, metallic-roughness, normal, occlusion, emissive */
    GLuint units[5] = { mat->base_tex, mat->mr_tex, mat->normal_tex,
                        mat->occlusion_tex, mat->emissive_tex };
    glBindTextures(0, 5, units);
}

static void draw_pass(const Model *m, const Pose *pose, GLuint prog, mat4 view_proj,
                      mat4 transform, const DrawParams *dp, bool blended)
{
    for (int k = 0; k < m->node_count; k++) {
        int ni = m->order[k];
        const ModelNode *node = &m->nodes[ni];
        if (node->mesh < 0 || (dp->skip && dp->skip[ni]))
            continue;

        /* skinned meshes are placed by their bones, not their node */
        bool skinned = node->skin >= 0;
        mat4 world, mvp;
        if (skinned)
            glm_mat4_copy(transform, world);
        else
            glm_mat4_mul(transform, (vec4 *)pose->world[ni], world);
        glm_mat4_mul(view_proj, world, mvp);
        glProgramUniformMatrix4fv(prog, U_MVP, 1, GL_FALSE, (const float *)mvp);
        /* the shadow program only has u_mvp and u_skinned; other locations there
         * may belong to something else entirely */
        if (!dp->depth_only) {
            /* inverse-transpose keeps normals correct under non-uniform scale */
            mat3 normal_matrix;
            glm_mat4_pick3(world, normal_matrix);
            glm_mat3_inv(normal_matrix, normal_matrix);
            glm_mat3_transpose(normal_matrix);
            glProgramUniformMatrix4fv(prog, U_MODEL, 1, GL_FALSE, (const float *)world);
            glProgramUniformMatrix3fv(prog, U_NORMAL_MATRIX, 1, GL_FALSE, (const float *)normal_matrix);
        }

        if (skinned) {
            const Skin *s = &m->skins[node->skin];
            int n = s->joint_count < MAX_JOINTS ? s->joint_count : MAX_JOINTS;
            glNamedBufferSubData(joint_buffer, 0, n * sizeof(mat4), pose->joint_mats[s->offset]);
        }

        const Mesh *mesh = &m->meshes[node->mesh];
        for (int j = 0; j < mesh->count; j++) {
            const Primitive *p = &m->prims[mesh->first + j];
            if (!p->count || (p->mat.alpha_mode == ALPHA_BLEND) != blended)
                continue;

            glProgramUniform1i(prog, U_SKINNED, skinned && p->skinned);
            if (!dp->depth_only && !dp->no_material) {
                set_material(prog, p);
                glProgramUniform4fv(prog, U_TINT, 1, dp->tint);
            }
            glBindVertexArray(p->vao);
            if (p->ebo)
                glDrawElements(GL_TRIANGLES, p->count, GL_UNSIGNED_INT, NULL);
            else
                glDrawArrays(GL_TRIANGLES, 0, p->count);
        }
    }
}

void model_draw(const Model *m, const Pose *pose, GLuint prog, mat4 view_proj, mat4 transform,
                const DrawParams *params)
{
    static const DrawParams defaults = {0};
    const DrawParams *dp = params ? params : &defaults;
    if (!pose)
        pose = &m->rest;
    mat4 full;
    model_matrix(m, transform, full);

    glUseProgram(prog);
    draw_pass(m, pose, prog, view_proj, full, dp, false);
    if (dp->depth_only)
        return;

    /* transparent parts: blend over what's already drawn, don't hide what's behind them.
     * (not sorted back to front, so overlapping transparent parts may look off) */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    draw_pass(m, pose, prog, view_proj, full, dp, true);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
