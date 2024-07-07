#version 330 core

layout (location = 0) in vec4 position;
layout (location = 1) in vec4 aNormal;
layout (location = 2) in vec3 aColor;

out vec4 myNormal;
out vec3 ourColor;

void main() {
    myNormal = aNormal;
    gl_Position = vec4(position.xy, -position.z/16, position.w);
    ourColor = aColor;
}
