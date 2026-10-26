# Copyright (c) 2026 Dos-si-Doge
# SPDX-License-Identifier: MIT

"""Builds docs/user-guide/openFences-User-Guide.html: a single self-contained file.

The prose lives in docs/user-guide/guide.src.html. This script
  * replaces <!--svg:NAME--> placeholders with illustrations drawn here (inline SVG, openFences style),
  * replaces <!--svg-defs--> with the shared icon symbols, gradients and filters,
  * inlines every <img src="images/..."> as a data: URI.

Usage: python scripts/build_user_guide.py
       python scripts/build_user_guide.py --export-pages DIR   (one HTML page per illustration, used by
                                                                 scripts/render_welcome_images.ps1 for the app's hero images)
"""
import base64
import html
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
GUIDE = ROOT / "docs" / "user-guide"
SRC = GUIDE / "guide.src.html"
OUT = GUIDE / "openFences-User-Guide.html"

FONT = "'Segoe UI Variable Text','Segoe UI',system-ui,-apple-system,sans-serif"
ACCENT = "#2f7cf6"


def esc(s):
    return html.escape(s, quote=True)


# ---------------------------------------------------------------- shared definitions

PAGE = '<path d="M9 3h15l8 8v26H9z" fill="#fbfcfe"/><path d="M24 3v8h8z" fill="#cfd8e3"/>'


def badge(color, glyph):
    return PAGE + f'<rect x="5" y="17" width="17" height="14" rx="2" fill="{color}"/>{glyph}'


SYMBOLS = {
    "i-folder": '<path d="M3 10a3 3 0 0 1 3-3h9l4 4h15a3 3 0 0 1 3 3v17a3 3 0 0 1-3 3H6a3 3 0 0 1-3-3z" fill="#e3a21a"/>'
                '<path d="M3 15h34v16a3 3 0 0 1-3 3H6a3 3 0 0 1-3-3z" fill="#fcc94a"/>',
    "i-zip": '<path d="M3 10a3 3 0 0 1 3-3h9l4 4h15a3 3 0 0 1 3 3v17a3 3 0 0 1-3 3H6a3 3 0 0 1-3-3z" fill="#e3a21a"/>'
             '<path d="M3 15h34v16a3 3 0 0 1-3 3H6a3 3 0 0 1-3-3z" fill="#fcc94a"/>'
             '<path d="M19 15h3v3h-3zm0 5h3v3h-3zm0 5h3v3h-3z" fill="#7a5a12"/>',
    "i-doc": badge("#2b62c9", '<path d="M7.5 20.5l2 7 2-5 2 5 2-7" stroke="#fff" stroke-width="1.5" fill="none"/>'
                   '<path d="M25 19h5M25 23h5M25 27h5" stroke="#9fb3cc" stroke-width="1.4"/>'),
    "i-sheet": badge("#1f8f4e", '<path d="M8 21l6 7M14 21l-6 7" stroke="#fff" stroke-width="1.6"/>'
                     '<path d="M24 18h7v12h-7zM24 22h7M24 26h7M27.5 18v12" stroke="#9cc8ad" stroke-width="1" fill="none"/>'),
    "i-pdf": badge("#d23b2f", '<path d="M8 28c3-8 6-10 7-6s-6 5-6 5 5-1 8 1" stroke="#fff" stroke-width="1.4" fill="none"/>'),
    "i-ppt": badge("#d35b23", '<circle cx="13.5" cy="24" r="4" fill="none" stroke="#fff" stroke-width="1.6"/>'
                   '<path d="M25 20h5M25 24h5M25 28h5" stroke="#e9b59a" stroke-width="1.4"/>'),
    "i-txt": PAGE + '<path d="M13 15h14M13 19h14M13 23h14M13 27h10" stroke="#8a97a8" stroke-width="1.5"/>',
    "i-md": PAGE + '<rect x="11" y="18" width="18" height="11" rx="2" fill="#56657a"/>'
                   '<path d="M14 26v-5l2 2.5 2-2.5v5M23 21v5m-2-2l2 2 2-2" stroke="#fff" stroke-width="1.3" fill="none"/>',
    "i-music": badge("#7b4fd0", '<path d="M15 20v7.5a2 2 0 1 1-1.5-1.9V21l-5 1v6.5a2 2 0 1 1-1.5-1.9V21.5z" fill="#fff"/>'),
    "i-msi": '<rect x="6" y="9" width="28" height="24" rx="3" fill="#6b7a90"/><rect x="6" y="9" width="28" height="7" rx="3" fill="#8796ab"/>'
             '<circle cx="20" cy="24" r="5" fill="#dfe6ef"/><circle cx="20" cy="24" r="1.6" fill="#6b7a90"/>',
    "i-notepad": '<rect x="5" y="4" width="30" height="32" rx="6" fill="#2f6fd6"/>'
                 '<path d="M11 14h18M11 19h18M11 24h18M11 29h12" stroke="#fff" stroke-width="2"/>',
    "i-calc": '<rect x="6" y="4" width="28" height="32" rx="6" fill="#3a3d45"/><rect x="10" y="8" width="20" height="7" rx="1.5" fill="#9fe3c1"/>'
              '<path d="M11 19h4v4h-4zm7 0h4v4h-4zm7 0h4v4h-4zm-14 7h4v4h-4zm7 0h4v4h-4zm7 0h4v4h-4z" fill="#c9ced8"/>',
    "i-paint": '<rect x="5" y="4" width="30" height="32" rx="6" fill="#ffffff"/><circle cx="14" cy="14" r="3.5" fill="#e8453c"/>'
               '<circle cx="24" cy="12" r="3.5" fill="#f6b026"/><circle cx="28" cy="22" r="3.5" fill="#2fa84f"/><circle cx="14" cy="25" r="3.5" fill="#2f6fd6"/>'
               '<rect x="5" y="4" width="30" height="32" rx="6" fill="none" stroke="#d3d8e0"/>',
    "i-cmd": '<rect x="5" y="5" width="30" height="30" rx="6" fill="#1d1f24"/><path d="M11 15l6 5-6 5" stroke="#e9eef5" stroke-width="2.2" fill="none"/>'
             '<path d="M20 27h9" stroke="#e9eef5" stroke-width="2.2"/>',
    "i-ps": '<rect x="5" y="5" width="30" height="30" rx="6" fill="#244b8f"/><path d="M11 14l8 6-8 6" stroke="#fff" stroke-width="2.4" fill="none"/>'
            '<path d="M21 27h8" stroke="#fff" stroke-width="2.4"/>',
    "i-taskmgr": '<rect x="5" y="5" width="30" height="30" rx="6" fill="#23875a"/>'
                 '<path d="M9 24h5l3-8 4 13 3-9 2 4h5" stroke="#fff" stroke-width="2" fill="none" stroke-linejoin="round"/>',
    "i-gear": '<rect x="5" y="5" width="30" height="30" rx="6" fill="#5b6b8a"/><circle cx="20" cy="20" r="7" fill="none" stroke="#fff" stroke-width="3"/>'
              '<path d="M20 9v4M20 27v4M9 20h4M27 20h4M12.2 12.2l2.8 2.8M25 25l2.8 2.8M12.2 27.8l2.8-2.8M25 15l2.8-2.8" stroke="#fff" stroke-width="3"/>',
    "i-rdp": '<rect x="5" y="5" width="30" height="30" rx="6" fill="#0f6cbd"/><rect x="10" y="11" width="20" height="13" rx="1.5" fill="#fff"/>'
             '<path d="M16 29h8M20 24v5" stroke="#fff" stroke-width="2"/>',
    "i-game": '<rect x="5" y="5" width="30" height="30" rx="6" fill="#8b3fd1"/>'
              '<path d="M11 17h18a3 3 0 0 1 3 3l1 6a2.5 2.5 0 0 1-4.5 1.7L26 25H14l-2.5 2.7A2.5 2.5 0 0 1 7 26l1-6a3 3 0 0 1 3-3z" fill="#fff"/>'
              '<circle cx="25" cy="21" r="1.3" fill="#8b3fd1"/><path d="M13 21h4M15 19v4" stroke="#8b3fd1" stroke-width="1.4"/>',
    "i-chess": '<rect x="5" y="5" width="30" height="30" rx="6" fill="#3d5a3c"/>'
               '<path d="M16 30h9l-1-3h-7zM17 26h7l-1-6h2v-3h-3v-3h-3v3h-3v3h2z" fill="#f1ead6"/>',
    "i-cards": '<rect x="5" y="5" width="30" height="30" rx="6" fill="#1f7a4c"/><rect x="11" y="10" width="13" height="18" rx="2" fill="#fff" transform="rotate(-10 17 19)"/>'
               '<rect x="16" y="11" width="13" height="18" rx="2" fill="#fff"/><path d="M22.5 16l-2.5 3.5 2.5 3.5 2.5-3.5z" fill="#d23b2f"/>',
    "i-recycle": '<path d="M10 12h20l-2 23H12z" fill="#dfe8f2" stroke="#9fb0c4"/><rect x="8" y="9" width="24" height="4" rx="1.5" fill="#b9c7d8"/>'
                 '<path d="M15 17v14M20 17v14M25 17v14" stroke="#9fb0c4" stroke-width="1.4"/>',
    "i-pc": '<rect x="4" y="7" width="32" height="21" rx="2" fill="#2d3340"/><rect x="6" y="9" width="28" height="17" fill="#43b0f1"/>'
            '<path d="M15 33h10M20 28v5" stroke="#2d3340" stroke-width="2.5"/>',
    "i-arrow": '<rect x="0" y="0" width="14" height="14" rx="2" fill="#fff"/><path d="M4 11c0-4 2-6 6-6M7 3l3 2-3 2" stroke="#2f6fd6" stroke-width="1.8" fill="none"/>',
}

