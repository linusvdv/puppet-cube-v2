#version 330 core

layout (location = 0) in vec3 aPos;

out vec4 ourColor;

uniform mat4 transform;
uniform mat4 view_rotation;
uniform vec4 aColor;

void main() {
   gl_Position = view_rotation * transform * vec4(aPos, 1.0);
   gl_Position[2] = -gl_Position[2];  // inverse Z buffer
   ourColor = aColor;
}
