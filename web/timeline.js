// The move timeline: two rows of move chips (scramble on top, solution
// below), a playhead on the current boundary, click to jump. Port of the
// layout rules of gui/timeline.js (timeline_test verifies the click
// mapping there; here the same boundary->chip math).

const LABEL_W = 70;
const CHIP_W = 28;
const CHIP_H = 22;
const SPACING = 4;
const PAD = 8;
const TOP = 4;
const ROW_SPACING = 8;

export class MoveTimeline {
    constructor(container) {
        this.container = container;
        this.canvas = document.createElement('canvas');
        this.canvas.style.display = 'block';
        container.appendChild(this.canvas);
        this.ctx = this.canvas.getContext('2d');
        this.scramble = [];
        this.solution = null;
        this.boundary = 0;
        this.boundaryRow = null;
        this.hover = null;
        this.clickHandler = null;

        this.canvas.addEventListener('mousemove', (e) => {
            this.hover = this.pick(e.offsetX, e.offsetY);
            this.canvas.style.cursor = this.hover ? 'pointer' : 'default';
            this.draw();
        });
        this.canvas.addEventListener('mouseleave', () => {
            this.hover = null;
            this.draw();
        });
        this.canvas.addEventListener('click', (e) => {
            const hit = this.pick(e.offsetX, e.offsetY);
            if (hit && this.clickHandler) this.clickHandler(hit.boundary, hit.row);
        });
    }

    get singleRow() {
        return this.scramble.length === 0;
    }

    get numMoves() {
        return this.scramble.length + (this.solution || []).length;
    }

    setMoves(scramble, solution) {
        this.scramble = scramble.slice();
        this.solution = solution ? solution.slice() : null;
        this.boundary = Math.min(this.boundary, this.numMoves);
        this.boundaryRow = null;
        this.applySize();
        this.draw();
    }

    setBoundary(boundary, row = null) {
        this.boundary = boundary;
        if (row !== null) {
            this.boundaryRow = row;
        } else if (boundary > this.scramble.length) {
            this.boundaryRow = 1;      // past the scramble: solution line
        } else if (boundary < this.scramble.length) {
            this.boundaryRow = 0;      // inside the scramble: scramble line
        } else if (this.boundaryRow === null) {
            this.boundaryRow = 0;      // the scrambled boundary lies on both
        }
        this.draw();
        this.ensureVisible();
    }

    // keeps the playhead inside the visible window of the scroll area
    // (minimal scrolling - manual scrolling while paused stays)
    ensureVisible() {
        const wrap = this.canvas.parentElement;
        if (!wrap || wrap.scrollWidth <= wrap.clientWidth) return;
        const x = this.boundaryX(this.boundary);
        if (x - 60 < wrap.scrollLeft) {
            wrap.scrollLeft = Math.max(0, x - 60);
        } else if (x + 60 > wrap.scrollLeft + wrap.clientWidth) {
            wrap.scrollLeft = Math.min(wrap.scrollWidth - wrap.clientWidth, x + 60 - wrap.clientWidth);
        }
    }

    onTimelineClick(handler) {
        this.clickHandler = handler;
    }

    rows() {
        return this.singleRow ? [{moves: this.solution || [], row: 1, dim: false}]
            : [{moves: this.scramble, row: 0, dim: true},
               {moves: this.solution || [], row: 1, dim: false}];
    }

    rowY(row) {
        if (this.singleRow) return TOP;
        return TOP + row * (CHIP_H + ROW_SPACING);
    }

    height() {
        return TOP + (this.singleRow ? 1 : 2) * CHIP_H
            + (this.singleRow ? 0 : ROW_SPACING) + 4;
    }

    width() {
        let longest = 0;
        for (const {moves} of this.rows()) longest = Math.max(longest, moves.length);
        return LABEL_W + PAD + Math.max(1, longest) * (CHIP_W + SPACING) + 8;
    }

    applySize() {
        this.canvas.width = this.width();
        this.canvas.height = this.height();
        this.canvas.style.width = this.canvas.width + 'px';
        this.canvas.style.height = this.canvas.height + 'px';
    }

    // boundary -> x position of its playhead
    boundaryX(b) {
        if (b <= this.scramble.length && !this.singleRow) {
            return LABEL_W + PAD + b * (CHIP_W + SPACING) - SPACING / 2;
        }
        const b2 = this.singleRow ? b : b - this.scramble.length;
        return LABEL_W + PAD + b2 * (CHIP_W + SPACING) - SPACING / 2;
    }

    pick(x, y) {
        for (const {moves, row} of this.rows()) {
            const ry = this.rowY(row);
            if (y < ry || y > ry + CHIP_H) continue;
            const rel = x - LABEL_W - PAD;
            if (rel < -SPACING) continue;
            // right half of a chip = after the move
            let chip = Math.floor((rel + SPACING / 2) / (CHIP_W + SPACING));
            chip = Math.max(0, Math.min(moves.length, chip));
            const base = this.singleRow ? 0 : (row === 0 ? 0 : this.scramble.length);
            return {boundary: base + chip, row};
        }
        return null;
    }

    draw() {
        const ctx = this.ctx;
        ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
        ctx.font = '11px monospace';

        for (const {moves, row, dim} of this.rows()) {
            const ry = this.rowY(row);
            // the row label
            ctx.fillStyle = '#888d96';
            ctx.fillText(row === 0 ? 'scramble' : 'solution', 6, ry + CHIP_H / 2 + 4);

            for (let i = 0; i < moves.length; i++) {
                const x = LABEL_W + PAD + i * (CHIP_W + SPACING);
                const hovered = this.hover && this.hover.row === row && this.hover.boundary ===
                    (this.singleRow ? i + 1 : (row === 0 ? i + 1 : this.scramble.length + i + 1));
                // the chip
                ctx.fillStyle = dim ? '#b8bcc4' : '#2c3a4d';
                ctx.fillRect(x, ry, CHIP_W, CHIP_H);
                if (hovered) {
                    ctx.strokeStyle = '#5087d0';
                    ctx.strokeRect(x + 0.5, ry + 0.5, CHIP_W - 1, CHIP_H - 1);
                }
                ctx.fillStyle = dim ? '#3d4046' : '#f0f2f5';
                ctx.fillText(moves[i].name, x + 5, ry + CHIP_H / 2 + 4);
            }

            // the "searching..." indicator while the solution is unknown
            if (row === 1 && this.solution === null) {
                const x = LABEL_W + PAD + moves.length * (CHIP_W + SPACING);
                ctx.fillStyle = '#888d96';
                ctx.fillText(this.scramble.length === 0 ? 'searching...' : '', x + 6, ry + CHIP_H / 2 + 4);
                if (this.scramble.length === 0) {
                    ctx.strokeStyle = '#888d96';
                    ctx.strokeRect(x + 2, ry, 90, CHIP_H);
                }
            }
        }

        // the playhead
        const bx = this.boundaryX(this.boundary);
        const row = this.boundaryRow === null ? 0 : this.boundaryRow;
        const ry = this.rowY(this.singleRow ? 1 : row);
        ctx.fillStyle = '#e8832d';
        ctx.fillRect(bx - 1.5, ry - 2, 3, CHIP_H + 4);
    }
}
