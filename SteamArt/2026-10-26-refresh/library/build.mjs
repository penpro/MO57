/** Four Steam library uploads, using captured game pixels and the established wordmark. */
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
let sharp;
try { sharp = require('sharp'); }
catch { sharp = require(process.env.MO57_SHARP_MODULE || 'C:/Users/penum/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp'); }
const ROOT = path.dirname(fileURLToPath(import.meta.url));
const PARENT = path.resolve(ROOT, '..');
const REPO = path.resolve(ROOT, '../../..');
const OUT = path.join(ROOT, 'exports');
for (const d of ['exports', 'sources', 'previews']) fs.mkdirSync(path.join(ROOT, d), { recursive: true });
const sha = b => crypto.createHash('sha256').update(b).digest('hex');
const sourcePath = path.join(ROOT, 'sources/hero_panorama_3840x2160.png');
if (!fs.existsSync(sourcePath)) fs.copyFileSync(path.join(REPO, 'Saved/SteamArt/plates/S2_gold.png'), sourcePath, fs.constants.COPYFILE_EXCL);
const cliffPath = path.join(PARENT, 'masters/03_cliffs_native.png');
const wordmarkPath = path.join(PARENT, 'sources/wordmark_transparent.png');
const headerPath = path.join(PARENT, 'exports/header_capsule_920x430.png');
const mark = fs.readFileSync(wordmarkPath);
const svg = (w,h,s) => Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}">${s}</svg>`);
async function rgb(b, dest) { await sharp(b).removeAlpha().toColourspace('srgb').withIccProfile('srgb').png({ compressionLevel: 9, palette: false }).toFile(dest); }
async function shadow(png, sigma, padding=0) {
  const {data,info}=await sharp(png).ensureAlpha().raw().toBuffer({resolveWithObject:true});
  for(let i=0;i<data.length;i+=4){data[i]=7;data[i+1]=6;data[i+2]=5;data[i+3]=Math.round(data[i+3]*0.82);}
  let image=sharp(data,{raw:info});
  if(padding)image=image.extend({top:padding,bottom:padding,left:padding,right:padding,background:{r:0,g:0,b:0,alpha:0}});
  return image.blur(sigma).png().toBuffer();
}

// Recompose for the narrower 2:3 library format; don't crop the branded store capsule.
const capsuleCrop = { left: 178, top: 0, width: 677, height: 1015 };
const W = 1200, H = 1800;
const base = await sharp(cliffPath).extract(capsuleCrop).resize(W,H,{ fit:'fill', kernel:'lanczos3' }).png().toBuffer();
const lw = 1080, lx = 60, ly = 130;
const capsuleMark = await sharp(mark).resize({width:lw}).png().toBuffer();
const capsuleShadow = await shadow(capsuleMark,2.3,10);
const shade = svg(W,H,`<defs><linearGradient id="t" x2="0" y2="1"><stop stop-color="#0E0D0B" stop-opacity=".69"/><stop offset=".5" stop-color="#0E0D0B" stop-opacity=".3"/><stop offset="1" stop-color="#0E0D0B" stop-opacity="0"/></linearGradient><radialGradient id="v"><stop offset=".45" stop-opacity="0"/><stop offset="1" stop-color="#0E0D0B" stop-opacity=".17"/></radialGradient></defs><rect width="1200" height="1150" fill="url(#t)"/><rect width="1200" height="1800" fill="url(#v)"/>`);
const fullCapsule = await sharp(base).composite([{input:shade},{input:capsuleShadow,left:lx-10,top:ly-8},{input:capsuleMark,left:lx,top:ly}]).png().toBuffer();
await rgb(await sharp(fullCapsule).resize(600,900,{kernel:'lanczos3'}).png().toBuffer(), path.join(OUT,'library_capsule_600x900.png'));
fs.copyFileSync(headerPath, path.join(OUT,'library_header_920x430.png'));

