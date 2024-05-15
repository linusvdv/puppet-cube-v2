#version 330 core

layout (location = 0) in vec3 position;

out vec4 ourColor;


// 5 bits:
//     piece index
// 3 bits:
//     color index
// 1 bit:
//     effected by rotation of current move
uniform uint piece_data;

// rotation of piece
// 8 corners
// 12 edges
uniform mat4 rotations[26];

// all six colors
uniform vec4 colors[7];

// rotation of current move
uniform mat4 current_rotation_axis;

uniform mat4 view_rotation;


void main() {
    // get rotation data
    // mat4 rotation = rotations[(piece_data >> 0) & (uint)31];
    // mat4 rotation = rotations[0];
    mat4 rotation = rotations[(piece_data >> 0) & uint(31)];

    // get color data
    ourColor = colors[(piece_data >> 5) & uint(7)];

    vec4 rotated_position = rotation * vec4(position, 1.0);

    // effected by rotation of current move
    if (((piece_data >> 8) & uint(1)) == uint(1)) {
        rotated_position = current_rotation_axis * rotated_position;
    }

    // view rotation
    vec4 view_position = view_rotation * rotated_position;

    gl_Position = vec4(view_position.xy, -view_position.z, view_position.w);
}