THUMBS = {
    "t-sunset": ('<linearGradient id="g-sunset" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#5b2d86"/><stop offset=".6" stop-color="#f08a4b"/>'
                 '<stop offset="1" stop-color="#2a1a3a"/></linearGradient>',
                 '<rect x="2" y="7" width="36" height="26" fill="url(#g-sunset)"/><circle cx="20" cy="23" r="4" fill="#ffd77a"/>'
                 '<path d="M2 26c8-3 14 2 22-1s10-1 14 0v8H2z" fill="#24162f"/>'),
    "t-mountains": ('<linearGradient id="g-mount" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#9fd0f2"/><stop offset="1" stop-color="#e2eef8"/></linearGradient>',
                    '<rect x="2" y="7" width="36" height="26" fill="url(#g-mount)"/><path d="M2 30l10-14 6 8 6-10 14 16z" fill="#4c6273"/>'
                    '<path d="M22 14l2 3h-4zM12 16l2 3h-4z" fill="#fff"/><rect x="2" y="29" width="36" height="4" fill="#3f8a4c"/>'),
    "t-beach": ('<linearGradient id="g-beach" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#7cc8ff"/><stop offset="1" stop-color="#2a74c4"/></linearGradient>',
                '<rect x="2" y="7" width="36" height="26" fill="url(#g-beach)"/><rect x="2" y="21" width="36" height="6" fill="#1d6aa8"/>'
                '<path d="M2 27c10-2 20 1 36-1v7H2z" fill="#ecd59e"/>'),
    "t-city": ('<linearGradient id="g-city" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#151a40"/><stop offset="1" stop-color="#5a2f6e"/></linearGradient>',
               '<rect x="2" y="7" width="36" height="26" fill="url(#g-city)"/>'
               '<path d="M4 33V19h5v14zm7 0V14h5v19zm7 0V22h5v11zm7 0V16h5v17zm7 0V24h4v9z" fill="#262b3d"/>'
               '<path d="M5 21h1v1H5zm7-4h1v1h-1zm2 3h1v1h-1zm12-2h1v1h-1zm2 4h1v1h-1zm-9 2h1v1h-1z" fill="#ffd26e"/>'),
    "t-forest": ('<linearGradient id="g-forest" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#d4ecd4"/><stop offset="1" stop-color="#92c493"/></linearGradient>',
                 '<rect x="2" y="7" width="36" height="26" fill="url(#g-forest)"/>'
                 '<path d="M6 33l5-16 5 16zm8 0l6-20 6 20zm10 0l5-14 5 14z" fill="#1f6b36"/>'),
}