// Native 3840-wide render: no enlargement, no faces near the cropped edges, no text.
const heroCrop = {left:0,top:600,width:3840,height:1240};
const sourceMeta = await sharp(sourcePath).metadata();
if (sourceMeta.width !== 3840 || sourceMeta.height !== 2160) throw Error('Unexpected panorama size');
const panorama = await sharp(sourcePath).extract(heroCrop).modulate({brightness:0.96,saturation:1.025})
  .linear([0.995,1,1.015],[0,0,0]).png().toBuffer();
const heroShade = svg(3840,1240,`<defs><linearGradient id="b" x2="0" y2="1"><stop stop-color="#0E0D0B" stop-opacity=".05"/><stop offset=".48" stop-color="#0E0D0B" stop-opacity="0"/><stop offset="1" stop-color="#0E0D0B" stop-opacity=".38"/></linearGradient><linearGradient id="l"><stop stop-color="#0E0D0B" stop-opacity=".14"/><stop offset=".48" stop-color="#0E0D0B" stop-opacity="0"/></linearGradient></defs><rect width="3840" height="1240" fill="url(#b)"/><rect width="3840" height="1240" fill="url(#l)"/>`);
await rgb(await sharp(panorama).composite([{input:heroShade}]).png().toBuffer(),path.join(OUT,'library_hero_3840x1240.png'));

// Natural wordmark aspect ratio, with a little transparent room for a soft shadow.
const logoMark = await sharp(mark).resize({width:1248}).png().toBuffer();
const logoShadow = await shadow(logoMark,1.9);
await sharp({create:{width:1280,height:368,channels:4,background:{r:0,g:0,b:0,alpha:0}}})
  .composite([{input:logoShadow,left:16,top:19},{input:logoMark,left:16,top:16}])
  .toColourspace('srgb').withIccProfile('srgb').png({compressionLevel:9,palette:false})
  .toFile(path.join(OUT,'library_logo_1280x368.png'));

const safe = {left:1490,top:430,width:860,height:380};
const placement = {anchor:'bottom-left',left:144,top:816,width:1280,height:368};
const hero = fs.readFileSync(path.join(OUT,'library_hero_3840x1240.png'));
const logo = fs.readFileSync(path.join(OUT,'library_logo_1280x368.png'));
await rgb(await sharp(hero).composite([{input:logo,left:placement.left,top:placement.top}]).png().toBuffer(),path.join(ROOT,'previews/hero_with_separate_logo.png'));
const guide = svg(3840,1240,`<rect x="${safe.left}" y="${safe.top}" width="860" height="380" fill="none" stroke="#6CB8A9" stroke-width="5"/><text x="1490" y="410" fill="#EEE7D7" font-family="Arial" font-size="30">CENTRE SAFE AREA / 860 × 380</text>`);
await rgb(await sharp(hero).composite([{input:guide}]).png().toBuffer(),path.join(ROOT,'previews/hero_safe_area.png'));

// Transparency proof is a review file only; exported logo retains its alpha.
const checker = svg(1280,368,`<defs><pattern id="c" width="40" height="40" patternUnits="userSpaceOnUse"><rect width="40" height="40" fill="#282622"/><path d="M0 0h20v20H0zM20 20h20v20H20z" fill="#3A3833"/></pattern></defs><rect width="1280" height="368" fill="url(#c)"/>`);
await rgb(await sharp(checker).composite([{input:logo}]).png().toBuffer(),path.join(ROOT,'previews/logo_on_checkerboard.png'));

