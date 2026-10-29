# EIGHT ECHO (Studio Wiga) - notice PDF bilingue (anglais complet puis francais complet).
# Usage : python3 tools/manual.py <fichier PDF de sortie>
# Illustrations : rendu reel de la pedale (tools/manual/render_base.png, meme repere 578 x 896 que background.png)
# dont les LED des memoires et le capuchon SYNC sont recomposes a chaque generation avec leds_on.png et sync_on.png
# du depot (couleur livree dans le plugin) ; schemas repris de la notice precedente (tools/manual/).
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.platypus import (BaseDocTemplate, PageTemplate, NextPageTemplate, Frame, Paragraph, Spacer, Image as RLImage,
                                Table, TableStyle, PageBreak, CondPageBreak, KeepTogether)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
A = os.path.join(ROOT, "tools", "manual")
OUT = sys.argv[1]
TMP = os.path.join(os.path.dirname(os.path.abspath(OUT)), "_manual_tmp"); os.makedirs(TMP, exist_ok=True)
VERSION = next(l.split("VERSION")[1].strip(" )\n") for l in open(os.path.join(ROOT, "CMakeLists.txt")) if l.startswith("project("))

DJ = "/usr/share/fonts/truetype/dejavu/"
for n, f in (("R", "DejaVuSans"), ("B", "DejaVuSans-Bold"), ("I", "DejaVuSans-Oblique"), ("M", "DejaVuSansMono-Bold")):
    pdfmetrics.registerFont(TTFont(n, DJ + f + ".ttf"))
pdfmetrics.registerFontFamily("R", normal="R", bold="B", italic="I", boldItalic="B")

ICE = colors.HexColor("#5FB8F0"); ICE_L = colors.HexColor("#A9DCFF")
DARK = colors.HexColor("#1B2430"); GREY = colors.HexColor("#5E6875"); LIGHT = colors.HexColor("#EAF4FB"); ORANGE = colors.HexColor("#F07A1A")

# ---------------------------------------------------------------- rendu de la pedale
def P_(name): return os.path.join(ROOT, name)
bg = Image.open(P_("background.png")).convert("RGBA")
leds_on = Image.open(P_("leds_on.png")).convert("RGBA")
sync_on = Image.open(P_("sync_on.png")).convert("RGBA")
SLOT_Y = [124, 175, 228, 279, 331, 383, 435]

def pedal(led=(1.0, 0, 0, 0, 0, 0, 0), sync=1.0):
    """Rendu 578 x 896 : rendu reel + LED memoires / SYNC recomposees avec les images du depot (PluginEditor.cpp)."""
    im = Image.open(os.path.join(A, "render_base.png")).convert("RGBA")
    im.paste(bg.crop((0, 90, leds_on.width, 470)), (0, 90))                 # LED eteintes (plaque d'origine)
    im.paste(bg.crop((139, 426, 139 + sync_on.width, 426 + sync_on.height)), (139, 426))
    for i, b in enumerate(led):
        if b <= 0.01: continue
        top = 95 if i == 0 else int(0.5 * (SLOT_Y[i - 1] + SLOT_Y[i])); bot = 465 if i == 6 else int(0.5 * (SLOT_Y[i] + SLOT_Y[i + 1]))
        part = leds_on.crop((0, top - 90, leds_on.width, bot - 90))
        a = np.array(part); a[..., 3] = (a[..., 3] * b).astype(np.uint8)
        im.alpha_composite(Image.fromarray(a), (0, top))
    if sync > 0.01:
        a = np.array(sync_on); a[..., 3] = (a[..., 3] * sync).astype(np.uint8)
        im.alpha_composite(Image.fromarray(a), (139, 426))
    im.putalpha(bg.getchannel("A"))                                          # silhouette du boitier + jacks
    return im

base = pedal()
S = 2   # agrandissement des illustrations

def on_white(im):
    w = Image.new("RGB", im.size, (255, 255, 255)); w.paste(im, (0, 0), im); return w

