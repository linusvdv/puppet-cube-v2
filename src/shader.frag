#version 330 core

out vec4 FragColor;

in vec3 Color;


uniform float transparency = 0.5;


void main() {
    FragColor = vec4(Color.rgb, transparency);
}
