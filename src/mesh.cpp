#include <array>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstddef>
#include <iostream>
#include <map>
#include <ostream>
#include <vector>


#include "mesh.h"
#include <strings.h>


struct PieceMesh {
    std::vector<float> points;
    // vector of triangles for different color
    std::vector<std::vector<int>> triangles;
    std::vector<int> lines;
};
extern const std::array<PieceMesh, kNumPieceTypes> kPieceMeshPreInitialisation;

void PieceMeshInitialisationOneCorner(std::array<PieceMesh, kNumPieceTypes>& piece_meshes);
void PieceMeshInitialisationTwoCorners(std::array<PieceMesh, kNumPieceTypes>& piece_meshes);


struct PieceColorType {
    int type;
    std::vector<Colors> colors;
};
extern const std::array<PieceColorType, kNumPieces> kPieceColorTypes;


// converting piece mesh to cube mesh and 
// taking the average norm of the different triangles if they are in a sufficienly small angle
struct VertexPieceIndex {
    int piece_index;
    std::array<float, 3> position;
    Colors color;
};
struct VertexPieceValue {
    std::vector<int> triangle_index;
    std::vector<int> line_index;
    std::vector<std::array<float, 3>> normals;
};


std::array<float, 3> CrossProduct(const std::array<float, 3>& vec1, const std::array<float, 3>& vec2) {
    return {vec1[1]*vec2[2] - vec1[2]*vec2[1],
            vec1[2]*vec2[0] - vec1[0]*vec2[2],
            vec1[0]*vec2[1] - vec1[1]*vec2[0]};
}


std::array<float, 3> GetVector(const std::array<float, 3>& first, const std::array<float, 3>& second) {
    return {first[0] - second[0], first[1] - second[1], first[2] - second[2]};
}


std::array<float, 3> GetNormal(const std::array<float, 3>& first,
                               const std::array<float, 3>& second,
                               const std::array<float, 3>& third) {
    std::array<float, 3> vec1 = GetVector(first, second);
    std::array<float, 3> vec2 = GetVector(first, third);
    std::array<float, 3> cross = CrossProduct(vec1, vec2);
    return cross;
}


float DotProduct(const std::array<float, 3>& vec1, const std::array<float, 3>& vec2) {
    return vec1[0]*vec2[0] + vec1[1]*vec2[1] + vec1[2]*vec2[2];
}


float GetMagnetude(const std::array<float, 3>& vec) {
    return sqrt(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]);
}


float GetSmallestAngle(const std::array<float, 3>& first,
                       const std::array<float, 3>& second) {
    float angle = std::acos(DotProduct(first, second) / (GetMagnetude(first) * GetMagnetude(second)));
    return std::min({std::abs(angle), std::abs(M_PIf - angle), std::abs(angle - M_PIf)});
}


std::array<float, 3> AddVecMaxMagnetude(const std::array<float, 3>& vec1,
                                        const std::array<float, 3>& vec2) {
    return std::max(std::array<float, 3>{vec1[0] + vec2[0], vec1[1] + vec2[1], vec1[2] + vec2[2]},
                    std::array<float, 3>{vec1[0] - vec2[0], vec1[1] - vec2[1], vec1[2] - vec2[2]},
                    [](const std::array<float, 3>& first,
                       const std::array<float, 3>& second)
                    {return GetMagnetude(first) < GetMagnetude(second);});
}


std::array<float, 3> GetVertexOfPieceMesh(const std::array<PieceMesh, kNumPieceTypes>& piece_meshes, int piece_type, int color, int index) {
    int piece_mesh_index = piece_meshes[piece_type].triangles[color][index];
    assert((size_t)piece_mesh_index*3+2 < piece_meshes[piece_type].points.size());
    return {piece_meshes[piece_type].points[piece_mesh_index*3],
            piece_meshes[piece_type].points[piece_mesh_index*3+1],
            piece_meshes[piece_type].points[piece_mesh_index*3+2]};
}


auto vertex_piece_data_comparision = [](const VertexPieceIndex& first, const VertexPieceIndex& second) {
    return first.piece_index != second.piece_index ? first.piece_index < second.piece_index :
           first.position[0] != second.position[0] ? first.position[0] < second.position[0] :
           first.position[1] != second.position[1] ? first.position[1] < second.position[1] :
           first.position[2] != second.position[2] ? first.position[2] < second.position[2] :
           first.color < second.color;
};