# vue d'ensemble annotee
ov = base.resize((base.width * S, base.height * S), Image.LANCZOS)
M = 120
canvas = Image.new("RGB", (ov.width + 2 * M, ov.height + 20), (255, 255, 255)); canvas.paste(ov, (M, 10), ov)
dr = ImageDraw.Draw(canvas)
fb = ImageFont.truetype(DJ + "DejaVuSans-Bold.ttf", 30)
TARGETS = {1: (204, 157, "L", 175), 2: (394.5, 157, "R", 175), 3: (203.5, 329, "L", 345), 4: (394.5, 328, "R", 345),
           5: (396.5, 495, "R", 495), 6: (40, 117, "L", 117), 7: (204, 491.5, "L", 495), 8: (144.5, 621, "L", 621),
           9: (434, 620, "R", 621), 10: (141, 679, "L", 690), 11: (436, 676, "R", 690), 12: (143, 775, "L", 775),
           13: (438, 775, "R", 760), 14: (290, 778, "R", 835), 15: (482, 57, "R", 57)}
OR = (240, 122, 26)
for n, (x, y, side, ly) in TARGETS.items():
    tx, ty = M + x * S, 10 + y * S
    cx = 44 if side == "L" else canvas.width - 44
    cy = 10 + ly * S
    ex = M - 6 if side == "L" else M + ov.width + 6
    dr.line([(cx, cy), (ex, cy), (tx, ty)], fill=OR, width=4)
    dr.ellipse((tx - 7, ty - 7, tx + 7, ty + 7), fill=OR)
    dr.ellipse((cx - 30, cy - 30, cx + 30, cy + 30), fill=OR)
    dr.text((cx, cy + 1), str(n), font=fb, fill=(255, 255, 255), anchor="mm")
ov_img = os.path.join(TMP, "overview.png"); canvas.save(ov_img)

# memoires : bande des 7 LED (memoire 1 pleine) et memoire modifiee (LED adoucie)
def strip(led):
    p = pedal(led=led).crop((0, 70, 140, 470)); return on_white(p.resize((p.width * S, p.height * S), Image.LANCZOS))
m1, m2 = strip((1, 0, 0, 0, 0, 0, 0)), strip((0, 0, 0.5, 0, 0, 0, 0))
mem = Image.new("RGB", (m1.width * 2 + 30, m1.height), (255, 255, 255)); mem.paste(m1, (0, 0)); mem.paste(m2, (m1.width + 30, 0))
mem_img = os.path.join(TMP, "memories.png"); mem.save(mem_img)

# vignettes LED : lentilles (inchangees) + LED memoire et SYNC recomposees
def thumb(im, box, size=180):
    c = on_white(im.crop(box)); return c.resize((size, int(size * c.height / c.width)), Image.LANCZOS)
th_mem = thumb(pedal(), (0, 94, 100, 146), 180); th_mem_path = os.path.join(TMP, "th_mem.png"); th_mem.save(th_mem_path)
th_sync = thumb(pedal(), (164, 451, 244, 531), 180); th_sync_path = os.path.join(TMP, "th_sync.png"); th_sync.save(th_sync_path)

# couverture : pedale detouree, reflet au sol, halo bleu glace tres doux
cut = base.crop(base.getbbox()); cw, ch = cut.size; pad = int(cw * 0.18)
cv = Image.new("RGBA", (cw + 2 * pad, int(ch * 1.32) + pad), (0, 0, 0, 255))
yy, xx = np.mgrid[0:cv.height, 0:cv.width].astype(np.float64)
glow = np.exp(-(((xx - cv.width / 2) / (cw * 0.62)) ** 2 + ((yy - (pad + ch * 0.45)) / (ch * 0.5)) ** 2)) * 38
edge = np.clip(np.minimum.reduce([xx, cv.width - 1 - xx, yy, cv.height - 1 - yy]) / (pad * 0.9), 0, 1); glow *= edge ** 2
cv = Image.alpha_composite(cv, Image.fromarray(np.dstack([glow * 0.37, glow * 0.72, glow * 0.94, np.full_like(glow, 255)]).astype(np.uint8), "RGBA"))
refl = np.array(cut.transpose(Image.FLIP_TOP_BOTTOM)).astype(np.float64)
refl[..., 3] *= (np.clip(1 - np.arange(ch) / (ch * 0.22), 0, 1) ** 1.6 * 0.22)[:, None]
cv.alpha_composite(Image.fromarray(refl.astype(np.uint8), "RGBA"), (pad, pad + ch + 4)); cv.alpha_composite(cut, (pad, pad))
cv = cv.crop((0, 0, cv.width, pad + int(ch * 1.2)))
cover_img = os.path.join(TMP, "cover.png"); cv.convert("RGB").save(cover_img)

