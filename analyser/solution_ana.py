import sys

import cube


if __name__ == "__main__":
    cube.Cube.Initatlization()
    for line in sys.stdin:
        line = line.strip()
        moves = line.split()
        moves.reverse()

        my_cube = cube.Cube()
        i = 0
        for move in moves:
            i += 1
            my_cube.Rotate(cube.GetRevRotation(cube.GetRotationFromStr(move)))
            print(i, sum([my_cube.heuristic_corner, my_cube.heuristic_edge_1, my_cube.heuristic_edge_2]), max(my_cube.heuristic_corner, my_cube.heuristic_edge_1, my_cube.heuristic_edge_2), my_cube.heuristic_corner, my_cube.heuristic_edge_1, my_cube.heuristic_edge_2)

        print("")
