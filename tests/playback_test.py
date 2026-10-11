"""Tests of the random-access playback controller (no Qt, no gl, no solver).

Run:  python tests/playback_test.py
"""

import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gui"))

from cube_model import Move, VisualCube  # noqa: E402
from playback import PlaybackController  # noqa: E402
from verify_model import REFERENCE_MOVES  # noqa: E402


def make_move(entry_idx):
    """Move object from the matura reference table (no solver module needed)."""
    name, _axis, rel_val, matrix, _visual_axis, _flipped = REFERENCE_MOVES[entry_idx]
    return Move(name, entry_idx, matrix, rel_val)


# mixed face and slice moves (R, R', U, M, S, L')
TEST_MOVE_ENTRIES = [0, 1, 4, 12, 16, 3]


def make_moves(entries):
    return [make_move(e) for e in entries]


def tick_until_idle(controller, limit=100.0, dt=0.01):
    elapsed = 0.0
    while elapsed < limit:
        changed = controller.tick(dt)
        if not changed and not controller.is_animating and controller._wait <= 0:
            break
        elapsed += dt
    assert elapsed < limit, "playback did not settle"
    return elapsed


def rotations_equal(cube_a, cube_b):
    return (len(cube_a.rotations) == len(cube_b.rotations)
            and all(np.array_equal(a, b)
                    for a, b in zip(cube_a.rotations, cube_b.rotations))
            and np.array_equal(cube_a.rotation_offset, cube_b.rotation_offset))


def test_set_boundary_equivalence():
    moves = make_moves(TEST_MOVE_ENTRIES)
    cube = VisualCube()
    controller = PlaybackController(cube)
    controller.set_run(0, moves[:4], moves[4:])
    for boundary in range(controller.num_moves + 1):
        reference = VisualCube()
        for move in moves[:boundary]:
            reference.apply_move(move)
        controller.set_boundary(boundary)
        assert controller.boundary == boundary
        assert rotations_equal(cube, reference), f"boundary {boundary} differs"
    print("set_boundary equivalence: ok")


def test_replay_invariant():
    # the solution undoes the scramble (inverse sequence reversed)
    scramble_entries = [0, 4, 12, 16, 2]
    solution_entries = list(reversed([1, 5, 13, 17, 3]))  # inverses in reverse order
    scramble = make_moves(scramble_entries)
    solution = make_moves(solution_entries)
    cube = VisualCube()
    controller = PlaybackController(cube)
    controller.set_run(0, scramble, solution)
    controller.set_boundary(controller.num_moves)
    assert cube.is_solved_bookkeeping(), "scramble + solution must solve the cube"
    print("replay invariant: ok")


def test_forward_play():
    moves = make_moves(TEST_MOVE_ENTRIES)
    cube = VisualCube()
    controller = PlaybackController(cube)
    controller.set_run(0, moves[:4], moves[4:])
    controller.play()
    tick_until_idle(controller)
    assert controller.boundary == controller.num_moves
    assert controller.state == "idle"
    reference = VisualCube()
    controller_ref = PlaybackController(reference)
    controller_ref.set_run(0, moves[:4], moves[4:])
    controller_ref.set_boundary(controller_ref.num_moves)
    assert rotations_equal(cube, reference)
    print("forward play: ok")


def test_backward_step():
    moves = make_moves(TEST_MOVE_ENTRIES)
    cube = VisualCube()
    controller = PlaybackController(cube)
    controller.set_run(0, moves[:4], moves[4:])
    boundary = controller.num_moves
    controller.set_boundary(boundary)
    while boundary > 0:
        controller.step_backward()
        assert controller.is_animating
        tick_until_idle(controller)
        assert controller.boundary == boundary - 1
        assert controller.state == "idle"
        reference = VisualCube()
        for move in moves[:boundary - 1]:
            reference.apply_move(move)
        assert rotations_equal(cube, reference), f"after backward step to {boundary - 1}"
        boundary -= 1
    print("backward step: ok")


def test_forward_step():
    moves = make_moves(TEST_MOVE_ENTRIES)
    cube = VisualCube()
    controller = PlaybackController(cube)
    controller.set_run(0, moves[:4], moves[4:])
    for expected in range(1, controller.num_moves + 1):
        controller.step_forward()
        tick_until_idle(controller)
        assert controller.boundary == expected
    reference = VisualCube()
    for move in moves:
        reference.apply_move(move)
    assert rotations_equal(cube, reference)
    print("forward step: ok")


def test_pause_resume_mid_move():
    moves = make_moves(TEST_MOVE_ENTRIES)
    cube = VisualCube()
    controller = PlaybackController(cube)
    controller.set_run(0, [], moves)
    controller.play()
    controller.tick(0.01)  # starts the first move
    controller.tick(0.01)
    assert controller.is_animating
    angle = controller.angle
    assert 0.0 < angle < math.pi / 2.0
    controller.pause()
    # frozen: repeated ticks do not advance the angle
    for _ in range(5):
        assert not controller.tick(0.01)
    assert controller.angle == angle, "angle must freeze while paused"
    controller.play()
    tick_until_idle(controller)
    assert controller.boundary == controller.num_moves
    print("pause/resume mid move: ok")