def img(path, width):
    im = Image.open(path); return RLImage(path, width=width, height=width * im.height / im.width)

# ---------------------------------------------------------------- styles
st = {
    "h1": ParagraphStyle("h1", fontName="B", fontSize=16, leading=20, textColor=DARK, spaceBefore=8, spaceAfter=7, keepWithNext=1),
    "h2": ParagraphStyle("h2", fontName="B", fontSize=10.5, leading=14, textColor=colors.HexColor("#2A7FC0"), spaceBefore=6, spaceAfter=2, keepWithNext=1),
    "p": ParagraphStyle("p", fontName="R", fontSize=9.2, leading=13.4, textColor=DARK, spaceAfter=5),
    "cap": ParagraphStyle("cap", fontName="I", fontSize=7.8, leading=10.5, textColor=GREY, alignment=TA_CENTER, spaceBefore=2, spaceAfter=6),
    "cell": ParagraphStyle("cell", fontName="R", fontSize=8.2, leading=10.8, textColor=DARK),
    "cellb": ParagraphStyle("cellb", fontName="B", fontSize=8.2, leading=10.8, textColor=DARK),
    "hcell": ParagraphStyle("hcell", fontName="B", fontSize=8.2, leading=10.8, textColor=colors.white),
    "note": ParagraphStyle("note", fontName="R", fontSize=8.8, leading=12.6, textColor=DARK),
    "num": ParagraphStyle("num", fontName="B", fontSize=9, leading=11, textColor=colors.white, alignment=TA_CENTER),
    "cover": ParagraphStyle("cover", fontName="B", fontSize=30, leading=36, textColor=colors.white, alignment=TA_CENTER),
    "coversub": ParagraphStyle("coversub", fontName="R", fontSize=10.5, leading=15, textColor=colors.HexColor("#C9CED6"), alignment=TA_CENTER),
    "coverlang": ParagraphStyle("coverlang", fontName="B", fontSize=8.5, leading=11, textColor=ICE_L),
}
def P(t, s="p"): return Paragraph(t, st[s])
W = A4[0] - 36 * mm
def bullets(items): return [P("&bull;&nbsp;&nbsp;" + i) for i in items]
def table(rows, widths, header=True, first_bold=True):
    data = [[P(c, "hcell" if (header and r == 0) else ("cellb" if (first_bold and ci == 0) else "cell")) for ci, c in enumerate(row)] for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    s = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.3, colors.HexColor("#D5DEE6")),
         ("TOPPADDING", (0, 0), (-1, -1), 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 3)]
    if header: s += [("BACKGROUND", (0, 0), (-1, 0), DARK)]
    s += [("BACKGROUND", (0, r), (-1, r), colors.HexColor("#F6F9FC")) for r in range(2 if header else 1, len(rows), 2)]
    t.setStyle(TableStyle(s)); return t
def note(t):
    tb = Table([[P(t, "note")]], colWidths=[W]); tb.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), LIGHT), ("LINEBEFORE", (0, 0), (0, -1), 2.5, ICE),
                                                                     ("LEFTPADDING", (0, 0), (-1, -1), 8), ("TOPPADDING", (0, 0), (-1, -1), 6), ("BOTTOMPADDING", (0, 0), (-1, -1), 6)]))
    return KeepTogether([Spacer(1, 2), tb, Spacer(1, 5)])
