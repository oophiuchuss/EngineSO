#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 viewProj;
    vec4 cameraPos;
} uCamera;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outWorldPos;

void main()
{
    gl_Position = uCamera.viewProj * vec4(inPosition, 1.0);
    outUV       = inUV;
    outNormal   = inNormal;
    outWorldPos = inPosition;
}