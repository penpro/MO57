/** Deterministic Steam export: captured pixels + existing wordmark; no generative edits. */
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
let sharp;
try { sharp = require('sharp'); }
catch {
  sharp = require(process.env.MO57_SHARP_MODULE ||
    'C:/Users/penum/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');
}
const ROOT = path.dirname(fileURLToPath(import.meta.url));
const REPO = path.resolve(ROOT, '../..');
const OUT = path.join(ROOT, 'exports');
const SRC = path.join(ROOT, 'sources');
const MASTER = path.join(ROOT, 'masters');
for (const dir of [OUT, SRC, MASTER, path.join(OUT, 'screenshots'), path.join(ROOT, 'previews')])
  fs.mkdirSync(dir, { recursive: true });
const hash = b => crypto.createHash('sha256').update(b).digest('hex');
const shots = [
  ['01_rocky_coast', '30327a36-f1de-4d44-8028-cd063ad72574'],
  ['02_river', '602b4ad1-6aa4-4266-b1eb-1d76e78370d1'],
  ['03_cliffs', 'c74a8fb3-8c7d-451a-bc8f-a6125994996b'],
  ['04_beach', '24b2dcd5-aafa-4622-b56e-50739871de2d'],
  ['05_night', '7fd3c42c-95ff-4ee7-9ab1-f4e8c6f8d6d4'],
  ['06_fog_forest', '58f8dc4b-c3a1-4f43-aa75-c57e1753aaf8'],
];
function copyNew(source, dest) {
  if (fs.existsSync(dest)) {
    if (fs.existsSync(source) && hash(fs.readFileSync(dest)) !== hash(fs.readFileSync(source)))
      throw new Error(`Source copy differs: ${dest}`);
    return;
  }
  fs.copyFileSync(source, dest, fs.constants.COPYFILE_EXCL);
}
for (const [name, id] of shots)
  copyNew(`C:/Users/penum/AppData/Local/Temp/codex-clipboard-${id}.jpg`, path.join(SRC, `${name}.jpg`));
copyNew(path.join(REPO, 'SteamArt/wordmark_transparent.png'), path.join(SRC, 'wordmark_transparent.png'));

const kitPath = path.join(REPO, 'DesignKit/MO57_UI_Kit/MO57_UI_Kit/tokens.json');
const tokens = JSON.parse(fs.readFileSync(kitPath, 'utf8'));
const grades = {
  '03_cliffs': { ev: 0.04, gamma: 0.965, contrast: 0.06, saturation: 1.035, gains: [1.005, 1, 0.998] },
  '04_beach': { ev: 0.025, gamma: 1, contrast: 0.06, saturation: 1.025, gains: [1.003, 1, 0.998] },
  '05_night': { ev: 0.12, gamma: 0.92, contrast: 0.02, saturation: 1.02, gains: [1, 1, 1] },
};
const clamp = v => Math.min(1, Math.max(0, v));
async function grade(source, recipe) {
  const { data, info } = await sharp(source).toColourspace('srgb').removeAlpha().raw()
    .toBuffer({ resolveWithObject: true });
  const lut = Array.from({ length: 256 }, (_, i) => {
    let v = Math.pow(i / 255, recipe.gamma) * Math.pow(2, recipe.ev);
    v += recipe.contrast * v * (1 - v) * (2 * v - 1);
    return clamp(v);
  });
  for (let i = 0; i < data.length; i += 3) {
    let r = lut[data[i]], g = lut[data[i + 1]], b = lut[data[i + 2]];
    const l = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    r = (l + (r - l) * recipe.saturation) * recipe.gains[0];
    g = (l + (g - l) * recipe.saturation) * recipe.gains[1];
    b = (l + (b - l) * recipe.saturation) * recipe.gains[2];
    data[i] = Math.round(clamp(r) * 255);
    data[i + 1] = Math.round(clamp(g) * 255);
    data[i + 2] = Math.round(clamp(b) * 255);
  }
  return sharp(data, { raw: info }).png().toBuffer();
}
async function saveRgb(buffer, dest) {
  await sharp(buffer).removeAlpha().toColourspace('srgb').withIccProfile('srgb')
    .png({ compressionLevel: 9, palette: false }).toFile(dest);
}
const masters = {};
for (const [name, recipe] of Object.entries(grades)) {
  masters[name] = await grade(path.join(SRC, `${name}.jpg`), recipe);
  await saveRgb(masters[name], path.join(MASTER, `${name}_native.png`));
}