def defs():
    parts = [
        '<svg width="0" height="0" style="position:absolute" aria-hidden="true" focusable="false"><defs>',
        '<linearGradient id="wall" x1="0" y1="0" x2="0.35" y2="1"><stop offset="0" stop-color="#1f2550"/><stop offset=".45" stop-color="#5a3f7e"/>'
        '<stop offset=".78" stop-color="#c46a7d"/><stop offset="1" stop-color="#f1a75f"/></linearGradient>',
        '<radialGradient id="glow" cx=".72" cy=".78" r=".45"><stop offset="0" stop-color="#ffd59a" stop-opacity=".55"/><stop offset="1" stop-color="#ffd59a" stop-opacity="0"/></radialGradient>',
        '<filter id="ts" x="-20%" y="-30%" width="140%" height="160%"><feDropShadow dx="0" dy="1" stdDeviation=".8" flood-color="#000" flood-opacity=".9"/></filter>',
        '<filter id="ms" x="-15%" y="-10%" width="130%" height="130%"><feDropShadow dx="0" dy="4" stdDeviation="5" flood-color="#000" flood-opacity=".3"/></filter>',
        # Icon Tint (luminance mapped to blue) and Chameleon (desaturated) looks.
        '<filter id="tint"><feColorMatrix type="matrix" values=".10 .34 .03 0 0  .15 .50 .05 0 0  .21 .72 .07 0 .12  0 0 0 1 0"/></filter>',
        '<filter id="gray"><feColorMatrix type="saturate" values="0"/></filter>',
        f'<marker id="ah" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">'
        f'<path d="M0 0L10 5L0 10z" fill="{ACCENT}"/></marker>',
        '<marker id="ahw" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">'
        '<path d="M0 0L10 5L0 10z" fill="#fff"/></marker>',
    ]
    for sid, body in SYMBOLS.items():
        vb = "0 0 14 14" if sid == "i-arrow" else "0 0 40 40"
        parts.append(f'<symbol id="{sid}" viewBox="{vb}">{body}</symbol>')
    for sid, (grad, body) in THUMBS.items():
        parts.append(grad)
        parts.append(f'<symbol id="{sid}" viewBox="0 0 40 40"><rect x="1" y="6" width="38" height="28" fill="#fff"/>{body}</symbol>')
    parts.append("</defs></svg>")
    return "".join(parts)


# ---------------------------------------------------------------- drawing helpers

def svg(w, h, body, label):
    return (f'<svg class="ill" viewBox="0 0 {w} {h}" role="img" aria-label="{esc(label)}" xmlns="http://www.w3.org/2000/svg" '
            f'font-family="{FONT}">{body}</svg>')


def wallpaper(w, h, taskbar=True):
    out = [f'<rect width="{w}" height="{h}" fill="url(#wall)"/>', f'<rect width="{w}" height="{h}" fill="url(#glow)"/>',
           f'<path d="M0 {h * .78}C{w * .2} {h * .70} {w * .35} {h * .82} {w * .55} {h * .76}S{w * .85} {h * .70} {w} {h * .77}V{h}H0z" fill="#2a1f3d" opacity=".55"/>',
           f'<path d="M0 {h * .88}C{w * .25} {h * .82} {w * .5} {h * .92} {w * .75} {h * .86}S{w} {h * .85} {w} {h * .86}V{h}H0z" fill="#1a1428" opacity=".7"/>']
    if taskbar:
        out.append(f'<rect y="{h - 34}" width="{w}" height="34" fill="#1d1e26" opacity=".94"/>')
        cx = w / 2 - 60
        for i, color in enumerate(["#3d8bfd", "#f0b429", "#9b6dff", "#2fbf71", "#e5484d"]):
            out.append(f'<rect x="{cx + i * 26}" y="{h - 26}" width="18" height="18" rx="4" fill="{color}" opacity=".9"/>')
        out.append(f'<text x="{w - 16}" y="{h - 13}" font-size="11" fill="#e8eaf0" text-anchor="end">10:24 AM</text>')
    return "".join(out)


def wrap(label, width_chars=12, lines=2):
    words, out, cur = label.split(), [], ""
    for wd in words:
        cand = (cur + " " + wd).strip()
        if len(cand) <= width_chars or not cur:
            cur = cand
        else:
            out.append(cur)
            cur = wd
    if cur:
        out.append(cur)
    out = [ln if len(ln) <= width_chars + 2 else ln[:width_chars] + "…" for ln in out]
    if len(out) > lines:
        out = out[:lines]
        out[-1] = out[-1][: width_chars - 1] + "…"
    return out


def icon_item(cx, top, icon, label, selected=False, shortcut=False, cell_w=70, cell_h=76, size=34):
    parts = []
    if selected:
        parts.append(f'<rect x="{cx - cell_w / 2 + 3}" y="{top}" width="{cell_w - 6}" height="{cell_h - 4}" rx="4" fill="#8cbeff" fill-opacity=".35" stroke="#a9d0ff" stroke-opacity=".7"/>')
    parts.append(f'<use href="#{icon}" x="{cx - size / 2}" y="{top + 5}" width="{size}" height="{size}"/>')
    if shortcut:
        parts.append(f'<use href="#i-arrow" x="{cx - size / 2}" y="{top + size - 6}" width="11" height="11"/>')
    for i, line in enumerate(wrap(label)):
        parts.append(f'<text x="{cx}" y="{top + size + 18 + i * 13}" font-size="10.5" fill="#fff" text-anchor="middle" filter="url(#ts)">{esc(line)}</text>')
    return "".join(parts)


def menu_glyph(x, y, opacity=.85):
    return "".join(f'<path d="M{x - 6} {y + d}h12" stroke="#fff" stroke-opacity="{opacity}" stroke-width="1.5"/>' for d in (-4, 0, 4))