def numbered(rows):
    data = []
    for n, t in rows:
        b = Table([[P(str(n), "num")]], colWidths=[6 * mm], rowHeights=[6 * mm])
        b.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), ORANGE), ("VALIGN", (0, 0), (-1, -1), "MIDDLE"), ("LEFTPADDING", (0, 0), (-1, -1), 0),
                               ("RIGHTPADDING", (0, 0), (-1, -1), 0), ("TOPPADDING", (0, 0), (-1, -1), 0), ("BOTTOMPADDING", (0, 0), (-1, -1), 0)]))
        data.append([b, P(t)])
    tb = Table(data, colWidths=[9 * mm, W - 9 * mm]); tb.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0)]))
    return tb

# ---------------------------------------------------------------- donnees (code livre : PluginProcessor.cpp)
PRESETS = [("Clean Slapback", "STANDARD"), ("Rockabilly Tape", "TAPE"), ("Doubler", "STANDARD"), ("Bread & Butter", "STANDARD"),
           ("Dotted 8th Arena", "STANDARD"), ("Ping Pong Eighths", "STANDARD"), ("BBD Crunch Lead", "ANALOG"), ("Dark BBD Wash", "ANALOG"),
           ("Floating Tape Solo", "TAPE"), ("Long Tape Ambience", "TAPE"), ("Soft Ballad", "WARM"), ("Jazz Room", "WARM"),
           ("Lush Chorus Echo", "MOD"), ("80s Stereo Lead", "MOD"), ("Echo Hall", "+RV"), ("Cathedral", "+RV"), ("Heaven Swell", "SHIM"),
           ("Octave Haze", "SHIM"), ("Dream Swell (hold)", "WARP"), ("Space Freeze (hold)", "WARP"), ("Backwards Solo", "REVERSE"),
           ("Backwards Pad", "REVERSE"), ("Machine Gun (hold)", "GLT"), ("Stutter Groove (hold)", "GLT"), ("Granular Buzz (hold)", "GLT"), ("Looper", "LOOP")]
MODES_T = [("LOOP", "40 s (20 s stereo)<br/>40 s"), ("ANALOG", "20–800 ms<br/>40–1600 ms"), ("STANDARD", "20–800 ms<br/>40–1600 ms"),
           ("TAPE", "20–800 ms<br/>40–1600 ms"), ("WARM", "20–800 ms<br/>40–1600 ms"), ("REVERSE", "300–5000 ms<br/>600–10000 ms"),
           ("+RV", "20–800 ms<br/>40–1600 ms"), ("SHIM", "200–800 ms<br/>400–1600 ms"), ("MOD", "20–800 ms<br/>40–1600 ms"),
           ("WARP", "20–800 ms<br/>40–1600 ms"), ("GLT", "10–400 ms<br/>20–800 ms")]
MIDI_ROWS = [("E.LEVEL", 20, "cont"), ("MIX", 21, "cont"), ("FEEDBACK", 22, "cont"), ("TIME", 23, "cont"), ("MODE", 24, "sel"),
             ("X2", 25, "tog"), ("SYNC", 26, "tog"), ("EXP", 27, "exp"), ("BYPASS", 80, "fs"), ("TAP", 81, "tap"),
             ("PREV MEMORY", 82, "act"), ("NEXT MEMORY", 83, "act")]

from manual_text import T   # textes EN / FR (tools/manual_text.py)

