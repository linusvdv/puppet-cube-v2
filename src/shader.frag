#version 330 core

out vec4 FragColor;

//in vec4 myNormal;
in vec3 ourColor;


uniform float transparency = 1.0;


void main() {
   FragColor = vec4(ourColor.r, ourColor.g, ourColor.b, transparency);
}
