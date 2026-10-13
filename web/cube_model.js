// Port of gui/cube_model.py: the physical move and animation model of the
// visual cube (26 pieces, integer grid bookkeeping, physical slice moves
// with the rotation_offset frame compensation).

export const NUM_PIECES = 26;
export const NUM_MOVES = 18;

// --- small matrix helpers (3x3, plain arrays) ------------------------------

export function matMul(a, b) {
    const out = [[0, 0, 0], [0, 0, 0], [0, 0, 0]];
    for (let i = 0; i < 3; i++) {
        for (let j = 0; j < 3; j++) {
            let s = 0;
            for (let k = 0; k < 3; k++) s += a[i][k] * b[k][j];
            out[i][j] = s;
        }
    }
    return out;
}

export function matVec(a, v) {
    return [a[0][0]*v[0] + a[0][1]*v[1] + a[0][2]*v[2],
            a[1][0]*v[0] + a[1][1]*v[1] + a[1][2]*v[2],
            a[2][0]*v[0] + a[2][1]*v[1] + a[2][2]*v[2]];
}

export function matTranspose(a) {
    return [[a[0][0], a[1][0], a[2][0]], [a[0][1], a[1][1], a[2][1]], [a[0][2], a[1][2], a[2][2]]];
}

export const IDENTITY = [[1, 0, 0], [0, 1, 0], [0, 0, 1]];

// rotation axis (right hand rule) of a 90 degree rotation matrix, / 2
function rodriguesAxis(m) {
    return [(m[2][1] - m[1][2]) / 2, (m[0][2] - m[2][0]) / 2, (m[1][0] - m[0][1]) / 2];
}

// rotation matrix (right hand rule) around a unit axis by angle
export function rotationAbout(axis, angle) {
    const n = Math.hypot(axis[0], axis[1], axis[2]);
    const x = axis[0]/n, y = axis[1]/n, z = axis[2]/n;
    const s = Math.sin(angle), c = Math.cos(angle), t = 1 - c;
    // the cross product matrix of the axis, then I + sin*C + (1-cos)*C^2
    const c0 = [[ 0, -z,  y], [ z,  0, -x], [-y,  x,  0]];
    const c1 = matMul(c0, c0);
    return [[1 + s*c0[0][0] + t*c1[0][0], s*c0[0][1] + t*c1[0][1], s*c0[0][2] + t*c1[0][2]],
            [s*c0[1][0] + t*c1[1][0], 1 + s*c0[1][1] + t*c1[1][1], s*c0[1][2] + t*c1[1][2]],
            [s*c0[2][0] + t*c1[2][0], s*c0[2][1] + t*c1[2][1], 1 + s*c0[2][2] + t*c1[2][2]]];
}

// --- visual moves -----------------------------------------------------------

export class Move {
    constructor(name, index, matrix, activate) {
        this.name = name;
        this.index = index;
        this.matrix = matrix;      // the solver int matrix (3x3)
        this.activate = activate;
        this.axis = [0, 1, 2].find(i => matrix[i][i] !== 0);  // rotation axis index
        this.isSlice = (activate === 0);

        // the visual animation of a face move rotates the face layer following
        // the matrix; a slice move animates the middle slice turning the
        // opposite direction (the natural look)
        const axisVec = rodriguesAxis(matrix);
        this.visualAxis = this.isSlice ? axisVec.map(v => -v) : axisVec;

        // grid coordinate of the animated layer
        this.relevantValue = this.isSlice ? 0 : activate;
    }
}

export function deriveMoves(rotationReps) {
    const moves = new Array(NUM_MOVES).fill(null);
    for (const rep of rotationReps) {
        const matrix = [rep.matrix.slice(0, 3), rep.matrix.slice(3, 6), rep.matrix.slice(6, 9)];
        moves[rep.index] = new Move(rep.name, rep.index, matrix, rep.activate);
    }
    if (moves.some(m => m === null)) throw new Error('incomplete rotation reps');
    return moves;
}

// --- the visual cube ---------------------------------------------------------

export class VisualCube {
    constructor(solved) {
        // solved: the entries of meshes.json's "solved" (type, grid, rotation)
        this.solved = solved;
        this.reset();
    }

    reset() {
        this.grids = this.solved.map(p => p.grid.slice());
        this.rotations = this.solved.map(p => p.rotation.map(r => r.slice()));
        this.current = new Array(NUM_PIECES).fill(false);
        this.rotationOffset = IDENTITY.map(r => r.slice());
        this.activeMove = null;
        this.activeAxis = null;
    }

    beginMove(move) {
        this.activeMove = move;
        this.activeAxis = matVec(this.rotationOffset, move.visualAxis);
        this.current = this.grids.map(g => g[move.axis] === move.relevantValue);
        return {pieces: this.current.map((c, i) => c ? i : -1).filter(i => i >= 0),
                axis: this.activeAxis};
    }

    pose(pieceIdx, angle) {
        if (this.current[pieceIdx]) {
            return matMul(rotationAbout(this.activeAxis, angle), this.rotations[pieceIdx]);
        }
        return this.rotations[pieceIdx];
    }

    endMove() {
        const move = this.activeMove;
        const rotation = rotationAbout(this.activeAxis, Math.PI / 2);
        for (let i = 0; i < NUM_PIECES; i++) {
            if (this.current[i]) {
                this.rotations[i] = matMul(rotation, this.rotations[i]);
                if (!move.isSlice) {
                    this.grids[i] = matVec(move.matrix, this.grids[i]);
                }
            } else if (move.isSlice) {
                this.grids[i] = matVec(move.matrix, this.grids[i]);
            }
        }
        this.current = new Array(NUM_PIECES).fill(false);
        if (move.isSlice) {
            this.rotationOffset = matMul(rotation, this.rotationOffset);
        }
        this.activeMove = null;
        this.activeAxis = null;
    }

    applyMove(move) {
        this.beginMove(move);
        this.endMove();
    }

    isSolvedBookkeeping() {
        return this.grids.every((g, i) => g.every((c, k) => c === this.solved[i].grid[k]));
    }
}
