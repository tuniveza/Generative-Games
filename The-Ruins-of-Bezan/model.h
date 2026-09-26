#ifndef MODEL_H
#define MODEL_H

#include <glad/gl.h>
#include <cglm/cglm.h>
#include <stdbool.h>

/* where something is, which way it's turned, and how big it is. turns are radians,
 * applied yaw (around Y) first, then pitch (around X), then roll (around Z) */
typedef struct {
    vec3 pos;
    float yaw, pitch, roll;
    vec3 scale;             /* 1, 1, 1 = as the model was made */
} Transform;

/* position + yaw, at normal size */
#define TRANSFORM_AT(p, y) ((Transform){ .pos = { (p)[0], (p)[1], (p)[2] }, .yaw = (y), .scale = { 1, 1, 1 } })

/* translate * yaw * pitch * roll * scale */
void transform_matrix(const Transform *t, mat4 out);

typedef enum { ALPHA_OPAQUE, ALPHA_MASK, ALPHA_BLEND } AlphaMode;

/* glTF metallic-roughness material; a texture id of 0 means "not present" */
typedef struct {
    vec4 base_color;
    vec3 emissive;
    float metallic;
    float roughness;
    float normal_scale;
    float occlusion_strength;
    float alpha_cutoff;
    AlphaMode alpha_mode;
    bool double_sided;

    GLuint base_tex;        /* sRGB,   rgba            */
    GLuint mr_tex;          /* linear, g = roughness, b = metallic */
    GLuint normal_tex;      /* linear, tangent space   */
    GLuint occlusion_tex;   /* linear, r               */
    GLuint emissive_tex;    /* sRGB,   rgb             */
} Material;

/* one draw call: a glTF mesh primitive uploaded to the GPU */
typedef struct {
    GLuint vao;
    GLuint vbo[6];          /* positions, normals, uvs, tangents, joints, weights */
    GLuint ebo;             /* 0 when the primitive has no indices */
    GLsizei count;          /* index count, or vertex count if ebo == 0 */
    bool has_normals;
    bool has_tangents;
    bool skinned;           /* has joints + weights */
    vec3 bmin, bmax;        /* bounds of its vertices */
    Material mat;
} Primitive;

typedef struct {
    int first;              /* index into Model.prims */
    int count;
} Mesh;

/* scene graph node; t/r/s is the rest (bind) pose */
typedef struct {
    char name[64];
    int parent;             /* -1 = root */
    int mesh;               /* -1 = none */
    int skin;               /* -1 = not skinned */
    vec3 t;
    versor r;               /* quaternion x, y, z, w */
    vec3 s;
} ModelNode;

/* a skeleton: which nodes are bones, and how each bone relates to the mesh at rest */
typedef struct {
    int joint_count;
    int *joints;            /* node index per joint */
    mat4 *inverse_bind;     /* mesh space -> bone space at rest */
    int offset;             /* where this skin's matrices start in Pose.joint_mats */
} Skin;

typedef enum { PATH_TRANSLATION, PATH_ROTATION, PATH_SCALE } AnimPath;
typedef enum { INTERP_LINEAR, INTERP_STEP, INTERP_CUBIC } AnimInterp;

/* keyframes that drive one property of one node */
typedef struct {
    int node;
    AnimPath path;
    AnimInterp interp;
    int count;              /* keyframes */
    float *times;
    float *values;          /* 3 or 4 floats per key; x3 for cubic (in-tangent, value, out-tangent) */
} AnimChannel;

typedef struct {
    char name[64];
    float duration;         /* seconds */
    int channel_count;
    AnimChannel *channels;
} Animation;

/* per-instance animation state: every node's current local transform, plus the
 * matrices computed from it. many instances can share one Model. */
typedef struct {
    int node_count;
    vec3 *t;
    versor *r;
    vec3 *s;
    mat4 *world;            /* node -> model space */
    mat4 *joint_mats;       /* all skins' bone matrices, uploaded for skinning */
} Pose;

