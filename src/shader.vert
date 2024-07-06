#version 330 core

layout (location = 0) in vec4 position;
layout (location = 1) in vec4 normal;
layout (location = 2) in vec3 color;

out vec4 myNormal;
out vec3 myColor;

void main() {
    myNormal = normal;
    myColor = vec3(0, 0, 0);
    gl_Position = vec4(position.xy, position.z, position.w);
}
