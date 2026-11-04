# TWIN / CYGNO04 documentation for this fork

This fork (`Oppedix/digitizationpp`, branch `twin-cygno04`) adapts the CYGNO
digitizer to produce the TWIN training data. Each observation is one CYGNO04 side:
**one of three camera images plus all eight PMT waveforms**. The same parent
track is digitized at four drift depths so its depth changes but its shape does not.
The scientific plan is kept outside this repo (`TWIN_CYGNO04_Compact_Implementation_Plan.md`)
and is expected to evolve. This folder documents only what the digitizer does.

| Document | Content |
|---|---|
| [CHANGES_REVIEW.md](CHANGES_REVIEW.md) | The six commits on top of upstream, what each changes, the review verdict and the open caveats |
| [GEOMETRY_CYGNO04.md](GEOMETRY_CYGNO04.md) | The nominal CYGNO04 camera/PMT/GEM geometry, its sources, how the PMT positions were inferred, and the collected drawings |
| [drawings/](drawings/) | Generated geometry figure plus CAD/paper/slide drawings of CYGNO-04 |
| [../doc/TWIN.md](../doc/TWIN.md) | Detailed placement, pairing and containment rules (stage 2) |
| [../doc/TWIN_SCHEMA.md](../doc/TWIN_SCHEMA.md) | ROOT output contract `ml_*` / `obs_*` (stage 3) |

## Changes in brief

1. **Drift boundaries** (D. Fiorina): deposits outside `[0, drift_length]` are
   dropped from the response and the full-track metadata is kept. Includes the fix
   for fractional negative z.
2. **Eight PMTs**: `pmt_number` 1–8 everywhere (light collection, SPE/noise,
   ROOT branches) with strict geometry validation. LIME 4-PMT output is unchanged.
3. **Camera placement + paired depths** (`twin_cygno04_mode`): a deterministic
   camera per parent and one rigid XY shift contained in that FOV and the GEM,
   shared by all 4 variants. Z is placed at a target ionisation-weighted centroid
   per `variant_id`. The camera is rendered in its local frame; PMTs see global XY.
4. **ML records**: truth (placed hits, QF weights, generated direction, class,
   IDs, seeds), observable-light PMT trigger, offline seeded noise, rejection
   bookkeeping, full config and commit stored in each file.
5. **CYGNO04 geometry (this update)**: `ConfigFile_TWIN_CYGNO04.txt` now holds
   `CYGNO04_nominal_v1` (Quest2, 0.130 mm pixels, 3 cameras at y = 0/±265 mm, 8 inferred
   PMT positions, side-L MC mapping). Adds `tests/test_cygno04_geometry.cxx` and
   `tools/plot_cygno04_geometry.py`.

## Still needed before production

* Confirm the PMT positions/height and the pixel size (0.130 vs 0.152 mm) with the
  hardware group, then set `twin_geometry_verified: True`.
* Replace the dense per-event camera storage (≈75 MB/event at 4096 × 2304);
  see CHANGES_REVIEW.md, item 1.
* Fill the per-run inputs still marked `REQUIRED`: `twin_source_id`,
  `twin_mc_commit`, `twin_primary_pdg`, gas pressure/temperature, `twin_pmt_noise_rms_v`.
* Full build with OpenCV/ROOTANA and real CYGNO-MC ER/He/C/F files on CNAF.
