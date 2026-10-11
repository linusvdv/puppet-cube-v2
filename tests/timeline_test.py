"""Tests of the two line move timeline widget (headless: offscreen platform).

Run:  QT_QPA_PLATFORM=offscreen python tests/timeline_test.py
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gui"))
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtCore import QPoint, Qt  # noqa: E402
from PySide6.QtTest import QTest  # noqa: E402
from PySide6.QtWidgets import QApplication  # noqa: E402

from timeline import (CHIP_H, CHIP_W, LABEL_W, PAD, ROW_SOLUTION,  # noqa: E402
                      ROW_SCRAMBLE, SPACING, TOP, MoveTimeline)

app = QApplication.instance() or QApplication(sys.argv)

SCRAMBLE = ["R", "U'", "F"]
SOLUTION = ["U", "R'", "R'", "M"]


class Spy:
    def __init__(self):
        self.values = []

    def __call__(self, boundary, row):
        self.values.append((boundary, row))


def click(timeline, x, y):
    QTest.mouseClick(timeline, Qt.MouseButton.LeftButton,
                     Qt.KeyboardModifier.NoModifier, QPoint(int(x), int(y)))


def chip_center_x(i):
    return LABEL_W + PAD + i * (CHIP_W + SPACING) + CHIP_W / 2


def test_geometry():
    timeline = MoveTimeline()
    timeline.set_moves(SCRAMBLE, SOLUTION)
    timeline.resize(timeline.sizeHint())
    expected_width = LABEL_W + PAD + 4 * (CHIP_W + SPACING) + SPACING
    assert timeline.sizeHint().width() >= expected_width, timeline.sizeHint()
    assert timeline.size() == timeline.minimumSizeHint(), "set_moves must resize the widget"
    # boundaries: scramble line holds 0..len(scramble), solution line the rest
    assert timeline.boundary_x(0) == LABEL_W + PAD - SPACING / 2.0
    assert timeline.boundary_x(len(SCRAMBLE)) == LABEL_W + PAD + 3 * (CHIP_W + SPACING) - SPACING / 2.0
    assert timeline.boundary_x(len(SCRAMBLE) + 1) == \
        LABEL_W + PAD + 1 * (CHIP_W + SPACING) - SPACING / 2.0
    assert timeline.boundary_x(timeline.num_moves) == \
        LABEL_W + PAD + 4 * (CHIP_W + SPACING) - SPACING / 2.0
    print("geometry: ok")


def test_click_mapping():
    timeline = MoveTimeline()
    timeline.set_moves(SCRAMBLE, SOLUTION)
    timeline.resize(timeline.sizeHint())
    timeline.show()
    spy = Spy()
    timeline.boundaryRequested.connect(spy)

    y_scramble = TOP + CHIP_H / 2
    y_solution = TOP + CHIP_H + 8 + CHIP_H / 2
    n = timeline.num_moves
    cases = [
        # scramble line: clamped to [0, len(SCRAMBLE)]
        (0, y_scramble, 0, ROW_SCRAMBLE),
        (LABEL_W - 30, y_scramble, 0, ROW_SCRAMBLE),
        (chip_center_x(0), y_scramble, 1, ROW_SCRAMBLE),
        (chip_center_x(1), y_scramble, 2, ROW_SCRAMBLE),
        (chip_center_x(2), y_scramble, 3, ROW_SCRAMBLE),
        (LABEL_W + PAD + 20 * (CHIP_W + SPACING), y_scramble, 3, ROW_SCRAMBLE),
        # solution line: clamped to [len(SCRAMBLE), n]
        (0, y_solution, 3, ROW_SOLUTION),
        (chip_center_x(0), y_solution, 4, ROW_SOLUTION),
        (chip_center_x(3), y_solution, 7, ROW_SOLUTION),
        (LABEL_W + PAD + 20 * (CHIP_W + SPACING), y_solution, 7, ROW_SOLUTION),
    ]
    for x, y, expected, row in cases:
        spy.values.clear()
        click(timeline, x, y)
        assert spy.values == [(expected, row)], (x, y, spy.values, expected, row)
    print("click mapping: ok")


def test_playhead():
    timeline = MoveTimeline()
    timeline.set_moves(SCRAMBLE, SOLUTION)
    timeline.resize(timeline.sizeHint())
    spy = Spy()
    timeline.boundaryRequested.connect(spy)
    for b in range(timeline.num_moves + 1):
        timeline.set_boundary(b)
        assert spy.values == [], "set_boundary must not emit"
        assert math.isclose(timeline.playhead_x(), timeline.boundary_x(b, timeline.boundary_row)), b

    # the scrambled boundary lies on both lines: it defaults to the end of
    # the scramble line, an explicit row moves it, and follow up calls
    # without a row keep the line
    timeline.set_moves(SCRAMBLE, SOLUTION)
    s = len(SCRAMBLE)
    timeline.set_boundary(s)
    assert timeline.boundary_row == ROW_SCRAMBLE
    assert math.isclose(timeline.playhead_x(), timeline.boundary_x(s, ROW_SCRAMBLE))
    timeline.set_boundary(s, ROW_SOLUTION)
    assert timeline.boundary_row == ROW_SOLUTION
    assert math.isclose(timeline.playhead_x(), timeline.boundary_x(s, ROW_SOLUTION))
    timeline.set_boundary(s)
    assert timeline.boundary_row == ROW_SOLUTION, "ambiguous boundary keeps its line"
    timeline.set_boundary(s - 1)
    assert timeline.boundary_row == ROW_SCRAMBLE
    timeline.set_boundary(s)
    assert timeline.boundary_row == ROW_SCRAMBLE
    print("playhead: ok")


def test_pending_solution():
    timeline = MoveTimeline()
    timeline.set_moves(SCRAMBLE, None)
    timeline.resize(timeline.sizeHint())
    timeline.show()
    spy = Spy()
    timeline.boundaryRequested.connect(spy)
    # clicks on the solution line are clamped to the scramble length while
    # the solution is unknown
    click(timeline, LABEL_W + PAD + 10 * (CHIP_W + SPACING), TOP + CHIP_H + 8 + CHIP_H / 2)
    assert spy.values == [(len(SCRAMBLE), ROW_SOLUTION)]
    timeline.set_moves(SCRAMBLE, SOLUTION)
    timeline.resize(timeline.sizeHint())
    spy.values.clear()
    click(timeline, LABEL_W + PAD + 10 * (CHIP_W + SPACING), TOP + CHIP_H + 8 + CHIP_H / 2)
    assert spy.values == [(len(SCRAMBLE) + len(SOLUTION), ROW_SOLUTION)]
    timeline.clear()
    assert timeline.num_moves == 0
    assert timeline.size() == timeline.minimumSizeHint()
    print("pending solution / clear: ok")


def main():
    test_geometry()
    test_click_mapping()
    test_playhead()
    test_pending_solution()
    print("ALL TIMELINE TESTS PASSED")


if __name__ == "__main__":
    main()