def fence(x, y, w, h, title=None, tabs=None, active=0, tab_style="flat", tab_colors=None, items=(), mode="icons", rolled=False,
          bg="#15161c", opacity=.74, breadcrumb=None, hint=None, close=False, highlight=False, selected=(), scroll=False,
          cell=(70, 76)):
    th = 26
    hh = th if rolled else h
    p = [f'<rect x="{x}" y="{y}" width="{w}" height="{hh}" rx="7" fill="{bg}" fill-opacity="{opacity}"/>']
    if tab_colors and tabs and tab_colors[active]:
        p.append(f'<rect x="{x}" y="{y}" width="{w}" height="{hh}" rx="7" fill="{tab_colors[active]}" fill-opacity=".22"/>')
    if rolled:
        p.append(f'<rect x="{x}" y="{y}" width="{w}" height="{th}" rx="7" fill="#000" fill-opacity=".24"/>')
    else:
        p.append(f'<path d="M{x} {y + th}V{y + 7}a7 7 0 0 1 7-7H{x + w - 7}a7 7 0 0 1 7 7V{y + th}z" fill="#000" fill-opacity=".24"/>')
    p.append(menu_glyph(x + 14, y + th / 2))
    if close:
        cx, cy = x + w - 14, y + th / 2
        p.append(f'<path d="M{cx - 4} {cy - 4}l8 8M{cx - 4} {cy + 4}l8-8" stroke="#fff" stroke-opacity=".8" stroke-width="1.4"/>')
    if tabs:
        widths = [len(t) * 6.6 + 24 for t in tabs]
        total = sum(widths) + 2 * (len(tabs) - 1)
        tx = x + (w - total) / 2
        for i, (t, tw) in enumerate(zip(tabs, widths)):
            color = (tab_colors[i] if tab_colors else None) or "#ffffff"
            on = i == active
            if tab_style == "segmented":
                p.append(f'<rect x="{tx}" y="{y + 3}" width="{tw}" height="{th - 6}" fill="{color}" fill-opacity="{.36 if on else .13}"/>')
            elif tab_style == "rounded":
                p.append(f'<rect x="{tx}" y="{y + 3}" width="{tw}" height="{th - 6}" rx="{(th - 6) / 2}" fill="{color}" fill-opacity="{.4 if on else .12}"/>')
            elif on:
                p.append(f'<rect x="{tx + 6}" y="{y + th - 5}" width="{tw - 12}" height="2" fill="{color}"/>')
            p.append(f'<text x="{tx + tw / 2}" y="{y + th / 2 + 4}" font-size="11.5" font-weight="{600 if on else 400}" fill="#fff" '
                     f'fill-opacity="{1 if on else .7}" text-anchor="middle" filter="url(#ts)">{esc(t)}</text>')
            tx += tw + 2
    elif title:
        p.append(f'<text x="{x + w / 2}" y="{y + th / 2 + 4}" font-size="12" font-weight="600" fill="#fff" text-anchor="middle" filter="url(#ts)">{esc(title)}</text>')
    if rolled:
        if highlight:
            p.append(f'<rect x="{x}" y="{y}" width="{w}" height="{th}" rx="7" fill="none" stroke="{ACCENT}" stroke-width="2"/>')
        return "".join(p)
    top = y + th
    if breadcrumb:
        p.append(f'<rect x="{x}" y="{top}" width="{w}" height="22" fill="#000" fill-opacity=".16"/>')
        p.append(f'<text x="{x + 10}" y="{top + 15}" font-size="11.5" fill="#fff" filter="url(#ts)">‹ {esc(breadcrumb)}</text>')
        top += 22
    if not items and hint:
        p.append(f'<text x="{x + w / 2}" y="{(top + y + h) / 2 + 4}" font-size="11.5" fill="#fff" fill-opacity=".55" text-anchor="middle">{esc(hint)}</text>')
    if mode == "icons":
        cw, ch = cell
        cols = max(1, int((w - 12) // cw))
        x0 = x + (w - cols * cw) / 2
        for i, it in enumerate(items):
            icon, label = it[0], it[1]
            opts = it[2] if len(it) > 2 else {}
            r, c = divmod(i, cols)
            ty = top + 6 + r * ch
            if ty + ch > y + h + 4:
                break
            p.append(icon_item(x0 + c * cw + cw / 2, ty, icon, label, selected=i in selected, shortcut=opts.get("shortcut", False), cell_w=cw, cell_h=ch))
    else:
        for i, it in enumerate(items):
            ry = top + 6 + i * 23
            if ry + 22 > y + h:
                break
            if i in selected:
                p.append(f'<rect x="{x + 6}" y="{ry}" width="{w - 20}" height="21" rx="3" fill="#8cbeff" fill-opacity=".3"/>')
            p.append(f'<use href="#{it[0]}" x="{x + 12}" y="{ry + 2}" width="17" height="17"/>')
            p.append(f'<text x="{x + 36}" y="{ry + 15}" font-size="11" fill="#fff" filter="url(#ts)">{esc(it[1])}</text>')
    if scroll:
        p.append(f'<rect x="{x + w - 8}" y="{top + 8}" width="4" height="{(y + h - top) * .45}" rx="2" fill="#fff" fill-opacity=".45"/>')
    if highlight:
        p.append(f'<rect x="{x + 1}" y="{y + 1}" width="{w - 2}" height="{h - 2}" rx="7" fill="none" stroke="{ACCENT}" stroke-width="2"/>')
    return "".join(p)


def menu(x, y, entries, width=230):
    row, sep = 26, 9
    height = 8 + sum(sep if e is None else row for e in entries)
    p = [f'<g filter="url(#ms)"><rect x="{x}" y="{y}" width="{width}" height="{height}" rx="8" fill="#fbfbfd" stroke="#d6d9e0"/></g>']
    cy = y + 4
    for e in entries:
        if e is None:
            p.append(f'<path d="M{x + 10} {cy + sep / 2}h{width - 20}" stroke="#e1e4ea"/>')
            cy += sep
            continue
        text, flags = (e, "") if isinstance(e, str) else e
        if "hl" in flags:
            p.append(f'<rect x="{x + 4}" y="{cy + 1}" width="{width - 8}" height="{row - 2}" rx="4" fill="#e3edfd"/>')
        weight = 600 if "bold" in flags else 400
        color = "#9aa1ad" if "gray" in flags else "#1d2129"
        p.append(f'<text x="{x + 32}" y="{cy + 17}" font-size="12" font-weight="{weight}" fill="{color}">{esc(text)}</text>')
        if "check" in flags:
            p.append(f'<path d="M{x + 12} {cy + 13}l3.5 3.5 6.5-7" stroke="#1d2129" stroke-width="1.6" fill="none"/>')
        if "sub" in flags:
            p.append(f'<path d="M{x + width - 18} {cy + 9}l4 4-4 4" stroke="#5a606b" stroke-width="1.4" fill="none"/>')
        cy += row
    return "".join(p)


def cursor(x, y, scale=1.0):
    return (f'<path transform="translate({x} {y}) scale({scale})" d="M0 0v17l4.5-4 3 7 3-1.3-3-6.8h6z" fill="#fff" stroke="#111" stroke-width="1.1" stroke-linejoin="round"/>')


def callout(n, x, y, tx, ty):
    return (f'<path d="M{x} {y}L{tx} {ty}" stroke="{ACCENT}" stroke-width="1.6"/><circle cx="{x}" cy="{y}" r="3" fill="{ACCENT}"/>'
            f'<circle cx="{tx}" cy="{ty}" r="12" fill="{ACCENT}" stroke="#fff" stroke-width="2"/>'
            f'<text x="{tx}" y="{ty + 4.5}" font-size="12.5" font-weight="700" fill="#fff" text-anchor="middle">{n}</text>')


def arrow(x1, y1, x2, y2, bend=0, color=ACCENT, dashed=False, width=2.4, white=False):
    mx, my = (x1 + x2) / 2, (y1 + y2) / 2 - bend
    dash = ' stroke-dasharray="7 5"' if dashed else ""
    marker = "ahw" if white else "ah"
    return f'<path d="M{x1} {y1}Q{mx} {my} {x2} {y2}" stroke="{color}" stroke-width="{width}" fill="none"{dash} marker-end="url(#{marker})"/>'


def label(x, y, text, size=13, anchor="middle", color="#fff", weight=600):
    return f'<text x="{x}" y="{y}" font-size="{size}" font-weight="{weight}" fill="{color}" text-anchor="{anchor}" filter="url(#ts)">{esc(text)}</text>'


def keycap(x, y, text, w=None):
    w = w or len(text) * 8 + 22
    return (f'<rect x="{x}" y="{y}" width="{w}" height="28" rx="6" fill="#f4f5f8" stroke="#b9bfca"/>'
            f'<rect x="{x}" y="{y + 24}" width="{w}" height="4" rx="2" fill="#b9bfca"/>'
            f'<text x="{x + w / 2}" y="{y + 18}" font-size="12.5" font-weight="600" fill="#2a2f38" text-anchor="middle">{esc(text)}</text>'), w


def app_window(x, y, w, h, title):
    p = [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="8" fill="#f6f7f9" stroke="#c9ced8"/>',
         f'<path d="M{x} {y + 30}V{y + 8}a8 8 0 0 1 8-8H{x + w - 8}a8 8 0 0 1 8 8V{y + 30}z" fill="#e7eaf0"/>',
         f'<text x="{x + 14}" y="{y + 19}" font-size="11.5" fill="#3a404b">{esc(title)}</text>']
    for i in range(3):
        p.append(f'<path d="M{x + w - 70 + i * 22} {y + 15}h10" stroke="#5a606b" stroke-width="1.2"/>')
    for i in range(int((h - 60) / 22)):
        lw = w - 80 - (i * 37 % 120)
        p.append(f'<rect x="{x + 30}" y="{y + 52 + i * 22}" width="{lw}" height="8" rx="4" fill="#d5dae3"/>')
    return "".join(p)


def mini(x, y, scale, body, w=1200, h=640):
    return f'<g transform="translate({x} {y}) scale({scale})"><svg width="{w}" height="{h}" viewBox="0 0 {w} {h}" overflow="hidden">{body}</svg></g>'


# ---------------------------------------------------------------- content

PROGRAMS = [("i-notepad", "Notepad", {"shortcut": True}), ("i-calc", "Calculator", {"shortcut": True}), ("i-paint", "Paint", {"shortcut": True}),
            ("i-cmd", "Command Prompt", {"shortcut": True}), ("i-ps", "Windows PowerShell", {"shortcut": True}),
            ("i-taskmgr", "Task Manager", {"shortcut": True}), ("i-gear", "Control Panel", {"shortcut": True}), ("i-rdp", "Remote Desktop", {"shortcut": True})]
DOCUMENTS = [("i-doc", "Quarterly Report.docx"), ("i-pdf", "Project Plan.pdf"), ("i-txt", "Meeting Notes.txt"), ("i-ppt", "Team Presentation.pptx"),
             ("i-doc", "Resume.docx"), ("i-md", "Ideas.md")]
IMAGES = [("t-sunset", "Sunset.png"), ("t-mountains", "Mountains.png"), ("t-beach", "Beach day.png"), ("t-city", "City at night.png"), ("t-forest", "Forest walk.png")]
DOWNLOADS = [("i-pdf", "Invoice-0042.pdf"), ("i-zip", "Holiday photos.zip"), ("i-music", "Podcast episode 12.mp3"), ("i-pdf", "Product manual.pdf"),
             ("i-msi", "driver-update.msi"), ("i-txt", "Release notes.txt"), ("i-doc", "Conference agenda.docx"), ("i-zip", "Wallpaper pack.zip"),
             ("i-sheet", "Expenses.xlsx")]
GAMES = [("i-cards", "Solitaire", {"shortcut": True}), ("i-chess", "Chess", {"shortcut": True}), ("i-game", "Minesweeper", {"shortcut": True})]


def desktop_scene(w=1200, h=640, taskbar=True):
    return "".join([
        wallpaper(w, h, taskbar),
        fence(24, 24, 300, 262, title="Programs", items=PROGRAMS),
        fence(340, 24, 330, 262, tabs=["Documents", "Spreadsheets"], tab_colors=["#3a78d8", "#2f9e5a"], items=DOCUMENTS),
        fence(686, 24, 300, 262, title="Images", items=IMAGES, bg="#163a7a", opacity=.62),
        fence(24, 302, 380, 252, title="Downloads", items=DOWNLOADS, mode="list", scroll=True),
        fence(420, 302, 220, 0, title="Games", rolled=True),
        fence(420, 342, 220, 150, title="Inbox", hint="Drag icons here"),
        icon_item(1150, 22, "i-recycle", "Recycle Bin"),
        icon_item(1150, 104, "i-pc", "This PC"),
    ])


def ill_hero():
    return svg(1200, 640, desktop_scene(), "A desktop organized into fences: Programs, Documents and Spreadsheets tabs, Images, a Downloads portal, a rolled-up Games fence and an empty Inbox fence")


def ill_anatomy():
    body = [wallpaper(820, 450, taskbar=False),
            fence(170, 40, 470, 340, tabs=["Documents", "Spreadsheets"], tab_colors=["#3a78d8", "#2f9e5a"],
                  items=DOCUMENTS + [("i-sheet", "Budget 2026.xlsx"), ("i-pdf", "Invoice-0042.pdf"), ("i-txt", "Release notes.txt"), ("i-zip", "Archive.zip")],
                  close=True, selected={1}, scroll=True, cell=(76, 80)),
            '<path d="M640 120v180" stroke="#fff" stroke-opacity=".9" stroke-width="3" stroke-dasharray="4 4"/>',
            callout(1, 184, 53, 110, 40), callout(2, 405, 40, 470, 16), callout(3, 626, 53, 700, 40), callout(4, 196, 168, 110, 180),
            callout(5, 634, 150, 720, 200), callout(6, 640, 300, 720, 340)]
    return svg(820, 450, "".join(body), "Parts of a fence: menu button, title and tabs, delete button, icons, scroll bar and resize edges")


def ill_create():
    body = [wallpaper(820, 430, taskbar=False),
            icon_item(60, 20, "i-doc", "Quarterly Report.docx", selected=True), icon_item(60, 104, "i-sheet", "Budget 2026.xlsx", selected=True),
            icon_item(60, 188, "i-pdf", "Project Plan.pdf", selected=True), icon_item(60, 272, "i-txt", "Notes.txt"),
            '<rect x="22" y="14" width="400" height="262" fill="#78aaff" fill-opacity=".18" stroke="#a9ccff" stroke-width="1.5" stroke-dasharray="6 4"/>',
            cursor(422, 276),
            menu(440, 250, [("Create fence here", "hl"), "Create fence with the 3 selected icon(s)", "Create folder portal here…", None, "Cancel"], width=300)]
    return svg(820, 430, "".join(body), "Dragging a rectangle on the desktop opens a menu to create a fence or a folder portal")


def ill_tabs():
    photos = [("t-sunset", "Sunset.png"), ("t-beach", "Beach day.png"), ("t-city", "City at night.png")]
    music = [("i-music", "Podcast 12.mp3"), ("i-music", "Morning mix.mp3")]
    body = [wallpaper(980, 340, taskbar=False),
            fence(30, 40, 250, 200, title="Photos", items=photos, highlight=True),
            fence(310, 130, 200, 160, title="Music", items=music, opacity=.84),
            cursor(412, 140), arrow(400, 150, 170, 60, bend=60, color="#fff", white=True),
            label(575, 175, "→", size=40),
            fence(640, 40, 250, 200, tabs=["Photos", "Music"], items=photos),
            label(765, 280, "One fence, two tabs", size=14)]
    return svg(980, 340, "".join(body), "Drop one fence's title onto another fence's title to combine them into tabs")


def ill_tab_styles():
    body = ['<rect width="980" height="96" rx="10" fill="#26293a"/>']
    for i, (style, name) in enumerate([("flat", "Flat"), ("segmented", "Segmented"), ("rounded", "Rounded")]):
        x = 20 + i * 320
        body.append(fence(x, 18, 290, 0, tabs=["Work", "Personal", "Archive"], tab_colors=["#3a78d8", "#2f9e5a", "#c4703a"], tab_style=style, rolled=True))
        body.append(f'<text x="{x + 145}" y="78" font-size="12.5" fill="#d8dbe6" text-anchor="middle">{name}</text>')
    return svg(980, 96, "".join(body), "Tab styles: flat, segmented and rounded")


def ill_rollup():
    body = [wallpaper(860, 300, taskbar=False),
            fence(40, 40, 300, 190, title="Games", items=GAMES),
            label(430, 120, "double-click the title", size=13), arrow(370, 132, 490, 132, color="#fff", white=True),
            fence(520, 40, 300, 0, title="Games", rolled=True, highlight=True),
            cursor(650, 52), label(670, 110, "Point at it (or click) to open it again", size=12.5, weight=400)]
    return svg(860, 300, "".join(body), "Double-clicking the title rolls a fence up to just its title bar")


def ill_portal():
    files = [("i-folder", "Assets"), ("i-folder", "Drafts"), ("i-doc", "Brief.docx"), ("i-pdf", "Proposal v3.pdf"), ("i-sheet", "Timeline.xlsx"),
             ("t-mountains", "Hero image.png"), ("i-txt", "Notes.txt")]
    body = [wallpaper(860, 400, taskbar=False),
            fence(40, 30, 420, 330, title="Projects", items=files, mode="list", breadcrumb="Website redesign", selected={1}),
            '<use href="#i-pdf" x="640" y="180" width="44" height="44"/>', cursor(668, 212),
            arrow(632, 205, 470, 220, bend=-30, dashed=True, color="#fff", white=True),
            label(660, 260, "Drop files to move or copy", size=12.5, weight=400), label(660, 278, "them into the folder", size=12.5, weight=400),
            label(650, 72, "Back to the parent folder", size=12.5, weight=400), arrow(560, 72, 470, 66, color="#fff", white=True)]
    return svg(860, 400, "".join(body), "A folder portal in list view, showing a subfolder with a back bar; files dropped on it go into the folder")


def ill_quickhide():
    left = desktop_scene()
    right = "".join([wallpaper(1200, 640), fence(24, 302, 380, 252, title="Downloads", items=DOWNLOADS, mode="list")])
    body = ['<rect width="980" height="300" rx="10" fill="#1d2030"/>',
            mini(20, 34, 0.35, left), mini(540, 34, 0.35, right),
            arrow(452, 145, 528, 145, color="#fff", white=True),
            '<circle cx="290" cy="215" r="10" fill="none" stroke="#fff" stroke-width="2" opacity=".9"/>',
            '<circle cx="290" cy="215" r="18" fill="none" stroke="#fff" stroke-width="1.5" opacity=".5"/>', cursor(290, 215, .9),
            '<text x="230" y="285" font-size="12.5" fill="#d8dbe6" text-anchor="middle">Double-click empty desktop</text>',
            '<text x="750" y="285" font-size="12.5" fill="#d8dbe6" text-anchor="middle">Fences and icons fade away (Downloads is set to stay visible)</text>']
    return svg(980, 300, "".join(body), "Double-clicking the desktop hides fences and icons; excluded fences stay visible")


def ill_peek():
    body = [wallpaper(860, 440, taskbar=True),
            app_window(40, 24, 780, 360, "Quarterly Report - Word processor"),
            '<rect width="860" height="406" fill="#000" fill-opacity=".45"/>',
            fence(80, 60, 300, 250, title="Programs", items=PROGRAMS[:6]),
            fence(400, 60, 330, 250, tabs=["Documents", "Spreadsheets"], tab_colors=["#3a78d8", "#2f9e5a"], items=DOCUMENTS)]
    k1, w1 = keycap(330, 340, "Win")
    k2, w2 = keycap(330 + w1 + 30, 340, "Space", 90)
    body += [k1, label(330 + w1 + 15, 360, "+", size=16), k2]
    return svg(860, 440, "".join(body), "Peek shows fences above your open windows, which are dimmed")


def ill_pages():
    def page(name, groups):
        return "".join([wallpaper(1200, 640)] + groups)
    work = page("Work", [fence(24, 24, 300, 262, title="Programs", items=PROGRAMS), fence(340, 24, 330, 262, title="Documents", items=DOCUMENTS)])
    play = page("Play", [fence(24, 24, 300, 200, title="Games", items=GAMES), fence(340, 24, 300, 262, title="Images", items=IMAGES, bg="#163a7a", opacity=.62)])
    school = page("School", [fence(24, 24, 360, 262, title="Assignments", items=DOCUMENTS[:4])])
    body = ['<rect width="980" height="270" rx="10" fill="#1d2030"/>']
    for i, (name, scene) in enumerate([("Work", work), ("Play", play), ("School", school)]):
        x = 20 + i * 320
        body.append(mini(x, 24, 0.25, scene))
        body.append(f'<rect x="{x}" y="24" width="300" height="160" fill="none" stroke="{"#fff" if i == 1 else "#5a6077"}" stroke-width="{2 if i == 1 else 1}"/>')
        body.append(f'<text x="{x + 150}" y="208" font-size="13" font-weight="600" fill="#e7e9f2" text-anchor="middle">{name}</text>')
    for i in range(3):
        body.append(f'<circle cx="{474 + i * 16}" cy="232" r="4.5" fill="#fff" fill-opacity="{1 if i == 1 else .35}"/>')
    body += [cursor(344, 100, .9), arrow(352, 112, 430, 112, color="#fff", white=True),
             '<text x="490" y="257" font-size="12" fill="#c7cadb" text-anchor="middle">Press at the screen edge and pull to switch pages</text>']
    return svg(980, 270, "".join(body), "Three pages of fences; pull from the screen edge to switch")


def ill_installer():
    W, H = 560, 420
    body = [f'<rect width="{W}" height="{H}" rx="8" fill="#f0f0f0" stroke="#9aa1ad"/>',
            f'<path d="M0 32V8a8 8 0 0 1 8-8H{W - 8}a8 8 0 0 1 8 8V32z" fill="#ffffff"/>',
            '<text x="16" y="21" font-size="12" fill="#1d2129">openFences Setup</text>',
            f'<path d="M{W - 34} 12l8 8M{W - 34} 20l8-8" stroke="#1d2129" stroke-width="1.2"/>',
            f'<rect y="32" width="{W}" height="64" fill="#fff"/><path d="M0 96h{W}" stroke="#c9ced8"/>',
            '<text x="22" y="56" font-size="14" font-weight="700" fill="#1d2129">Startup</text>',
            '<text x="34" y="78" font-size="12" fill="#1d2129">Choose whether openFences starts with Windows.</text>',
            '<rect x="28" y="124" width="15" height="15" rx="2" fill="#fff" stroke="#5a606b"/><path d="M31 132l3 3 6-7" stroke="#1d2129" stroke-width="1.8" fill="none"/>',
            '<text x="52" y="136" font-size="12.5" fill="#1d2129">Start openFences automatically when I sign in</text>',
            '<text x="52" y="162" font-size="11.5" fill="#4a505b">This becomes the default for every user of this PC. Anyone can</text>',
            '<text x="52" y="178" font-size="11.5" fill="#4a505b">change it later in Configure openFences or from the tray icon.</text>',
            f'<path d="M0 {H - 54}h{W}" stroke="#c9ced8"/>']
    for i, (t, default) in enumerate([("< Back", False), ("Next >", True), ("Cancel", False)]):
        x = W - 270 + i * 88
        body.append(f'<rect x="{x}" y="{H - 40}" width="80" height="26" rx="3" fill="#fdfdfd" stroke="{ACCENT if default else "#9aa1ad"}" stroke-width="{2 if default else 1}"/>')
        body.append(f'<text x="{x + 40}" y="{H - 23}" font-size="12" fill="#1d2129" text-anchor="middle">{t}</text>')
    return svg(W, H, "".join(body), "The installer's Startup page with the 'Start openFences automatically when I sign in' checkbox")


def ill_appearance():
    def row(x, y, items, wrap="", skip=None):
        out = []
        for i, (icon, name) in enumerate(items):
            if i == skip:
                continue
            out.append(icon_item(x + 50 + i * 70, y, icon, name, shortcut=name in ("Notepad", "Paint")))
        inner = "".join(out)
        return f'<g {wrap}>{inner}</g>' if wrap else inner
    items = [("i-notepad", "Notepad"), ("i-paint", "Paint"), ("i-doc", "Report.docx"), ("t-sunset", "Sunset.png")]
    body = [wallpaper(980, 330, taskbar=False),
            fence(20, 30, 300, 150, title="Default"), row(20, 60, items),
            fence(340, 30, 300, 150, title="Icon Tint", bg="#163a7a", opacity=.62), row(340, 60, items, 'filter="url(#tint)"'),
            # Chameleon: faded icons, except the one under the pointer (Paint, slot 1).
            fence(660, 30, 300, 150, title="Chameleon"), row(660, 60, items, 'filter="url(#gray)" opacity=".38"', skip=1),
            icon_item(660 + 50 + 70, 60, "i-paint", "Paint", selected=True, shortcut=True), cursor(745, 92, .9),
            label(170, 230, "Every fence can have its own color", size=12.5, weight=400),
            label(490, 230, "Icons tinted one color, globally or per fence", size=12.5, weight=400),
            label(810, 230, "Icons fade into the wallpaper until you point at them", size=12.5, weight=400)]
    return svg(980, 330, "".join(body), "Fence colors, Icon Tint and Chameleon side by side")


def ill_tray():
    body = ['<rect width="420" height="410" rx="10" fill="#e9ebf0"/>',
            menu(110, 16, ["Peek", "Show desktop", "Hide fences and icons", ("Pages", "sub"), None, "Lock fences", "Take snapshot",
                           "Welcome to openFences…", ("Configure openFences…", "hl"), ("Start openFences when I sign in", "check"), None,
                           "Exit"], width=270),
            '<rect y="364" width="420" height="46" fill="#1d1e26"/>',
            '<path d="M296 380l5 6 5-6" stroke="#c9cdd8" stroke-width="1.5" fill="none" transform="rotate(180 301 383)"/>',
            '<rect x="318" y="374" width="26" height="26" rx="4" fill="#ffffff" fill-opacity=".12"/>',
            '<g transform="translate(320 376) scale(.55)"><rect x="2" y="6" width="22" height="30" rx="3" fill="#3474e2"/><rect x="18" y="16" width="20" height="22" rx="3" fill="#2fbfa4"/></g>',
            '<text x="404" y="392" font-size="11" fill="#e8eaf0" text-anchor="end">10:24</text>', cursor(330, 386, .9)]
    return svg(420, 410, "".join(body), "The openFences tray icon menu")


def ill_desktop_menu():
    entries = [("New fence here", "hl"), "New folder portal here…", None, "Paste", "Refresh", "Arrange desktop icons", ("Pages", "sub"), None,
               "Peek (show fences above windows)", "Hide fences and icons", "Lock fences", "Sort desktop with rules now", None,
               ("New", "sub"), "Display settings", "Personalize", None, "Configure openFences…"]
    body = [wallpaper(360, 560, taskbar=False), menu(40, 20, entries, width=290), cursor(52, 34, .9)]
    return svg(360, 560, "".join(body), "Right-click menu on empty desktop")


def ill_fence_menu():
    entries = ["Rename fence", ("Tabs", "sub"), None, ("View", "sub"), ("Roll up", ""), None, ("Color", "sub"), ("Icon tint", "sub"), None,
               "Keep visible during Quick-Hide", "Show on all pages", "Lock position and size", "Lock contents", None,
               "Select all", "Paste", "Configure openFences…", None, "Delete fence"]
    body = [wallpaper(560, 580, taskbar=False), fence(20, 20, 250, 220, title="Programs", items=PROGRAMS[:4]),
            menu(34, 46, entries, width=250), menu(286, 116, [("Default icon size", "check"), "Small icons", "Medium icons", "Large icons",
                                                             "Extra large icons", None, ("Sort: custom order", "check"), "Sort by name", "Sort by type",
                                                             ("Sort by date modified", "hl")], width=240)]
    return svg(560, 580, "".join(body), "The fence menu, opened from the menu button in the title bar, with the View submenu")


ILLUSTRATIONS = {
    "hero": ill_hero, "anatomy": ill_anatomy, "create": ill_create, "tabs": ill_tabs, "tab-styles": ill_tab_styles, "rollup": ill_rollup,
    "portal": ill_portal, "quickhide": ill_quickhide, "peek": ill_peek, "pages": ill_pages, "installer": ill_installer, "tray": ill_tray,
    "desktop-menu": ill_desktop_menu, "fence-menu": ill_fence_menu, "appearance": ill_appearance,
}


# ---------------------------------------------------------------- build

def inline_images(text):
    def repl(m):
        path = GUIDE / m.group(1)
        mime = "image/png" if path.suffix == ".png" else "image/jpeg"
        data = base64.b64encode(path.read_bytes()).decode("ascii")
        return f'src="data:{mime};base64,{data}"'
    return re.sub(r'src="(images/[^"]+)"', repl, text)


def main():
    text = SRC.read_text(encoding="utf-8")
    text = text.replace("<!--svg-defs-->", defs())
    missing = []

    def repl(m):
        name = m.group(1)
        if name not in ILLUSTRATIONS:
            missing.append(name)
            return m.group(0)
        return ILLUSTRATIONS[name]()

    text = re.sub(r"<!--svg:([a-z0-9-]+)-->", repl, text)
    if missing:
        raise SystemExit(f"unknown illustrations: {missing}")
    text = inline_images(text)
    OUT.write_text(text, encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)} ({len(text) // 1024} KB)")


def export_pages(out_dir):
    """Writes <name>.html (the illustration alone, filling the viewport) and prints "name width height" lines."""
    out = pathlib.Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    for name, fn in ILLUSTRATIONS.items():
        svg_text = fn()
        w, h = re.search(r'viewBox="0 0 ([0-9.]+) ([0-9.]+)"', svg_text).groups()
        page = ('<!doctype html><html><head><meta charset="utf-8"><style>html,body{margin:0;background:#fff}'
                'svg.ill{display:block;width:100vw;height:auto}</style></head><body>' + defs() + svg_text + '</body></html>')
        (out / f"{name}.html").write_text(page, encoding="utf-8")
        print(name, w, h)


if __name__ == "__main__":
    import sys
    if len(sys.argv) == 3 and sys.argv[1] == "--export-pages":
        export_pages(sys.argv[2])
    else:
        main()
