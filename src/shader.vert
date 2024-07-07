#version 330 core

layout (location = 0) in vec4 position;
layout (location = 1) in vec4 aNormal;
layout (location = 2) in vec3 aColor;

out vec3 Color;

void main() {
    vec4 Normal = normalize(aNormal);
    float diffusion = abs(dot(Normal, vec4(0, 0, 1, 0))) -0.5;
    Color = aColor + diffusion;
    gl_Position = vec4(position.xy, -position.z/16, position.w);
}