// convertes form piece mesh to vertex piece vertex_piece_data
// calculates face normals
void TransformPieceMeshToVertexPieceData(std::map<VertexPieceIndex, VertexPieceValue,
        decltype(vertex_piece_data_comparision)>& vertex_piece_data, const std::array<PieceMesh, kNumPieceTypes>& piece_meshes,
        int& num_triangles, int& num_lines) {
    int triangle_index = 0;
    int line_index = 0;
    for (int piece_index = 0; piece_index < (int)kNumPieces; piece_index++) {
        int piece_type = kPieceColorTypes[piece_index].type;
        // pieces
        for (size_t j = 0; j < piece_meshes[piece_type].triangles.size(); j++) {
            for (size_t k = 0; k < piece_meshes[piece_type].triangles[j].size(); k+=3) {
                std::array<float, 3> normal = GetNormal(GetVertexOfPieceMesh(piece_meshes, piece_type, j, k),
                                                        GetVertexOfPieceMesh(piece_meshes, piece_type, j, k+1),
                                                        GetVertexOfPieceMesh(piece_meshes, piece_type, j, k+2));

                for (int vertex = 0; vertex < 3; vertex++) {
                    VertexPieceIndex vertex_piece_index = {piece_index,
                        GetVertexOfPieceMesh(piece_meshes, piece_type, j, k+vertex),
                        kPieceColorTypes[piece_index].colors[j]};

                    vertex_piece_data[vertex_piece_index].triangle_index.push_back(triangle_index);
                    vertex_piece_data[vertex_piece_index].normals.push_back(normal);
                }

                triangle_index++;
            }
        }

        // lines
        for (size_t j = 0; j < piece_meshes[piece_type].lines.size(); j++) {
            VertexPieceIndex vertex_piece_index = {piece_index,
                {piece_meshes[piece_type].points[piece_meshes[piece_type].lines[j]*3],
                 piece_meshes[piece_type].points[piece_meshes[piece_type].lines[j]*3+1],
                 piece_meshes[piece_type].points[piece_meshes[piece_type].lines[j]*3+2]},
                Colors::kBlack
            };
            vertex_piece_data[vertex_piece_index].line_index.push_back(line_index);
            vertex_piece_data[vertex_piece_index].normals.push_back({-1, -1, -1});
            if (j%2 == 1) {
                line_index++;
            }
        }
    }
    num_triangles = triangle_index;
    num_lines = line_index;
}


void WeightedNormalAverage(VertexPieceValue& vertex_piece_value, 
        std::vector<std::array<float, 3>>& different_normals, std::vector<int>& different_normal_index, int index) {
    for (size_t j = 0; j < different_normals.size(); j++) {
        if (GetSmallestAngle(different_normals[j], vertex_piece_value.normals[index]) < M_PIf / 8 && false) { // TODO: Get min angle
            different_normals[j] = AddVecMaxMagnetude(different_normals[j], vertex_piece_value.normals[index]);
            different_normal_index.push_back(j);
            return;
        }
    }
    different_normals.push_back(vertex_piece_value.normals[index]);
    different_normal_index.push_back(different_normals.size()-1);
}


