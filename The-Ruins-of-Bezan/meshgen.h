#ifndef MESHGEN_H
#define MESHGEN_H

#include "model.h"

/* collects triangles for procedural geometry (ruin walls, torches, hands...).
 * vertices carry position, normal, uv, tangent and one bone index. */
typedef struct {
    float *verts;           /* 12 floats each: pos 3, normal 3, uv 2, tangent 4 */
    GLushort *joints;       /* bone per vertex (only used for skinned models) */
    GLuint *idx;
    int vert_count, vert_cap;
    int idx_count, idx_cap;
    int joint;              /* bone the next vertices are attached to */
} MeshBuilder;

void mb_init(MeshBuilder *mb);
void mb_free(MeshBuilder *mb);

/* box of half-size `half` placed by `xf`; uvs come from world position / `tile`
 * so neighboring boxes line up seamlessly */
void mb_box(MeshBuilder *mb, mat4 xf, vec3 half, float tile);

/* cylinder (or cone) standing on the origin along +Y */
void mb_cylinder(MeshBuilder *mb, mat4 xf, float r_bottom, float r_top, float height,
                 int segments, float tile);

/* triangular prism placed by `xf`: a triangle 2*half[0] wide at the bottom (y = -half[1])
 * rising to a ridge at y = +half[1], run 2*half[2] deep along z. gable ends, roofs */
void mb_wedge(MeshBuilder *mb, mat4 xf, vec3 half, float tile);

/* capsule from the origin along -Z for `length`, rounded ends of radius r */
void mb_capsule(MeshBuilder *mb, mat4 xf, float r, float length, int segments);

/* material from a Poly Haven texture folder (albedo.jpg, normal.jpg, arm.jpg).
 * textures are cached, so asking twice for the same folder is free. */
void material_from_dir(Material *mat, const char *dir);
void material_color(Material *mat, float r, float g, float b, float roughness, float metallic);

/* turns builders into a drawable model: one primitive per builder/material.
 * skinned: the model gets a skeleton made of `bone_count` nodes (parents/rest
 * transforms given) and each vertex follows its builder joint. */
void model_from_builders(Model *m, MeshBuilder *mbs, const Material *mats, int count);
void model_from_builders_skinned(Model *m, MeshBuilder *mbs, const Material *mats, int count,
                                 int bone_count, const int *parents, const mat4 *rest_local);

#endif
