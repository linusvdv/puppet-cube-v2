#include "cube.cuh"

void UploadCubeComputationToDevice(
    const std::vector<uint16_t>& corner_orientations,
    const std::vector<uint16_t>& corner_positions,
    const std::vector<uint16_t>& corner_heuristics,

    const std::vector<uint16_t>& edge_orientations,
    const std::vector<uint32_t>& edge_positions,
    const std::vector<uint8_t>& edge_heuristics
    ) {
    DCube::UploadComputationToDevice(corner_orientations, corner_positions, corner_heuristics, edge_orientations, edge_positions, edge_heuristics);
}