# ---------------------------------------------------------------- mise en page
def section(lang):
    t = T[lang]; f = [NextPageTemplate("p")]
    f += [Spacer(1, 1 * mm), P(t["lang"], "coverlang"), Spacer(1, 3 * mm), img(os.path.join(A, "title.png"), W * 0.62), Spacer(1, 4 * mm),
          img(cover_img, W * 0.50), Spacer(1, 3 * mm), P(t["sub"], "coversub"), Spacer(1, 1.5 * mm), P(t["ver"], "coversub"), Spacer(1, 7 * mm),
          img(P_("logo.png"), 50 * mm), PageBreak()]
    f.append(P(t["toc_t"], "h1"))
    toc = [[P(a, "cell"), P(b, "cell")] for a, b in zip(t["toc"][:9], t["toc"][9:] + [""])]
    tt = Table(toc, colWidths=[W / 2, W / 2]); tt.setStyle(TableStyle([("TOPPADDING", (0, 0), (-1, -1), 1), ("BOTTOMPADDING", (0, 0), (-1, -1), 1), ("LEFTPADDING", (0, 0), (-1, -1), 0)]))
    f.append(tt)
    f.append(P(t["s1"], "h1")); f += [P(x) for x in t["intro"]]
    f.append(P(t["s2"], "h1")); f += bullets(t["inst"]); f.append(P(t["where_h"], "h2")); f.append(P(t["where"]))
    f.append(PageBreak())
    f.append(P(t["s3"], "h1")); f.append(img(ov_img, W * 0.70)); f.append(P(t["ov_cap"], "cap"))
    f.append(numbered(t["ov"]))
    f.append(P(t["s4"], "h1"))
    for h, x in t["knobs"]: f.append(P(h, "h2")); f.append(P(x))
    f.append(note(t["mouse"]))
    f.append(CondPageBreak(110 * mm))
    f.append(P(t["s5"], "h1")); f.append(img(os.path.join(A, "modes_dial.png"), W * 0.48)); f.append(P(t["dial_cap"], "cap"))
    rows = [t["modes_hdr"]] + [[m, r, t["modes"][i][0], t["modes"][i][1]] for i, (m, r) in enumerate(MODES_T)]
    f.append(table(rows, [20 * mm, 26 * mm, W - 46 * mm - 44 * mm, 44 * mm]))
    f.append(P(t["s6"], "h1")); f.append(P(t["fs_intro"]))
    f.append(img(os.path.join(A, "footswitches_" + lang + ".jpg"), W)); f.append(P(t["fs_cap"], "cap"))
    f.append(table(t["fs_rows"], [30 * mm, 38 * mm, 50 * mm, W - 118 * mm]))
    f.append(note(t["tempo_mode"]))
    f.append(P(t["s7"], "h1"))
    icons = [("lens_red", 0), ("lens_off", 1), ("lens_green", 2), ("lens_orange", 3), ("lens_red", 4), ("lens_green", 5), ("lens_ice", 6)]
    rows = [[P(t["led_hdr"][0], "hcell"), P(t["led_hdr"][1], "hcell"), P(t["led_hdr"][2], "hcell")]]
    for nm, i in icons:
        rows.append([RLImage(os.path.join(A, nm + ".png"), width=8 * mm, height=8 * mm), P(t["leds"][i][0], "cellb"), P(t["leds"][i][1], "cell")])
    rows.append([RLImage(th_mem_path, width=12 * mm, height=12 * mm * th_mem.height / th_mem.width), P(t["leds"][7][0], "cellb"), P(t["leds"][7][1], "cell")])
    rows.append([RLImage(th_sync_path, width=8 * mm, height=8 * mm), P(t["leds"][8][0], "cellb"), P(t["leds"][8][1], "cell")])
    rows.append([RLImage(os.path.join(A, "x2.png"), width=14 * mm, height=14 * mm * 200 / 360), P("X2", "cellb"), P(t["x2"], "cell")])
    lt = Table(rows, colWidths=[16 * mm, 36 * mm, W - 52 * mm])
    lt.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "MIDDLE"), ("BACKGROUND", (0, 0), (-1, 0), DARK), ("LINEBELOW", (0, 0), (-1, -1), 0.3, colors.HexColor("#D5DEE6")),
                            ("TOPPADDING", (0, 0), (-1, -1), 2), ("BOTTOMPADDING", (0, 0), (-1, -1), 2)]))
    f.append(lt)
    f.append(CondPageBreak(120 * mm))
    f.append(P(t["s8"], "h1"))
    mt = Table([[img(mem_img, 52 * mm), [P(x) for x in t["mem"]] + [P(t["mem_cap"], "cap")]]], colWidths=[56 * mm, W - 56 * mm])
    mt.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0)])); f.append(mt)
    f.append(P(t["s9"], "h1")); f += [P(x) for x in t["tempo"]]
    f.append(img(os.path.join(A, "taps_" + lang + ".png"), W * 0.78)); f.append(P(t["taps_cap"], "cap"))
    f.append(P(t["s10"], "h1")); f += bullets(t["twist"])
    f.append(P(t["s11"], "h1")); f += [P(x) for x in t["looper"]]
    f.append(img(os.path.join(A, "looper_" + lang + ".png"), W * 0.72)); f.append(P(t["looper_cap"], "cap")); f.append(note(t["loop_note"]))
    f.append(P(t["s12"], "h1")); f.append(img(os.path.join(A, "stereo.png"), W * 0.70)); f.append(P(t["stereo_cap"], "cap")); f += bullets(t["stereo"])
    f.append(P(t["s13"], "h1")); f += bullets(t["menu"]); f.append(P(t["win_h"], "h2")); f.append(P(t["win"]))
    f.append(CondPageBreak(150 * mm))
    f.append(P(t["s14"], "h1")); f.append(P(t["midi_intro"]))
    rows = [t["midi_hdr"]] + [[n, "CC %d" % c, t["types"][k]] for n, c, k in MIDI_ROWS]
    f.append(table(rows, [32 * mm, 24 * mm, W - 56 * mm]))
    f.append(P(t["learn_h"], "h2")); f.append(numbered(list(enumerate(t["learn"], 1))))
    f += bullets(t["learn_more"])
    f.append(P(t["auto_h"], "h2")); f.append(P(t["auto"]))
    f.append(CondPageBreak(160 * mm))
    f.append(P(t["s15"], "h1"))
    f.append(table([t["pre_hdr"]] + [[n, m, t["pre"][i]] for i, (n, m) in enumerate(PRESETS)], [40 * mm, 22 * mm, W - 62 * mm]))
    f.append(P(t["pre_cap"], "cap"))
    f.append(CondPageBreak(140 * mm))
    f.append(P(t["s16"], "h1")); f.append(img(os.path.join(A, "signal_flow.png"), W * 0.82)); f.append(P(t["flow_cap"], "cap"))
    f.append(KeepTogether([table(t["spec"], [34 * mm, W - 34 * mm], header=False)]))
    f.append(P(t["s17"], "h1")); f += bullets(t["trouble"])
    f.append(PageBreak())
    return f

