// SPDX-License-Identifier: GPL-3.0-only
// SARDINE in Node, as the browser's worklet runs it (build/sa_emu/sardine.wasm, web/emu/sa_web.c). After Sardine's test.
//   node web/emu/sa_emu_test.mjs build/sa_emu/sardine.wasm
// Checks: it boots stopped and silent on PLAY; PLAY plays the groove (sound, the bars moving, PLAY's LED); the
// pages open; PLAY again stops to silence; SETS stores a groove in the flash; the next start has another groove and
// the set still there.
import fs from "fs";

const bytes = fs.readFileSync(process.argv[2] || "build/sa_emu/sardine.wasm");
const stub = new Proxy({}, { get: () => () => 0 });   // wasi imports the reactor never calls
const { instance } = await WebAssembly.instantiate(bytes, { wasi_snapshot_preview1: stub });
let w = instance.exports;
const B = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12, OCTUP: 13 };
const EN = { SELECT: 0, ALGO: 1, PRESETS: 2, K1: 3, K2: 4, K3: 5, K4: 6 };
let fails = 0;
const check = (what, ok) => { console.log(`sardine emu: ${what.padEnd(60)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const str = (p) => { const m = new Uint8Array(w.memory.buffer); let s = ""; while (m[p]) s += String.fromCharCode(m[p++]); return s; };

w._initialize();
w.web_nor_erase();
// another firmware's project in the first sets' object (Felucca's slot B, type 2), its sequence high
const crc32 = (b) => { let c = ~0; for (const x of b) { c ^= x; for (let k = 0; k < 8; k++) c = c & 1 ? (c >>> 1) ^ 0xEDB88320 : c >>> 1; } return ~c >>> 0; };
{
  const nor = new Uint8Array(w.memory.buffer, w.web_nor(), w.web_nor_size()), base = 0x9A000;   // its copy B
  const body = new Uint8Array(64).fill(7), h = new DataView(new ArrayBuffer(32));
  h.setUint32(0, 0x554C4546, true); h.setUint16(4, 2, true); h.setUint16(6, 1, true);
  h.setUint32(8, 100, true); h.setUint32(12, body.length, true); h.setUint32(16, crc32(body), true);
  h.setUint32(20, 0xFFFFFFFF, true); h.setUint32(24, 0xFFFFFFFF, true);
  h.setUint32(28, crc32(new Uint8Array(h.buffer, 0, 28)), true);
  nor.set(new Uint8Array(h.buffer), base);
  nor.set(body, base + 256);
}
w.web_boot();
let peak = 0;
const run = (sec) => {
  for (let i = 0; i < (sec * 44100) / 512; i++) {
    w.web_render(512);
    for (const v of new Float32Array(w.memory.buffer, w.web_out_l(), 512)) peak = Math.max(peak, Math.abs(v));
  }
};
const press = (b) => { w.web_buttons(1 << b); run(0.05); w.web_buttons(0); run(0.05); };

run(2);
check(`boots Sardine ${str(w.web_da_version())} on MAIN, stopped`, w.web_da_page() === 0 && !w.web_playing());
check(`a fresh groove is in (${w.web_da_style()} layers)`, w.web_da_style() === 10);   // LEAD CHORD BASS and the seven drum rows
const groove1 = w.web_da_groove();
check("silent while stopped", peak < 1e-3);

press(B.PLAY);
peak = 0;
run(8);
check(`PLAY plays (peak ${peak.toFixed(3)}, bar ${w.web_da_bar()})`, w.web_playing() && peak > 0.02 && peak <= 1 && w.web_da_bar() >= 3);
check("PLAY's green LED lit", (w.web_lit_buttons() >> 14) & 1);

// PRESETS to a layer, FX through its pages, SCL the KEY page, HOME back; SELECT is the tempo
const pages = [];
w.web_enc(EN.PRESETS, 2); run(0.2); pages.push(w.web_da_page(), w.web_da_view());   // CHORD: LAYER
press(B.FX); pages.push(w.web_da_fx());                                                // its SOUND page
press(B.SCL); pages.push(w.web_da_page());                                             // KEY
check(`PRESETS, FX, SCL: CHORD, SOUND, KEY (${pages.join(" ")})`, pages[0] === 1 && pages[1] === 2 && pages[2] === 1 && pages[3] === 2);
press(B.HOME);
run(0.5);
check("HOME returns HOME", w.web_da_page() === 0);
const bpm = w.web_da_bpm();
w.web_enc(EN.SELECT, 1); run(0.2);
check("SELECT: the tempo", w.web_da_bpm() === bpm + 1 && w.web_da_page() === 0);
press(B.HOME); run(0.3);

press(B.PLAY);
run(4);
peak = 0;
run(1);
check(`PLAY again stops (tail peak ${peak.toFixed(4)})`, !w.web_playing() && peak < 1e-3);

const writes = w.web_flash_writes_count();
w.web_enc(EN.K1, -2);  // FILTER
run(4);
check("a setting changed, stopped: LAST keeps it", w.web_flash_writes_count() > writes && w.web_da_set_used(8) === 1);
const groove1b = w.web_da_groove();
press(B.SAVE);
check("SAVE opens SETS", w.web_da_page() === 4);
w.web_enc(EN.K2, 1); run(0.1);  // STORE
w.web_enc(EN.K1, 2); run(0.1);  // set 3
press(B.REC); run(0.2);
check("REC: STORE writes set 3", w.web_da_set_used(2) && !w.web_da_set_used(0));
w.web_enc(EN.K1, -2); run(0.1);  // set 1 (over the old project)
press(B.REC); run(0.2);
w.web_enc(EN.K1, 2); run(0.1);
const nor = new Uint8Array(w.memory.buffer, w.web_nor(), w.web_nor_size()).slice();
{
  const again = await WebAssembly.instantiate(bytes, { wasi_snapshot_preview1: stub });
  w = again.instance.exports;
  w._initialize();
  new Uint8Array(w.memory.buffer, w.web_nor(), w.web_nor_size()).set(nor);
  w.web_boot();
  run(1);
  check("the next start: another groove", w.web_da_groove() !== groove1 && w.web_da_style() === 10);
  check("the next start: sets 1 and 3 still there", w.web_da_set_used(2) === 1 && w.web_da_set_used(0) === 1);
  press(B.SAVE);
  w.web_enc(EN.K1, 2); run(0.1);
  press(B.REC); run(0.2);
  check("REC: LOAD brings set 3 back", w.web_da_groove() === groove1);
  w.web_enc(EN.K1, 9); run(0.1);
  press(B.REC); run(0.2);
  check("REC: LOAD LAST brings the last groove back", w.web_da_groove() === groove1b);
  press(B.SAVE); run(0.2);                                   // (back from SETS)
  press(B.GLO); press(B.FX); press(B.FX); press(B.FX);       // the MIXER's STYLE page
  w.web_enc(EN.K1, -20); run(0.1); w.web_enc(EN.K1, 7); run(0.1);   // the 8th style: GABBER (170..180)
  const g0 = w.web_da_groove();
  press(B.SEQ); run(0.3);
  check(`STYLE: SEQ, a groove in it (${w.web_da_bpm()} BPM)`, w.web_da_groove() !== g0 && w.web_da_bpm() >= 170 && w.web_da_bpm() <= 180);
}

console.log(fails ? `sardine emu: ${fails} FAILED` : "sardine emu: all passed");
process.exit(fails ? 1 : 0);
