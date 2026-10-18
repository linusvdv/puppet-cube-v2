// Steps 2-3 verification: drives the real app (web/index.html) through the
// full flow - generation, scramble demo with playback, the editor (program
//matic painting of the run-0 position + one raycast click), the custom
// solve and its playback. Run from the repo root:
//   node web/app_test.js
// (serves web/ on localhost:8124)

const { chromium } = require('playwright');
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

const PORT = 8124;
const URL = `http://localhost:${PORT}/`;

function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

async function main() {
    const server = spawn('python3', ['-m', 'http.server', String(PORT)],
                         {cwd: path.join(__dirname, '..'), stdio: 'ignore'});
    await sleep(1000);

    const browser = await chromium.launch({
        args: ['--enable-unsafe-swiftshader'],
    });
    const page = await browser.newPage({viewport: {width: 1280, height: 800}});
    page.on('pageerror', err => console.log('  [pageerror]', String(err).slice(0, 400)));
    page.on('console', msg => {
        if (msg.type() === 'error') console.log('  [console.error]', msg.text().slice(0, 200));
    });

    try {
        await page.goto(URL, {waitUntil: 'load'});
        await page.waitForTimeout(2500);
        if (!(await page.evaluate(() => crossOriginIsolated))) {
            throw new Error('NOT cross origin isolated');
        }
        console.log('crossOriginIsolated ok');

        // the app booted (meshes + reps loaded, scenes built)
        await page.waitForFunction(() => window.app && window.app.meshData, null, {timeout: 30000});
        console.log('app booted');

        // --- the showcase: playable before any tables are generated ---
        await page.waitForFunction(() => window.app.runs.size === 1
            && window.app.runs.get(-1) !== undefined, null, {timeout: 30000});
        const showcase = await page.evaluate(() => {
            const r = window.app.runs.get(-1);
            return {label: r.label, depth: r.depth,
                    solutionLen: r.solution.length, scrambleLen: r.scramble.length,
                    selected: window.app.selectedRun};
        });
        if (showcase.label !== 'hardest proven cube' || showcase.depth !== 31
            || showcase.solutionLen !== 31 || showcase.scrambleLen !== 0
            || showcase.selected !== -1) {
            throw new Error('bad showcase run: ' + JSON.stringify(showcase));
        }
        // autoplay started on its own
        await page.waitForFunction(() => window.app.controller.boundary > 1, null, {timeout: 30000});
        await page.click('#play');   // pause
        // the full solution replayed from the hardest position ends solved
        await page.evaluate(() => { window.app.controller.jumpSolved(); });
        await page.waitForTimeout(300);
        const hardestSolved = await page.evaluate(() => ({
            boundary: window.app.controller.boundary,
            solved: window.app.cube.isSolvedBookkeeping()}));
        if (hardestSolved.boundary !== 31 || !hardestSolved.solved) {
            throw new Error('showcase replay does not end solved: ' + JSON.stringify(hardestSolved));
        }
        console.log(`showcase ok: ${showcase.label}, replay ends solved`);
        await page.screenshot({path: path.join(__dirname, 'testdata', 'app_hardest.png')});

        // --- solve before generation: refusal + red flash, no garbage ---
        await page.evaluate(() => window.app.setEditMode(true));
        await sleep(200);
        const refusalWire = JSON.parse(fs.readFileSync(path.join(__dirname, 'testdata', 'run0_placements.json')));
        await page.evaluate((wire) => {
            const app = window.app;
            const mesh = app.meshData;
            const pieceOfGrid = new Map(mesh.solved.map((p, i) => [p.grid.join(','), i]));
            for (const list of [wire.corners, wire.edges]) {
                for (const flat of list) {
                    const slot = flat.slice(0, 3);
                    const pieceHome = flat.slice(3, 6);
                    const piece = pieceOfGrid.get(pieceHome.join(','));
                    for (let s = 0; s < (flat.length - 6) / 6; s++) {
                        const home = flat.slice(6 + s*6, 9 + s*6);
                        const cur = flat.slice(9 + s*6, 12 + s*6);
                        const pg = mesh.stickerFaces[piece].findIndex(
                            d => d[0] === home[0] && d[1] === home[1] && d[2] === home[2]);
                        const color = mesh.stickerColors[piece][pg];
                        const slotIdx = mesh.solved.findIndex(
                            p => p.grid[0] === slot[0] && p.grid[1] === slot[1] && p.grid[2] === slot[2]);
                        const sg = app.editorMeshes.groupFaces[slotIdx].findIndex(
                            d => d[0] === cur[0] && d[1] === cur[1] && d[2] === cur[2]);
                        app.editorState.setSticker(slotIdx, sg, color);
                    }
                }
            }
            app.paintEditorScene();
        }, refusalWire);
        await page.click('#solve_position');
        await sleep(300);
        const refusal = await page.evaluate(() => ({
            status: document.getElementById('editor_status').textContent,
            flash: document.getElementById('init').classList.contains('flash-red'),
            logText: document.getElementById('log').textContent,
        }));
        if (refusal.status !== 'generate the tables first' || !refusal.flash) {
            throw new Error('solve-before-init refusal failed: ' + JSON.stringify(refusal));
        }
        if (refusal.logText.includes('[object Object]')) {
            throw new Error('log contains [object Object]');
        }
        await page.evaluate(() => window.app.setEditMode(false));
        console.log('solve before init: refusal + red generate flash ok');

        // --- generation ---
        await page.click('#init');
        await page.waitForFunction(() => window.app.ready, null, {timeout: 600000});
        console.log('generation done, heap:', await page.textContent('#heap'));

        // the showcase placements + solution verified through the wasm
        // module: convert -> state, then the solution applied to it ends
        // in the solved state
        const wasmCheck = await page.evaluate(async () => {
            const app = window.app;
            const res = await fetch('hardest.json');
            const hardest = await res.json();
            const conv = await app.solver.convert(hardest.placements.corners,
                                                  hardest.placements.edges);
            if (!conv.ok) return {ok: false, message: conv.message};
            const state = await app.solver.stateAfterMoves(hardest.solution, conv.state);
            return {ok: true, solved: state.every(v => v === 0), state};
        });
        if (!wasmCheck.ok || !wasmCheck.solved) {
            throw new Error('wasm showcase verification failed: ' + JSON.stringify(wasmCheck));
        }
        console.log('wasm verifies: hardest.json solution solves the stored position');

        // --- the scramble demo with playback ---
        await page.evaluate(() => {
            window._iterations = [];
            window.app.solver.onEvent(ev => { if (ev.kind === 'iteration') window._iterations.push(ev); });
        });
        await page.click('#run');
        // runs: the showcase (-1) + the demo run 0
        await page.waitForFunction(() => window.app.runs.get(0) !== undefined, null, {timeout: 600000});
        // the follow mode selected the run and the replay is playing
        await page.waitForFunction(() => window.app.controller.boundary > 2, null, {timeout: 60000});
        console.log('demo run selected, replay playing (boundary > 2)');
        await page.click('#play');   // pause
        // jump to the scrambled position
        await page.evaluate(() => { window.app.controller.jumpScrambled(); });
        await page.waitForTimeout(300);
        const boundary = await page.evaluate(() => window.app.controller.boundary);
        const scrambleLen = await page.evaluate(() => window.app.controller.scrambleLen);
        if (boundary !== scrambleLen) throw new Error(`boundary ${boundary} != scrambleLen ${scrambleLen}`);
        console.log(`paused at the scrambled position (boundary ${boundary}/${scrambleLen})`);
        await page.screenshot({path: path.join(__dirname, 'testdata', 'app_scrambled.png')});

        // wait for the solution of run 0
        // generous timeout: the shared host's load can slow the search 6x+
        await page.waitForFunction(() => { const r = window.app.runs.get(0); return r && r.solution !== null; },
                                   null, {timeout: 1800000});
        const depth = await page.evaluate(() => window.app.runs.get(0).depth);
        console.log(`run 0 solved, depth ${depth}`);
        const iterations = await page.evaluate(() => window._iterations);
        if (!iterations || iterations.length < 1) {
            throw new Error('no iteration progress events received');
        }
        const iterText = await page.evaluate(() => document.getElementById('log').textContent);
        if (!iterText.includes('no solution with depth')) {
            throw new Error('no iteration progress in the log');
        }
        console.log(`iteration progress ok (${iterations.length} events)`);

        // jump to solved
        await page.evaluate(() => { window.app.controller.jumpSolved(); });
        await page.waitForTimeout(300);
        const solved = await page.evaluate(() => window.app.cube.isSolvedBookkeeping());
        if (!solved) throw new Error('the solved boundary is not solved!');
        console.log('jump to solved: cube solved');
        await page.screenshot({path: path.join(__dirname, 'testdata', 'app_solved.png')});

        // --- the editor: paint the run-0 position ---
        await page.evaluate(() => window.app.setEditMode(true));
        await page.waitForTimeout(200);
        // paint programmatically from the test placements
        const painted = await page.evaluate(() => {
            const app = window.app;
            const placements = app.pendingTestPlacements;
            return app.editorState.colors.size;
        });
        // load the placements + paint in the page (needs meshes + wire data)
        const wire = JSON.parse(fs.readFileSync(path.join(__dirname, 'testdata', 'run0_placements.json')));
        await page.evaluate((wire) => {
            const app = window.app;
            const mesh = app.meshData;
            const pieceOfGrid = new Map(mesh.solved.map((p, i) => [p.grid.join(','), i]));
            // per slot: which color is painted on which slot group.
            // the placement tells: the piece at the slot has a sticker with
            // home dir h currently pointing along dir c; its color is the
            // piece group with that home dir.
            for (const list of [wire.corners, wire.edges]) {
                for (const flat of list) {
                    const slot = flat.slice(0, 3);
                    const pieceHome = flat.slice(3, 6);
                    const piece = pieceOfGrid.get(pieceHome.join(','));
                    const numStickers = (flat.length - 6) / 6;
                    for (let s = 0; s < numStickers; s++) {
                        const home = flat.slice(6 + s*6, 9 + s*6);
                        const cur = flat.slice(9 + s*6, 12 + s*6);
                        // the color of this sticker of the piece
                        const pg = mesh.stickerFaces[piece].findIndex(
                            d => d[0] === home[0] && d[1] === home[1] && d[2] === home[2]);
                        const color = mesh.stickerColors[piece][pg];
                        // the slot group whose face dir is the current dir
                        const slotIdx = mesh.solved.findIndex(
                            p => p.grid[0] === slot[0] && p.grid[1] === slot[1] && p.grid[2] === slot[2]);
                        const sg = app.editorMeshes.groupFaces[slotIdx].findIndex(
                            d => d[0] === cur[0] && d[1] === cur[1] && d[2] === cur[2]);
                        app.editorState.setSticker(slotIdx, sg, color);
                    }
                }
            }
            app.paintEditorScene();
            app.updateColorButtons();
            app.updateEditorStatus();
        }, wire);
        const remaining = await page.evaluate(() => window.app.editorState.remainingPerColor());
        if (remaining.some(c => c !== 0)) throw new Error('incomplete paint: ' + remaining);
        console.log('run-0 position painted (all 48 stickers)');

        // undo/redo round trip
        const undoOk = await page.evaluate(() => {
            const app = window.app;
            const before = app.editorState.stickerColor(6, 0);
            app.editorState.setSticker(6, 0, (before + 1) % 6);
            const changed = app.editorState.stickerColor(6, 0);
            app.editorState.undo();
            const restored = app.editorState.stickerColor(6, 0);
            app.editorState.redo();
            const redone = app.editorState.stickerColor(6, 0);
            app.editorState.undo();   // back to the painted state
            return changed === (before + 1) % 6 && restored === before && redone === changed
                && app.editorState.stickerColor(6, 0) === before;
        });
        if (!undoOk) throw new Error('undo/redo round trip failed');
        console.log('undo/redo ok');

        // one raycast paint: find a sticker that is actually clickable at
        // its projected position (front facing, not occluded), click it with
        // a color different from its current one and verify the change.
        const target = await page.evaluate(() => {
            const app = window.app;
            const cam = app.viewport.camera;
            cam.updateMatrixWorld();
            const canvas = app.viewport.renderer.domElement;
            const rect = canvas.getBoundingClientRect();
            const project = (local, rot) => {
                const center = [
                    rot[0][0]*local[0] + rot[0][1]*local[1] + rot[0][2]*local[2],
                    rot[1][0]*local[0] + rot[1][1]*local[1] + rot[1][2]*local[2],
                    rot[2][0]*local[0] + rot[2][1]*local[1] + rot[2][2]*local[2]];
                const m = cam.matrixWorldInverse.elements;
                const p = cam.projectionMatrix.elements;
                const vx = m[0]*center[0] + m[4]*center[1] + m[8]*center[2] + m[12];
                const vy = m[1]*center[0] + m[5]*center[1] + m[9]*center[2] + m[13];
                const vz = m[2]*center[0] + m[6]*center[1] + m[10]*center[2] + m[14];
                const cx = p[0]*vx + p[8]*vz, cy = p[5]*vy + p[9]*vz, cw = -vz;
                return {x: rect.left + (cx/cw + 1) / 2 * rect.width,
                        y: rect.top + (1 - cy/cw) / 2 * rect.height};
            };
            for (let piece = 18; piece < 26; piece++) {   // corners: the axis-mirrored pieces
                const groups = app.editorMeshes.pieces[piece].groups;
                const verts = app.editorMeshes.pieces[piece].vertices;
                for (let g = 0; g < groups.length; g++) {
                    const face = groups[g].faces[0];
                    const center = [0, 0, 0];
                    for (const v of face) {
                        center[0] += verts[v][0] / 3; center[1] += verts[v][1] / 3; center[2] += verts[v][2] / 3;
                    }
                    const pos = project(center, app.editorMeshes.rotations[piece]);
                    if (pos.x < rect.left + 5 || pos.x > rect.right - 5
                        || pos.y < rect.top + 5 || pos.y > rect.bottom - 5) continue;
                    const picked = app.pick(pos.x, pos.y);
                    if (picked && picked.piece === piece && picked.group === g) {
                        return {piece, group: g, x: pos.x, y: pos.y,
                                cur: app.editorState.stickerColor(piece, g)};
                    }
                }
            }
            return null;
        });
        if (!target) throw new Error('no clickable corner sticker found');
        const rayColor = (target.cur + 1) % 6;
        await page.evaluate((c) => { window.app.selectedColor = c; window.app.updateColorButtons(); }, rayColor);
        await page.mouse.click(target.x, target.y);
        await page.waitForTimeout(200);
        const rayPainted = await page.evaluate((t) =>
            window.app.editorState.stickerColor(t.piece, t.group), target);
        if (rayPainted !== rayColor) {
            throw new Error(`raycast paint failed: (${target.piece},${target.group}) `
                + `${rayPainted} != ${rayColor}`);
        }
        console.log(`raycast painting ok (${target.piece},${target.group})`);
        // undo the raycast paint to restore the run-0 position
        await page.evaluate(() => window.app.onEditorUndo());
        const restored = await page.evaluate((t) => ({
            cur: window.app.editorState.stickerColor(t.piece, t.group),
            remaining: window.app.editorState.remainingPerColor()}), target);
        if (restored.cur !== target.cur || restored.remaining.some(c => c !== 0)) {
            throw new Error('undo did not restore the painted position: ' + JSON.stringify(restored));
        }

        // the puppet cube preview
        await page.click('#puppet_view');
        await page.waitForTimeout(300);
        const previewOk = await page.evaluate(() => window.app.puppetView === true);
        if (!previewOk) throw new Error('puppet preview did not activate');
        await page.screenshot({path: path.join(__dirname, 'testdata', 'app_preview.png')});
        await page.click('#puppet_view');   // back to painting
        console.log('puppet cube preview ok');

        // --- solve the painted position ---
        await page.click('#solve_position');
        await page.waitForFunction(() => {
            const runs = window.app.runs;
            for (const run of runs.values()) {
                if (run.label === 'custom' && run.solution !== null) return true;
            }
            return false;
        }, null, {timeout: 1800000});
        const customDepth = await page.evaluate(() => {
            for (const run of window.app.runs.values()) {
                if (run.label === 'custom') return run.depth;
            }
        });
        if (customDepth !== depth) {
            throw new Error(`custom depth ${customDepth} != run 0 depth ${depth}`);
        }
        console.log(`custom position solved: depth ${customDepth} (same as run 0)`);

        // the custom run's solution, replayed from the PAINTED arrangement,
        // must end solved - verifies the initial placement (what the user
        // sees) matches the position the solver actually solved
        await page.evaluate(() => { window.app.controller.jumpSolved(); });
        await page.waitForTimeout(300);
        const customSolved = await page.evaluate(() => window.app.cube.isSolvedBookkeeping());
        if (!customSolved) throw new Error('the custom run replay does not end solved!');
        console.log('custom run replay ends solved');
        await page.screenshot({path: path.join(__dirname, 'testdata', 'app_custom.png')});

        // --- a second demo: another click, a fresh cube (run 1) ---
        const run0Scramble = await page.evaluate(() =>
            window.app.runs.get(0).scramble.map(m => m.index));
        await page.click('#run');
        await page.waitForFunction(() => window.app.runs.get(1) !== undefined,
                                   null, {timeout: 600000});
        const run1 = await page.evaluate(() => {
            const r = window.app.runs.get(1);
            return {scramble: r.scramble.map(m => m.index), selected: window.app.selectedRun};
        });
        if (run1.scramble.length === 0 || JSON.stringify(run1.scramble) === JSON.stringify(run0Scramble)) {
            throw new Error('the second demo did not produce a fresh cube');
        }
        if (run1.selected !== 1) throw new Error('follow did not select run 1');
        await page.waitForFunction(() => { const r = window.app.runs.get(1); return r && r.solution !== null; },
                                   null, {timeout: 1800000});
        const depth1 = await page.evaluate(() => window.app.runs.get(1).depth);
        console.log(`second demo ok: run 1 solved, depth ${depth1}, fresh scramble`);

        console.log('APP TEST PASSED');
    } finally {
        await browser.close();
        server.kill();
    }
}

main().catch(err => { console.error(err); process.exit(1); });
