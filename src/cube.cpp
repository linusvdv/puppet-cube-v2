#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <bitset>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ostream>
#include <random>
#include <vector>


#include "cube.h"
#include "error_handler.h"
#include "mesh.h"
#include "settings.h"


const std::array<glm::vec4, kNumPieces> kPieceOffset =
    {glm::vec4( 0.F,  1.F,  0.F,  0.F),
     glm::vec4( 0.F,  1.F,  1.F,  0.F),
     glm::vec4( 1.F,  1.F,  1.F,  0.F),
     glm::vec4(-1.F,  1.F,  1.F,  0.F),
     glm::vec4(-1.F,  1.F, -1.F,  0.F),
     glm::vec4( 1.F,  1.F,  1.F,  0.F),
    };


Cube::Cube(ErrorHandler error_handler, Setting settings) {
    legal_moves_ = std::vector<uint16_t>(kNumOrientations, 0);
    std::string legal_move_path = "legal-move-generation/legal_moves.bin";
    legal_move_path.insert(0, settings.rootPath);
    if (std::FILE* file = std::fopen(legal_move_path.c_str(), "rb")) {
        std::fread(legal_moves_.data(), sizeof(legal_moves_[0]), legal_moves_.size(), file);
        std::fclose(file);
    }
    else {
        error_handler.Handle(ErrorHandler::kCriticalError, "cube.cpp", "legal_moves file not found");
    }
    for (int i = 0; i < 100; i++) {
        std::cout << i << ": " << legal_moves_[i] << std::endl;
    }

    cube_mesh_ = CubeMeshInitialisation();

    // OpenGL objects
    glGenVertexArrays(1, &vertex_array_object_);
    glBindVertexArray(vertex_array_object_);

    glGenBuffers(1, &vertex_buffer_objects_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_objects_);

    glGenBuffers(1, &element_buffer_object_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer_object_);

    // linking vertex attributes
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(2);

    // unbind
    glBindVertexArray(0);
}


Cube::~Cube() {
    // glDeleteVertexArrays(1, &vertex_array_object_);
    // glDeleteBuffers(1, &vertex_buffer_objects_);
    // glDeleteBuffers(1, &element_buffer_object_);
}


void Cube::Draw(Setting settings) const {
    CubeMesh rotated_cube_mesh = cube_mesh_;

    // translate the pieces
    for (Vertex& vertex : rotated_cube_mesh.vertices) {
        vertex.position += kPieceOffset[pieces_[vertex.piece_index].type] * settings.pieceOffset;
    }

    // current rotation
    glm::mat4 current_rotation = glm::mat4(1.0F);
    if (started_current_rotation_) {
        current_rotation = glm::rotate(current_rotation, rotation_angle_, current_rotation_vector_);
    }

    // view
    glm::mat4 view_rotation = glm::mat4(1.0F);
    view_rotation = glm::rotate(view_rotation, glm::radians(settings.rotation.second), glm::vec3(1.0, 0.0, 0.0)); // second
    view_rotation = glm::rotate(view_rotation, glm::radians(settings.rotation.first), glm::vec3(0.0, 1.0, 0.0)); // first
    glm::mat4 scale = glm::scale(glm::mat4(1.0F), glm::vec3(settings.scroll/2));

    // calculate rotation
    for (Vertex& vertex : rotated_cube_mesh.vertices) {
        // piece rotation
        vertex.position = pieces_[vertex.piece_index].rotation * vertex.position;
        vertex.normal   = pieces_[vertex.piece_index].rotation * vertex.normal;

        // current rotation
        if (pieces_[vertex.piece_index].current_rotation) {
            vertex.position = current_rotation * vertex.position;
            vertex.normal   = current_rotation * vertex.normal;
        }

        // view rotation
        vertex.position = view_rotation * vertex.position;
        vertex.position = scale * vertex.position;
        vertex.normal   = view_rotation * vertex.normal;
    }

    rotated_cube_mesh.triangles = {};
    for (std::array<int, 3> triangle : cube_mesh_.triangles) {
        if (rotated_cube_mesh.vertices[triangle[0]].piece_index == 20 || true) {
            rotated_cube_mesh.triangles.push_back(triangle);
        }
    }

    std::sort(rotated_cube_mesh.triangles.begin(), rotated_cube_mesh.triangles.end(),
            [=](std::array<int, 3> first, std::array<int, 3> second){return rotated_cube_mesh.vertices[first[0] ].position[2] +
                                                                            rotated_cube_mesh.vertices[first[1] ].position[2] +
                                                                            rotated_cube_mesh.vertices[first[2] ].position[2] <
                                                                            rotated_cube_mesh.vertices[second[0]].position[2] +
                                                                            rotated_cube_mesh.vertices[second[1]].position[2] +
                                                                            rotated_cube_mesh.vertices[second[2]].position[2];});

    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*rotated_cube_mesh.vertices.size(), rotated_cube_mesh.vertices.data(), GL_STATIC_DRAW);
    glBindVertexArray(vertex_array_object_);

    // black outline
    glEnable(GL_LINE_SMOOTH);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, rotated_cube_mesh.lines.size() * sizeof(std::array<int, 2>), rotated_cube_mesh.lines.data(), GL_STATIC_DRAW);
    glDrawElements(GL_LINES, 2 * rotated_cube_mesh.lines.size(), GL_UNSIGNED_INT, 0);
    glDisable(GL_LINE_SMOOTH);

    // triangles
    glEnable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, rotated_cube_mesh.triangles.size() * sizeof(std::array<int, 3>), rotated_cube_mesh.triangles.data(), GL_STATIC_DRAW);
    glDrawElements(GL_TRIANGLES, 3 * rotated_cube_mesh.triangles.size(), GL_UNSIGNED_INT, 0);
    glDisable(GL_BLEND);

    glBindVertexArray(0);
}


