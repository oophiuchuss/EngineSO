#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

void main()
{
    // Simple normal visualization for now — will be replaced by G-buffer output
    outColor = vec4(inNormal * 0.5 + 0.5, 1.0);
}