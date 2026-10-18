// Port of the editor logic of gui/editor.py + gui/pieces.py: the painted
// sticker state, validation against the piece inventory, the extraction of
// the solver placements from the painted cube and the initial placement
// data for custom runs.

import { matVec, matTranspose, matMul } from './cube_model.js';

export const NUM_PIECES = 26;
export const CENTERS = 6;
export const GRAY = [0.42, 0.42, 0.45];
export const DARK_RED = [0.55, 0.08, 0.08];

export class InvalidPosition extends Error {
    constructor(message, slots) {
        super(message);
        this.slots = slots;
    }
}

// determinant of the 3x3 matrix of direction rows (unit axis vectors -
// the matrix is a signed permutation, det = +-1)
function det3(dirs) {
    const [a, b, c] = dirs;
    return a[0]*(b[1]*c[2] - b[2]*c[1])
         - a[1]*(b[0]*c[2] - b[2]*c[0])
         + a[2]*(b[0]*c[1] - b[1]*c[0]);
}

export class EditorState {
    constructor(meshData, editorMeshes) {
        this.meshData = meshData;
        this.colors = new Map();       // "piece,group" -> color index
        this.revision = 0;
        this._undoStack = [];
        this._redoStack = [];
        // face direction of every sticker group at every slot of the EDITOR
        // scene (corners use the axis-mapped order - differs from the real
        // piece's sticker faces!)
        this.slotFaces = (editorMeshes && editorMeshes.groupFaces)
            || meshData.stickerFaces;
        // (slot grid key) -> piece idx
        this.pieceOfGrid = new Map(
            meshData.solved.map((p, i) => [p.grid.join(','), i]));
        // the piece occupying every slot: slots hold the piece whose HOME
        // position they are (piece home grid)
        this._homeSlots = meshData.solved.map(p => p.grid.slice());
    }

    stickerColor(piece, group) {
        return this.colors.get(`${piece},${group}`);
    }

    setSticker(piece, group, color) {
        if (piece < CENTERS) return;
        const key = `${piece},${group}`;
        const old = this.colors.get(key);
        if (old === color) return;
        this._undoStack.push([[piece, group, old]]);
        this._redoStack = [];
        this.revision++;
        if (color === null || color === undefined) this.colors.delete(key);
        else this.colors.set(key, color);
    }

    clear() {
        if (this.colors.size === 0) return;
        this._undoStack.push([...this.colors.entries()].map(([k, c] ) => {
            const [p, g] = k.split(',').map(Number);
            return [p, g, c];
        }));
        this._redoStack = [];
        this.revision++;
        this.colors.clear();
    }

    _apply(changes) {
        for (const [piece, group, color] of changes) {
            const key = `${piece},${group}`;
            if (color === null || color === undefined) this.colors.delete(key);
            else this.colors.set(key, color);
        }
        this.revision++;
    }

    undo() {
        if (this._undoStack.length === 0) return null;
        const changes = this._undoStack.pop();
        this._redoStack.push(changes.map(([p, g, c]) => {
            const key = `${p},${g}`;
            return [p, g, this.colors.has(key) ? this.colors.get(key) : null];
        }));
        this._apply(changes);
        return changes.map(([p, g]) => [p, g]);
    }

    redo() {
        if (this._redoStack.length === 0) return null;
        const changes = this._redoStack.pop();
        this._undoStack.push(changes.map(([p, g, c]) => {
            const key = `${p},${g}`;
            return [p, g, this.colors.has(key) ? this.colors.get(key) : null];
        }));
        this._apply(changes);
        return changes.map(([p, g]) => [p, g]);
    }

    remainingPerColor() {
        const remaining = new Array(6).fill(8);
        for (const color of this.colors.values()) {
            if (color !== null && color !== undefined) remaining[color]--;
        }
        return remaining;
    }

