#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 ourColor;

uniform mat4 transform;
uniform mat4 view_rotation;

void main() {
   gl_Position = view_rotation * transform * vec4(aPos, 1.0);
   ourColor = aColor;
}
