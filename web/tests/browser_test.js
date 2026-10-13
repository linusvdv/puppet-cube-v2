// Step 1 browser verification: drives the debug page (web/debug.html) with
// headless chromium through the full flow - coi service worker isolation,
// wasm generation, scramble solve, custom position solve - and asserts the
// depths. Run from the repo root:
//   node web/browser_test.js
// (serves web/ on localhost:8123 with python3 -m http.server)

const { chromium } = require('playwright');
const { spawn } = require('child_process');
const path = require('path');

const PORT = 8123;
const URL = `http://localhost:${PORT}/`;

function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

async function main() {
    // serve the web directory (the coi service worker needs http(s))
    const server = spawn('python3', ['-m', 'http.server', String(PORT)],
                         {cwd: path.join(__dirname, '..'), stdio: 'ignore'});
    await sleep(1000);

    const browser = await chromium.launch({
        args: ['--enable-unsafe-swiftshader'],   // software webgl for the headless shell
    });
    const page = await browser.newPage();
    page.on('console', msg => {
        const text = msg.text();
        if (!text.startsWith('[stage]')) console.log('  [browser]', text.slice(0, 160));
    });
    page.on('pageerror', err => console.log('  [pageerror]', String(err).slice(0, 300)));

    try {
        await page.goto(URL + 'debug.html', {waitUntil: 'load'});
        // the coi service worker reloads the page once on the first visit
        await page.waitForTimeout(2500);
        const isolated = await page.evaluate(() => crossOriginIsolated);
        console.log('crossOriginIsolated:', isolated);
        if (!isolated) throw new Error('NOT cross origin isolated - the service worker failed');

        // generate the tables (takes a minute or two)
        console.log('starting generation ...');
        await page.click('#init');
        try {
            await page.waitForFunction(() => {
                const log = document.getElementById('log').textContent;
                return log.includes('initialization complete') || log.includes('error');
            }, null, {timeout: 600000});
        } catch (e) {
            console.log('--- page log on timeout ---');
            console.log(await page.textContent('#log'));
            throw e;
        }
        const pageLog = await page.textContent('#log');
        if (pageLog.includes('error') && !pageLog.includes('initialization complete')) {
            throw new Error('generation failed: ' + pageLog.slice(-500));
        }
        const heap = await page.textContent('#heap');
        console.log('generation done, heap:', heap);

        // run one scramble search (num_runs is 4 in the page args - we just
        // wait for the first solution)
        await page.click('#run');
        await page.waitForFunction(() => {
            const log = document.getElementById('log').textContent;
            return /depth \d+/.test(log);
        }, null, {timeout: 600000});
        const runLog = await page.textContent('#log');
        const firstDepth = runLog.match(/depth (\d+)/)[1];
        console.log('first scramble solved, depth:', firstDepth);

        // solve the test position (run 0 painted as placements)
        if (await page.evaluate(() => !document.getElementById('solve').disabled)) {
            await page.click('#solve');
            await page.waitForFunction(() => {
                const log = document.getElementById('log').textContent;
                const matches = log.match(/depth (\d+)/g);
                return matches && matches.length >= 2;
            }, null, {timeout: 600000});
            const solveLog = await page.textContent('#log');
            const depths = solveLog.match(/depth (\d+)/g);
            console.log('custom position solved:', depths[depths.length - 1]);
            if (!depths[depths.length - 1].includes('18')) {
                throw new Error('custom depth mismatch - expected 18');
            }
        } else {
            console.log('(solve button disabled - no test data)');
        }

        console.log('BROWSER TEST PASSED');
    } finally {
        await browser.close();
        server.kill();
    }
}

main().catch(err => { console.error(err); process.exit(1); });