// create cube mesh
CubeMesh CubeMeshInitialisation() {
    // create the piece mesh
    // previously vertex_buffer_object
    std::array<PieceMesh, kNumPieceTypes> piece_meshes = kPieceMeshPreInitialisation;
    PieceMeshInitialisationOneCorner(piece_meshes);
    PieceMeshInitialisationTwoCorners(piece_meshes);


    // conversion to vertex piece data
    // index position, color and piece type
    // value triangles, lines and normals of area
    std::map<VertexPieceIndex, VertexPieceValue,
        decltype(vertex_piece_data_comparision)> vertex_piece_data;
    int num_triangles = 0;
    int num_lines = 0;
    TransformPieceMeshToVertexPieceData(vertex_piece_data, piece_meshes, num_triangles, num_lines);


    // initialize all triangle indices to -1
    CubeMesh cube_mesh = {{}, std::vector<std::array<int, 3>>(num_triangles, {-1, -1, -1}), std::vector<std::array<int, 2>>(num_lines, {-1, -1})};
    for (auto it = vertex_piece_data.begin(); it != vertex_piece_data.end(); it++) {
        VertexPieceValue vertex_piece_value = it->second;

        // calculate the waited average of the direction and the surface area
        std::vector<std::array<float, 3>> different_normals;
        std::vector<int> different_normal_index;

        for (size_t i = 0; i < vertex_piece_value.triangle_index.size(); i++) {
            WeightedNormalAverage(vertex_piece_value, different_normals, different_normal_index, i);
        }
        if (vertex_piece_value.triangle_index.empty()) {
            different_normals = {{-1, -1, -1}};
        }

        // Normalize the NormalVector
        for (std::array<float, 3>& normal : different_normals) {
            float magnetude = GetMagnetude(normal);
            normal = {normal[0]/magnetude, normal[1]/magnetude, normal[2]/magnetude};
        }

        // creating CubeMesh
        int index = cube_mesh.vertices.size();

        // adding the vertex to cube mesh
        for (std::array<float, 3>& normal : different_normals) {
            cube_mesh.vertices.push_back(
                    {(unsigned int)it->first.piece_index,
                     {it->first.position[0], it->first.position[1], it->first.position[2], 1.0},
                     {normal[0], normal[1], normal[2], 1.0},
                     {kColors[it->first.color][0], kColors[it->first.color][1], kColors[it->first.color][2]}}
            );
        }
        // triangle
        assert(vertex_piece_value.triangle_index.size() == different_normal_index.size());
        for (size_t i = 0; i < vertex_piece_value.triangle_index.size(); i++) {
            // set the first -1 to the desired index
            for (int j = 0; j < 3; j++) {
                if (cube_mesh.triangles[vertex_piece_value.triangle_index[i]][j] == -1) {
                    cube_mesh.triangles[vertex_piece_value.triangle_index[i]][j] = index + different_normal_index[i];
                    break;
                }
            }
        }
        // lines
        for (size_t i = 0; i < vertex_piece_value.line_index.size(); i++) {
            // set the first -1 to the desired index
            for (int j = 0; j < 2; j++) {
                if (cube_mesh.lines[vertex_piece_value.line_index[i]][j] == -1) {
                    cube_mesh.lines[vertex_piece_value.line_index[i]][j] = index;
                    break;
                }
            }
        }
    }

    return cube_mesh;
}


const std::array<PieceColorType, kNumPieces> kPieceColorTypes = {{
    {0, {Colors::kYellow}},
    {0, {Colors::kOrange}},
    {0, {Colors::kGreen }},
    {0, {Colors::kRed   }},
    {0, {Colors::kBlue  }},
    {0, {Colors::kWhite }},
    {1, {Colors::kYellow, Colors::kOrange}},
    {1, {Colors::kYellow, Colors::kGreen }},
    {1, {Colors::kYellow, Colors::kRed   }},
    {1, {Colors::kYellow, Colors::kBlue  }},
    {1, {Colors::kBlue,   Colors::kOrange}},
    {1, {Colors::kGreen,  Colors::kOrange}},
    {1, {Colors::kGreen,  Colors::kRed   }},
    {1, {Colors::kBlue,   Colors::kRed   }},
    {1, {Colors::kWhite,  Colors::kOrange}},
    {1, {Colors::kWhite,  Colors::kGreen }},
    {1, {Colors::kWhite,  Colors::kRed   }},
    {1, {Colors::kWhite,  Colors::kBlue  }},
    {2, {Colors::kYellow, Colors::kOrange, Colors::kBlue  }},
    {3, {Colors::kYellow, Colors::kGreen,  Colors::kOrange}},
    {3, {Colors::kOrange, Colors::kWhite,  Colors::kBlue  }},
    {3, {Colors::kBlue,   Colors::kRed,    Colors::kYellow}},
    {4, {Colors::kYellow, Colors::kRed,    Colors::kGreen }},
    {4, {Colors::kOrange, Colors::kGreen,  Colors::kWhite }},
    {4, {Colors::kBlue,   Colors::kWhite,  Colors::kRed   }},
    {5, {Colors::kWhite,  Colors::kGreen,  Colors::kRed   }}
}};