const screenshots = [];
for (const [order, name] of ['03_cliffs', '04_beach', '05_night'].entries()) {
  const m = await sharp(masters[name]).metadata();
  const units = Math.floor(Math.min(m.width / 16, m.height / 9));
  const rect = { left: Math.floor((m.width - 16 * units) / 2),
    top: Math.floor((m.height - 9 * units) / 2), width: 16 * units, height: 9 * units };
  const buffer = await sharp(masters[name]).extract(rect).resize(1920, 1080, { kernel: 'lanczos3' })
    .sharpen({ sigma: 0.5, m1: 0.35, m2: 0.7 }).png().toBuffer();
  const file = `screenshots/${String(order + 1).padStart(2, '0')}_${name.slice(3)}_1920x1080.png`;
  await saveRgb(buffer, path.join(OUT, file));
  screenshots.push({ source: `${name}.jpg`, file, nativeSize: [m.width, m.height],
    crop: rect, outputSize: [1920, 1080], resizeScale: 1920 / rect.width,
    grade: grades[name], sharpening: { sigma: 0.5, m1: 0.35, m2: 0.7 },
    edits: 'Existing pixel values only: tonal curve, saturation, crop, Lanczos resize, mild sharpening.' });
}

async function cover(source, width, height, focus = [0.5, 0.5]) {
  const m = await sharp(source).metadata();
  const scale = Math.max(width / m.width, height / m.height);
  const w = Math.min(m.width, Math.round(width / scale));
  const h = Math.min(m.height, Math.round(height / scale));
  const rect = { left: Math.max(0, Math.min(m.width - w, Math.round(focus[0] * m.width - w / 2))),
    top: Math.max(0, Math.min(m.height - h, Math.round(focus[1] * m.height - h / 2))), width: w, height: h };
  return { buffer: await sharp(source).extract(rect).resize(width, height, { fit: 'fill', kernel: 'lanczos3' })
    .png().toBuffer(), rect };
}
const svg = (w, h, body) => Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}">${body}</svg>`);
const logo = fs.readFileSync(path.join(SRC, 'wordmark_transparent.png'));
const capsuleSpecs = [
  { file: 'main_capsule_1232x706.png', size: [1232, 706], source: '03_cliffs',
    focus: [0.5, 0.5], logoWidth: 0.76, logoTop: 0.052, topOpacity: 0.68 },
  { file: 'header_capsule_920x430.png', size: [920, 430], source: '03_cliffs',
    focus: [0.5, 0.41], logoWidth: 0.74, logoTop: 0.05, topOpacity: 0.71 },
  { file: 'small_capsule_462x174.png', size: [462, 174], source: '05_night',
    focus: [0.5, 0.36], logoWidth: 0.93, logoTop: 0.17, topOpacity: 0.18, padOpacity: 0.58 },
  { file: 'vertical_capsule_748x896.png', size: [748, 896], source: '03_cliffs',
    focus: [0.356, 0.5], logoWidth: 0.88, logoTop: 0.075, topOpacity: 0.68 },
];
const capsules = [];
for (const spec of capsuleSpecs) {
  const [w, h] = spec.size;
  const base = await cover(masters[spec.source], w * 2, h * 2, spec.focus);
  const W = w * 2, H = h * 2;
  const lw = Math.round(W * spec.logoWidth);
  const mark = await sharp(logo).resize({ width: lw }).png().toBuffer();
  const lm = await sharp(mark).metadata();
  const lx = Math.round((W - lw) / 2), ly = Math.round(H * spec.logoTop);
  const shade = svg(W, H, `<defs>
    <linearGradient id="t" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#0E0D0B" stop-opacity="${spec.topOpacity}"/><stop offset="0.5" stop-color="#0E0D0B" stop-opacity="${spec.topOpacity * 0.38}"/><stop offset="1" stop-color="#0E0D0B" stop-opacity="0"/></linearGradient>
    <radialGradient id="p"><stop offset="0" stop-color="#0E0D0B" stop-opacity="${spec.padOpacity ?? 0.18}"/><stop offset="1" stop-color="#0E0D0B" stop-opacity="0"/></radialGradient>
    <radialGradient id="v"><stop offset="0.48" stop-opacity="0"/><stop offset="1" stop-color="#0E0D0B" stop-opacity="0.16"/></radialGradient></defs>
    <rect width="${W}" height="${H * 0.72}" fill="url(#t)"/>
    <ellipse cx="${W / 2}" cy="${ly + lm.height / 2}" rx="${W * 0.59}" ry="${lm.height * 0.95}" fill="url(#p)"/>
    <rect width="${W}" height="${H}" fill="url(#v)"/>`);
  const shadow = await sharp(mark).tint('#070605').ensureAlpha()
    .extend({ top: 12, bottom: 12, left: 12, right: 12, background: { r: 0, g: 0, b: 0, alpha: 0 } })
    .blur(2.3).png().toBuffer();
  const fullSize = await sharp(base.buffer).composite([
    { input: shade }, { input: shadow, left: lx - 12, top: ly - 10 }, { input: mark, left: lx, top: ly },
  ]).png().toBuffer();
  const composed = await sharp(fullSize).resize(w, h, { kernel: 'lanczos3' }).png().toBuffer();
  await saveRgb(composed, path.join(OUT, spec.file));
  capsules.push({ ...spec, sourceCrop: base.rect, logoPixelBounds: { left: lx / 2, top: ly / 2,
    width: lw / 2, height: lm.height / 2 }, onlyText: 'METAVERSE / ORIGINS (existing wordmark)' });
}

