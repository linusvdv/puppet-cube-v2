import numpy as np


def GetRotationFromStr(my_str):
    conversion = {"R": 0 ,
                  "R'": 1 ,
                  "L": 2 ,
                  "L'": 3 ,
                  "U": 4 ,
                  "U'": 5 ,
                  "D": 6 ,
                  "D'": 7 ,
                  "F": 8 ,
                  "F'": 9 ,
                  "B": 10,
                  "B'": 11,
                  "M": 12,
                  "M'": 13,
                  "E": 14,
                  "E'": 15,
                  "S": 16,
                  "S'": 17}
    return conversion[my_str]


def GetRevRotation(rotation):
    if rotation % 2 == 0:
        return rotation + 1
    return rotation - 1


class Cube:
    corner_orientations = []
    corner_positions = []
    corner_heuristics = []

    edge_orientations = []
    edge_positions = []
    edge_heuristics = []

    kNumEdges = 12
    kNumRotations = 18
    kNumCornerPositions = 40320
    kNumEdgePositions = 665280
    kLegalMoveIndex = [8, 9, 8, 9, 10, 11, 10, 11, 12, 13, 12, 13, 0, 0, 0, 0, 0, 0]


    @staticmethod
    def Initatlization():
        Cube.corner_orientations = np.fromfile("../precomputation/corner_orientations.bin", dtype=np.uint16)
        Cube.corner_positions = np.fromfile("../precomputation/corner_positions.bin", dtype=np.uint16)
        Cube.corner_heuristics = np.fromfile("../precomputation/corner_heuristics.bin", dtype=np.uint16)

        Cube.edge_orientations = np.fromfile("../precomputation/edge_orientations.bin", dtype=np.uint16)
        Cube.edge_positions = np.fromfile("../precomputation/edge_positions.bin", dtype=np.uint32)
        Cube.edge_heuristics = np.fromfile("../precomputation/edge_heuristics.bin", dtype=np.uint8)


    def __init__(self):
        self.corner_orientation = 0
        self.corner_position = 0

        self.edge_orientation = 0
        self.edge_position_1 = 0
        self.edge_position_2 = 665279

        self.GetHeuristic()


    def GetHeuristic(self):
        self.heuristic_corner = int(Cube.corner_heuristics[self.corner_orientation*Cube.kNumCornerPositions + self.corner_position] & ((1 << 8) - 1))
        self.heuristic_edge_1 = int(Cube.edge_heuristics[(self.edge_orientation*Cube.kNumEdgePositions) + self.edge_position_1])

        orientation = self.edge_orientation | ((self.edge_orientation.bit_count()%2) << (Cube.kNumEdges-1))
        orientation_r = 0
        for i in range(1, Cube.kNumEdges):
            orientation_r = orientation_r | ((orientation >> i) & 1) << (Cube.kNumEdges-1-i)
        position = self.edge_position_2
        position_r = 0
        temp = Cube.kNumEdgePositions
        for i in range(Cube.kNumEdges-1, 6-1, -1):
            temp = temp // (i+1)
            position_r *= i+1
            position_r += i - ((position // temp) % (i + 1))
        self.heuristic_edge_2 = int(Cube.edge_heuristics[(orientation_r*Cube.kNumEdgePositions) + position_r])


    def Rotate(self, rotation):
        if Cube.kLegalMoveIndex[rotation] != 0 and ((Cube.corner_heuristics[(self.corner_orientation*Cube.kNumCornerPositions) + self.corner_position] >> Cube.kLegalMoveIndex[rotation]) & 1) == 0:
            print("ILLEGAL")
            return False

        self.corner_orientation = int(Cube.corner_orientations[(self.corner_orientation*Cube.kNumRotations) + rotation])
        self.corner_position = int(Cube.corner_positions[(self.corner_position*Cube.kNumRotations) + rotation])

        self.edge_orientation = int(Cube.edge_orientations[(self.edge_orientation*Cube.kNumRotations) + rotation])
        self.edge_position_1 = int(Cube.edge_positions[(self.edge_position_1*Cube.kNumRotations) + rotation])
        self.edge_position_2 = int(Cube.edge_positions[(self.edge_position_2*Cube.kNumRotations) + rotation])

        self.GetHeuristic()


if __name__ == "__main__":
    Cube.Initatlization()
    cube = Cube()
    print(cube.heuristic_corner, cube.heuristic_edge_1, cube.heuristic_edge_2)
    cube.Rotate(0)
    print(cube.heuristic_corner, cube.heuristic_edge_1, cube.heuristic_edge_2)
