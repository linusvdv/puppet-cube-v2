// The web app: viewer + playback + editor wired to the wasm solver worker.

import * as THREE from 'three';
import { deriveMoves, VisualCube } from './cube_model.js';
import { PuppetScene, Viewport } from './scene.js';
import { PlaybackController } from './playback.js';
import { MoveTimeline } from './timeline.js';
import { EditorState, InvalidPosition, initialPlacementData, GRAY, DARK_RED, NUM_PIECES } from './editor.js';
import { Solver } from './solver.js';

const $ = (id) => document.getElementById(id);

async function fetchJson(url) {
    const r = await fetch(url);
    if (!r.ok) throw new Error(`failed to load ${url}`);
    return r.json();
}

class App {
    constructor() {
        this.solver = new Solver();
        this.ready = false;
        this.editing = false;
        this.puppetView = false;
        this.runs = new Map();       // runIdx -> {scramble, solution, depth, initial, label}
        this.customId = 1000000;
        this.pendingCustomIds = [];  // app-side ids of queued custom solves
        this.activeCustomId = null;
        this.initStarted = false;
        this.selectedRun = null;
        this.follow = true;
        this.userPaused = false;
        this.flash = null;
        this.selectedColor = null;
        this.selectedEraser = false;
    }

    async boot() {
        const [meshData, editorMeshes, reps, hardest] = await Promise.all([
            fetchJson('meshes.json'), fetchJson('editor_meshes.json'),
            fetchJson('rotation_reps.json'), fetchJson('hardest.json')]);
        this.meshData = meshData;
        this.editorMeshes = editorMeshes;
        this.moves = deriveMoves(reps);

        this.cube = new VisualCube(meshData.solved);
        this.controller = new PlaybackController(this.cube);
        this.viewport = new Viewport($('view'));
        this.scene = new PuppetScene(meshData);
        this.editorScene = new PuppetScene({pieces: editorMeshes.pieces});
        // the editor pieces are local meshes posed by their placement
        // rotation (the same convention as the puppet scene, statically)
        this.editorScene.setRotations(editorMeshes.rotations);
        this.editorState = new EditorState(meshData, editorMeshes);

        this.timeline = new MoveTimeline($('timeline'));
        this.timeline.onTimelineClick((boundary, row) => {
            this.follow = false;
            this.controller.pause();
            this.controller.setBoundary(boundary);
            this.timeline.setBoundary(boundary, row);
            this.updateStatus();
        });

        this.setupUi();

        // the solver events
        this.solver.onEvent(ev => this.onSolverEvent(ev));
        this.solver.waitForLoaded().then(() => {
            $('init').disabled = false;
            $('status').textContent = 'module loaded';
        });
        $('isolated').textContent = crossOriginIsolated;

        // the hardest known position: playable before any tables are
        // generated (display needs no solver - the moves are stored)
        this.runs.set(-1, {
            scramble: [],
            solution: hardest.solution.map(i => this.moves[i]),
            depth: hardest.depth,
            initial: initialPlacementData(EditorState.fromWire(hardest.placements), meshData),
            label: 'hardest proven cube',
        });
        this.refreshRunList();
        this.selectRun(-1, true);

        this.renderLoop();
    }

    static bootLogArgs() {
        const level = new URLSearchParams(location.search).get('log');
        return ['-r', '1', '-l', level || 'warning'];
    }

