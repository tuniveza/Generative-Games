#version 450 core

// Receive the smoothly interpolated factor from the vertex shader
layout(location = 0) in float v_gradientFactor;

layout(location = 1) uniform vec3 u_color;
uniform vec2 u_resolution; // Resolution of the canvas

out vec4 f_col;

// Optional: You can pass colors as uniforms or hardcode them
const vec3 colorStart = vec3(1.0, 0.4, 0.0); // Bright Orange
const vec3 colorEnd   = vec3(0.0, 0.6, 1.0); // Bright Blue

void main()
{   
    vec3 red = vec3(1.0, 0.0, 0.0);
    vec3 green = vec3(0.0, 1.0, 0.0);
    vec3 blue = vec3(0.0, 0.0, 1.0);

    // Linearly mix the colors using the per-pixel interpolated factor
    vec3 gradient = mix(red, green, clamp(v_gradientFactor, 0.0, 1.0));

    f_col = vec4(gradient, 1.0); // Set the output color
}