const std::array<PieceMesh, kNumPieceTypes> kPieceMeshPreInitialisation = {{
    // centers (default yellow)
    // points
    {{ 0.2,  0.6,  0.2,
       0.2,  0.6, -0.2,
      -0.2,  0.6, -0.2,
      -0.2,  0.6,  0.2,
       0.2,  0.2,  0.2,
       0.2,  0.2, -0.2,
      -0.2,  0.2, -0.2,
      -0.2,  0.2,  0.2},

    // triangles
     {{0, 1, 2,
       0, 2, 3,
       0, 4, 5,
       0, 1, 5,
       1, 5, 6,
       1, 2, 6,
       2, 6, 7,
       2, 3, 7,
       3, 7, 4,
       3, 0, 4}},

    // lines
     {0, 1,
      1, 2,
      2, 3,
      3, 0,
      0, 4,
      1, 5,
      2, 6,
      3, 7,
      4, 5,
      5, 6,
      6, 7,
      7, 4}
    },

    // edges (default yellow/orange)
    // points
    {{ 0.2,  0.6,  0.6,
       0.2,  0.6,  0.2,
      -0.2,  0.6,  0.2,
      -0.2,  0.6,  0.6,
       0.2,  0.2,  0.6,
       0.2,  0.2,  0.2,
      -0.2,  0.2,  0.2,
      -0.2,  0.2,  0.6},

    // triangles
     {{0, 1, 2,
       0, 2, 3,
       0, 5, 1,
       1, 5, 6,
       1, 2, 6,
       2, 6, 3},
      {0, 3, 7,
       0, 7, 4,
       0, 4, 5,
       4, 5, 6,
       4, 6, 7,
       3, 6, 7}},

    // lines
     {0, 1,
      1, 2,
      2, 3,
      3, 0,
      0, 4,
      1, 5,
      2, 6,
      3, 7,
      4, 5,
      5, 6,
      6, 7,
      7, 4}
    },

    // small corners (default white/orange/blue)
    // points
    {{ 0.6,  0.6,  0.6,
       0.6,  0.6,  0.2,
       0.2,  0.6,  0.2,
       0.2,  0.6,  0.6,
       0.6,  0.2,  0.6,
       0.6,  0.2,  0.2,
       0.2,  0.2,  0.2,
       0.2,  0.2,  0.6},

    // triangles
     {{0, 1, 2,
       0, 2, 3,
       1, 2, 6,
       2, 6, 3},
      {0, 3, 7,
       0, 7, 4,
       3, 6, 7,
       4, 6, 7},
      {0, 4, 5,
       0, 5, 1,
       4, 6, 5,
       1, 5, 6}},

    // lines
     {0, 1,
      1, 2,
      2, 3,
      3, 0,
      0, 4,
      1, 5,
      2, 6,
      3, 7,
      4, 5,
      5, 6,
      6, 7,
      7, 4}
    },

    // one direction facing out (default yellow/green/orange)
    // some of this initialisation is done in cube.cpp
    // points
    {{-0.2,  0.6,  0.2,
      -0.2,  0.6,  0.6,
      -1.0,  0.6,  0.6,
      -1.0,  0.6, -0.2,
      -0.2,  0.2,  0.2, // 6 - 4
      -0.2,  0.2,  0.6, // 7 - 5
      -1.0, -0.2,  0.6, //10 - 6
      -1.0, -0.2, -0.2, //11 - 7
      -0.6,  0.2,  0.2, //12 - 8
      -0.6,  0.6, -0.2, // 4 -  9
      -0.6,  0.6,  0.2, // 5 - 10
      -0.6, -0.2,  0.6, // 9 - 11
      -0.6,  0.2,  0.6, // 8 - 12
     },

    // triangles
     {{0, 1, 2,
       2, 3, 9,
       9, 10, 2,
       10, 2, 0,
       0, 1, 4,
       0, 10, 8,
       0, 8, 4},
      {2, 3, 6,
       6, 7, 3},
      {4, 5, 1,
       5, 1, 2,
       5, 12, 2,
       2, 12, 11,
       2, 11, 6,
       4, 5, 12,
       4, 12, 8}},

    // lines
     {0, 1,
      1, 2,
      2, 3,
      3, 9,
      9, 10,
      10, 0,
      0, 4,
      4, 5,
      5, 1,
      5, 12,
      12, 11,
      11, 6,
      6, 7,
      2, 6,
      3, 7
     }
    },

    // corner two directions facing out (default yellow/red/green)
    // some of this initialisation is done in cube.cpp
    // points
    {{-0.2,  0.6, -0.2,
      -0.2,  0.6, -1.0,
      -1.0,  0.6, -1.0,
      -1.0,  0.6, -0.2,
      -0.2, -0.2, -1.0,
      -1.0, -0.2, -1.0,
      -1.0, -0.2, -0.2,
      -0.2,  0.2, -0.2,
      -0.2,  0.2, -0.6,
      -0.2,  0.2, -1.0,
      -0.6,  0.2, -0.2,
      -1.0,  0.2, -0.2},

    // triangles
     {{0, 1, 2,
       0, 2, 3,
       0, 7, 8,
       0, 8, 1,
       0, 7, 10,
       0, 10, 3},
      {1, 4, 5,
       1, 5, 2,
       8, 9, 1,
       4, 9, 14,
       9, 14, 15,
       4, 5, 14},
      {2, 5, 6,
       2, 6, 3,
       10, 11, 3,
       6, 11, 12,
       11, 12, 13,
       5, 6, 12}},

    // lines
     {0, 1,
      1, 2,
      2, 3,
      3, 0,
      1, 4,
      2, 5,
      3, 6,
      4, 5,
      5, 6,
      0, 7,
      7, 13,
      7, 15,
      12, 13,
      14, 15,
      12, 6,
      14, 4}
    },

    // big corners (default white/orange/blue)
    // points
    {{ 1.0,  1.0,  1.0,
       1.0,  1.0,  0.2,
       0.2,  1.0,  0.2,
       0.2,  1.0,  1.0,
       1.0,  0.2,  1.0,
       1.0,  0.2,  0.2,
       0.2,  0.2,  0.2,
       0.2,  0.2,  1.0},

    // triangles
     {{0, 1, 2,
       0, 2, 3,
       1, 2, 6,
       2, 6, 3},
      {0, 3, 7,
       0, 7, 4,
       3, 6, 7,
       4, 6, 7},
      {0, 4, 5,
       0, 5, 1,
       4, 6, 5,
       1, 5, 6}},

    // lines
     {0, 1,
      1, 2,
      2, 3,
      3, 0,
      0, 4,
      1, 5,
      2, 6,
      3, 7,
      4, 5,
      5, 6,
      6, 7,
      7, 4}
    },
}};