// Ambient background: the right-hand forest crop excludes the person by framing.
const bgRect = { left: 650, top: 200, width: 1091, height: 615 };
const ambient = await sharp(path.join(SRC, '06_fog_forest.jpg')).extract(bgRect)
  .resize(1438, 810, { kernel: 'lanczos3' }).modulate({ brightness: 0.63, saturation: 0.58 })
  .linear(0.67, 5).blur(2.1).png().toBuffer();
const ambientShade = svg(1438, 810, `<defs><radialGradient id="a"><stop offset="0" stop-color="#0E0D0B" stop-opacity="0.05"/><stop offset="1" stop-color="#0E0D0B" stop-opacity="0.32"/></radialGradient></defs><rect width="1438" height="810" fill="url(#a)"/>`);
await saveRgb(await sharp(ambient).composite([{ input: ambientShade }]).png().toBuffer(),
  path.join(OUT, 'page_background_1438x810.png'));

const exports = [];
for (const file of [...capsuleSpecs.map(x => x.file), 'page_background_1438x810.png', ...screenshots.map(x => x.file)]) {
  const b = fs.readFileSync(path.join(OUT, file));
  const m = await sharp(b).metadata();
  const expected = screenshots.find(x => x.file === file)?.outputSize ||
    capsuleSpecs.find(x => x.file === file)?.size || [1438, 810];
  if (m.width !== expected[0] || m.height !== expected[1] || m.hasAlpha || m.channels !== 3 || m.format !== 'png')
    throw new Error(`Invalid export: ${file}`);
  exports.push({ file, width: m.width, height: m.height, channels: m.channels, hasAlpha: m.hasAlpha,
    colourSpace: m.space, bytes: b.length, sha256: hash(b) });
}
const inputs = [];
for (const [name] of shots) {
  const b = fs.readFileSync(path.join(SRC, `${name}.jpg`));
  const m = await sharp(b).metadata();
  inputs.push({ file: `sources/${name}.jpg`, width: m.width, height: m.height, bytes: b.length, sha256: hash(b) });
}
const manifest = {
  project: 'Metaverse: Origins', createdUtc: new Date().toISOString(), pipeline: 'Deterministic Sharp image processing; no AI image generation/repainting',
  userDirection: 'Yes—preserve actual gameplay (Recommended)', inputs, screenshots, capsules,
  background: { source: '06_fog_forest.jpg', crop: bgRect, outputSize: [1438, 810], text: false },
  branding: { source: 'SteamArt/wordmark_transparent.png', copiedSource: 'sources/wordmark_transparent.png',
    sha256: hash(logo), font: 'Existing Sitka Display wordmark; reused without redrawing',
    kit: 'DesignKit/MO57_UI_Kit/MO57_UI_Kit', kitDisplayFont: 'Bitter (specified, not bundled)',
    palette: { cream: tokens['color.text.primary'], iron: tokens['color.surface.0'], verdigris: tokens['color.accent'], highlight: tokens['color.accent.hover'] } },
  steamReferencesChecked: '2026-10-26', steamSources: ['https://partner.steamgames.com/doc/store/assets/standard', 'https://partner.steamgames.com/doc/store/assets/rules'],
  exports,
};
fs.writeFileSync(path.join(ROOT, 'manifest.json'), JSON.stringify(manifest, null, 2) + '\n');

