// Renderuje SVG z resources/ikony do PNG 16/32/64 px (resources/ikony/png).
// Qt ładuje PNG bez dodatkowego modułu QtSvg, więc do programu trafiają PNG.
// Użycie: node tools/ikony/renderuj_png.js resources/ikony
const fs = require('fs'), path = require('path'), { chromium } = require('playwright');
const dir = process.argv[2], out = path.join(dir, 'png');
fs.mkdirSync(out, { recursive: true });
(async () => {
    const browser = await chromium.launch();
    const page = await browser.newPage();
    for (const f of fs.readdirSync(dir).filter(f => f.endsWith('.svg'))) {
        const svg = fs.readFileSync(path.join(dir, f), 'utf8');
        const src = 'data:image/svg+xml;base64,' + Buffer.from(svg).toString('base64');
        for (const size of [16, 32, 64]) {
            await page.setViewportSize({ width: size, height: size });
            await page.setContent(`<style>body{margin:0}</style><img src="${src}" width="${size}" height="${size}">`);
            await page.screenshot({ path: path.join(out, `${path.basename(f, '.svg')}_${size}.png`), omitBackground: true });
        }
    }
    await browser.close();
})();
