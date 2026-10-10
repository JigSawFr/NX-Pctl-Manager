// Renders branding/*.svg to the raster files the build and the stores use:
//   icon.jpg (256x256, NRO icon), images/store/icon.png (256x150),
//   images/store/banner.png (848x208), images/social-preview.png (1280x640, the
//   repository's social preview, uploaded by hand in Settings › General).
// Needs Node + Playwright (Chromium); loads the Sora font from Google Fonts.
//   node tools/render_branding.mjs
import { chromium } from 'playwright';
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const jobs = [
  { svg: 'branding/icon.svg',       out: 'icon.jpg',                w: 256, h: 256, type: 'jpeg' },
  { svg: 'branding/store-icon.svg', out: 'images/store/icon.png',   w: 256, h: 150, type: 'png' },
  { svg: 'branding/banner.svg',     out: 'images/store/banner.png', w: 848, h: 208, type: 'png' },
  { svg: 'branding/social-preview.svg', out: 'images/social-preview.png', w: 1280, h: 640, type: 'png' },
];

const browser = await chromium.launch(process.env.CHROMIUM_PATH ? { executablePath: process.env.CHROMIUM_PATH } : {});
const page = await browser.newPage({ deviceScaleFactor: 1 });
for (const job of jobs) {
  const svg = readFileSync(join(root, job.svg), 'utf8');
  await page.setViewportSize({ width: job.w, height: job.h });
  await page.setContent(`<!doctype html><html><head>
<link href="https://fonts.googleapis.com/css2?family=Sora:wght@400;700&display=swap" rel="stylesheet">
<style>html,body{margin:0;overflow:hidden}svg{display:block}</style></head><body>${svg}</body></html>`,
    { waitUntil: 'networkidle' });
  await page.evaluate(async () => {
    await document.fonts.load('700 30px Sora');
    await document.fonts.load('400 20px Sora');
    await document.fonts.ready;
  });
  const ok = await page.evaluate(() => document.fonts.check('700 30px Sora'));
  if (!ok) throw new Error('Sora font did not load');
  await page.screenshot({ path: join(root, job.out), type: job.type, quality: job.type === 'jpeg' ? 95 : undefined,
                          clip: { x: 0, y: 0, width: job.w, height: job.h } });
  console.log(`${job.svg} -> ${job.out}`);
}
await browser.close();