void PieceMeshInitialisationOneCorner(std::array<PieceMesh, kNumPieceTypes>& piece_meshes) {
    // corner one direction facing out
    int current_triangle_size = piece_meshes[3].points.size() / 3;
    const int num_edges_radius_lower = 8; // from area to 45 degree yellow
    const int num_edges_radius_middle= 8; // from area to 45 degree green
    const float begin_angle = atan(1 / 1);
    const float middle_angle = acos((1+2*sqrt(2)) / (sqrt(2)*3)); // 45 degree from corner
    const float end_angle = asin(1 / (sqrt(2)*3)); // edge
    const float length = 0.2*sqrt(2)*3;

    for (int i = 0; i < num_edges_radius_lower; i++) {
        float angle = begin_angle + (middle_angle - begin_angle) / (num_edges_radius_lower) * (i+1);

        // points
        piece_meshes[3].points.insert(piece_meshes[3].points.end(),
                {-float(length * cos(angle)), float(length * sin(angle)), -0.2, // outer yellow
                 -float(length * cos(angle)), float(length * sin(angle)),  0.2, // inner
                 -float(length * cos(angle)), -0.2, float(length * sin(angle)), // outer green
                 -float(length * cos(angle)),  0.2, float(length * sin(angle))}); // inner

        // triangles
        int last_batch = current_triangle_size + i*4;
        piece_meshes[3].triangles[0].insert(piece_meshes[3].triangles[0].end(), // yellow
                {last_batch + 0, last_batch - 4, 3, // outer
                 last_batch + 0, last_batch + 1, last_batch - 4, // smooth
                 last_batch + 1, last_batch - 4, last_batch - 3, // smooth
                 last_batch + 1, last_batch - 3, 8}); // inner
                                                      //
        piece_meshes[3].triangles[2].insert(piece_meshes[3].triangles[2].end(), // orange
                {last_batch + 2, last_batch - 2, 6, // outer
                 last_batch + 2, last_batch + 3, last_batch - 2, // smooth
                 last_batch + 3, last_batch - 2, last_batch - 1, // smooth
                 last_batch + 3, last_batch - 1, 8}); // inner

        // lines
        piece_meshes[3].lines.insert(piece_meshes[3].lines.end(),
                {last_batch + 0, last_batch - 4,
                 last_batch + 2, last_batch - 2});
    }

    current_triangle_size = piece_meshes[3].points.size() / 3;
    for (int i = 0; i < num_edges_radius_middle; i++) {
        float angle = middle_angle + (end_angle - middle_angle) / (num_edges_radius_middle) * (i+1);

        // points
        piece_meshes[3].points.insert(piece_meshes[3].points.end(),
                {-float(length * cos(angle)), float(length * sin(angle)), -0.2, // outer yellow
                 -float(length * cos(angle)), float(length * sin(angle)),  0.2, // inner
                 -float(length * cos(angle)), -0.2, float(length * sin(angle)), // outer green
                 -float(length * cos(angle)),  0.2, float(length * sin(angle))}); // inner

        // triangles
        int last_batch = current_triangle_size + i*4;
        piece_meshes[3].triangles[1].insert(piece_meshes[3].triangles[1].end(), // yellow side
                {last_batch + 0, last_batch - 4, 3, // outer
                 last_batch + 0, last_batch + 1, last_batch - 4, // smooth
                 last_batch + 1, last_batch - 4, last_batch - 3, // smooth
                 last_batch + 1, last_batch - 3, 8, // inner
                 // orange side
                 last_batch + 2, last_batch - 2, 6, // outer
                 last_batch + 2, last_batch + 3, last_batch - 2, // smooth
                 last_batch + 3, last_batch - 2, last_batch - 1, // smooth
                 last_batch + 3, last_batch - 1, 8}); // inner

        // lines
        piece_meshes[3].lines.insert(piece_meshes[3].lines.end(),
                {last_batch + 0, last_batch - 4,
                 last_batch + 2, last_batch - 2});
    }
    current_triangle_size = piece_meshes[3].points.size() / 3;
    piece_meshes[3].triangles[1].insert(piece_meshes[3].triangles[1].end(),
            {current_triangle_size - 4, 3, 7,
             current_triangle_size - 2, 6, 7});

    current_triangle_size = piece_meshes[3].points.size() / 3;
    const int num_square = 8;
    for (int i = 0; i <= num_square; i++) { // both sides inclusive
        float angle_i = end_angle - (2 * end_angle) / num_square * i;
        for (int j = 0; j <= num_square; j++) {
            float angle_j = end_angle - (2 * end_angle) / num_square * j;
            piece_meshes[3].points.insert(piece_meshes[3].points.end(),
                    {-float(length * std::max(cos(angle_i), cos(angle_j))), float(length * sin(angle_j)), float(length * sin(angle_i))});

            // area
            int last_batch = piece_meshes[3].points.size() / 3 - 1;
            if (j != 0 && i != 0) {
                piece_meshes[3].triangles[1].insert(piece_meshes[3].triangles[1].end(),
                        {last_batch, last_batch - 1, last_batch - num_square-1,
                         last_batch - 1, last_batch - num_square-1, last_batch - num_square-2});
            }
            
            // outer
            if (j == num_square && i != 0) {
                piece_meshes[3].triangles[1].insert(piece_meshes[3].triangles[1].end(),
                        {last_batch, last_batch - num_square-1, 7});
                piece_meshes[3].lines.insert(piece_meshes[3].lines.end(),
                        {last_batch, last_batch - num_square-1});
            }
            if (i == num_square && j != 0) { 
                piece_meshes[3].triangles[1].insert(piece_meshes[3].triangles[1].end(),
                        {last_batch, last_batch - 1, 7});
                piece_meshes[3].lines.insert(piece_meshes[3].lines.end(),
                        {last_batch, last_batch - 1});
            }

            // inner
            if (j == 0 && i > 1) {
                piece_meshes[3].triangles[1].insert(piece_meshes[3].triangles[1].end(),
                        {last_batch, last_batch - num_square-1, current_triangle_size});
            }
            if (i == 0 && j > 1) { 
                piece_meshes[3].triangles[1].insert(piece_meshes[3].triangles[1].end(),
                        {last_batch, last_batch - 1, current_triangle_size});
            }
        }
    }

    int last_batch = piece_meshes[3].points.size() / 3 - 1;
    piece_meshes[3].lines.insert(piece_meshes[3].lines.end(),
            {last_batch, 7});
}


