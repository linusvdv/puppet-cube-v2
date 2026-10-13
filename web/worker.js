// The wasm module host: runs inside a dedicated Web Worker so the search's
// barrier loops never block the ui thread. The pthread workers (the search
// threads) spawn as nested workers under this one.
//
// NOTE: emscripten resolves the pthread worker script to THIS worker's url
// (the module was loaded via importScripts, so its _scriptName is
// worker.js) - the pthread workers therefore load this file too. In that
// case puppetweb.js (importScripts above) has already detected the
// 'em-pthread' worker name and installed the pthread protocol handlers,
// and the rpc host setup below must not run.
//
// Message protocol (main thread <-> worker):
//   -> {type: "init", args: [...], threads: n}
//   -> {type: "run"}
//   -> {type: "solve", corners: [...], edges: [...]}
//   -> {type: "convert", corners: [...], edges: [...]}
//   <- {type: "events", events: [...]}     (polled here, forwarded)
//   <- {type: "status", ...}               (module state, heap size)

importScripts('puppetweb.js');

if (globalThis.name !== 'em-pthread') {

let module = null;
let threads_reported = false;

const post = (msg) => postMessage(msg);

// emscripten 6 embind returns std::vector as instances (size/get), not arrays
function vecToArray(v) {
    const out = [];
    for (let i = 0; i < v.size(); i++) out.push(v.get(i));
    return out;
}

function pollLoop() {
    if (!module) return;
    try {
        const queue = module.poll_events();
        if (queue.size() > 0) {
            // structured clone would strip the embind accessors - convert
            // to plain objects here in the worker
            let report_threads = false;
            const events = [];
            for (let i = 0; i < queue.size(); i++) {
                const ev = queue.get(i);
                if (ev.kind === 'ready' || ev.kind === 'stage') report_threads = true;
                events.push({kind: ev.kind, text: ev.text, runIdx: ev.runIdx,
                             depth: ev.depth, moves: vecToArray(ev.moves)});
            }
            post({type: 'events', events: events});
            // the thread count is known once the settings are parsed (the
            // first stage event) - show it during the generation already
            if (report_threads && !threads_reported) {
                threads_reported = true;
                post({type: 'status', threads: module.num_threads()});
            }
        }
        post({type: 'status', heapMB: (module.wasmMemory ? module.wasmMemory.buffer.byteLength : module.HEAPU8.buffer.byteLength) / 1048576});
    } catch (err) {
        post({type: 'status', error: String(err)});
    }
    setTimeout(pollLoop, 250);
}

createPuppetModule({
    print: (text) => console.log('[wasm]', text),
    printErr: (text) => console.error('[wasm]', text),
}).then(mod => {
    module = mod;
    post({type: 'status', loaded: true});
    pollLoop();
}).catch(err => {
    post({type: 'status', error: 'module load failed: ' + err});
});

onmessage = (msg) => {
    const data = msg.data;
    if (!module) {
        post({type: 'status', error: 'module not loaded yet'});
        return;
    }
    try {
        if (data.type === 'init') {
            const args = data.args || [];
            // leave two cores for the ui when possible
            const threads = data.threads ||
                Math.max(1, (navigator.hardwareConcurrency || 4) - 2);
            args.push('-t', String(threads));
            module.initialize(args);
        } else if (data.type === 'run') {
            module.run();
        } else if (data.type === 'solve') {
            module.solve_position(data.corners, data.edges);
        } else if (data.type === 'convert') {
            const r = module.convert_position(data.corners, data.edges);
            // plain object - the embind accessors would be stripped by the
            // structured clone (the state is useful for the tests)
            post({type: 'convert', result: {ok: r.ok, state: vecToArray(r.state),
                                            error: r.error, message: r.message}});
        } else if (data.type === 'state_after_moves') {
            const state = module.state_after_moves(data.moves, data.start || []);
            post({type: 'state_after_moves', state: state});
        }
    } catch (err) {
        post({type: 'error', message: String(err)});
    }
};

}
