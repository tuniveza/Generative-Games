#version 450 core

/* depth-only pass from the sun's point of view (same skinning as model.vert) */

layout(location = 0) in vec3 a_pos;
layout(location = 4) in uvec4 a_joints;
layout(location = 5) in vec4 a_weights;

layout(location = 0)  uniform mat4 u_mvp;
layout(location = 10) uniform bool u_skinned;

layout(std430, binding = 2) readonly buffer Joints { mat4 u_joints[]; };

void main()
{
    mat4 skin = mat4(1.0);
    if (u_skinned)
        skin = a_weights.x * u_joints[a_joints.x] + a_weights.y * u_joints[a_joints.y]
             + a_weights.z * u_joints[a_joints.z] + a_weights.w * u_joints[a_joints.w];
    gl_Position = u_mvp * skin * vec4(a_pos, 1.0);
}
