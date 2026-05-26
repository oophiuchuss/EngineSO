#version 450
layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 viewProj;
    vec4 cameraPos;
} uCamera;

void main() {
	gl_Position = uCamera.viewProj * vec4(inPosition, 1.0);
}