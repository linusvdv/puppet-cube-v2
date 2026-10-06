"""Move timeline widget of the Qt gui.

Displays the moves of one solver run on two lines: the scramble on the
first (dimmed) and the solution on the second (bright), each with a row
label. A boundary b is the cube state after applying moves[0:b]
(scramble + solution); the playhead marks the current boundary on the
line it belongs to and clicking a line jumps to the nearest boundary on
it (right half of a chip = after the move, left half = before it).

Pure widget - no solver, cube or gl dependencies. The integrator places
it into a QScrollArea with widgetResizable(False) - the widget resizes
itself to the full unclipped width whenever the moves change.
"""

import math

from PySide6.QtCore import QPoint, QRect, Qt, Signal
from PySide6.QtGui import QColor, QFont, QPainter, QPen, QPolygon
from PySide6.QtWidgets import QWidget

# chip geometry (class attributes so tests can use the same constants)
LABEL_W = 70  # left gutter with the row labels
CHIP_W = 28
CHIP_H = 22
SPACING = 4
PAD = 8  # padding between the row label and the first chip
TOP = 4  # space above the first row
ROW_SPACING = 8

ROW_SCRAMBLE = 0
ROW_SOLUTION = 1


class MoveTimeline(QWidget):
    """Two line move timeline: scramble (top, dimmed) + solution (bottom)."""

    boundaryRequested = Signal(int, int)  # (boundary, row the click was on)

    # colors
    SCRAMBLE_BG = QColor(0xb8, 0xbc, 0xc4)
    SCRAMBLE_FG = QColor(0x3d, 0x40, 0x46)
    SOLUTION_BG = QColor(0x2c, 0x3a, 0x4d)
    SOLUTION_FG = QColor(0xf0, 0xf2, 0xf5)
    LABEL_FG = QColor(0x88, 0x8d, 0x96)
    PLAYHEAD = QColor(0xe8, 0x83, 0x2d)
    HOVER_BORDER = QColor(0x50, 0x87, 0xd0)
    PENDING_FG = QColor(0x88, 0x8d, 0x96)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.scramble = []
        self.solution = None
        self.boundary = 0
        # the line the playhead is drawn on: the scrambled boundary lies on
        # both lines (end of the scramble / start of the solution) - the
        # playhead stays on the line it was put on (clicked / last played)
        self.boundary_row = None
        self.hover_chip = (-1, -1)  # (row, chip index within the row)
        self.setMouseTracking(True)

    # --- public api ---------------------------------------------------------

    def set_moves(self, scramble, solution):
        """scramble/solution are lists of move name strings (e.g. ["R", "U'"]).

        solution may be None (search still running): the solution line then
        shows a searching indicator; call again with the solution later.
        """
        self.scramble = list(scramble)
        self.solution = list(solution) if solution is not None else None
        self.boundary = min(self.boundary, self.num_moves)
        self.boundary_row = None
        self.hover_chip = (-1, -1)
        self._apply_size()

    def set_boundary(self, b, row=None):
        """Moves the playhead to boundary b (no signal emitted).

        row selects the line for the ambiguous scrambled boundary (it is
        the end of the scramble line and the start of the solution line);
        without an explicit row the ambiguous boundary keeps the current
        line and everything else is derived from b.
        """
        b = max(0, min(b, self.num_moves))
        if row is None:
            if b == self.scramble_len and self.boundary_row is not None:
                row = self.boundary_row
            else:
                row = ROW_SCRAMBLE if b <= self.scramble_len else ROW_SOLUTION
        self.boundary = b
        self.boundary_row = row
        self.update()

    def clear(self):
        self.scramble = []
        self.solution = None
        self.boundary = 0
        self.boundary_row = None
        self.hover_chip = (-1, -1)
        self._apply_size()

    def playhead_x(self):
        """x position of the playhead (for auto scrolling)."""
        return self.boundary_x(self.boundary, self.boundary_row)

    @property
    def num_moves(self):
        return len(self.scramble) + len(self.solution or [])

    @property
    def scramble_len(self):
        return len(self.scramble)

    def sizeHint(self):
        return self.minimumSizeHint()

    def minimumSizeHint(self):
        longest = max(len(self.scramble), len(self.solution or []))
        width = LABEL_W + PAD + longest * (CHIP_W + SPACING) + SPACING
        height = TOP + 2 * CHIP_H + ROW_SPACING + 4
        return QRect(0, 0, max(width, 200), height).size()

    # --- geometry helpers ------------------------------------------------------

    def row_y(self, row):
        return TOP + row * (CHIP_H + ROW_SPACING)

    def row_split_y(self):
        """y that separates the scramble line from the solution line."""
        return TOP + CHIP_H + ROW_SPACING / 2.0

    def chip_rect(self, row, i):
        return QRect(LABEL_W + PAD + i * (CHIP_W + SPACING),
                     self.row_y(row), CHIP_W, CHIP_H)

    def boundary_x(self, b, row=None):
        """x position of boundary b. b <= scramble_len lies on the scramble
        line, everything after on the solution line; an explicit row moves
        the ambiguous scrambled boundary onto the other line."""
        if row is None:
            row = ROW_SCRAMBLE if b <= len(self.scramble) else ROW_SOLUTION
        i = b if row == ROW_SCRAMBLE else b - len(self.scramble)
        return LABEL_W + PAD + i * (CHIP_W + SPACING) - SPACING / 2.0

    def x_to_boundary(self, x, row):
        """Nearest boundary to a click x on the given row (right half of a
        chip rounds to the boundary after the move, left half to before)."""
        t = (x - LABEL_W - PAD + SPACING / 2.0) / (CHIP_W + SPACING) + 0.5
        b = math.floor(t)
        if row == ROW_SCRAMBLE:
            return max(0, min(b, len(self.scramble)))
        start = len(self.scramble)
        return max(start, min(start + b, self.num_moves))

    # --- events -----------------------------------------------------------------

    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            row = ROW_SCRAMBLE if event.position().y() < self.row_split_y() else ROW_SOLUTION
            self.boundaryRequested.emit(self.x_to_boundary(event.position().x(), row), row)

    def mouseMoveEvent(self, event):
        row = ROW_SCRAMBLE if event.position().y() < self.row_split_y() else ROW_SOLUTION
        names = self.scramble if row == ROW_SCRAMBLE else (self.solution or [])
        chip = math.floor((event.position().x() - LABEL_W - PAD) / (CHIP_W + SPACING))
        inside = (event.position().x() - LABEL_W - PAD) >= 0 and 0 <= chip < len(names)
        chip = chip if inside else -1
        if (row, chip) != self.hover_chip:
            self.hover_chip = (row, chip)
            self.update()

    def leaveEvent(self, event):
        if self.hover_chip != (-1, -1):
            self.hover_chip = (-1, -1)
            self.update()

    # --- painting ----------------------------------------------------------------

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        chip_font = QFont(self.font())
        chip_font.setPointSizeF(8.5)
        chip_font.setBold(True)
        label_font = QFont(self.font())
        label_font.setPointSizeF(8.0)

        rows = [(ROW_SCRAMBLE, self.scramble, "scramble"),
                (ROW_SOLUTION, self.solution if self.solution is not None else [],
                 "solution")]

        for row, names, label in rows:
            # row label
            painter.setFont(label_font)
            painter.setPen(self.LABEL_FG)
            painter.drawText(QRect(4, self.row_y(row), LABEL_W - 8, CHIP_H),
                             Qt.AlignmentFlag.AlignVCenter | Qt.AlignmentFlag.AlignLeft, label)

            painter.setFont(chip_font)
            if row == ROW_SOLUTION and self.solution is None:
                # the solution is still being searched (a wide rect - the
                # text does not fit into a single chip)
                painter.setPen(self.PENDING_FG)
                painter.drawText(QRect(LABEL_W + PAD, self.row_y(row), 140, CHIP_H),
                                 Qt.AlignmentFlag.AlignVCenter | Qt.AlignmentFlag.AlignLeft,
                                 "searching...")
                continue
            for i, name in enumerate(names):
                rect = self.chip_rect(row, i)
                is_scramble = row == ROW_SCRAMBLE
                painter.setPen(Qt.PenStyle.NoPen)
                painter.setBrush(self.SCRAMBLE_BG if is_scramble else self.SOLUTION_BG)
                painter.drawRoundedRect(rect, 4.0, 4.0)
                if (row, i) == self.hover_chip:
                    pen = QPen(self.HOVER_BORDER)
                    pen.setWidth(2)
                    painter.setPen(pen)
                    painter.setBrush(Qt.BrushStyle.NoBrush)
                    painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), 4.0, 4.0)
                painter.setPen(self.SCRAMBLE_FG if is_scramble else self.SOLUTION_FG)
                painter.drawText(rect, Qt.AlignmentFlag.AlignCenter, name)

        # playhead on the line of the current boundary
        row = self.boundary_row
        if row is None:
            row = ROW_SCRAMBLE if self.boundary <= len(self.scramble) else ROW_SOLUTION
        x = int(round(self.boundary_x(self.boundary, row)))
        top = self.row_y(row)
        pen = QPen(self.PLAYHEAD)
        pen.setWidth(2)
        painter.setPen(pen)
        painter.drawLine(QPoint(x, top - 2), QPoint(x, top + CHIP_H + 2))
        painter.setBrush(self.PLAYHEAD)
        painter.drawPolygon(QPolygon(
            [QPoint(x - 4, top - 2), QPoint(x + 4, top - 2), QPoint(x, top + 3)]))

    # --- internals -----------------------------------------------------------------

    def _apply_size(self):
        # the scroll area never resizes the widget (widgetResizable False) -
        # apply the full unclipped size ourselves
        self.resize(self.minimumSizeHint())
        self.updateGeometry()
        self.update()