struct PieceRotationMatrix {
    // to check which are effected by current rotation
    int relevant_index;
    int relevant_value;

    std::array<std::array<int, 3>, 3> full_rotation;
    glm::vec3 rotation_axis;
};


const std::array<PieceRotationMatrix, Cube::kNumRotations> kPieceRotationMatrices = {{
    // L
    {0, -1, {{{ 1, 0, 0}, { 0, 0,-1}, { 0,  1, 0}}}, { 1.F, 0.F, 0.F}},
    // L'
    {0, -1, {{{ 1, 0, 0}, { 0, 0, 1}, { 0, -1, 0}}}, {-1.F, 0.F, 0.F}},
    // R
    {0, 1, {{{ 1, 0, 0}, { 0, 0, 1}, { 0, -1, 0}}}, {-1.F, 0.F, 0.F}},
    // R'
    {0, 1, {{{ 1, 0, 0}, { 0, 0,-1}, { 0,  1, 0}}}, { 1.F, 0.F, 0.F}},

    // U
    {1, 1, {{{ 0, 0,-1}, { 0, 1, 0}, { 1,  0, 0}}}, { 0.F,-1.F, 0.F}},
    // U'
    {1, 1, {{{ 0, 0, 1}, { 0, 1, 0}, {-1,  0, 0}}}, { 0.F, 1.F, 0.F}},
    // D
    {1, -1, {{{ 0, 0, 1}, { 0, 1, 0}, {-1,  0, 0}}}, { 0.F, 1.F, 0.F}},
    // D'
    {1, -1, {{{ 0, 0,-1}, { 0, 1, 0}, { 1,  0, 0}}}, { 0.F,-1.F, 0.F}},

    // F
    {2, 1, {{{ 0, 1, 0}, {-1, 0, 0}, { 0,  0, 1}}}, { 0.F, 0.F,-1.F}},
    // F'
    {2, 1, {{{ 0,-1, 0}, { 1, 0, 0}, { 0,  0, 1}}}, { 0.F, 0.F, 1.F}},
    // B
    {2, -1, {{{ 0,-1, 0}, { 1, 0, 0}, { 0,  0, 1}}}, { 0.F, 0.F, 1.F}},
    // B'
    {2, -1, {{{ 0, 1, 0}, {-1, 0, 0}, { 0,  0, 1}}}, { 0.F, 0.F,-1.F}},

    // M
    {0, 0, {{{ 1, 0, 0}, { 0, 0,-1}, { 0,  1, 0}}}, { 1.F, 0.F, 0.F}},
    // M'
    {0, 0, {{{ 1, 0, 0}, { 0, 0, 1}, { 0, -1, 0}}}, {-1.F, 0.F, 0.F}},

    // E
    {1, 0, {{{ 0, 0, 1}, { 0, 1, 0}, {-1,  0, 0}}}, { 0.F, 1.F, 0.F}},
    // E'
    {1, 0, {{{ 0, 0,-1}, { 0, 1, 0}, { 1,  0, 0}}}, { 0.F,-1.F, 0.F}},

    // F
    {2, 0, {{{ 0, 1, 0}, {-1, 0, 0}, { 0,  0, 1}}}, { 0.F, 0.F,-1.F}},
    // F'
    {2, 0, {{{ 0,-1, 0}, { 1, 0, 0}, { 0,  0, 1}}}, { 0.F, 0.F, 1.F}},
}};


std::array<int, 3> IntMat3MultWithVec3(const std::array<std::array<int, 3>, 3>& mat3, const std::array<int, 3>& vec3) {
    return {{mat3[0][0]*vec3[0] + mat3[0][1]*vec3[1] + mat3[0][2]*vec3[2],
             mat3[1][0]*vec3[0] + mat3[1][1]*vec3[1] + mat3[1][2]*vec3[2],
             mat3[2][0]*vec3[0] + mat3[2][1]*vec3[1] + mat3[2][2]*vec3[2]
            }};
}