    setupUi() {
        $('init').onclick = () => {
            $('init').disabled = true;
            $('init').textContent = 'generating ...';
            this.initStarted = true;
            this.solver.initialize(App.bootLogArgs());
        };
        $('run').onclick = () => {
            this.follow = true;      // the demo explicitly follows its runs
            this.solver.run();       // every click: one new cube
        };
        $('edit').onclick = () => this.setEditMode(!this.editing);
        $('puppet_view').onclick = () => this.setPuppetView(!$('puppet_view').classList.contains('selected'));

        for (const [id, fn, row] of [
            ['jump_scrambled', () => this.controller.jumpScrambled(), 1],
            ['step_back', () => this.controller.stepBackward(), null],
            ['play', () => this.controller.isPlaying ? this.controller.pause() : this.controller.play(), null],
            ['step_forward', () => this.controller.stepForward(), null],
            ['jump_solved', () => this.controller.jumpSolved(), 1],
        ]) {
            $(id).onclick = () => {
                this.follow = false;
                fn();
                this.timeline.setBoundary(this.controller.boundary, row);
                this.updateStatus();
            };
        }

        // the editor palette
        const colors = this.meshData.colors;
        const palette = $('colors');
        this.colorButtons = [];
        for (let c = 0; c < 6; c++) {
            const btn = document.createElement('button');
            btn.className = 'color';
            const rgb = colors[c].map(v => Math.round(v * 255));
            btn.style.background = `rgb(${rgb.join(',')})`;
            btn.onclick = () => {
                this.selectedColor = this.selectedColor === c ? null : c;
                this.selectedEraser = false;
                this.updateColorButtons();
            };
            palette.appendChild(btn);
            this.colorButtons.push(btn);
        }
        const eraser = document.createElement('button');
        eraser.className = 'tool';
        eraser.textContent = 'eraser';
        eraser.onclick = () => {
            this.selectedEraser = !this.selectedEraser;
            this.selectedColor = null;
            this.updateColorButtons();
        };
        palette.appendChild(eraser);
        this.eraserButton = eraser;

        for (const [id, fn] of [['undo', () => this.onEditorUndo()],
                                ['redo', () => this.onEditorRedo()],
                                ['clear', () => this.onEditorClear()],
                                ['solve_position', () => this.onEditorSolve()]]) {
            $(id).onclick = fn;
        }

        // picking: paint on click, orbit on drag
        const dom = this.viewport.renderer.domElement;
        let downX = 0, downY = 0;
        dom.addEventListener('pointerdown', (e) => { downX = e.clientX; downY = e.clientY; });
        dom.addEventListener('pointerup', (e) => {
            if (!this.editing || this.puppetView) return;
            if (Math.hypot(e.clientX - downX, e.clientY - downY) > 6) return;
            this.paintAt(e);
        });
    }

    updateColorButtons() {
        if (!this.editorState) return;
        const remaining = this.editorState.remainingPerColor();
        this.colorButtons.forEach((btn, c) => {
            btn.classList.toggle('selected', this.selectedColor === c);
            btn.textContent = remaining[c];
        });
        this.eraserButton.classList.toggle('selected', this.selectedEraser);
    }

    // --- solver events -------------------------------------------------------

    onSolverEvent(ev) {
        // custom solves all report the fixed C++ offset id - remap to the
        // app-side id of the solve currently in flight (searches are
        // serialized, so at most one is active)
        if (ev.runIdx >= 1000000) {
            if (ev.kind === 'scramble') {
                this.activeCustomId = this.pendingCustomIds.shift() ?? 1000000;
            }
            ev.runIdx = this.activeCustomId;
        }
        if (ev.kind === 'stage') {
            this.log(`[stage] ${ev.text}`, 'stage');
        } else if (ev.kind === 'ready') {
            this.ready = true;
            this.log(ev.text, 'ready');
            $('status').textContent = 'ready';
            $('init').textContent = 'tables ready';
            $('run').disabled = false;
            this.updateColorButtons();
        } else if (ev.kind === 'iteration') {
            // a depth was exhausted without finding a solution
            const run = this.runs.get(ev.runIdx);
            const label = run ? run.label : `run ${ev.runIdx}`;
            const text = `${label}: no solution with depth ${ev.depth}, `
                + `searched ${Math.round(ev.positions).toLocaleString('en-US')} positions`;
            this.log(text, 'stage');
            if (run) run.progress = text;
        } else if (ev.kind === 'scramble') {
            const custom = ev.runIdx >= 1000000;
            const label = custom ? 'custom' : `run ${ev.runIdx}`;
            this.runs.set(ev.runIdx, {
                scramble: ev.moves.map(i => this.moves[i]),
                solution: null, depth: null,
                initial: custom ? this.pendingInitial : null, label});
            this.refreshRunList();
            if (custom && ev.runIdx === this.autoShowCustom) {
                this.autoShowCustom = null;
                this.selectRun(ev.runIdx, true);
            } else if (this.follow && !custom && !this.editing) {
                this.selectRun(ev.runIdx, true);
            }
        } else if (ev.kind === 'solution') {
            const run = this.runs.get(ev.runIdx);
            if (!run) return;
            run.solution = ev.moves.map(i => this.moves[i]);
            run.depth = ev.depth;
            this.refreshRunList();
            if (this.controller.runId === ev.runIdx) {
                this.controller.setSolution(run.solution);
                this.timeline.setMoves(run.scramble, run.solution);
                if (!this.userPaused) this.controller.play();
            }
            this.log(`${run.label}: depth ${ev.depth}`, 'solution');
        } else if (ev.kind === 'error') {
            this.log(ev.text, 'error');
        }
    }