typedef struct {
    Primitive *prims;  int prim_count;
    Mesh      *meshes; int mesh_count;
    ModelNode *nodes;  int node_count;
    int       *order;       /* nodes sorted so parents come before children */
    Skin      *skins;  int skin_count;
    int joint_total;
    Animation *anims;  int anim_count;
    GLuint    *textures; int texture_count;   /* 2 slots per image: linear, sRGB */
    Pose rest;              /* the model as authored, for static drawing */
    mat4 base;              /* fixes the asset itself (model_adjust); every draw applies it */
    vec3 min, max;          /* bounds at rest, after the base fix */
} Model;

typedef struct {
    /* > 0: builds a chain of this many bones along the model's longest axis and
     * skins the mesh to it, so an unrigged model (like the rat) can be animated
     * by setting bone rotations from code. bone 0 is at the axis' minimum end. */
    int autorig_bones;
    /* if set, called on every color texture's pixels as it loads
     * (e.g. to turn skin goblin-green) */
    void (*recolor)(unsigned char *rgba, int w, int h);
} ModelOptions;

bool model_load(Model *m, const char *path);
bool model_load_ex(Model *m, const char *path, const ModelOptions *opts);
void model_free(Model *m);

int model_find_node(const Model *m, const char *name);     /* -1 if missing */
int model_find_anim(const Model *m, const char *name);     /* -1 if missing */

/* stops drawing a node's mesh (e.g. the dagger's scabbard) and refits the bounds */
void model_hide_node(Model *m, const char *name);

/* fixes the asset itself, once after loading, for every copy of it drawn anywhere:
 * its size, which way it faces, where it sits. with ground = true it is then lifted
 * so its lowest point is at y = 0, i.e. it stands on whatever it's placed at.
 * calls add up. the bounds (min, max) follow. */
void model_adjust(Model *m, const Transform *fix, bool ground);

/* the full matrix a draw at `place` uses: place * base. for attaching things to a
 * model's bones */
void model_matrix(const Model *m, mat4 place, mat4 out);

/* matrix that centers the model on the origin and scales it to fit `size` */
void model_fit(const Model *m, float size, mat4 dest);

/* ---------- poses and animation ---------- */

void pose_init(Pose *p, const Model *m);   /* starts at the rest pose */
void pose_free(Pose *p);
void pose_reset(Pose *p, const Model *m);  /* back to the rest pose */

/* samples an animation at `time` (wraps if loop) and blends it into the pose;
 * weight 1 replaces, weight 0.3 moves 30% of the way from the current pose */
void anim_apply(const Model *m, int anim, float time, bool loop, float weight, Pose *p);

/* recomputes world and bone matrices after changing t/r/s */
void pose_update(const Model *m, Pose *p);

/* ---------- drawing (shaders/model.{vert,frag} or shadow.{vert,frag}) ---------- */

typedef struct {
    vec4 tint;              /* rgb added as glow (hit flash), a unused */
    bool depth_only;        /* shadow pass: no materials */
    bool no_material;       /* the shader has its own look (water): skip material uniforms */
    const unsigned char *skip;  /* if set, nodes with skip[node] != 0 aren't drawn (armour pieces) */
} DrawParams;

/* draws every mesh node of the model; `pose` may be NULL for the rest pose */
void model_draw(const Model *m, const Pose *pose, GLuint prog, mat4 view_proj, mat4 transform,
                const DrawParams *params);

/* bone matrix buffer shared by all skinned draws (binding 2) */
void model_init_joint_buffer(void);

/* ---------- shared with meshgen.c ---------- */

/* immutable GPU buffer holding `bytes` of `data` */
GLuint upload_buffer(const void *data, size_t bytes);

/* after nodes and skins are filled in: joint offsets, node order and the rest pose */
void model_prepare(Model *m);

#endif
