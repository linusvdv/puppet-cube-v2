// Port of gui/playback.py: random-access playback over one run's move
// sequence (scramble + solution). A boundary b is the cube state after
// moves[0:b]; any boundary can be jumped to instantly (reset + apply).

import { NUM_PIECES } from './cube_model.js';

export class PlaybackController {
    constructor(cube, opts = {}) {
        this.cube = cube;
        this.scrambleDuration = opts.scrambleDuration ?? 0.1;
        this.solutionDuration = opts.solutionDuration ?? 0.35;
        this.pauseDuration = opts.pause ?? 0.04;
        this.stepDuration = opts.stepDuration ?? 0.15;

        this._runId = null;
        this._scramble = [];
        this._solution = null;
        this._boundary = 0;
        this._state = 'idle';       // idle | playing | stepping
        this._anim = null;          // {moveIdx, direction, angle, duration}
        this._wait = 0.0;
        this._parkedWaiting = false;
        this._initial = null;       // custom runs: (piece, grid, rotation)[]
    }

    // --- run management ----------------------------------------------------

    setRun(runId, scramble, solution = null, initial = null) {
        this._runId = runId;
        this._scramble = scramble.slice();
        this._solution = solution ? solution.slice() : null;
        this._initial = initial;
        this._cancelAnimation();
        this._resetToStart();
        this._boundary = 0;
        this._state = 'idle';
        this._wait = 0.0;
        this._parkedWaiting = false;
    }

    setSolution(solution) {
        if (this._solution !== null) return;
        this._solution = solution.slice();
        if (this._parkedWaiting) {
            this._parkedWaiting = false;
            this._state = 'playing';
        }
    }

    // --- state queries -----------------------------------------------------

    get runId() { return this._runId; }
    get boundary() { return this._boundary; }
    get scrambleLen() { return this._scramble.length; }
    get hasSolution() { return this._solution !== null; }
    get numMoves() { return this._scramble.length + (this._solution || []).length; }
    get moves() { return this._scramble.concat(this._solution || []); }
    get isPlaying() { return this._state === 'playing'; }
    get isAnimating() { return this._anim !== null; }
    get state() { return this._state; }
    get angle() { return this._anim ? this._anim.angle : 0.0; }

    // --- instant navigation --------------------------------------------------

    setBoundary(b) {
        b = Math.max(0, Math.min(b, this.numMoves));
        this._cancelAnimation();
        this._resetToStart();
        for (const move of this.moves.slice(0, b)) {
            this.cube.applyMove(move);
        }
        this._boundary = b;
        this._wait = 0.0;
        this._parkedWaiting = false;
    }

    jumpScrambled() { this.setBoundary(this.scrambleLen); }

    jumpSolved() {
        if (this.hasSolution) this.setBoundary(this.numMoves);
    }

    // --- transport -----------------------------------------------------------

    play() {
        if (this._boundary >= this.numMoves && this._anim === null) {
            if (!this.hasSolution) {
                // nothing to play yet, solution unknown - arm the auto continue
                this._state = 'playing';
                this._parkedWaiting = true;
            }
            return;
        }
        this._state = 'playing';
        this._parkedWaiting = false;
    }

    pause() {
        if (this._state === 'playing') this._state = 'idle';
        this._parkedWaiting = false;
    }

    stepForward() {
        if (this._anim !== null && this._anim.direction > 0
                && this._anim.moveIdx + 1 >= this.numMoves) {
            this._state = 'stepping';
            this._parkedWaiting = false;
            return;
        }
        this._finishAnimation();
        if (this._boundary >= this.numMoves) {
            this._state = 'idle';
            return;
        }
        this._state = 'stepping';
        this._parkedWaiting = false;
        this._wait = 0.0;
        this._startAnimation(this._boundary, +1, this.stepDuration);
    }

    stepBackward() {
        if (this._anim !== null && this._anim.direction < 0 && this._anim.moveIdx <= 0) {
            this._state = 'stepping';
            this._parkedWaiting = false;
            return;
        }
        this._finishAnimation();
        if (this._boundary <= 0) {
            this._state = 'idle';
            return;
        }
        this._state = 'stepping';
        this._parkedWaiting = false;
        const target = this._boundary - 1;
        // bookkeeping to the target boundary first, then visibly turn the
        // last undone move back without applying it again
        this._cancelAnimation();
        this._resetToStart();
        for (const move of this.moves.slice(0, target)) {
            this.cube.applyMove(move);
        }
        this._boundary = target;
        this._startAnimation(target, -1, this.stepDuration);
    }

    // --- per frame -------------------------------------------------------------

    tick(dt) {
        dt = Math.min(dt, 1.0 / 30.0);
        if (this._wait > 0.0) {
            this._wait -= dt;
            return this._state === 'playing';
        }
        if (this._anim === null) {
            if (this._state !== 'playing') return false;
            this._advance();
            return true;
        }
        if (this._state === 'idle') return false;  // frozen mid move

        const anim = this._anim;
        anim.angle += anim.direction * dt / anim.duration * (Math.PI / 2.0);
        if (anim.direction > 0 && anim.angle >= Math.PI / 2.0) {
            this.cube.endMove();
            this._boundary = anim.moveIdx + 1;
            this._anim = null;
            if (this._state === 'playing') this._wait = this.pauseDuration;
            else this._state = 'idle';
        } else if (anim.direction < 0 && anim.angle <= 0.0) {
            this._cancelAnimation();
            this._state = 'idle';
        }
        return true;
    }

    // --- internals --------------------------------------------------------------

    _resetToStart() {
        this.cube.reset();
        if (this._initial) {
            for (const [piece, grid, rotation] of this._initial) {
                this.cube.grids[piece] = grid.slice();
                this.cube.rotations[piece] = rotation.map(r => r.slice());
            }
        }
    }

    _startAnimation(moveIdx, direction, duration) {
        this.cube.beginMove(this.moves[moveIdx]);
        this._anim = {moveIdx, direction,
                      angle: direction < 0 ? Math.PI / 2.0 : 0.0,
                      duration};
    }

    _cancelAnimation() {
        if (this._anim === null) return;
        this.cube.activeMove = null;
        this.cube.activeAxis = null;
        this.cube.current = new Array(NUM_PIECES).fill(false);
        this._anim = null;
    }

    _finishAnimation() {
        if (this._anim === null) return;
        if (this._anim.direction > 0) {
            this.cube.endMove();
            this._boundary = this._anim.moveIdx + 1;
            this._anim = null;
        } else {
            this._cancelAnimation();
        }
    }

    _advance() {
        if (this._boundary < this.numMoves) {
            const duration = this._boundary < this.scrambleLen
                ? this.scrambleDuration : this.solutionDuration;
            this._startAnimation(this._boundary, +1, duration);
        } else if (this.hasSolution) {
            this._state = 'idle';
        } else {
            this._state = 'idle';
            this._parkedWaiting = true;
        }
    }
}