    refreshRunList() {
        const list = $('runs');
        list.innerHTML = '';
        for (const [idx, run] of [...this.runs.entries()].sort((a, b) => a[0] - b[0])) {
            const item = document.createElement('div');
            item.className = 'run' + (this.selectedRun === idx ? ' selected' : '');
            item.textContent = run.solution
                ? `${run.label} - depth ${run.depth}`
                : `${run.label} - searching...`;
            item.onclick = () => {
                this.follow = false;
                this.selectRun(idx, false);
            };
            list.appendChild(item);
        }
    }

    selectRun(idx, autoplay) {
        const run = this.runs.get(idx);
        if (!run) return;
        this.selectedRun = idx;
        this.userPaused = !autoplay;
        this.controller.setRun(idx, run.scramble, run.solution, run.initial);
        this.timeline.setMoves(run.scramble, run.solution);
        this.timeline.setBoundary(this.controller.boundary, 1);
        if (autoplay) this.controller.play();
        this.refreshRunList();
        this.updateStatus();
    }

    // --- edit mode -------------------------------------------------------------

    setEditMode(on) {
        if (on === this.editing) return;
        this.editing = on;
        $('edit').textContent = on ? 'done' : 'edit';
        $('editor_panel').style.display = on ? 'flex' : 'none';
        this.stopFlash();
        this.setPuppetView(false);
        if (on) {
            this.follow = false;
            this.paintEditorScene();
            this.updateEditorStatus();
            this.updateColorButtons();
        }
    }

    paintEditorScene() {
        // recolor the editor stickers from the state
        const recolors = {};
        for (let p = 6; p < NUM_PIECES; p++) {
            const groups = this.editorMeshes.pieces[p].groups.length;
            for (let g = 0; g < groups; g++) {
                const color = this.editorState.stickerColor(p, g);
                recolors[`${p},${g}`] = color === undefined
                    ? GRAY : this.meshData.colors[color];
            }
        }
        this.editorScene.recolorGroups(recolors);
    }

    setPuppetView(on) {
        if (on && !this.puppetView) {
            try {
                const placements = this.editorState.placements();
                this.previewInitial = initialPlacementData(placements, this.meshData);
                if (!this.previewScene) {
                    this.previewScene = new PuppetScene(this.meshData);
                    this.previewCube = new VisualCube(this.meshData.solved);
                }
                this.puppetView = true;
                $('puppet_view').classList.add('selected');
                $('puppet_view').textContent = 'standard cube';
                this.previewCube.reset();
                for (const [piece, grid, rotation] of this.previewInitial) {
                    this.previewCube.grids[piece] = grid.slice();
                    this.previewCube.rotations[piece] = rotation.map(r => r.slice());
                }
                this.previewScene.updatePoses(this.previewCube, 0.0);
                return;
            } catch (err) {
                if (err instanceof InvalidPosition) this.reportEditorError(err);
                else this.log(String(err), 'error');
                // fall through to off
            }
        }
        this.puppetView = false;
        $('puppet_view').classList.remove('selected');
        $('puppet_view').textContent = 'puppet cube';
    }

    // what a click at the given client coordinates would paint
    pick(clientX, clientY) {
        const rect = this.viewport.renderer.domElement.getBoundingClientRect();
        const x = ((clientX - rect.left) / rect.width) * 2 - 1;
        const y = -((clientY - rect.top) / rect.height) * 2 + 1;
        const raycaster = new THREE.Raycaster();
        raycaster.setFromCamera({x, y}, this.viewport.camera);
        const hits = raycaster.intersectObjects(
            this.editorScene.pieces.map(p => p.children[0]), false);
        if (hits.length === 0) return null;
        const hit = hits[0];
        const piece = this.editorScene.pieces.indexOf(hit.object.parent);
        const group = this.faceGroup(piece, hit.faceIndex);
        return {piece, group, point: hit.point.toArray(), distance: hit.distance};
    }

