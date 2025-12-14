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
            print(cube.GetRevRotation(cube.GetRotationFromStr(move)), end=", ")
            my_cube.Rotate(cube.GetRevRotation(cube.GetRotationFromStr(move)))
            # print(my_cube)
            # print(i, "max_heurisitc:", max(my_cube.heuristic_corner, my_cube.heuristic_edge_1, my_cube.heuristic_edge_2) + len(moves)-i)

        print()
