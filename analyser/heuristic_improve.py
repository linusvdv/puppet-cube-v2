import numpy as np
from scipy.optimize import curve_fit
import matplotlib.pyplot as plt
import sys

import cube


# Your model
def model(X, a, b, c, d, e):
    first, second, third = X
    return a * first + b * second + b * third + c * np.minimum.reduce([first, second, third]) + d * np.maximum.reduce([first, second, third]) + e


if __name__ == "__main__":
    cube.Cube.Initatlization()

    first = []
    second = []
    third = []
    y = []

    for line in sys.stdin:
        line = line.strip()
        moves = line.split()
        moves.reverse()

        my_cube = cube.Cube()
        i = 0
        for move in moves:
            i += 1
            my_cube.Rotate(cube.GetRevRotation(cube.GetRotationFromStr(move)))
            # print(i, sum([my_cube.heuristic_corner, my_cube.heuristic_edge_1, my_cube.heuristic_edge_2]), max(my_cube.heuristic_corner, my_cube.heuristic_edge_1, my_cube.heuristic_edge_2), my_cube.heuristic_corner, my_cube.heuristic_edge_1, my_cube.heuristic_edge_2)

            # if (i <= 6):
            #     continue

            first.append(my_cube.heuristic_corner)
            second.append(my_cube.heuristic_edge_1)
            third.append(my_cube.heuristic_edge_2)
            y.append(i)

        # print("")

    first = np.array(first)
    second = np.array(second)
    third = np.array(third)
    y = np.array(y)
    plt.axis('equal')

    # Fit
    params, covariance = curve_fit(
        model,
        (first, second, third),  # pass inputs as a tuple
        y
    )

    a, b, c, d, e = params
    print("a, b, c, d =", a, b, c, d, e)

    y_fit = model((first, second, third), a, b, c, d, e)
    y_sum = first + second + third

    coeffs_fit = np.polyfit(y, y_fit, 1)
    trend_fit = np.polyval(coeffs_fit, y)
    rmse_fit = np.sqrt(np.mean((y_fit - trend_fit)**2))
    print("rmse_fit", rmse_fit)

    coeffs_sum = np.polyfit(y, y_sum, 1)
    trend_sum = np.polyval(coeffs_sum, y)
    rmse_sum = np.sqrt(np.mean((y_sum - trend_sum)**2))
    print("rmse_sum", rmse_sum)

    plt.scatter(y, y_fit)
    plt.scatter(y, y_sum)
    plt.plot(y, trend_fit)
    plt.plot(y, trend_sum)
    plt.plot(y, y)
    plt.show()
