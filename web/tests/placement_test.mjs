// Placement regression tests (no browser, no solver): the editor state ->
// placements -> initialPlacementData pipeline.
//   node web/placement_test.mjs
import { EditorState, initialPlacementData } from '../editor.js';
import { VisualCube, deriveMoves, matMul } from '../cube_model.js';
import { readFileSync } from 'fs';

const meshData = JSON.parse(readFileSync(new URL('../meshes.json', import.meta.url)));
const editorMeshes = JSON.parse(readFileSync(new URL('../editor_meshes.json', import.meta.url)));
const reps = JSON.parse(readFileSync(new URL('../rotation_reps.json', import.meta.url)));
const moves = deriveMoves(reps);

function faceColors() {
    const out = {};
    for (let p = 0; p < 6; p++) {
        out[meshData.solved[p].grid.map(g => Math.sign(g)).join(',')] = meshData.stickerColors[p][0];
    }
    return out;
}

function paintRun0(state) {
    const wire = JSON.parse(readFileSync(new URL('testdata/run0_placements.json', import.meta.url)));
    const pieceOfGrid = new Map(meshData.solved.map((p, i) => [p.grid.join(','), i]));
    for (const list of [wire.corners, wire.edges]) {
        for (const flat of list) {
            const slot = flat.slice(0, 3), pieceHome = flat.slice(3, 6);
            const piece = pieceOfGrid.get(pieceHome.join(','));
            for (let s = 0; s < (flat.length - 6) / 6; s++) {
                const home = flat.slice(6 + s*6, 9 + s*6);
                const cur = flat.slice(9 + s*6, 12 + s*6);
                const pg = meshData.stickerFaces[piece].findIndex(
                    d => d[0] === home[0] && d[1] === home[1] && d[2] === home[2]);
                const slotIdx = meshData.solved.findIndex(
                    p => p.grid[0] === slot[0] && p.grid[1] === slot[1] && p.grid[2] === slot[2]);
                const sg = editorMeshes.groupFaces[slotIdx].findIndex(
                    d => d[0] === cur[0] && d[1] === cur[1] && d[2] === cur[2]);
                state.setSticker(slotIdx, sg, meshData.stickerColors[piece][pg]);
            }
        }
    }
}

let failures = 0;
function check(name, ok) {
    console.log(ok ? `  ok: ${name}` : `  FAIL: ${name}`);
    if (!ok) failures++;
}

// 1. the solved painting: every sticker gets its face color -> the solved
//    placements and the solved visual setup
{
    const fc = faceColors();
    const state = new EditorState(meshData, editorMeshes);
    for (let slot = 6; slot < 26; slot++) {
        for (const [g, d] of editorMeshes.groupFaces[slot].entries()) {
            state.setSticker(slot, g, fc[d.join(',')]);
        }
    }
    const pl = state.placements();
    const solved = pl.corners.concat(pl.edges).every(p =>
        JSON.stringify(p.slot) === JSON.stringify(p.piece)
        && JSON.stringify(p.homeDirs) === JSON.stringify(p.curDirs));
    check('solved painting -> solved placements', solved);
    const init = initialPlacementData(pl, meshData);
    const visualOk = init.every(([piece, grid, rotation]) => {
        const s = meshData.solved[piece];
        return JSON.stringify(grid) === JSON.stringify(s.grid)
            && rotation.every((row, i) => row.every(
                (v, j) => Math.abs(v - s.rotation[i][j]) < 1e-9));
    });
    check('solved painting -> solved visual setup', visualOk);
}

// 2. the run-0 position: the derived placement must match a VisualCube
//    replay of the scramble (up to the visual frame offset)
{
    const state = new EditorState(meshData, editorMeshes);
    paintRun0(state);
    const init = initialPlacementData(state.placements(), meshData);
    const scrambles = JSON.parse(readFileSync(new URL('testdata/scrambles.json', import.meta.url)));
    const cube = new VisualCube(meshData.solved);
    for (const rot of scrambles[0]) {
        cube.beginMove(moves[rot]);
        cube.endMove();
    }
    const ok = init.every(([piece, grid, rotation]) => {
        if (JSON.stringify(cube.grids[piece]) !== JSON.stringify(grid)) return false;
        // the replay rotations are in the VISUAL frame: visual = offset @ physical
        const expected = matMul(cube.rotationOffset, rotation);
        return expected.every((row, i) => row.every(
            (v, j) => Math.abs(v - cube.rotations[piece][i][j]) < 1e-9));
    });
    check('run-0 painting -> placement matches the scramble replay', ok);
}

// 3. a mirrored corner coloring is rejected with the offending slot
{
    const state = new EditorState(meshData, editorMeshes);
    paintRun0(state);
    const c0 = state.stickerColor(18, 0), c1 = state.stickerColor(18, 1);
    state.setSticker(18, 0, c1);
    state.setSticker(18, 1, c0);
    let threw = null;
    try { state.placements(); } catch (e) { threw = e; }
    check('mirrored corner rejected',
        threw !== null && threw.slots
            && JSON.stringify(threw.slots[0]) === JSON.stringify([1, 1, 1]));
}

console.log(failures === 0 ? 'PLACEMENT TESTS PASSED' : `${failures} FAILURES`);
process.exit(failures === 0 ? 0 : 1);