void PieceMeshInitialisationTwoCorners(std::array<PieceMesh, kNumPieceTypes>& piece_meshes) {
    // corner two directions facing out
    int current_triangle_size = piece_meshes[4].points.size() / 3;
    const int num_edges_radius = 20; // multible of two
    const float begin_angle = asin(1 / (sqrt(2) * 3));
    const float end_angle = asin(1 / sqrt(2));
    const float length = -0.2*sqrt(2)*3;

    for (int i = 0; i < num_edges_radius/2; i++) {
        float angle = begin_angle + (end_angle - begin_angle) / (num_edges_radius/2) * i;

        // left bottom
        piece_meshes[4].points.insert(piece_meshes[4].points.end(),
                {float(length * cos(angle)), -0.2, float(length * sin(angle)),   // left bottom
                 float(length * cos(angle)),  0.2, float(length * sin(angle)),   // left top
                 float(length * sin(angle)), -0.2, float(length * cos(angle)),   // right bottom
                 float(length * sin(angle)),  0.2, float(length * cos(angle))}); // right top

        if (i == 0) {
            continue;
        }

        int last_batch = current_triangle_size + i*4;
        piece_meshes[4].triangles[1].insert(piece_meshes[4].triangles[1].end(),
                {last_batch + 2, last_batch + 3, last_batch - 1,
                 last_batch + 2, last_batch - 2, last_batch - 1,
                 last_batch + 3, last_batch - 1, 7,
                 last_batch + 2, last_batch - 2, 5});

        piece_meshes[4].triangles[2].insert(piece_meshes[4].triangles[2].end(),
                {last_batch + 0, last_batch + 1, last_batch - 3,
                 last_batch + 0, last_batch - 4, last_batch - 3,
                 last_batch + 1, last_batch - 3, 7,
                 last_batch + 0, last_batch - 4, 5});

        piece_meshes[4].lines.insert(piece_meshes[4].lines.end(),
                {last_batch - 4, last_batch + 0, 
                 last_batch - 2, last_batch + 2,});
    }

    // middle
    piece_meshes[4].points.insert(piece_meshes[4].points.end(),
            {float(length * cos(end_angle)), -0.2, float(length * sin(end_angle)),
             float(length * cos(end_angle)), 0.2, float(length * sin(end_angle))});

    int last_batch = current_triangle_size + num_edges_radius/2*4;
    piece_meshes[4].triangles[1].insert(piece_meshes[4].triangles[1].end(),
            {last_batch + 0, last_batch + 1, last_batch - 1,
             last_batch + 0, last_batch - 2, last_batch - 1,
             last_batch + 1, last_batch - 1, 7,
             last_batch + 0, last_batch - 2, 5});

    piece_meshes[4].triangles[2].insert(piece_meshes[4].triangles[2].end(),
            {last_batch + 0, last_batch + 1, last_batch - 3,
             last_batch + 0, last_batch - 4, last_batch - 3,
             last_batch + 1, last_batch - 3, 7,
             last_batch + 0, last_batch - 4, 5});

    piece_meshes[4].lines.insert(piece_meshes[4].lines.end(),
            {last_batch - 4, last_batch + 0, 
             last_batch - 2, last_batch + 0,});
}
