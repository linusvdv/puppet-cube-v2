#version 330 core

out vec4 FragColor;

in vec4 myNormal;
in vec3 ourColor;


uniform float transparency = 1.0;


void main() {
    vec4 Normal = normalize(myNormal);
    float diffusion = abs(dot(Normal, vec4(0, 0, 1, 0))) -0.5;
    FragColor = vec4(ourColor.r + diffusion, ourColor.g + diffusion, ourColor.b + diffusion, transparency);
}