def deco(lang):
    def _d(c, doc):
        t = T[lang]; c.saveState()
        c.setFillColor(DARK); c.rect(0, A4[1] - 11 * mm, A4[0], 11 * mm, stroke=0, fill=1)
        c.setFillColor(ICE); c.rect(0, A4[1] - 11.8 * mm, A4[0], 0.8 * mm, stroke=0, fill=1)
        c.drawImage(P_("logo.png"), 18 * mm, A4[1] - 8.2 * mm, width=32 * mm, height=32 * mm * 57 / 510, mask="auto")
        c.setFont("R", 7.5); c.setFillColor(colors.HexColor("#C9D3DD")); c.drawRightString(A4[0] - 18 * mm, A4[1] - 7 * mm, "EIGHT ECHO · " + t["hdr"])
        c.setFillColor(GREY); c.drawString(18 * mm, 10 * mm, "Eight Echo (Studio Wiga) · v" + VERSION)
        c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page)); c.restoreState()
    return _d
def cover_bg(c, doc):
    c.saveState(); c.setFillColor(colors.black); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(ICE); c.rect(0, A4[1] - 3 * mm, A4[0], 3 * mm, stroke=0, fill=1); c.restoreState()

doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=18 * mm, bottomMargin=17 * mm,
                      title="EIGHT ECHO - Manual / Notice v" + VERSION, author="Studio Wiga")
fr_ = lambda i: Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id=i)
doc.addPageTemplates([PageTemplate(id="cover", frames=[fr_("c")], onPage=cover_bg),
                      PageTemplate(id="p", frames=[fr_("p")], onPage=deco("en")),
                      PageTemplate(id="cover_fr", frames=[fr_("c2")], onPage=cover_bg),
                      PageTemplate(id="p_fr", frames=[fr_("p2")], onPage=deco("fr"))])
en = section("en"); fr = section("fr")
fr[0] = NextPageTemplate("p_fr")
en[-1:] = [NextPageTemplate("cover_fr"), PageBreak()]
doc.build(en + fr[:-1])
print(OUT, VERSION)
