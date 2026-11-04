// SPDX-License-Identifier: GPL-3.0-only
// SARDINE in Node, as the browser's worklet runs it (build/sa_emu/sardine.wasm, web/emu/sa_web.c). After Sardine's test.
//   node web/emu/sa_emu_test.mjs build/sa_emu/sardine.wasm
// Checks: it boots stopped and silent on PLAY; PLAY plays the groove (sound, the bars moving, PLAY's LED); the
// pages open; PLAY again stops to silence; a setting changed while stopped reaches the flash.
import fs from "fs";

const bytes = fs.readFileSync(process.argv[2] || "build/sa_emu/sardine.wasm");
const stub = new Proxy({}, { get: () => () => 0 });   // wasi imports the reactor never calls
const { instance } = await WebAssembly.instantiate(bytes, { wasi_snapshot_preview1: stub });
const w = instance.exports;
const B = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12, OCTUP: 13 };
const EN = { SELECT: 0, ALGO: 1, PRESETS: 2, K1: 3, K2: 4, K3: 5, K4: 6 };
let fails = 0;
const check = (what, ok) => { console.log(`sardine emu: ${what.padEnd(60)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const str = (p) => { const m = new Uint8Array(w.memory.buffer); let s = ""; while (m[p]) s += String.fromCharCode(m[p++]); return s; };

w._initialize();
w.web_nor_erase();
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
check(`the first groove is in (${w.web_da_style()} layers)`, w.web_da_style() === 5);
check("silent while stopped", peak < 1e-3);

press(B.PLAY);
peak = 0;
run(8);
check(`PLAY plays (peak ${peak.toFixed(3)}, bar ${w.web_da_bar()})`, w.web_playing() && peak > 0.02 && peak <= 1 && w.web_da_bar() >= 3);
check("PLAY's green LED lit", (w.web_lit_buttons() >> 14) & 1);

// SELECT to a layer, ARP opens its page, SCL the KEY page, HOME back to MAIN
const pages = [];
w.web_enc(EN.SELECT, 2); run(0.2); pages.push(w.web_da_page());   // MAIN, DRUMS, the bass layer: LAYER
press(B.ARP); pages.push(w.web_da_page());                          // ARP
press(B.SCL); pages.push(w.web_da_page());                          // KEY
check(`SELECT, ARP, SCL open LAYER, ARP, KEY (${pages.join(" ")})`, pages[0] === 1 && pages[1] === 2 && pages[2] === 3);
press(B.HOME);
run(0.5);
check("HOME returns to MAIN", w.web_da_page() === 0);
w.web_enc(EN.SELECT, 1); run(0.2);
check("SELECT after MAIN: DRUMS", w.web_da_page() === 5);
press(B.HOME); run(0.3);

press(B.PLAY);
run(4);
peak = 0;
run(1);
check(`PLAY again stops (tail peak ${peak.toFixed(4)})`, !w.web_playing() && peak < 1e-3);

const writes = w.web_flash_writes_count();
w.web_enc(EN.K1, 2);  // TEMPO
run(3);
check("a setting changed while stopped reaches the flash", w.web_flash_writes_count() > writes);

console.log(fails ? `sardine emu: ${fails} FAILED` : "sardine emu: all passed");
process.exit(fails ? 1 : 0);
