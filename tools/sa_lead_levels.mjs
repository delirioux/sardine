// SPDX-License-Identifier: GPL-3.0-only
// The lead audition by numbers (no ears): every LEAD sound alone on the CHORD layer (the LEAD waits a pattern) with a lead pattern, playing, its
// RMS and peak, and the trim that would bring it to the leads' median. Run after web/emu/sa_build.sh:
//   node tools/sa_lead_levels.mjs [build/sa_emu/sardine.wasm] [PATTERN]
import fs from "fs";
const bytes = fs.readFileSync(process.argv[2] || "build/sa_emu/sardine.wasm");
const stub = new Proxy({}, { get: () => () => 0 });
const { instance } = await WebAssembly.instantiate(bytes, { wasi_snapshot_preview1: stub });
const w = instance.exports;
const str = (p) => { const m = new Uint8Array(w.memory.buffer); let s = ""; while (m[p]) s += String.fromCharCode(m[p++]); return s; };
const PLAY = 10;
let peak = 0, sq = 0, n = 0, bad = false;
const run = (sec, meas) => {
  for (let i = 0; i < (sec * 44100) / 512; i++) {
    w.web_render(512);
    if (!meas) continue;
    const L = new Float32Array(w.memory.buffer, w.web_out_l(), 512), R = new Float32Array(w.memory.buffer, w.web_out_r(), 512);
    for (let k = 0; k < 512; k++) { const v = (L[k] + R[k]) / 2; if (!Number.isFinite(v)) bad = true; peak = Math.max(peak, Math.abs(L[k]), Math.abs(R[k])); sq += v * v; n++; }
  }
};
const press = (b) => { w.web_buttons(1 << b); run(0.03); w.web_buttons(0); run(0.03); };
w._initialize(); w.web_nor_erase(); w.web_boot(); run(1.0);
const patName = process.argv[3] || "RIFF";
let pat = -1;
for (let i = 0; i < w.web_pat_count(); i++) if (str(w.web_pat_name(i)) === patName) pat = i;
if (pat < 0) { console.log("no pattern " + patName); process.exit(1); }
const res = [];
for (let s = 0; s < w.web_sound_count(); s++) {
  if (w.web_sound_role(s) !== 3) continue;
  w.web_test_solo(1, s, pat); run(0.2);
  press(PLAY); peak = 0; sq = 0; n = 0; bad = false;
  run(1.0, false); run(4.0, true);
  press(PLAY); run(1.5);
  res.push({ name: str(w.web_sound_name(s)), rms: Math.sqrt(sq / Math.max(n, 1)), peak, bad });
}
const db = (x) => 20 * Math.log10(Math.max(x, 1e-9));
const med = res.map((r) => db(r.rms)).sort((a, b) => a - b)[res.length >> 1];
for (const r of res)
  console.log(`${r.name.padEnd(12)} rms ${db(r.rms).toFixed(1).padStart(6)} dB  peak ${r.peak.toFixed(3)}  trim ${(2 * (med - db(r.rms))).toFixed(0).padStart(4)} (half-dB)${r.bad ? " NaN" : ""}${r.peak < 0.005 ? " SILENT" : ""}${r.peak > 0.99 ? " CLIP" : ""}`);