    paintAt(event) {
        const picked = this.pick(event.clientX, event.clientY);
        if (!picked) return;
        const {piece, group} = picked;
        if (piece < 6 || group < 0) return;
        if (this.selectedColor === null && !this.selectedEraser) return;

        this.stopFlash();
        const color = this.selectedEraser ? null : this.selectedColor;
        this.editorState.setSticker(piece, group, color);
        const rgb = color === null ? GRAY : this.meshData.colors[color];
        this.editorScene.recolorGroups({[`${piece},${group}`]: rgb});
        this.updateColorButtons();
        this.updateEditorStatus();
    }

    faceGroup(piece, faceIndex) {
        const groups = this.editorMeshes.pieces[piece].groups;
        let offset = 0;
        for (let g = 0; g < groups.length; g++) {
            if (faceIndex < offset + groups[g].faces.length) return g;
            offset += groups[g].faces.length;
        }
        return -1;
    }

    onEditorUndo() {
        const keys = this.editorState.undo();
        if (!keys) return;
        this.stopFlash();
        this.recolorKeys(keys);
        this.updateColorButtons();
        this.updateEditorStatus();
    }

    onEditorRedo() {
        const keys = this.editorState.redo();
        if (!keys) return;
        this.stopFlash();
        this.recolorKeys(keys);
        this.updateColorButtons();
        this.updateEditorStatus();
    }

    onEditorClear() {
        this.editorState.clear();
        this.stopFlash();
        this.paintEditorScene();
        this.updateColorButtons();
        this.updateEditorStatus();
    }

    recolorKeys(keys) {
        const recolors = {};
        for (const [p, g] of keys) {
            const color = this.editorState.stickerColor(p, g);
            recolors[`${p},${g}`] = color === undefined ? GRAY : this.meshData.colors[color];
        }
        this.editorScene.recolorGroups(recolors);
    }

    updateEditorStatus() {
        const remaining = this.editorState.remainingPerColor();
        let text;
        if (remaining.some(c => c < 0)) {
            text = 'too many stickers of one color';
        } else if (remaining.every(c => c === 0)) {
            text = this.ready ? 'ready to solve' : 'ready (solver still generating)';
        } else {
            text = 'paint all the stickers';
        }
        $('editor_status').textContent = text;
    }

    async onEditorSolve() {
        // the solver needs its tables - refuse early with a hint instead of
        // surfacing the raw "not initialized" error
        if (!this.ready) {
            if (!this.initStarted) {
                $('editor_status').textContent = 'generate the tables first';
                this.flashInitButton();
            } else {
                $('editor_status').textContent = 'the solver is still generating its tables';
            }
            return;
        }
        let placements;
        try {
            placements = this.editorState.placements();
        } catch (err) {
            if (err instanceof InvalidPosition) this.reportEditorError(err);
            else this.log(String(err), 'error');
            return;
        }
        const wire = EditorState.toWire(placements);
        // the solver side checks (orientation parities, reachability) - fast
        const result = await this.solver.convert(wire.corners, wire.edges);
        if (!result.ok) {
            this.reportEditorError({message: result.message, slots: null, parity: true});
            return;
        }
        this.pendingInitial = initialPlacementData(placements, this.meshData);
        const id = this.customId++;
        this.pendingCustomIds.push(id);
        this.autoShowCustom = id;
        this.setEditMode(false);
        $('status').textContent = 'solving the custom position ...';
        this.solver.solve(wire.corners, wire.edges);
        this.log('custom position submitted', 'stage');
    }

    // flashes the generate tables button red (solve clicked before init)
    flashInitButton() {
        const btn = $('init');
        btn.classList.remove('flash-red');
        void btn.offsetWidth;   // restart the css animation
        btn.classList.add('flash-red');
        if (this._flashInitTimer) clearTimeout(this._flashInitTimer);
        this._flashInitTimer = setTimeout(() => btn.classList.remove('flash-red'), 2000);
    }

