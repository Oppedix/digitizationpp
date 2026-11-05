"""Draw the CYGNO04 one-side PMT positions (and camera FOVs) from a digitizer config.
Usage: python3 tools/plot_cygno04_geometry.py [config] [output.png]   (needs matplotlib)
Default: config/ConfigFile_TWIN_CYGNO04.txt -> docs/cygno04_pmt_positions.png"""
import ast, sys
from pathlib import Path
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle

repo = Path(__file__).resolve().parents[1]
cfg = ast.literal_eval(Path(sys.argv[1] if len(sys.argv) > 1 else repo/"config/ConfigFile_TWIN_CYGNO04.txt").read_text())
out = sys.argv[2] if len(sys.argv) > 2 else repo/"docs/cygno04_pmt_positions.png"

INK, INK2, MUTED, GRID = "#1f1f1e", "#5b5b57", "#8c8b85", "#e4e3dd"
CAM = ["#2a78d6", "#eb6834", "#1baf7a"]          # validated categorical slots 1-3
W, H, h, r = cfg["x_dim"], cfg["y_dim"], cfg["dist_gem_pmt"], cfg["pmt_radius"]
gx0, gx1, gy0, gy1 = cfg["twin_gem_xmin"], cfg["twin_gem_xmax"], cfg["twin_gem_ymin"], cfg["twin_gem_ymax"]
cams = [(cfg[f"twin_camera_{i}_x"], cfg[f"twin_camera_{i}_y"]) for i in range(3)]
pmts = [(cfg[f"pmt_{i}_x"], cfg[f"pmt_{i}_y"]) for i in range(1, cfg["pmt_number"] + 1)]

plt.rcParams.update({"font.size": 10, "axes.edgecolor": MUTED, "axes.labelcolor": INK2,
                     "xtick.color": INK2, "ytick.color": INK2})
fig = plt.figure(figsize=(12, 9.2), facecolor="white")
top = fig.add_axes([0.06, 0.08, 0.50, 0.84])
side = fig.add_axes([0.64, 0.08, 0.34, 0.84])

# ---- Top view: GEM plane as seen from the cameras (side L digitization frame)
for (cx, cy), c, i in zip(cams, CAM, range(3)):
    top.add_patch(Rectangle((cx - W/2, cy - H/2), W, H, color=c, alpha=.08, lw=0))
    top.add_patch(Rectangle((cx - W/2, cy - H/2), W, H, fill=False, ec=c, lw=1.6))
    top.plot(cx, cy, marker="+", color=c, ms=12, mew=2)
    top.text(cx, cy - 22, f"camera {i}: lens at y = {cy:+.0f}\nFOV {W:.0f} × {H:.0f} mm",
             color=INK2, fontsize=8.5, va="top", ha="center")
top.add_patch(Rectangle((gx0, gy0), gx1 - gx0, gy1 - gy0, fill=False, ec=INK, lw=2.2))
top.text(0, gy0 + 40, f"active GEM {gx1-gx0:.0f} × {gy1-gy0:.0f} mm", ha="center", va="bottom", color=INK, fontsize=9)
for y in sorted({p[1] for p in pmts}):                       # PMT rows = camera-tile boundaries
    top.axhline(y, color=MUTED, lw=.8, ls=(0, (4, 3)), zorder=1)
    top.text(274, y + 4, f"y = {y:+.1f}", va="bottom", ha="left", fontsize=8.5, color=INK2)
for x in sorted({p[0] for p in pmts}):
    top.axvline(x, color=MUTED, lw=.8, ls=(0, (4, 3)), zorder=1)
    top.text(x, 452, f"x = {x:+.0f}", ha="center", va="bottom", fontsize=8.5, color=INK2)
for i, (x, y) in enumerate(pmts, 1):
    top.add_patch(Circle((x, y), 15, color="white", zorder=4))   # 2px-style surface ring
    top.add_patch(Circle((x, y), 12.5, color=INK, zorder=5))
    top.text(x, y, str(i), color="white", ha="center", va="center", fontsize=9, weight="bold", zorder=6)
top.set(xlim=(-290, 345), ylim=(-470, 475), aspect="equal",
        xlabel="x [mm]  (50 cm GEM side, 4096-px camera axis)", ylabel="y [mm]  (80 cm GEM side)")
top.set_title("Top view: 8 PMTs at the camera-tile corners (inferred)", loc="left", color=INK, fontsize=11.5)
for s in ("top", "right"): top.spines[s].set_visible(False)

# ---- Side view along drift (z = 0 innermost GEM foil, z > 0 towards cathode; optics at z < 0)
side.add_patch(Rectangle((0, gy0), 500, gy1 - gy0, color="#eda100", alpha=.14, lw=0))
side.text(250, 0, "drift\n0 – 500 mm", ha="center", va="center", color=INK2, fontsize=9)
side.add_patch(Rectangle((500, gy0), 4, gy1 - gy0, color=INK2))
side.text(502, gy1 + 12, "cathode", fontsize=8.5, color=INK2, ha="center")
side.add_patch(Rectangle((-4.2, gy0), 4.2, gy1 - gy0, color=INK))
side.text(-4, gy1 + 12, "GEM", ha="right", fontsize=8.5, color=INK)
for y in sorted({p[1] for p in pmts}):
    side.add_patch(Rectangle((-h - 120, y - r), 120, 2*r, color=INK))
side.annotate("", xy=(-h, -470), xytext=(0, -470), arrowprops=dict(arrowstyle="<->", color=INK, lw=1))
side.text(-h/2, -482, f"PMT window – GEM  {h:.0f} mm (inferred)", ha="center", va="top", fontsize=8.5, color=INK)
lens = -697.8
for (cx, cy), c in zip(cams, CAM):
    side.add_patch(Rectangle((lens - 84, cy - 30), 84, 60, color=c))
    side.plot([lens, 0], [cy, cy + H/2], color=c, lw=.8); side.plot([lens, 0], [cy, cy - H/2], color=c, lw=.8)
side.annotate("", xy=(lens, 520), xytext=(0, 520), arrowprops=dict(arrowstyle="<->", color=INK2, lw=1))
side.text(lens/2, 532, "lens – GEM 698 mm (CYGNO-MC)", ha="center", va="bottom", fontsize=8.5, color=INK2)
side.set(xlim=(-880, 560), ylim=(-560, 600), xlabel="z [mm]  (drift; cameras/PMTs at z < 0)", ylabel="y [mm]")
side.set_title("Side view (y–z)", loc="left", color=INK, fontsize=11.5)
for s in ("top", "right"): side.spines[s].set_visible(False)
for ax in (top, side): ax.grid(color=GRID, lw=.6); ax.set_axisbelow(True)

fig.text(0.06, 0.965, f"CYGNO04 one side ({cfg['twin_side']}) — geometry {cfg['twin_geometry_id']}: "
         "PMT XY/height inferred, cameras/GEM/drift from CAD and CYGNO-MC", fontsize=10.5, color=INK2)
fig.savefig(out, dpi=150); print("wrote", out)