const unsigned int kEightFac = 40320; // 8!
const int kCornerOffset = 18;
unsigned int Cube::GetPositionHash() const {
    const std::map<std::array<int, 3>, unsigned int> corner_index = {
        {{ 1,  1,  1}, 0},
        {{-1,  1,  1}, 1},
        {{ 1, -1,  1}, 2},
        {{ 1,  1, -1}, 3},
        {{-1,  1, -1}, 4},
        {{-1, -1,  1}, 5},
        {{ 1, -1, -1}, 6},
        {{-1, -1, -1}, 7}
    };

    std::array<bool, kNumPieces> used;
    used.fill(false);

    unsigned int orientation_hash = 0;
    unsigned int hash = 0;
    for (int i = 0; i < kNumCorners-1; i++) {
        // index compressed to 8!
        // 0 to 8!-1
        unsigned int corner = corner_index.find(pieces_[i+kCornerOffset].orientation)->second;
        std::cout << corner << " " << pieces_[i+kCornerOffset].orientation[0] << " " 
            << pieces_[i+kCornerOffset].orientation[1] << " " <<
            pieces_[i+kCornerOffset].orientation[2] << std::endl;
        unsigned int small_corner_index = 0;
        for (int j = 0; j < corner; j++) {
            small_corner_index += u_int(!used[j]);
        }
        std::cout << small_corner_index << std::endl;
        used[corner] = true;
        hash *= kNumCorners-i;
        hash += small_corner_index;

        // piece orientation
        int orientation = 0;
        for (; orientation < 3; orientation++) {
            if (corner_orientation_[i][orientation] != 0) {
                break;
            }
        }
        orientation_hash *= 3;
        orientation_hash += orientation;
    }
    std::cout << "hash: " << hash << std::endl;
    std::cout << "hash: " << orientation_hash << std::endl;

    hash += orientation_hash * kEightFac;
    std::cout << "hash: " << hash << std::endl;

    assert(hash < kNumOrientations);
    return hash;
}


void Cube::Rotate(Setting settings) {
    float old_time = last_time_;
    last_time_ = glfwGetTime();

    if (!settings.should_rotate) {
        return;
    }

    if (!started_current_rotation_) {
        if (elapsed_time_since_last_rotation_ < settings.min_elapsed_time_since_last_rotation) {
            elapsed_time_since_last_rotation_ += last_time_ - old_time;
            return;
        }

        uint hash = GetPositionHash();
        if (legal_moves_[hash] == 0) {
            exit(0);
        }
        std::cout << hash << std::endl;
        std::cout << legal_moves_[hash] << std::endl;
        std::cout << std::bitset<6>(legal_moves_[hash]) << std::endl;
        while (nextRotations_.empty()) {
            Rotations random_rotation = Rotations(std::rand()%kNumRotations);
            if (random_rotation <= Rotations::kBc){
                if (random_rotation%4 <= 1) {
                    if ((legal_moves_[hash] & (1 << (random_rotation/2+random_rotation%4))) == 0) {
                        continue;
                    }
                }
                else {
                    if ((legal_moves_[hash] & (1 << (random_rotation/2-3+random_rotation%4))) == 0) {
                        continue;
                    }
                }
            }
            nextRotations_.push(random_rotation);
            std::cout << "random_rotation: " << random_rotation << std::endl;
            return;
        }

        started_current_rotation_ = true;
        current_rotation_ = nextRotations_.front();
        nextRotations_.pop();

        current_rotation_vector_ = kPieceRotationMatrices[current_rotation_].rotation_axis;

        for (Piece& piece : pieces_) {
            // this piece is affected by current rotation
            if (piece.orientation[kPieceRotationMatrices[current_rotation_].relevant_index] == kPieceRotationMatrices[current_rotation_].relevant_value) {
                piece.current_rotation = true;
            }
        }
    }

    rotation_angle_ += (last_time_ - old_time) * settings.rotation_speed;


    if (rotation_angle_ > M_PIf / 2) {
        rotation_angle_ = M_PIf / 2;
        glm::mat4 current_rotation = glm::mat4(1.0F);
        current_rotation = glm::rotate(current_rotation, rotation_angle_, current_rotation_vector_);

        started_current_rotation_ = false;
        rotation_angle_ = 0;
        elapsed_time_since_last_rotation_ = 0;

        for (int i = 0; i < kNumPieces; i++) {
            if (!pieces_[i].current_rotation) {
                continue;
            }
            pieces_[i].current_rotation = false;
            pieces_[i].orientation = IntMat3MultWithVec3(kPieceRotationMatrices[current_rotation_].full_rotation, pieces_[i].orientation);
            if (pieces_[i].type >= 2) {
                corner_orientation_[i-kCornerOffset] = IntMat3MultWithVec3(kPieceRotationMatrices[current_rotation_].full_rotation, corner_orientation_[i-kCornerOffset]);
            }
            pieces_[i].rotation = current_rotation * pieces_[i].rotation;
        }
        return;
    }
}
