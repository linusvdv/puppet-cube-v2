"""Random-access playback controller for one solver run.

The old AnimationController (removed with the pyglet viewer) replayed a
run strictly forwards as a queue of move commands. The Qt gui navigates
freely instead: a boundary b is the cube state after applying
moves[0:b] from solved (moves = scramble + solution), and any boundary
can be jumped to instantly (reset + apply, no animation).

Transport: play/pause (a move animation in progress freezes at its angle
and resumes), quick animated single steps forwards and backwards (the
backward step visibly turns the layer back - the bookkeeping is set to
the target boundary first, then the move is animated from pi/2 down to 0
without being applied again).

While the search is still running the solution is not known yet: playback
then parks at the end of the scramble and continues automatically when
the solution arrives (if it was playing, not paused).

Pure python (no Qt, no gl) - the gui drives this with tick(dt) from a
timer and repaints when tick returns True.
"""

import math


class PlaybackController:
    """Random-access playback over one run's move sequence (scramble + solution)."""

    def __init__(self, cube, scramble_duration=0.1, solution_duration=0.35,
                 pause=0.04, step_duration=0.15):
        self.cube = cube
        self.scramble_duration = scramble_duration
        self.solution_duration = solution_duration
        self.pause_duration = pause
        self.step_duration = step_duration

        self._run_id = None
        self._scramble = []
        self._solution = None
        self._boundary = 0
        self._state = "idle"  # idle | playing | stepping
        self._anim = None  # {"move_idx", "direction", "angle", "duration"}
        self._wait = 0.0  # remaining inter move pause (playing)
        # playback ran out of moves because the solution is not known yet -
        # continue automatically when it arrives
        self._parked_waiting = False

    # --- run management ----------------------------------------------------

    def set_run(self, run_id, scramble, solution=None):
        """Loads a run. Resets to boundary 0 (solved cube), stops playback."""
        self._run_id = run_id
        self._scramble = list(scramble)
        self._solution = list(solution) if solution is not None else None
        self._cancel_animation()
        self.cube.reset()
        self._boundary = 0
        self._state = "idle"
        self._wait = 0.0
        self._parked_waiting = False

    def set_solution(self, solution):
        """Solution arrives while the search is still running."""
        if self._solution is not None:
            return
        self._solution = list(solution)
        if self._parked_waiting:
            self._parked_waiting = False
            self._state = "playing"

    # --- state queries -----------------------------------------------------

    @property
    def run_id(self):
        return self._run_id

    @property
    def boundary(self):
        return self._boundary

    @property
    def scramble_len(self):
        return len(self._scramble)

    @property
    def has_solution(self):
        return self._solution is not None

    @property
    def num_moves(self):
        return len(self._scramble) + len(self._solution or [])

    @property
    def moves(self):
        """The full move sequence (scramble + solution if known)."""
        return self._scramble + (self._solution or [])

    @property
    def is_playing(self):
        return self._state == "playing"

    @property
    def is_animating(self):
        return self._anim is not None

    @property
    def state(self):
        return self._state

    @property
    def angle(self):
        """Current animation angle in radians (0.0 when idle)."""
        return self._anim["angle"] if self._anim else 0.0

    # --- instant navigation --------------------------------------------------

    def set_boundary(self, b):
        """Jumps to boundary b (state after moves[0:b]) without animation."""
        b = max(0, min(b, self.num_moves))
        self._cancel_animation()
        self.cube.reset()
        for move in self.moves[:b]:
            self.cube.apply_move(move)
        self._boundary = b
        self._wait = 0.0
        self._parked_waiting = False

    def jump_scrambled(self):
        """Jumps to the scrambled position (boundary = scramble length)."""
        self.set_boundary(self.scramble_len)

    def jump_solved(self):
        """Jumps to the solved position (no-op while the solution is unknown)."""
        if self.has_solution:
            self.set_boundary(self.num_moves)

    # --- transport -----------------------------------------------------------

    def play(self):
        """Plays forwards (animated). Resumes a frozen move animation."""
        if self._boundary >= self.num_moves and self._anim is None:
            return
        self._state = "playing"
        self._parked_waiting = False

    def pause(self):
        """Pauses. A move animation in progress freezes at its current angle."""
        if self._state == "playing":
            self._state = "idle"
        self._parked_waiting = False

    def step_forward(self):
        """Quick animated single move forwards.

        A move animation in progress is finished instantly first (the move
        is committed), then the next move steps - clicking repeatedly walks
        forward one move per click instead of restarting the same one. A
        running rotation is only cut short when a next move follows: when
        the animated move is the last one it is let to finish instead.
        """
        if (self._anim is not None and self._anim["direction"] > 0
                and self._anim["move_idx"] + 1 >= self.num_moves):
            # the running animation is the last move - nothing follows to
            # step into, let the rotation finish
            self._state = "stepping"
            self._parked_waiting = False
            return
        self._finish_animation()
        if self._boundary >= self.num_moves:
            self._state = "idle"
            return
        self._state = "stepping"
        self._parked_waiting = False
        self._wait = 0.0
        self._start_animation(self._boundary, +1, self.step_duration)

    def step_backward(self):
        """Quick animated single move backwards (reverse animation)."""
        if (self._anim is not None and self._anim["direction"] < 0
                and self._anim["move_idx"] <= 0):
            # the running reverse animation undoes the first move - nothing
            # follows to step into, let the rotation finish
            self._state = "stepping"
            self._parked_waiting = False
            return
        self._finish_animation()
        if self._boundary <= 0:
            self._state = "idle"
            return
        self._state = "stepping"
        self._parked_waiting = False
        target = self._boundary - 1
        # bookkeeping to the target boundary first, then visibly turn the
        # last undone move back without applying it again
        self._cancel_animation()
        self.cube.reset()
        for move in self.moves[:target]:
            self.cube.apply_move(move)
        self._boundary = target
        self._start_animation(target, -1, self.step_duration)

    # --- per frame -------------------------------------------------------------

    def tick(self, dt):
        """Advances playback by dt seconds.

        Returns True when the visual state may have changed (repaint).
        """
        # clamp the time step such that dropped frames never let a turn
        # complete within a single frame
        dt = min(dt, 1.0 / 30.0)

        if self._wait > 0.0:
            self._wait -= dt
            return self._state == "playing"

        if self._anim is None:
            if self._state != "playing":
                return False
            self._advance()
            return True

        if self._state == "idle":
            # paused mid move - the animation is frozen at its angle
            return False

        anim = self._anim
        anim["angle"] += anim["direction"] * dt / anim["duration"] * (math.pi / 2.0)
        if anim["direction"] > 0 and anim["angle"] >= math.pi / 2.0:
            self.cube.end_move()
            self._boundary = anim["move_idx"] + 1
            self._anim = None
            if self._state == "playing":
                self._wait = self.pause_duration
            else:
                self._state = "idle"
        elif anim["direction"] < 0 and anim["angle"] <= 0.0:
            self._cancel_animation()
            self._state = "idle"
        return True

    # --- internals --------------------------------------------------------------

    def _start_animation(self, move_idx, direction, duration):
        self.cube.begin_move(self.moves[move_idx])
        self._anim = {"move_idx": move_idx, "direction": direction,
                      "angle": math.pi / 2.0 if direction < 0 else 0.0,
                      "duration": duration}

    def _cancel_animation(self):
        if self._anim is None:
            return
        # drop the active move without applying it (the caller has already
        # set the bookkeeping to the target state)
        self.cube.active_move = None
        self.cube.active_axis = None
        self.cube.current = [False] * len(self.cube.current)
        self._anim = None

    def _finish_animation(self):
        """Completes a running move animation instantly (no visual turn).

        A forward animation commits its move; a reverse animation is just
        dropped (its bookkeeping is already at the target boundary).
        """
        if self._anim is None:
            return
        if self._anim["direction"] > 0:
            self.cube.end_move()
            self._boundary = self._anim["move_idx"] + 1
            self._anim = None
        else:
            self._cancel_animation()

    def _advance(self):
        """Starts the next move while playing, or parks at the end."""
        if self._boundary < self.num_moves:
            move = self.moves[self._boundary]
            duration = (self.scramble_duration if self._boundary < self.scramble_len
                        else self.solution_duration)
            self._start_animation(self._boundary, +1, duration)
        elif self.has_solution:
            self._state = "idle"
        else:
            # end of the scramble, solution still being searched
            self._state = "idle"
            self._parked_waiting = True