    reportEditorError(err) {
        $('editor_status').textContent = err.message;
        this.log(`invalid position: ${err.message}`, 'error');
        if (!this.editing) return;
        // flash the offending pieces dark red; parity errors have no single
        // piece - the whole piece class flashes
        let pieces = [];
        if (err.slots && err.slots.length) {
            const pieceOfGrid = new Map(this.meshData.solved.map((p, i) => [p.grid.join(','), i]));
            pieces = err.slots.map(slot => pieceOfGrid.get(
                (typeof slot === 'string' ? slot : slot.join(','))));
        } else {
            pieces = [...Array(20).keys()].map(i => i + 6);
        }
        this.flashPieces(pieces.filter(p => p !== undefined));
    }

    flashPieces(pieces) {
        this.stopFlash();
        if (!pieces.length) return;
        // capture the current colors fresh (a reused capture would restore
        // stale colors over later paints)
        const colors = pieces.flatMap(p => {
            const groups = this.editorMeshes.pieces[p].groups.length;
            return [...Array(groups).keys()].map(g => `${p},${g}`);
        });
        let on = false;
        let count = 0;
        this.flash = setInterval(() => {
            on = !on;
            const recolors = {};
            for (const key of colors) {
                const [p, g] = key.split(',').map(Number);
                const color = this.editorState.stickerColor(p, g);
                recolors[key] = on ? DARK_RED
                    : (color === undefined ? GRAY : this.meshData.colors[color]);
            }
            this.editorScene.recolorGroups(recolors);
            if (++count >= 6) this.stopFlash();
        }, 250);
        this.flashColors = colors;
    }

    stopFlash() {
        if (this.flash) {
            clearInterval(this.flash);
            this.flash = null;
            const recolors = {};
            for (const key of this.flashColors) {
                const [p, g] = key.split(',').map(Number);
                const color = this.editorState.stickerColor(p, g);
                recolors[key] = color === undefined ? GRAY : this.meshData.colors[color];
            }
            this.editorScene.recolorGroups(recolors);
        }
    }

    // --- the render loop --------------------------------------------------------

    updateStatus() {
         if (!this.editing) {
             const run = this.selectedRun !== null ? this.runs.get(this.selectedRun) : null;
             const total = this.controller.numMoves;
             if (run) {
                 // while searching: the latest iteration progress, else the
                 // move counter
                 $('status').textContent = run.solution === null
                     ? `${run.label}: ${run.progress || 'searching...'}`
                     : `${run.label}: move ${this.controller.boundary}/${total}`;
             } else if (this.ready) {
                 $('status').textContent = 'ready';
             }
         }
         // the spinner: table generation or an active search
         const searching = [...this.runs.values()].some(r => r.solution === null)
             || this.pendingCustomIds.length > 0;
         $('spinner').classList.toggle('busy', (this.initStarted && !this.ready) || searching);
         $('heap').textContent = this.solver.status.heapMB !== undefined
             ? `${Math.round(this.solver.status.heapMB)} MB` : '-';
        if (this.solver.status.threads !== undefined) {
            $('threads').textContent = this.solver.status.threads;
        }
    }

    renderLoop() {
        const loop = (time) => {
            requestAnimationFrame(loop);
            const dt = Math.min(0.05, (time - (this.lastTime || time)) / 1000);
            this.lastTime = time;
            if (this.editing) {
                if (this.puppetView && this.previewScene) {
                    this.viewport.render(this.previewScene);
                } else {
                    // the editor vertices are pre-posed - render as-is
                    this.viewport.render(this.editorScene);
                }
            } else {
                const changed = this.controller.tick(dt);
                if (changed || this.viewport.controls.enableDamping) {
                    this.scene.updatePoses(this.cube, this.controller.angle);
                    this.timeline.setBoundary(this.controller.boundary);
                    this.viewport.render(this.scene);
                    this.updateStatus();
                }
            }
        };
        requestAnimationFrame(loop);
    }

    log(text, cls) {
        const line = document.createElement('div');
        if (cls) line.className = cls;
        line.textContent = text;
        const el = $('log');
        el.appendChild(line);
        while (el.childNodes.length > 200) el.removeChild(el.firstChild);
        el.scrollTop = el.scrollHeight;
    }
}

const app = new App();
window.app = app;   // for debugging / tests
app.boot().catch(err => {
    console.error(err);
    document.getElementById('status').textContent = 'failed to start: ' + err;
});
