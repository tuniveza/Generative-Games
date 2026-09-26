#version 450 core

layout(location = 0) in vec3 a_pos;

layout(location = 0) uniform mat4 u_mvp;

// Output the interpolated gradient factor to the fragment shader
layout(location = 0) out float v_gradientFactor;

void main()
{
    gl_Position = u_mvp * vec4(a_pos, 1.0);

    // OPTION A: If your triangle's local Y coordinates span from -0.5 to 0.5:
    v_gradientFactor = a_pos.x + 0.5; 

    // OPTION B: If your triangle's local Y coordinates span from 0.0 to 1.0:
    // v_gradientFactor = a_pos.y;
    
    // OPTION C: If you just want a simple top-to-bottom blend regardless of scale:
    // (Assuming index 2 is the top tip of your flat triangle asset)
    // if (gl_VertexID % 3 == 0) v_gradientFactor = 0.0; // Base left
    // if (gl_VertexID % 3 == 1) v_gradientFactor = 0.0; // Base right
    // if (gl_VertexID % 3 == 2) v_gradientFactor = 1.0; // Top tip
}
