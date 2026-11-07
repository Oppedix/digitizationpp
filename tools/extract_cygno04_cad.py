"""Print the CYGNO04 camera, PMT and GEM positions in the digitizer frame from the readout CAD.
Usage: freecadcmd tools/extract_cygno04_cad.py   with CYGNO04_CAD=/path/to/CAMERAS+PMT+GEM.FCStd
(freecadcmd ships with FreeCAD, e.g. /Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd).

CAD frame of the 'Cygno04 - Detector' part: +X = GEM normal towards the optics, Y = 50 cm GEM side,
Z = 80 cm GEM side. Digitizer frame: x = CAD Y, y = CAD Z, distances along CAD X."""
import os
import FreeCAD

doc = FreeCAD.openDocument(os.environ["CYGNO04_CAD"])
to_det = doc.getObject("Cygno04___Detector").getGlobalPlacement().inverse()

def box(prefix):
    """Bounding boxes, in the detector frame, of the parts whose label starts with prefix."""
    out = []
    for o in doc.Objects:
        if o.TypeId == "Part::Feature" and o.Label.startswith(prefix):
            s = o.Shape.copy()
            s.Placement = to_det.multiply(o.getGlobalPlacement())
            out.append(s.BoundBox)
    return out

gems = sorted(box("Gem top"), key=lambda b: b.XMin)
pmts = box("PMT-1inch")
lenses = box("EHD-25085-C")
# Camera assembly origins lie on the optical axes (lens boxes are not symmetric about them).
cams = [to_det.multiply(o.getGlobalPlacement()).Base for o in doc.Objects
        if o.TypeId == "App::Part" and o.Label.startswith("Camera")]
assert len(gems) == 3 and len(pmts) == 8 and len(lenses) == 3 and len(cams) == 3
gem3 = gems[-1].XMax                                   # camera-side face of the camera-side foil
g = gems[0]
print(f"GEM foils (CAD X of drift-side faces): {[round(b.XMin, 2) for b in gems]}")
print(f"GEM foil outline: x {g.YMin:.2f}..{g.YMax:.2f}, y {g.ZMin:.2f}..{g.ZMax:.2f} mm")
print(f"Camera axes (x, y): {sorted((round(c.y, 2) + 0., round(c.z, 2)) for c in cams)}")
print(f"Lens front - GEM3: {min(b.XMin for b in lenses) - gem3:.1f} mm")
print(f"PMT centres (x, y): {sorted((round(b.Center.y, 2), round(b.Center.z, 2)) for b in pmts)}")
print(f"PMT window - GEM3 (dist_gem_pmt): {min(b.XMin for b in pmts) - gem3:.1f} mm")
print(f"PMT body diameter: {pmts[0].YLength:.2f} mm, length {pmts[0].XLength:.1f} mm")