const expected = [
  ['library_capsule_600x900.png',600,900,false],['library_header_920x430.png',920,430,false],
  ['library_hero_3840x1240.png',3840,1240,false],['library_logo_1280x368.png',1280,368,true]
];
const exports = [];
for (const [file,width,height,alpha] of expected) {
  const b = fs.readFileSync(path.join(OUT,file));
  const m = await sharp(b).metadata();
  if(m.width!==width||m.height!==height||m.hasAlpha!==alpha||m.channels!==(alpha?4:3)||m.format!=='png'||m.space!=='srgb') throw Error(`Invalid upload: ${file}`);
  exports.push({file,width,height,hasAlpha:m.hasAlpha,channels:m.channels,colourSpace:m.space,bytes:b.length,sha256:sha(b)});
}
const {data:alphaData}=await sharp(logo).raw().toBuffer({resolveWithObject:true});
let transparentPixels=0,visiblePixels=0;
for(let i=3;i<alphaData.length;i+=4){if(alphaData[i]===0)transparentPixels++;if(alphaData[i]>0)visiblePixels++;}
const cornerAlpha=[3,1280*4-1,(368-1)*1280*4+3,alphaData.length-1].map(i=>alphaData[i]);
if(!transparentPixels||!visiblePixels||cornerAlpha.some(a=>a!==0))throw Error('Logo alpha check failed');
const inputs=[];
for(const p of [sourcePath,cliffPath,wordmarkPath,headerPath]){const b=fs.readFileSync(p);inputs.push({file:path.relative(ROOT,p).replaceAll('\\','/'),bytes:b.length,sha256:sha(b)});}
const manifest = {project:'Metaverse: Origins',createdUtc:new Date().toISOString(),
  method:'Conventional crop/colour/compositing only; no generative edits',inputs,exports,
  capsule:{source:'../masters/03_cliffs_native.png',sourceCrop:capsuleCrop,logoWidth:540,logoTop:65},
  header:{source:'../exports/header_capsule_920x430.png',byteIdentical:true},
  hero:{source:'sources/hero_panorama_3840x2160.png',originalSource:'Saved/SteamArt/plates/S2_gold.png',sourceCrop:heroCrop,nativeResolution:true,containsText:false,containsLogo:false,safeArea:safe},
  logo:{source:'../sources/wordmark_transparent.png',transparentPixels,visiblePixels,transparentCorners:true,naturalAspect:true,softShadow:true},
  recommendedPlacement:placement,
  sourceSpec:'User pasted Steam library guidelines, 2026-10-26',officialSpec:'https://partner.steamgames.com/doc/store/assets/libraryassets',
  note:'Guide, checkerboard and hero-with-logo composites are previews only; upload only exports/. Nothing uploaded to Steam.'};
fs.writeFileSync(path.join(ROOT,'manifest.json'),JSON.stringify(manifest,null,2)+'\n');

const pw=1600,ph=1570,panels=[];
async function panel(file,left,top,width){const input=await sharp(path.join(ROOT,file)).resize({width}).png().toBuffer();panels.push({input,left,top});}
await panel('exports/library_capsule_600x900.png',36,112,330);
await panel('exports/library_header_920x430.png',410,112,920);
await panel('previews/logo_on_checkerboard.png',410,630,920);
await panel('previews/hero_with_separate_logo.png',36,990,1528);
const labels=[[36,52,27,'METAVERSE: ORIGINS / LIBRARY ASSETS'],[36,91,17,'CAPSULE / 600 × 900'],[410,91,17,'HEADER / 920 × 430'],
  [410,604,17,'LOGO / 1280 × 368 / TRANSPARENT PNG'],[36,964,17,'HERO / 3840 × 1240 / SEPARATE LOGO PLACEMENT PREVIEW'],
  [36,1530,16,'Actual hero upload has no text or logo. Checkerboard and labels are review aids only.']];
panels.push({input:svg(pw,ph,labels.map(([x,y,s,t])=>`<text x="${x}" y="${y}" font-family="Arial" font-size="${s}" fill="#EEE7D7" letter-spacing="1">${t}</text>`).join(''))});
await sharp({create:{width:pw,height:ph,channels:3,background:'#181612'}}).composite(panels).png({compressionLevel:9}).toFile(path.join(ROOT,'preview.png'));
console.log(JSON.stringify({exportCount:exports.length,exports,logoAlpha:{transparentPixels,visiblePixels},nativeHero:true,preview:path.join(ROOT,'preview.png')}));
