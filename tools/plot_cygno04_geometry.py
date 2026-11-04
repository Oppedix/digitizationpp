"""Draw the nominal CYGNO04 one-side readout geometry from config/ConfigFile_TWIN_CYGNO04.txt.
Usage: python3 tools/plot_cygno04_geometry.py [config] [output.png]   (needs matplotlib)."""
import ast, sys
from pathlib import Path
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle

repo = Path(__file__).resolve().parents[1]
cfg = ast.literal_eval(Path(sys.argv[1] if len(sys.argv) > 1 else repo/"config/ConfigFile_TWIN_CYGNO04.txt").read_text())
out = sys.argv[2] if len(sys.argv) > 2 else repo/"docs/drawings/cygno04_readout_nominal.png"
W, H = cfg["x_dim"], cfg["y_dim"]; h = cfg["dist_gem_pmt"]; r = cfg["pmt_radius"]
fig, (top, side) = plt.subplots(1, 2, figsize=(13, 7.2), gridspec_kw={"width_ratios": [1, 1.25]})

# Top view: GEM plane seen from the cameras (digitization frame, side L).
top.add_patch(Rectangle((cfg["twin_gem_xmin"], cfg["twin_gem_ymin"]), 500, 800, fill=False, lw=2.2, color="k", label="active GEM 500x800"))
for i, c in enumerate("rgb"[:0] or ["#1f77b4", "#2ca02c", "#9467bd"]):
    cx, cy = cfg[f"twin_camera_{i}_x"], cfg[f"twin_camera_{i}_y"]
    top.add_patch(Rectangle((cx - W/2, cy - H/2), W, H, color=c, alpha=.12))
    top.add_patch(Rectangle((cx - W/2, cy - H/2), W, H, fill=False, color=c, lw=1.2, label=f"camera {i} FOV {W:.0f}x{H:.0f}"))
    top.plot(cx, cy, "+", color=c, ms=14, mew=2)
for old in [(x, y) for x in (-190, 190) for y in (-380, 0, 380)]:
    top.add_patch(Circle(old, r, fill=False, ls="--", color="gray"))
for i in range(1, cfg["pmt_number"] + 1):
    x, y = cfg[f"pmt_{i}_x"], cfg[f"pmt_{i}_y"]
    top.add_patch(Circle((x, y), r, color="red", zorder=5))
    top.annotate(f"PMT{i}\n({x:.0f},{y:.1f})", (x, y), xytext=(14 if x > 0 else -14, 0), textcoords="offset points",
                 ha="left" if x > 0 else "right", va="center", fontsize=7.5, color="darkred")
top.plot([], [], "o", color="red", label=f"PMTs (inferred), h={h:.0f} mm")
top.plot([], [], "o", mfc="none", mec="gray", ls="", label="v2 CAD PMTs (6/side, superseded)")
top.set(xlim=(-330, 330), ylim=(-450, 450), aspect="equal", xlabel="x [mm] (50 cm side)", ylabel="y [mm] (80 cm side)",
        title=f"GEM plane, side {cfg['twin_side']} — {cfg['twin_geometry_id']}")
top.legend(loc="lower center", bbox_to_anchor=(.5, -.42), fontsize=7.5, ncol=2)
top.grid(alpha=.3)

# Side view along the drift axis (digitization z, 0 = innermost GEM foil, + towards cathode).
def band(z0, z1, y0, y1, color, label, alpha=.35):
    side.add_patch(Rectangle((z0, y0), z1 - z0, y1 - y0, color=color, alpha=alpha, label=label))
band(0, 500, -400, 400, "#ffd54f", "drift volume 0..500 mm", .25)
band(500, 502.5, -400, 400, "k", "cathode (CAD v3)", .9)
band(-4.2, 0, -404.5, 404.5, "#b87333", "triple GEM (3 foils, 2.1 mm pitch)", .9)
band(-97.8, -90, -600, 600, "#90caf9", "PMMA vessel end wall (v3, |y_cad|=600)")
band(-547.8, -540, -1050, 1050, "#b87333", "inner Cu shield (v3, |y_cad|=1050)", .5)
for y in (397.5, 132.5, -132.5, -397.5):
    side.add_patch(Rectangle((-h - 145, y - 11), 145, 22, color="red"))
side.plot([], [], "s", color="red", label=f"PMT rows (inferred, window at {h:.0f} mm)")
for i in range(3):
    cy = cfg[f"twin_camera_{i}_y"]
    side.add_patch(Rectangle((-697.8 - 41.75, cy - 30), 83.5, 60, color="#555"))
    side.add_patch(Rectangle((-697.8 - 41.75 - 122.3, cy - 42), 122.3, 84, color="#999"))
    side.plot([-697.8, 0], [cy, cy + H/2], color="#1f77b4", lw=.7); side.plot([-697.8, 0], [cy, cy - H/2], color="#1f77b4", lw=.7)
side.plot([], [], "s", color="#555", label="camera lens+body (CYGNO-MC, lens at 698 mm)")
side.set(xlim=(-900, 560), ylim=(-1100, 1100), xlabel="z [mm] (drift; camera side < 0)", ylabel="y [mm]",
         title="Side view (not to scale in shield thickness)")
side.legend(loc="lower center", bbox_to_anchor=(.5, -.42), fontsize=7.5, ncol=2)
side.grid(alpha=.3)
fig.suptitle("CYGNO04 one-side readout used by TWIN — CAD-derived except PMT XY/height (inferred)", fontsize=11)
fig.tight_layout(); fig.savefig(out, dpi=160); print("wrote", out)
