# TWIN one-side paired runs

Enable `twin_cygno04_mode` only with `config/ConfigFile_TWIN_CYGNO04.txt` completed.
Omitting the key (or False) leaves the legacy coordinate mapping, random shifts,
partial-drift response and PMT pipeline in effect. No Geant4 modification is required.

## Geometry evidence and required measurements

Reference: CYGNUS-RD/CYGNO-MC branch g4v11.3_cygno04, commit
7464329b3ed8c99e5e4bdaa84762d7d1db57441a, src/CYGNODetectorConstruction.cc:
https://github.com/CYGNUS-RD/CYGNO-MC/blob/7464329b3ed8c99e5e4bdaa84762d7d1db57441a/src/CYGNODetectorConstruction.cc

Lines 695–724 place the three lenses at AirBox coordinates (0,265,-1200),
(0,0,-1200), (0,-265,-1200) mm for L, and at the same XY with Z=+1200 for R.
Lens placements use `rot`, the identity in this placement block. Camera bodies
are displaced outward by half the 122.3 mm body plus half the 83.5 mm lens
thickness, giving Z=±1302.9 mm. These are mechanical placements, not a calibrated
optical field of view. CAD objects use a separate -90 degree X rotation and
(+470,0,0) mm translation (670–672); do not apply this CAD transform to the
analytically placed camera centres. World placement has further lab transforms.

The side-local GEM XY convention used here is parallel to AirBox XY; Z is drift
from the selected GEM inward. Supply the existing MC axis/offset mapping into
that frame, including any world/lab transforms of your input MC. The template's
projected lens XY centres require confirmation as optical centres. Sensor roll,
handedness/mapping, shared rectangular GEM-plane FOV width/height (`x_dim`,
`y_dim`) and active GEM bounds are REQUIRED; there is no inferred optical
magnification. `twin_camera_i_angle_rad` specifies a proper in-plane rotation:

    X = camera_x + cos(a)*u - sin(a)*v
    Y = camera_y + sin(a)*u + cos(a)*v

Pixel u/v are camera-local; a mirrored sensor image is not modeled. Supply an
orientation consistent with the existing renderer. All three cameras use the
same verified rectangular width/height and existing camera response. This
shared-footprint assumption must be checked before production.

Eight PMT positions/radius/GEM distance remain REQUIRED Stage 1 inputs in the
same side-local XY frame; the reference contains no explicit PMT placements.
They are never changed with camera or drift variant. The source declares
484×784×470 mm fiducial gas (574–576) and a 500×800 mm cathode (499–500).
Neither establishes active GEM area or a physical 500 mm ionization volume.
The 0..500 mm interval is the requested training design, not a verified MC
fiducial depth. Confirm active GEM bounds and the intended 500 mm drift before
setting `twin_geometry_verified=True`. No production-ready geometry is claimed.

## Pairing and containment

Run the same input files four times, with `variant_id=0..3` and separate output
directories. The four `twin_depth_i_min/max` ranges configure absolute weighted
centroid strata in mm; equal endpoints select a fixed depth. Template ranges
are design examples. Keep all other placement configuration identical.

Parent identity is `twin_source_id/basename:input_tree_entry`. Choose a stable,
unique dataset namespace and unique filenames within it. Identity does not
include output run number, file traversal order, event chunks or variant ID.
FNV-1a and mt19937_64 with explicit 53-bit uniform conversion make selection
repeatable without std::hash or thread-local randomness. Camera/XY draws use
parent, selected side and master seed only; depth draws add variant ID.
Physics/noise random draws retain the existing behavior and are not coupled
between separate detector simulations. Do not change geometry, seed or namespace
between paired runs.

A camera is chosen once. Rigid XY shifts must contain *all* original hits in
both its FOV and the GEM. Up to 10000 shifts are tried for rotated footprint/GEM
intersection; failure aborts explicitly with parent ID. There is no camera
resampling, hit removal or truth truncation. Z translation is target minus
ionization-weighted centroid: weights are the exact energy deposits passed to
the response (quenched NR deposits for nuclear recoils). Negative/nonfinite
weights, zero total weight, and any translated hit outside 0..500 abort before
mutating the source vectors. A parent failing one variant is not a usable
four-depth pair; the error identifies that variant. Exposure cuts must be off.
Tracks below the existing ionization threshold abort explicitly in TWIN mode.

One camera response is synthesized in camera-local XY. Light voxels from that
same realization are transformed back to common GEM XY for the eight fixed
PMTs. Existing diffusion can extend beyond nominal truth bounds; containment
applies to ionization hits, not a post-diffusion clipping of the physical cloud.
PMT time reference, gains, QE, pulse model and noise mechanism are unchanged.

TWIN-only ROOT fields: `camera_id`, `selected_side`, `parent_id`, `variant_id`,
`target_depth_mm`, `actual_depth_mm`, `visible_ionization_weights`, `source_x/y/z`,
`source_px/py/pz`. The source coordinate/momentum vectors are unmodified MC
inputs. Existing particle class and energy fields remain. Existing
`placement_d{x,y,z}_mm` store the extra rigid shifts **after** MC mapping/offsets,
in common GEM coordinates. Legacy geometric/pixel metadata is in the selected
camera frame. ROOT branch addresses are reset before their storage is destroyed.

## Checks and visualization

    c++ -std=c++17 -Wall -Wextra -Iinclude tests/test_twin.cxx src/PMTHits.cxx -o /tmp/test_twin
    /tmp/test_twin /tmp/twin-synthetic.svg
    python3 tests/run_pmt_tests.py --without-opencv

The first command needs only standard C++; its SVG shows three footprints,
eight fixed PMTs and example translated parents in a common XY plane. It is
explicitly SYNTHETIC, not a production detector drawing. Blue=FOV, red=PMTs,
green=hits; black=GEM. It checks pairing, centroid, containment, failed placement,
rigid separations and geometric PMT collection under source displacement.
The second suite uses ROOT and explicitly stubs only the unavailable OpenCV
noise IFFT. Full Geant4/ROOT/OpenCV detector integration still requires the
production dependencies, verified geometry and input MC files.
