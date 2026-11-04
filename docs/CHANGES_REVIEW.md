# TWIN changes in this fork — what they are and whether they are sound

Branch `twin-cygno04`, reviewed 2026-11-04 against the upstream base
`b2a7d3f` (CYGNUS-RD/digitizationpp, merge of PR #58). Six commits sit on top:

| # | Commit | Author | What it does | Verdict |
|---|---|---|---|---|
| 1 | `f72396d` | D. Fiorina | Negative-drift guard in `NelGEM2` used `int x` lambda → hits with −1 < z < 0 mm were truncated to 0 and accepted. Now `double`. | Correct bug fix. |
| 2 | `8a879ae` | D. Fiorina | Only deposits with z ≥ 0 are passed to the detector response; full-track metadata (length, vertices…) still uses all hits. Resets `N_primaries_reaching_GEMs` for empty input. | Correct. Tracks crossing the GEM plane no longer abort the event. |
| 3 | `57b338f` | D. Fiorina | Optional `drift_length` upper cut: response deposits restricted to `[0, drift_length]`. Invalid values throw. | Correct; omitting the key keeps legacy behaviour. |
| 4 | `54e8456` | G. M. Oppedisano | PMT code generalised from a hard-coded 4 channels to `pmt_number` ∈ 1..8: loops in `PMTSignal::simulated_signals`, `gen_noise`, ROOT branches `wav_{fast,slow}_pmtN`, new `pmt_signal_valid`, strict geometry validation before any output/noise download. | Correct. 4-PMT LIME output is byte-compatible in naming; waveforms are now always the declared length (zero-filled for skipped events). |
| 5 | `9a823af` | G. M. Oppedisano | `TwinPlacement.h`: deterministic camera choice (FNV-1a + mt19937_64 keyed on parent ID, side, master seed), rigid XY shift fully containing the track in the camera FOV **and** active GEM, Z shift to a target ionisation-weighted centroid from one of four depth strata (`variant_id`). Camera rendered in camera-local XY; PMT voxels mapped back to global GEM XY. | Correct and matches the plan (§3.2, §5.3). Camera/XY are identical across the 4 variants; only Z and detector noise change. |
| 6 | `ab08c85` | G. M. Oppedisano | `TwinML.h` + runner: `ml_*` truth branches (placed hits, QF weights, centroid, generated direction from `tree1`, class, IDs, seeds), file-local `tree1`↔`nTuple` join with duplicate checks, energy-convention checks, observable-light PMT trigger, offline synthetic Gaussian noise for camera and PMTs, named per-event RNG streams, single-thread response, full config + commit written to the file. Rejected events kept with `ml_valid=0` and a reason. | Correct in logic; see caveats below. |

## Verification performed

On macOS with ROOT 6.40 (no OpenCV/ROOTANA available locally):

```
c++ -std=c++17 -Iinclude tests/test_twin.cxx src/PMTHits.cxx -o test_twin && ./test_twin out.svg
python3 tests/run_pmt_tests.py --without-opencv
python3 tests/run_twin_ml_tests.py
```

All three pass: paired placement/containment/rigidity, 4- and 8-channel PMT
ROOT round-trip, and the real runner on synthetic MC fixtures (same seed →
identical output, different seed → different output). **Not tested here:** the
full CMake build with OpenCV/ROOTANA, measured-PSD PMT noise, and real
CYGNO-MC input files.

Code-reading checks that matter physically:

* PMT voxel coordinates are in mm in the same frame as the hits
  (`TrackProcessor.cxx:208`), so `twin->global(camera_id, voxel.x, voxel.y)`
  correctly restores common GEM XY for the PMT acceptance.
* TWIN arrival time is `z / v_drift` (signed, no `abs`) and the trigger
  subtracts the *first detected photoelectron* over all 8 channels, so the
  absolute Geant4/drift time is not a feature; only relative timing and
  diffusion carry depth.
* PMT photon yield in TWIN mode is `photons_per_el × twin_pmt_quantum_efficiency`
  applied to the same saturated GEM charge the camera uses — consistent with the
  legacy hard-coded `0.0136` (= 0.07 × ~0.19).

## Caveats / open issues (not bugs in the logic, but must be resolved before production)

1. **Dense camera storage (important).** In TWIN mode `obs_camera_counts` holds
   *every* pixel of the noisy image (`TwinML.h`, `Record::image`). With the real
   Quest2 sensor (4096 × 2304) this is ~9.4 M doubles ≈ 75 MB/event, ~30 TB for the
   400 k-observation dataset. The tests only pass because they use a 32 × 32
   camera. Options: store float32 crops around an *observed* (noise-thresholded)
   ROI, or store only the seed and regenerate noise in Python with a portable RNG.
2. **Vignetting.** The only maps shipped are for Fusion (LIME) and MANGO; with
   `Camera_type: 'Quest2'` the runner rejects the Fusion map. The CYGNO04 config
   therefore sets `Vignetting: False` (stated, deferred response uncertainty).
3. **PMT acceptance model (upstream).** `PhotonPropagation::pmt_hits` uses
   `r²h²/(4R⁴)`, i.e. an extra `cos θ` relative to an isotropic point source
   (`r²h/(4R³)`). It is the existing physics and is kept, but it
   down-weights PMTs at large angle; worth a dedicated check against the
   LIME/CYGNO04 PMT data.
4. **Per-voxel rounding (upstream).** `round(n_photons × qe)` per voxel discards
   light in faint voxels instead of Poisson-sampling it; biased at low energy.
5. **Primary PDG must be declared** (`twin_primary_pdg`); `tree1` has no PDG, so
   each MC file must contain a single primary species.
6. **Ideal trigger.** The time origin is the first photoelectron, without
   threshold/discriminator modelling. Apply common random time shifts in training
   (plan §4 "Timing").
7. The camera/PMT electronics constants (gain, SPE shape, noise RMS) are LIME
   values or required placeholders, not CYGNO04 calibrations.

## Conclusion

The changes are appropriate and implement plan §3–§4 without introducing a
second renderer. The remaining production blockers were the frozen geometry
(now filled from CAD/literature, see [GEOMETRY_CYGNO04.md](GEOMETRY_CYGNO04.md))
and item 1 above.
