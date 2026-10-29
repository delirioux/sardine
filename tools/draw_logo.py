#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Sardine's logo as SVG (assets/logo.svg): render it at 1024 x 1024 to assets/logo.png (any browser, rsvg-convert or
cairosvg).
Usage: draw_logo.py [ACCENT_COLOUR] [OUT.svg]"""
# The look (family style):
# black ground, a ring, audio bars, a line-art fish in one accent colour + cream,
# music notes, a little control box, a wide italic name.
# Sardine's own touches: acid lime accent with an ultraviolet second colour (club lights);
# pulse rings and four-on-the-floor bars; a slim silver sardine with its row of dark spots and
# a small shoal swimming behind it; the control box is an opened sardine tin drawn in 3D, its lid
# rolled back on the key, with four 3D knobs and the PLAY and REC buttons on top.
import math, sys

ACC = sys.argv[1] if len(sys.argv) > 1 else "#c8ff1e"   # the accent colour (acid lime)
OUT = sys.argv[2] if len(sys.argv) > 2 else str(__import__("pathlib").Path(__file__).resolve().parent.parent / "assets" / "logo.svg")
UV = "#9b6bff"                                          # second colour: ultraviolet
CREAM, INK, BG = "#f4ffe8", "#050803", "#000000"
REC = "#ff2e4d"                                         # the REC dot
W = H = 1024
CX, CY, R = 512, 440, 362

# ---- letters on a 6 x 7 grid, chamfered like the rest of the family (holes via even-odd)
HOLE = [(1.7, 1.6), (4.3, 1.6), (4.3, 3.0), (1.7, 3.0)]
L = {
    "S": [[(0.9,0),(6,0),(6,1.6),(1.7,1.6),(1.7,2.7),(5.1,2.7),(6,3.6),(6,6.1),(5.1,7),
           (0,7),(0,5.4),(4.3,5.4),(4.3,4.3),(0.9,4.3),(0,3.4),(0,0.9)]],
    "A": [[(0.9,0),(5.1,0),(6,0.9),(6,7),(4.3,7),(4.3,4.6),(1.7,4.6),(1.7,7),(0,7),(0,0.9)], HOLE],
    "R": [[(0,0),(5.1,0),(6,0.9),(6,3.7),(5.4,4.3),(6,4.9),(6,7),(4.3,7),(4.3,5.3),(3.6,4.6),
           (1.7,4.6),(1.7,7),(0,7)], HOLE],
    "D": [[(0,0),(5.1,0),(6,0.9),(6,6.1),(5.1,7),(0,7)], [(1.7,1.6),(4.3,1.6),(4.3,5.4),(1.7,5.4)]],
    "I": [[(0,0),(1.8,0),(1.8,7),(0,7)]],
    "N": [[(0,0),(1.8,0),(4.3,4.1),(4.3,0),(6,0),(6,7),(4.2,7),(1.7,2.9),(1.7,7),(0,7)]],
    "E": [[(0.9,0),(6,0),(6,1.6),(1.7,1.6),(1.7,2.7),(5,2.7),(5,4.3),(1.7,4.3),(1.7,5.4),
           (6,5.4),(6,7),(0.9,7),(0,6.1),(0,0.9)]],
}
WID = {c: 6 for c in L}
WID["I"] = 1.8
GAP, WORD, SK = 0.75, "SARDINE", 0.30
total = sum(WID[c] for c in WORD) + GAP * (len(WORD) - 1)
U = 18.5
TY0 = 826


def word_path(dx=0.0, dy=0.0):
    x0 = (W - total * U - 7 * U * SK) / 2
    d, x = [], 0.0
    for c in WORD:
        for poly in L[c]:
            pts = [f"{x0 + (x + px) * U + (7 - py) * U * SK + dx:.1f},{TY0 + py * U + dy:.1f}" for px, py in poly]
            d.append("M" + " L".join(pts) + " Z")
        x += WID[c] + GAP
    return " ".join(d)


# ---- pulse rings round the main ring, evenly spaced like a steady kick
pulses = "".join(
    f'<circle cx="{CX}" cy="{CY}" r="{R + 26 + 26*i}" fill="none" stroke="{ACC if i % 2 == 0 else UV}" '
    f'stroke-width="{6 - 1.4*i:.1f}" opacity="{0.6 - 0.16*i:.2f}"/>' for i in range(3))

# ---- audio bars: four on the floor (every fourth bar is the kick)
bars = []
for i in range(24):
    x = 176 + i * 29
    h = 420 if i % 4 == 0 else (250 if i % 4 == 2 else 150)
    h *= 0.85 + 0.15 * abs(math.sin(i * 1.3 + 0.2))
    bars.append(f'<rect x="{x}" y="{CY - 40 - h * 0.6:.0f}" width="13" height="{h:.0f}" rx="2" '
                f'opacity="{1.0 if i % 4 == 0 else 0.55}"/>')

# ---- the sardine, facing right: slim torpedo body, blunt head, deeply forked tail
body = ("M 872 352 "
        "C 852 322, 812 300, 760 292 "                  # snout up to the head
        "C 660 278, 520 280, 410 300 "                  # the straight, slim back
        "C 360 310, 320 326, 292 340 "                  # down to the tail stem
        "Q 232 290, 162 250 Q 196 320, 214 362 "        # upper tail lobe
        "Q 196 404, 162 474 Q 232 434, 292 384 "        # lower tail lobe
        "C 330 400, 400 420, 500 426 "                  # belly
        "C 620 432, 760 420, 830 392 "                  # throat
        "C 856 380, 868 366, 872 352 Z")
back_band = ("M 760 292 C 660 278, 520 280, 410 300 C 360 310, 320 326, 292 340 "
             "C 370 330, 450 320, 560 318 C 660 316, 730 320, 800 328 Z")   # blue-green back
belly = ("M 830 392 C 760 420, 620 432, 500 426 C 400 420, 330 400, 292 384 "
         "C 380 400, 480 410, 580 408 C 690 406, 780 396, 840 382 Z")
dorsal = "M 520 284 L 556 236 L 612 280 Z"
pelvic = "M 540 428 L 524 466 L 586 428 Z"
anal = "M 380 412 L 360 446 L 440 422 Z"
pect = "M 720 372 Q 668 386, 628 392 Q 664 360, 716 356 Z"
gill = "M 744 300 C 716 334, 716 380, 748 414"
mouth = "M 872 352 C 856 350, 842 352, 830 358"
spots = "".join(f'<circle cx="{x}" cy="{340 + (720 - x) * 0.02:.0f}" r="{9 - k:.0f}"/>'
                for k, x in enumerate(range(700, 440, -44)))           # the sardine's row of dark spots
scales = "".join(
    f'<path d="M {x} {y} q 10 8 0 16"/>'
    for x in range(340, 720, 34) for y in range(318, 420, 22)
    if (x - 530) ** 2 / 200 ** 2 + (y - 360) ** 2 / 56 ** 2 < 1)


def mini_fish(x, y, s, op):
    """A small sardine silhouette for the shoal behind the main fish."""
    d = ("M 0 0 C -8 -10, -30 -16, -60 -16 C -90 -16, -110 -10, -124 -4 L -146 -20 L -138 0 "
         "L -146 20 L -124 4 C -110 10, -90 16, -60 16 C -30 16, -8 10, 0 0 Z")
    return (f'<path d="{d}" transform="translate({x} {y}) scale({s})" fill="{UV}" opacity="{op}"/>'
            f'<circle cx="{x - 14*s:.1f}" cy="{y - 4*s:.1f}" r="{3*s:.1f}" fill="{BG}" opacity="{op}"/>')


def note(x, y, s, double=False):
    """An eighth (or a pair of eighths) note."""
    g = [f'<ellipse cx="{x}" cy="{y}" rx="{13*s}" ry="{9.5*s}" transform="rotate(-22 {x} {y})"/>',
         f'<rect x="{x + 9*s:.1f}" y="{y - 58*s:.1f}" width="{4.5*s:.1f}" height="{58*s:.1f}"/>']
    if double:
        x2 = x + 44 * s
        g += [f'<ellipse cx="{x2}" cy="{y - 6*s}" rx="{13*s}" ry="{9.5*s}" transform="rotate(-22 {x2} {y - 6*s})"/>',
              f'<rect x="{x2 + 9*s:.1f}" y="{y - 64*s:.1f}" width="{4.5*s:.1f}" height="{58*s:.1f}"/>',
              f'<polygon points="{x + 9*s:.1f},{y - 58*s:.1f} {x2 + 13.5*s:.1f},{y - 64*s:.1f} {x2 + 13.5*s:.1f},{y - 52*s:.1f} {x + 9*s:.1f},{y - 46*s:.1f}"/>']
    else:
        g.append(f'<path d="M {x + 13.5*s:.1f} {y - 58*s:.1f} q {22*s:.1f} {14*s:.1f} {16*s:.1f} {38*s:.1f} '
                 f'q {-2*s:.1f} {-16*s:.1f} {-16*s:.1f} {-22*s:.1f} z"/>')
    return "".join(g)


def glint(x, y, r):
    """A four-point sparkle."""
    k = r * 0.22
    return (f'<path d="M {x} {y - r} Q {x + k} {y - k} {x + r} {y} Q {x + k} {y + k} {x} {y + r} '
            f'Q {x - k} {y + k} {x - r} {y} Q {x - k} {y - k} {x} {y - r} Z"/>')


def knob3d(x, y, ang, rx=27, ry=12, hgt=20):
    """A 3D knob standing on the tin lid: shadow, lime skirt cylinder, domed cap and a pointer."""
    top, bot = y - hgt, y
    px, py = x + (rx - 7) * math.cos(math.radians(ang)), top + (ry - 3) * math.sin(math.radians(ang))
    return (f'<ellipse cx="{x + 5}" cy="{bot + 4}" rx="{rx + 4}" ry="{ry + 3}" fill="{INK}" opacity="0.35"/>'
            f'<path d="M {x - rx} {top} L {x - rx} {bot} A {rx} {ry} 0 0 0 {x + rx} {bot} L {x + rx} {top} Z" '
            f'fill="url(#skirt)" stroke="{INK}" stroke-width="3.5" stroke-linejoin="round"/>'
            f'<ellipse cx="{x}" cy="{top}" rx="{rx}" ry="{ry}" fill="url(#cap)" stroke="{INK}" stroke-width="3.5"/>'
            f'<ellipse cx="{x - 8}" cy="{top - 4}" rx="9" ry="3.5" fill="#ffffff" opacity="0.6" transform="rotate(-10 {x - 8} {top - 4})"/>'
            f'<line x1="{x}" y1="{top}" x2="{px:.1f}" y2="{py:.1f}" stroke="{INK}" stroke-width="5" stroke-linecap="round"/>')


def button3d(x, y, kind):
    """A low 3D push button with a PLAY triangle or a REC dot on its face."""
    rx, ry, hgt = 34, 13, 9
    top, bot = y - hgt, y
    if kind == "play":
        icon = f'<path d="M {x - 8} {top - 7} L {x + 11} {top} L {x - 8} {top + 7} Z" fill="{ACC}" stroke="{INK}" stroke-width="1.5" stroke-linejoin="round"/>'
    else:
        icon = f'<ellipse cx="{x}" cy="{top}" rx="9" ry="5.5" fill="{REC}" stroke="{INK}" stroke-width="1.5"/>'
    return (f'<ellipse cx="{x + 4}" cy="{bot + 3}" rx="{rx + 3}" ry="{ry + 2}" fill="{INK}" opacity="0.35"/>'
            f'<path d="M {x - rx} {top} L {x - rx} {bot} A {rx} {ry} 0 0 0 {x + rx} {bot} L {x + rx} {top} Z" '
            f'fill="#2a2f36" stroke="{INK}" stroke-width="3.5" stroke-linejoin="round"/>'
            f'<ellipse cx="{x}" cy="{top}" rx="{rx}" ry="{ry}" fill="url(#btn)" stroke="{INK}" stroke-width="3.5"/>'
            f'{icon}')


# ---- the opened sardine tin in 3D: lid face (in perspective), front wall, rolled-back lid on its key
FL, FR, FY = 300, 724, 688          # front edge of the top face
BL, BR, BY = 344, 680, 586          # back edge (narrower: perspective)
DEPTH = 62                          # height of the front wall
top_face = f"M {FL} {FY} L {BL} {BY} L {BR} {BY} L {FR} {FY} Z"
front = f"M {FL} {FY} L {FR} {FY} L {FR} {FY + DEPTH} L {FL} {FY + DEPTH} Z"
side = f"M {FR} {FY} L {BR} {BY} L {BR} {BY + DEPTH - 14} L {FR} {FY + DEPTH} Z"
rim = f"M {FL + 14} {FY - 8} L {BL + 12} {BY + 8} L {BR - 12} {BY + 8} L {FR - 14} {FY - 8} Z"   # pressed lid rim
ridges = "".join(f'<line x1="{FL + 10}" y1="{FY + 14 + 12*i}" x2="{FR - 10}" y2="{FY + 14 + 12*i}"/>' for i in range(4))
curl_y = BY - 16


def lerp(a, b, t):
    return a + (b - a) * t


# the control layout on the lid: four knobs in a row at the back, PLAY and REC in front
def on_lid(u, v):
    """u = 0..1 left to right, v = 0..1 back to front on the lid face."""
    y = lerp(BY, FY, v)
    xl, xr = lerp(BL, FL, v), lerp(BR, FR, v)
    return lerp(xl, xr, u), y


knobs = "".join(knob3d(*on_lid(u, 0.42), ang) for u, ang in
                zip([0.17, 0.39, 0.61, 0.83], [-140, -95, -55, -20]))
buttons = button3d(*on_lid(0.38, 0.84), "play") + button3d(*on_lid(0.62, 0.84), "rec")

curl = (f'<rect x="{BL + 8}" y="{curl_y - 18}" width="{BR - BL - 16}" height="36" rx="18" fill="url(#roll)" stroke="{INK}" stroke-width="4"/>'
        + "".join(f'<path d="M {BL + 8 + 18} {curl_y - 18 + 4 + 0*i} A {14 - 4*i} {14 - 4*i} 0 1 0 {BL + 8 + 18} {curl_y + 10 - 8*i}" '
                  f'fill="none" stroke="#5b6570" stroke-width="2.5"/>' for i in range(3))
        + f'<g fill="none" stroke="#ffffff" stroke-width="3" opacity="0.5" stroke-linecap="round">'
          f'<line x1="{BL + 40}" y1="{curl_y - 9}" x2="{BR - 40}" y2="{curl_y - 9}"/></g>')
key = (f'<path d="M {BR - 10} {curl_y} L {BR + 34} {curl_y - 4}" stroke="#9aa5b0" stroke-width="7" stroke-linecap="round" fill="none"/>'
       f'<ellipse cx="{BR + 52}" cy="{curl_y - 6}" rx="22" ry="14" fill="none" stroke="{INK}" stroke-width="11"/>'
       f'<ellipse cx="{BR + 52}" cy="{curl_y - 6}" rx="22" ry="14" fill="none" stroke="#c9d1d8" stroke-width="6"/>')

word = word_path()
shadow = word_path(9, 9)
svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">
<defs>
<linearGradient id="hot" gradientUnits="userSpaceOnUse" x1="0" y1="{TY0}" x2="0" y2="{TY0 + 7*U}"><stop offset="0" stop-color="#ffffff"/><stop offset="0.2" stop-color="#efffc2"/><stop offset="0.5" stop-color="{ACC}"/><stop offset="0.8" stop-color="#5f9a08"/><stop offset="1" stop-color="#2c4a02"/></linearGradient>
<clipPath id="ring"><circle cx="{CX}" cy="{CY}" r="{R - 14}"/></clipPath>
<linearGradient id="silver" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#eef3f7"/><stop offset="0.5" stop-color="#b7c2cc"/><stop offset="1" stop-color="#7a8691"/></linearGradient>
<linearGradient id="wall" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#6d7984"/><stop offset="0.3" stop-color="#c9d2da"/><stop offset="0.55" stop-color="#f2f6f9"/><stop offset="0.8" stop-color="#aab5bf"/><stop offset="1" stop-color="#5d6873"/></linearGradient>
<linearGradient id="roll" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffffff"/><stop offset="0.4" stop-color="#c3ccd4"/><stop offset="1" stop-color="#66717c"/></linearGradient>
<linearGradient id="skirt" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#3d6200"/><stop offset="0.35" stop-color="{ACC}"/><stop offset="0.55" stop-color="#f0ffb0"/><stop offset="0.75" stop-color="{ACC}"/><stop offset="1" stop-color="#3d6200"/></linearGradient>
<radialGradient id="cap" cx="0.38" cy="0.3" r="0.8"><stop offset="0" stop-color="#f4ffc9"/><stop offset="0.45" stop-color="{ACC}"/><stop offset="1" stop-color="#5f9a08"/></radialGradient>
<radialGradient id="btn" cx="0.38" cy="0.3" r="0.8"><stop offset="0" stop-color="#6a7480"/><stop offset="1" stop-color="#1c2026"/></radialGradient>
<linearGradient id="fishbody" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#0f2a33"/><stop offset="0.45" stop-color="#1b2a30"/><stop offset="1" stop-color="#0a0f12"/></linearGradient></defs>
<rect width="{W}" height="{H}" fill="{BG}"/>
{pulses}
<circle cx="{CX}" cy="{CY}" r="{R}" fill="#060a04" stroke="{ACC}" stroke-width="11"/>
<g clip-path="url(#ring)" fill="{ACC}">{"".join(bars)}</g>

<!-- the shoal behind -->
<g clip-path="url(#ring)">{mini_fish(470, 214, 0.62, 0.55)}{mini_fish(640, 198, 0.5, 0.4)}{mini_fish(330, 250, 0.45, 0.35)}</g>

<!-- the sardine -->
<g transform="rotate(-3 520 360) translate(-4 -14)">
  <path d="{body}" fill="none" stroke="{BG}" stroke-width="26" stroke-linejoin="round"/>
  <path d="{dorsal}" fill="{INK}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <path d="{anal}" fill="{INK}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <path d="{pelvic}" fill="{INK}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <path d="{body}" fill="url(#fishbody)" stroke="{ACC}" stroke-width="10" stroke-linejoin="round"/>
  <path d="{back_band}" fill="{UV}" opacity="0.55"/>
  <path d="{belly}" fill="{CREAM}"/>
  <g fill="none" stroke="{ACC}" stroke-width="2.5" opacity="0.5">{scales}</g>
  <g fill="{INK}" stroke="{UV}" stroke-width="2.5">{spots}</g>
  <path d="M 286 334 Q 226 290, 180 266" fill="none" stroke="{CREAM}" stroke-width="5" stroke-linecap="round"/>
  <path d="M 286 390 Q 226 434, 180 458" fill="none" stroke="{CREAM}" stroke-width="5" stroke-linecap="round"/>
  <path d="{gill}" fill="none" stroke="{ACC}" stroke-width="6" stroke-linecap="round"/>
  <path d="{pect}" fill="{CREAM}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <path d="{mouth}" fill="none" stroke="{ACC}" stroke-width="6" stroke-linecap="round"/>
  <circle cx="796" cy="336" r="22" fill="{CREAM}" stroke="{ACC}" stroke-width="5"/>
  <circle cx="801" cy="335" r="11" fill="{INK}"/>
  <circle cx="805" cy="330" r="3.5" fill="{CREAM}"/>
</g>

<!-- the opened tin, in 3D -->
<g stroke-linejoin="round">
  <path d="{front}" fill="none" stroke="{BG}" stroke-width="26"/>
  <path d="{top_face}" fill="none" stroke="{BG}" stroke-width="26"/>
  <path d="{side}" fill="#4f5a64" stroke="{INK}" stroke-width="5"/>
  <path d="{front}" fill="url(#wall)" stroke="{INK}" stroke-width="5"/>
  <g stroke="#6f7b86" stroke-width="2" opacity="0.8">{ridges}</g>
  <path d="{top_face}" fill="url(#silver)" stroke="{ACC}" stroke-width="7"/>
  <path d="{rim}" fill="none" stroke="#8b96a1" stroke-width="3"/>
  {curl}{key}
  {knobs}{buttons}
</g>

<g fill="{CREAM}">{note(756, 186, 0.75)}{note(806, 152, 0.9, True)}{note(880, 226, 0.7)}</g>
<g fill="{UV}">{glint(724, 120, 16)}{glint(912, 160, 12)}{glint(860, 92, 9)}</g>

<g stroke-linecap="round"><line x1="40" y1="{TY0 + 36}" x2="150" y2="{TY0 + 36}" stroke="{ACC}" stroke-width="7"/><line x1="24" y1="{TY0 + 72}" x2="140" y2="{TY0 + 72}" stroke="{UV}" stroke-width="5" opacity="0.8"/><line x1="60" y1="{TY0 + 106}" x2="134" y2="{TY0 + 106}" stroke="{ACC}" stroke-width="4" opacity="0.5"/><line x1="880" y1="{TY0 + 46}" x2="996" y2="{TY0 + 46}" stroke="{UV}" stroke-width="5" opacity="0.8"/><line x1="870" y1="{TY0 + 84}" x2="972" y2="{TY0 + 84}" stroke="{ACC}" stroke-width="4" opacity="0.5"/></g>
<path d="{shadow}" fill="#24104d" stroke="#24104d" stroke-width="20" stroke-linejoin="round" fill-rule="evenodd"/>
<path d="{word}" fill="none" stroke="{BG}" stroke-width="24" stroke-linejoin="round"/>
<path d="{word}" fill="none" stroke="{UV}" stroke-width="12" stroke-linejoin="round"/>
<path d="{word}" fill="url(#hot)" stroke="#1c2b02" stroke-width="3" stroke-linejoin="round" fill-rule="evenodd"/>
<g fill="#ffffff">{glint(W/2 + 330, TY0 + 4, 26)}{glint(W/2 - 380, TY0 + 128, 15)}</g>
</svg>'''
open(OUT, "w").write(svg)
print("ok", OUT)
