#version 450 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec4 a_tangent;     /* xyz = direction, w = bitangent sign */
layout(location = 4) in uvec4 a_joints;
layout(location = 5) in vec4 a_weights;

layout(location = 0)  uniform mat4 u_mvp;
layout(location = 1)  uniform mat4 u_model;
layout(location = 10) uniform bool u_skinned;
layout(location = 12) uniform mat3 u_normal_matrix;   /* inverse-transpose of u_model, from the CPU */

layout(std430, binding = 2) readonly buffer Joints { mat4 u_joints[]; };

out vec3 v_world_pos;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_tangent;

void main()
{
    /* skinning: blend the bone matrices this vertex is attached to */
    mat4 skin = mat4(1.0);
    if (u_skinned)
        skin = a_weights.x * u_joints[a_joints.x] + a_weights.y * u_joints[a_joints.y]
             + a_weights.z * u_joints[a_joints.z] + a_weights.w * u_joints[a_joints.w];

    mat4 model = u_model * skin;

    v_world_pos = vec3(model * vec4(a_pos, 1.0));
    /* bones only rotate and scale evenly, so they turn normals as they are; the
     * inverse-transpose handles any uneven scale in the object's own transform */
    v_normal = u_normal_matrix * (mat3(skin) * a_normal);
    v_tangent = vec4(mat3(model) * a_tangent.xyz, a_tangent.w);
    v_uv = a_uv;
    gl_Position = u_mvp * skin * vec4(a_pos, 1.0);
}
