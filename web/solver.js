// The solver client: wraps the worker RPC in a promise-ish api and exposes
// the events as a subscription.

export class Solver {
    constructor() {
        this.worker = new Worker('worker.js');
        this.events = [];
        this.listeners = [];
        this.status = {loaded: false};
        this._waiters = [];

        this.worker.onmessage = (msg) => {
            const data = msg.data;
            if (data.type === 'events') {
                for (const ev of data.events) {
                    this.events.push(ev);
                    for (const listener of this.listeners) listener(ev);
                }
            } else if (data.type === 'status') {
                Object.assign(this.status, data);
                for (const waiter of this._waiters.splice(0)) waiter();
            } else if (data.type === 'error') {
                for (const listener of this.listeners) listener({kind: 'error', text: data.message});
            } else if (data.type === 'convert') {
                this._convertResult = data.result;
                for (const waiter of this._waiters.splice(0)) waiter();
            } else if (data.type === 'state_after_moves') {
                this._stateResult = data.state;
                for (const waiter of this._waiters.splice(0)) waiter();
            }
        };
    }

    onEvent(listener) {
        this.listeners.push(listener);
    }

    async waitForLoaded() {
        while (!this.status.loaded) {
            await new Promise(resolve => this._waiters.push(resolve));
        }
    }

    initialize(args) {
        this.worker.postMessage({type: 'init', args});
    }

    run() {
        this.worker.postMessage({type: 'run'});
    }

    solve(corners, edges) {
        this.worker.postMessage({type: 'solve', corners, edges});
    }

    convert(corners, edges) {
        return new Promise((resolve) => {
            this._convertResult = null;
            this.worker.postMessage({type: 'convert', corners, edges});
            const check = () => {
                if (this._convertResult !== null) resolve(this._convertResult);
                else this._waiters.push(check);
            };
            check();
        });
    }

    stateAfterMoves(moves, start = []) {
        return new Promise((resolve) => {
            this._stateResult = null;
            this.worker.postMessage({type: 'state_after_moves', moves, start});
            const check = () => {
                if (this._stateResult !== null) resolve(this._stateResult);
                else this._waiters.push(check);
            };
            check();
        });
    }

    // resolves on the next event matching the predicate
    waitFor(predicate) {
        return new Promise(resolve => {
            const listener = (ev) => {
                if (predicate(ev)) {
                    this.listeners.splice(this.listeners.indexOf(listener), 1);
                    resolve(ev);
                }
            };
            this.listeners.push(listener);
        });
    }
}
