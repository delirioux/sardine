// SPDX-License-Identifier: GPL-3.0-only
// The hat audit by numbers (no ears): every kit's HAT and OPEN in every variation, alone on OFFBEAT, and how much it
// rings like metal: TONAL (dB, how far the spectrum's sharpest lines stand above its body between 2 and 16 kHz; a
// noisy hat is smooth, a cluster of squares is lines) and TAIL (dB, its level half a beat after the hit against its
// peak). Run after web/emu/sa_build.sh:   node tools/sa_hat_audit.mjs [build/sa_emu/sardine.wasm] [TONAL_LIMIT]
import fs from "fs";
const bytes = fs.readFileSync(process.argv[2] || "build/sa_emu/sardine.wasm");
const limit = +(process.argv[3] ?? 18);
const stub = new Proxy({}, { get: () => () => 0 });
const { instance } = await WebAssembly.instantiate(bytes, { wasi_snapshot_preview1: stub });
const w = instance.exports;
const str = (p) => { const m = new Uint8Array(w.memory.buffer); let s = ""; while (m[p]) s += String.fromCharCode(m[p++]); return s; };
const SR = 44100, N = 2048;
let buf = [];
const run = (sec, meas) => { for (let i = 0; i < (sec * SR) / 512; i++) { w.web_render(512); if (!meas) continue;
  const L = new Float32Array(w.memory.buffer, w.web_out_l(), 512), R = new Float32Array(w.memory.buffer, w.web_out_r(), 512);
  for (let k = 0; k < 512; k++) buf.push((L[k] + R[k]) / 2); } };
const press = (b) => { w.web_buttons(1 << b); run(0.03); w.web_buttons(0); run(0.03); };
// a radix-2 FFT's magnitudes
const fftMag = (x) => {
  const n = x.length, re = Float64Array.from(x), im = new Float64Array(n);
  for (let i = 1, j = 0; i < n; i++) { let b = n >> 1; for (; j & b; b >>= 1) j ^= b; j ^= b; if (i < j) { [re[i], re[j]] = [re[j], re[i]]; [im[i], im[j]] = [im[j], im[i]]; } }
  for (let len = 2; len <= n; len <<= 1) { const a = -2 * Math.PI / len, wr = Math.cos(a), wi = Math.sin(a);
    for (let i = 0; i < n; i += len) { let cr = 1, ci = 0; for (let k = 0; k < len / 2; k++) {
      const ur = re[i + k], ui = im[i + k], vr = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci, vi = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr;
      re[i + k] = ur + vr; im[i + k] = ui + vi; re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
      const t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t; } } }
  const m = new Float64Array(n / 2); for (let i = 0; i < n / 2; i++) m[i] = Math.hypot(re[i], im[i]); return m; };
w._initialize(); w.web_nor_erase(); w.web_boot(); run(1.0);
const db = (x) => 20 * Math.log10(Math.max(x, 1e-9));
const rows = [];
for (let kit = 0; kit < 24; kit++)
  for (const [dv, dn] of [[3, "HAT"], [4, "OPEN"]])
    for (let v = 0; v < 8; v++) {
      w.web_test_kit(kit); w.web_test_row(3, dv, v, 9999); run(0.2);
      press(10); run(0.5); buf = []; run(3.0, true); press(10); run(0.8);
      // the spectrum, averaged over the take; the band's sharpest lines against its median
      const spec = new Float64Array(N / 2); let frames = 0;
      for (let s = 0; s + N <= buf.length; s += N / 2) { const x = new Float64Array(N); let e = 0;
        for (let i = 0; i < N; i++) { x[i] = buf[s + i] * (0.5 - 0.5 * Math.cos(2 * Math.PI * i / (N - 1))); e += x[i] * x[i]; }
        if (e < 1e-8) continue; const m = fftMag(x); for (let i = 0; i < N / 2; i++) spec[i] += m[i]; frames++; }
      const lo = Math.round(2000 / SR * N), hi = Math.round(16000 / SR * N), band = Array.from(spec.slice(lo, hi)).sort((a, b) => a - b);
      const tonal = db(band[Math.floor(band.length * 0.99)]) - db(band[band.length >> 1]);
      // the tail: 5 ms windows; the level half a beat after each peak
      const win = Math.round(SR * 0.005), env = [];
      for (let s = 0; s + win <= buf.length; s += win) { let e = 0; for (let i = 0; i < win; i++) e += buf[s + i] ** 2; env.push(Math.sqrt(e / win)); }
      const peak = Math.max(...env), bpm = 128, half = Math.round((60 / bpm / 2) / 0.005) - 2;
      let tails = [];
      for (let i = 1; i + half < env.length; i++) if (env[i] > peak * 0.5 && env[i] >= env[i - 1] && env[i] >= env[i + 1]) { tails.push(db(env[i + half]) - db(env[i])); i += half; }
      const tail = tails.length ? tails.reduce((a, b) => a + b) / tails.length : -99;
      rows.push({ kit: str(w.web_kit_name(kit)), dn, v: str(w.web_dvar_name(3, v)) || "-", tonal, tail, peak, frames });
    }
rows.sort((a, b) => b.tonal - a.tonal);
for (const r of rows)
  console.log(`${r.kit.padEnd(8)} ${r.dn.padEnd(5)} ${r.v.padEnd(6)} tonal ${r.tonal.toFixed(1).padStart(5)} dB  tail ${r.tail.toFixed(1).padStart(6)} dB  peak ${r.peak.toFixed(3)}${r.tonal > limit ? "  RINGS" : ""}${r.peak < 0.002 ? "  SILENT" : ""}`);