def test_set_boundary_cancels():
    moves = make_moves(TEST_MOVE_ENTRIES)
    cube = VisualCube()
    controller = PlaybackController(cube)
    controller.set_run(0, moves[:4], moves[4:])
    controller.play()
    controller.tick(0.01)
    assert controller.is_animating
    controller.set_boundary(0)
    assert not controller.is_animating
    assert controller.boundary == 0
    reference = VisualCube()
    assert rotations_equal(cube, reference)
    print("set_boundary cancels animation: ok")


def test_solution_arrives_late():
    moves = make_moves(TEST_MOVE_ENTRIES)
    scramble, solution = moves[:4], moves[4:]
    cube = VisualCube()
    controller = PlaybackController(cube)
    controller.set_run(0, scramble, None)
    assert controller.num_moves == 4
    controller.play()
    tick_until_idle(controller)
    assert controller.boundary == 4
    assert controller.state == "idle"
    # parked at the end of the scramble - the arriving solution continues playback
    controller.set_solution(solution)
    assert controller.is_playing
    tick_until_idle(controller)
    assert controller.boundary == controller.num_moves
    reference = VisualCube()
    for move in moves:
        reference.apply_move(move)
    assert rotations_equal(cube, reference)
    # paused playback does not auto continue
    controller.set_run(1, scramble, None)
    controller.play()
    controller.pause()
    tick_until_idle(controller)
    controller.set_solution(solution)
    assert not controller.is_playing
    print("late solution: ok")


def test_steps_at_the_ends():
    moves = make_moves(TEST_MOVE_ENTRIES)
    controller = PlaybackController(VisualCube())
    controller.set_run(0, moves[:4], moves[4:])
    controller.step_backward()  # at boundary 0: no-op
    assert not controller.is_animating and controller.boundary == 0
    controller.set_boundary(controller.num_moves)
    controller.step_forward()  # at the end: no-op
    assert not controller.is_animating and controller.boundary == controller.num_moves
    controller.jump_solved()
    assert controller.boundary == controller.num_moves
    controller.jump_scrambled()
    assert controller.boundary == controller.scramble_len
    print("steps at the ends: ok")


def test_step_during_animation():
    """Clicking step again mid-animation finishes the current move instantly
    and steps the next one (instead of restarting the same move)."""
    moves = make_moves(TEST_MOVE_ENTRIES)
    cube = VisualCube()
    controller = PlaybackController(cube)
    controller.set_run(0, moves[:4], moves[4:])
    controller.set_boundary(0)

    # two quick step clicks: the first move must be committed when the
    # second click arrives, ending at boundary 2 (not 1)
    controller.step_forward()
    controller.tick(0.01)
    controller.tick(0.01)
    assert controller.is_animating
    controller.step_forward()
    assert controller.is_animating, "the next move must be stepping"
    tick_until_idle(controller)
    assert controller.boundary == 2
    reference = VisualCube()
    for move in moves[:2]:
        reference.apply_move(move)
    assert rotations_equal(cube, reference)

    # the same backwards: two clicks walk back two moves
    controller.set_boundary(controller.num_moves)
    controller.step_backward()
    controller.tick(0.01)
    controller.tick(0.01)
    assert controller.is_animating
    controller.step_backward()
    assert controller.is_animating
    tick_until_idle(controller)
    assert controller.boundary == controller.num_moves - 2

    # opposite direction mid-animation: a backward click finishes the
    # running forward move first
    controller.set_boundary(0)
    controller.step_forward()
    controller.tick(0.01)
    controller.step_backward()
    tick_until_idle(controller)
    assert controller.boundary == 0
    print("step during animation: ok")


def test_step_at_end_during_animation():
    """Clicking next while the LAST move animates lets the rotation finish:
    no stale animation (previously IndexError in cube.end_move) and no snap
    to the end position - the second click must not cut it short."""
    moves = make_moves(TEST_MOVE_ENTRIES)
    cube = VisualCube()
    controller = PlaybackController(cube)
    controller.set_run(0, moves[:4], moves[4:])
    controller.set_boundary(controller.num_moves - 1)
    controller.step_forward()
    controller.tick(0.01)
    assert controller.is_animating
    angle = controller.angle
    controller.step_forward()  # second click during the last move
    assert controller.is_animating, "the last rotation must keep running"
    assert controller.angle >= angle, "the rotation must not jump"
    assert controller.state == "stepping"
    tick_until_idle(controller)
    assert controller.boundary == controller.num_moves
    reference = VisualCube()
    for move in moves:
        reference.apply_move(move)
    assert rotations_equal(cube, reference)
    # further clicks are no-ops
    controller.step_forward()
    assert not controller.is_animating and controller.state == "idle"

    # the mirrored case: clicking previous during the reverse animation of
    # the first move also lets it finish
    controller.set_boundary(1)
    controller.step_backward()
    controller.tick(0.01)
    assert controller.is_animating
    controller.step_backward()
    assert controller.is_animating, "the reverse rotation must keep running"
    tick_until_idle(controller)
    assert controller.boundary == 0
    reference = VisualCube()
    assert rotations_equal(cube, reference)
    print("step at end during animation: ok")


def main():
    test_set_boundary_equivalence()
    test_replay_invariant()
    test_forward_play()
    test_backward_step()
    test_forward_step()
    test_pause_resume_mid_move()
    test_set_boundary_cancels()
    test_solution_arrives_late()
    test_steps_at_the_ends()
    test_step_during_animation()
    test_step_at_end_during_animation()
    print("ALL PLAYBACK TESTS PASSED")


if __name__ == "__main__":
    main()