// Review board: labels live here only, never in the upload artwork.
const PW = 1680, PH = 1670;
const panels = [];
async function panel(file, left, top, width) {
  const b = await sharp(path.join(OUT, file)).resize({ width }).png().toBuffer();
  panels.push({ input: b, left, top });
}
await panel('main_capsule_1232x706.png', 36, 108, 1010);
await panel('vertical_capsule_748x896.png', 1090, 108, 550);
await panel('header_capsule_920x430.png', 36, 760, 920);
await panel('small_capsule_462x174.png', 1090, 865, 462);
await panel('page_background_1438x810.png', 36, 1240, 500);
for (const [i, file] of screenshots.map(x => x.file).entries())
  await panel(file, 570 + i * 358, 1240, 340);
for (const [i, width] of [231, 184, 120].entries())
  await panel('small_capsule_462x174.png', 1090 + (i === 0 ? 0 : i === 1 ? 255 : 455), 1090, width);
const labels = [
  [36, 52, 27, 'METAVERSE: ORIGINS / STEAM ART REFRESH'],
  [36, 89, 17, 'MAIN / 1232 × 706'], [1090, 89, 17, 'VERTICAL / 748 × 896'],
  [36, 735, 17, 'HEADER / 920 × 430'], [1090, 840, 17, 'SMALL / 462 × 174'],
  [1090, 1072, 15, 'Small capsule at 231 / 184 / 120 px'],
  [36, 1215, 17, 'PAGE BACKGROUND / 1438 × 810'],
  [570, 1215, 17, 'SCREENSHOTS / EACH 1920 × 1080'],
  [570, 1465, 15, '01 / CLIFFS'], [928, 1465, 15, '02 / COASTLINE'], [1286, 1465, 15, '03 / NIGHT SKY'],
  [36, 1600, 17, 'Captured gameplay, carefully graded. Existing cream-and-verdigris wordmark. No AI repainting.'],
];
const labelSvg = svg(PW, PH, labels.map(([x,y,size,t]) => `<text x="${x}" y="${y}" fill="#EEE7D7" font-family="Arial,sans-serif" font-size="${size}" letter-spacing="1">${t}</text>`).join(''));
panels.push({ input: labelSvg });
await sharp({ create: { width: PW, height: PH, channels: 3, background: '#181612' } })
  .composite(panels).png({ compressionLevel: 9 }).toFile(path.join(ROOT, 'preview.png'));

// Same-crop before/after proof for the screenshot grade.
const comparison = [];
const CW = 1440, CH = 1272;
for (const [i, record] of screenshots.entries()) {
  const raw = await sharp(path.join(SRC, record.source)).extract(record.crop).resize(696, 392).png().toBuffer();
  const after = await sharp(path.join(OUT, record.file)).resize(696, 392).png().toBuffer();
  comparison.push({ input: raw, left: 12, top: 30 + i * 420 }, { input: after, left: 732, top: 30 + i * 420 });
}
comparison.push({ input: svg(CW, CH, `<text x="14" y="22" font-family="Arial" font-size="16" fill="#EEE7D7">BEFORE / SAME CROP</text><text x="734" y="22" font-family="Arial" font-size="16" fill="#EEE7D7">AFTER / EXPOSURE, COLOUR, SHARPENING ONLY</text>`) });
await sharp({ create: { width: CW, height: CH, channels: 3, background: '#181612' } }).composite(comparison)
  .png({ compressionLevel: 9 }).toFile(path.join(ROOT, 'previews/before_after.png'));
console.log(JSON.stringify({ outputDirectory: OUT, exportCount: exports.length, exports, preview: path.join(ROOT, 'preview.png') }));