    // the placements of the painted cube as the solver binding expects them:
    // per corner/edge slot (slot grid, piece home grid, sticker home face
    // dirs, sticker current face dirs). The editor cube is in the solved
    // frame (no rotation offset), so the extraction is the solved-case of
    // pieces.py's placements_from_visual_cube: the piece at a slot is the
    // piece whose sticker colors match the slot's painted stickers.
    placements() {
        const solved = this.meshData.solved;
        const stickerFaces = this.meshData.stickerFaces;
        const stickerColors = this.meshData.stickerColors;
        const colors = this.meshData.colors;

        // identify the piece at every slot: match its painted sticker colors
        // (per group, direction-agnostic color multiset) against the
        // inventory - the full direction assignment is recovered after
        const slotPieces = new Map();   // slot key -> {piece, dirs per group}
        for (let slot = CENTERS; slot < NUM_PIECES; slot++) {
            const slotKey = solved[slot].grid.join(',');
            const numGroups = stickerColors[slot].length;
            const painted = [];
            let complete = true;
            for (let g = 0; g < numGroups; g++) {
                const c = this.stickerColor(slot, g);
                if (c === undefined) { complete = false; break; }
                painted.push(c);
            }
            if (!complete) {
                throw new InvalidPosition(
                    'not all stickers are painted', [solved[slot].grid]);
            }
            painted.sort();
            // find the piece whose color multiset matches
            let found = -1;
            for (let piece = CENTERS; piece < NUM_PIECES; piece++) {
                const inv = stickerColors[piece].slice().sort();
                if (inv.length !== painted.length) continue;
                if (inv.every((c, i) => c === painted[i])) {
                    if (found >= 0) {
                        throw new InvalidPosition(
                            'the same piece is used more than once',
                            [solved[slot].grid, solved[piece].grid]);
                    }
                    found = piece;
                }
            }
            if (found < 0) {
                throw new InvalidPosition(
                    'these stickers do not form a piece of the cube',
                    [solved[slot].grid]);
            }
            slotPieces.set(slotKey, found);
        }

        // per slot: the sticker directions. The piece at the slot is placed
        // with some rotation; its group k (home face dir) maps to the slot's
        // group j direction when their colors match (painting color c on
        // face F of a slot means the piece at the slot shows its c sticker
        // along F).
        const corners = [];
        const edges = [];
        for (let slot = CENTERS; slot < NUM_PIECES; slot++) {
            const slotGrid = solved[slot].grid;
            const piece = slotPieces.get(slotGrid.join(','));
            const slotDirs = this.slotFaces[slot];       // group -> face dir at the slot (editor scene)
            const pieceDirs = stickerFaces[piece];    // group -> home dir of the piece
            const pieceColors = stickerColors[piece];
            const slotColors = stickerColors[slot].map((_, g) => this.stickerColor(slot, g));
            const assignment = this.matchGroups(pieceColors, slotColors);
            const curDirs = pieceDirs.map((homeDir, pg) => slotDirs[assignment[pg]]);

            if (slot >= 18 && det3(curDirs) * det3(pieceDirs) < 0) {
                // a physical placement is a proper rotation - a mirrored
                // corner coloring is impossible
                throw new InvalidPosition(
                    'the corner at this slot is colored in an impossible (mirrored) way',
                    [slotGrid]);
            }

            const placement = {
                slot: slotGrid.slice(),
                piece: solved[piece].grid.slice(),
                homeDirs: pieceDirs.map(d => d.slice()),
                curDirs: curDirs.map(d => d.slice()),
            };
            (slot >= 18 ? corners : edges).push(placement);
        }
        return {corners, edges};
    }

    // assigns every piece group to a slot group with the same color
    // (each slot group used exactly once - the color multisets match)
    matchGroups(pieceColors, slotColors) {
        const used = new Array(slotColors.length).fill(false);
        const assignment = new Array(pieceColors.length).fill(-1);
        // pieces can have a color twice (e.g. the two-tone edges never do,
        // but corners like (Y, O, B) have distinct colors - a repeated
        // color in a piece is impossible: every piece has 2-3 DIFFERENT
        // colors by construction) - a greedy unique match suffices
        for (let pg = 0; pg < pieceColors.length; pg++) {
            const color = pieceColors[pg];
            let sg = slotColors.findIndex((c, i) => c === color && !used[i]);
            if (sg < 0) throw new InvalidPosition('inconsistent piece colors', []);
            used[sg] = true;
            assignment[pg] = sg;
        }
        return assignment;
    }

    // solver wire format: flat int arrays per piece
    // [slot(3), piece home(3), per sticker (home dir(3), cur dir(3))]
    static toWire(placements) {
        const wire = p => {
            let flat = p.slot.concat(p.piece);
            for (let i = 0; i < p.homeDirs.length; i++) {
                flat = flat.concat(p.homeDirs[i], p.curDirs[i]);
            }
            return flat;
        };
        return {corners: placements.corners.map(wire),
                edges: placements.edges.map(wire)};
    }

    // inverse of toWire (flat arrays -> placement objects)
    static fromWire(wire) {
        const unwire = flat => {
            const numStickers = (flat.length - 6) / 6;
            const placement = {slot: flat.slice(0, 3), piece: flat.slice(3, 6),
                               homeDirs: [], curDirs: []};
            for (let i = 0; i < numStickers; i++) {
                placement.homeDirs.push(flat.slice(6 + i*6, 9 + i*6));
                placement.curDirs.push(flat.slice(9 + i*6, 12 + i*6));
            }
            return placement;
        };
        return {corners: wire.corners.map(unwire),
                edges: wire.edges.map(unwire)};
    }
}

// the visual model setup of a custom position: (piece, grid, rotation) per
// piece - the total rotation (net composed with the solved placement) maps
// the mesh's base sticker directions to the current ones
export function initialPlacementData(placements, meshData) {
    const pieceOfGrid = new Map(meshData.solved.map((p, i) => [p.grid.join(','), i]));
    const out = [];
    for (const placement of [...placements.corners, ...placements.edges]) {
        const piece = pieceOfGrid.get(placement.piece.join(','));
        const placementRot = meshData.solved[piece].rotation;
        // mesh frame directions of the stickers: home = placement @ base
        const base = placement.homeDirs.map(h => matVec(matTranspose(placementRot), h));
        const cur = placement.curDirs.map(c => c.slice());
        let rotation;
        if (base.length === 3) {
            rotation = matMul(transposeColumns(cur), matTranspose(transposeColumns(base)));
        } else {
            const base3 = cross(base[0], base[1]);
            const cur3 = cross(cur[0], cur[1]);
            rotation = matMul(transposeColumns([...cur, cur3]),
                              matTranspose(transposeColumns([...base, base3])));
        }
        out.push([piece, placement.slot.slice(), rotation]);
    }
    return out;
}

function cross(a, b) {
    return [a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0]];
}

// columns(list of vec3) transposed: mat with columns = the vectors
function transposeColumns(vecs) {
    return [[vecs[0][0], vecs[1][0], vecs[2][0]],
            [vecs[0][1], vecs[1][1], vecs[2][1]],
            [vecs[0][2], vecs[1][2], vecs[2][2]]];
}
