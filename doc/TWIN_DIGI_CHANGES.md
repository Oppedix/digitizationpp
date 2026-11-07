# `twin-cygno04` vs `main`: what changes in the digitizer

Branch `twin-cygno04` of `Oppedix/digitizationpp` = **upstream `CYGNUS-RD/digitizationpp` `main`
(`f250be5`, 2026-11-04) + the additions below**. Fork `main` is an exact copy of upstream `main`.
Line numbers refer to this branch. They are written as `file:line` and link to the line on GitHub.

**Rule kept throughout:** with no new keys, or the new keys at their defaults, the digitizer does what upstream
`main` does. Every addition is either a new option or runs only under `twin_cygno04_mode: True`.

## 1. Three ways to run it

| Mode | Config | What you get |
|---|---|---|
| **Standard (upstream)** | `config/ConfigFile_new.txt` (LIME, 4 PMTs) | Same output as upstream `main` (§2) |
| **CYGNO04, 1 camera + 8 PMTs** | `config/ConfigFile_CYGNO04_8PMT.txt` | Standard digitizer with `pmt_number: 8`: Quest2 central camera and 8 waveform pairs `wav_fast/slow_pmt1..8`. All legacy options still work (pedestal `bckg`, `Vignetting`, `redpix_output`, `digitizers`, random shifts, `fixed_seed`, PSD noise) |
| **TWIN ML datasets** | `config/ConfigFile_TWIN_CYGNO04.txt` | `twin_cygno04_mode: True`: camera choice and paired depths, `ml_*`/`obs_*` truth records ([TWIN_SCHEMA.md](TWIN_SCHEMA.md)). |

## 2. Verified identical to upstream `main`

I built upstream `main` and this branch with the same dependency stubs: no ROOTANA, and the OpenCV
noise IFFT replaced by zero noise. Both ran on the same 7-event MC fixture (ER, He NR, a GEM-crossing
track, a track crossing `max_drift`, an empty event, a sub-`ion_pot` event, a long track) with upstream's
`ConfigFile_new.txt`, `fixed_seed: True` and `bckg: False`.

* `redpix_output: True`: the 35 scalar and redpix branches and all `param_dir` values are identical.
  PMT waveform lengths are identical too; their values are random in both versions (non-seeded `random_device`).
* `redpix_output: False`: all 7 camera images are **pixel-identical**.
* The only differences:
  * (a) a new branch `pmt_signal_valid`;
  * (b) `particle_type` of the empty event. Upstream reads `pdgID_hits[0]` of an empty vector, which is
    undefined behaviour; this branch writes 0 ([DigitizationRunner.cxx:870](../src/DigitizationRunner.cxx#L870)).
* The run had to use `min_drift: 1e-6`, because upstream `main` **aborts on the first event with its own
  default `min_drift: 0`** (§3).

## 3. Upstream PR #59 (`min_drift` / `max_drift`) merged, one bug fixed

Upstream replaced `drift_length` with the response interval `[min_drift, max_drift]`, with defaults 0 and 510 mm.
Its check `min_drift <= 0 → throw` rejects the default 0, so every run with `ConfigFile_new.txt` stops.
This branch moves the check into one helper, [DriftRange.h:11](../include/DriftRange.h#L11), used by both the
runner and TWIN:

* **Rules:** `0 <= min_drift < max_drift`, and both values must be finite numbers. The defaults stay 0 and 510.
* **Order check:** a swapped pair (`min_drift: 300`, `max_drift: 200`) or an empty interval is rejected.
  Upstream would accept it and silently select no deposits.
* **Removed key:** an old `drift_length` key now raises a clear error instead of being silently ignored.
* **When it runs:** once in the constructor ([DigitizationRunner.cxx:54](../src/DigitizationRunner.cxx#L54)), before any
  output is opened. The interval is applied at [:1131](../src/DigitizationRunner.cxx#L1131)–[:1134](../src/DigitizationRunner.cxx#L1134).
* **TWIN:** requires exactly 0 and 500 mm ([TwinPlacement.h:47](../include/TwinPlacement.h#L47)).
* **Tests:** [test_twin.cxx:98](../tests/test_twin.cxx#L98) covers the defaults, the shipped values, the swapped and empty
  intervals, negative/NaN/inf/non-numeric values, `drift_length`, and the TWIN 0–500 requirement.
* **Configs:** every config states the rule on its `min_drift`/`max_drift` lines
  ([ConfigFile_new.txt:25](../config/ConfigFile_new.txt#L25)–26).
* Upstream's second commit (`a079ff8`, setSeed revert) changes nothing here; this branch already had that `setSeed`.

The three earlier drift-boundary fixes (fractional negative z, z ≥ 0 only, upper cut) are already upstream.

## 4. Changes by file

### PMTs: 1–8 channels and two noise options
| Where | Change |
|---|---|
| [PMTHits.cxx:17](../src/PMTHits.cxx#L17), [PMTHits.h:44](../include/PMTHits.h#L44) | `validateGeometry`: `pmt_number` must be an integer 1–8. Every `pmt_N_x/y`, `pmt_radius` and `dist_gem_pmt` must be finite (a `'REQUIRED'` placeholder fails). Called at [DigitizationRunner.cxx:60](../src/DigitizationRunner.cxx#L60), before any download |
| [PMTSignal.cxx:163](../src/PMTSignal.cxx#L163)–165 | Waveforms are made for `pmt_1..pmt_number`; upstream hard-codes 4 |
| [DigitizationRunner.cxx:704](../src/DigitizationRunner.cxx#L704), [:782](../src/DigitizationRunner.cxx#L782), [:1249](../src/DigitizationRunner.cxx#L1249) | Branches `wav_fast_pmtN`/`wav_slow_pmtN` for N = 1..`pmt_number`, plus `pmt_signal_valid` (1 = light collected). LIME branch names are unchanged |
| [DigitizationRunner.cxx:802](../src/DigitizationRunner.cxx#L802) | Events without light: **empty** waveforms as on main. Only TWIN zero-fills them to the window length |
| [PMTSignal.cxx:196](../src/PMTSignal.cxx#L196), [:238](../src/PMTSignal.cxx#L238) | **New** `pmt_noise` (default `True` = upstream). `False` gives noiseless waveforms (SPE + 12-bit quantization) and downloads nothing |
| [PMTSignal.cxx:203](../src/PMTSignal.cxx#L203), [:245](../src/PMTSignal.cxx#L245) | **New** `pmt_noise_channels`, e.g. `'1;2;3;4;1;2;3;4'`: the PSD template each PMT uses. It is needed because the cloud only has `LIME_{fast,slow}_noise_pmt_1..4.txt` (`CYGNO04_*`, `GIN_*` and `MAN_*` return 404, checked 2026-11-05). Each PMT still gets independent random phases |
| [PMTSignal.cxx:271](../src/PMTSignal.cxx#L271) | A missing or too-short PSD now raises an error; upstream read past the end of the array |
| [DigitizationRunner.cxx:237](../src/DigitizationRunner.cxx#L237) | String-valued keys (`pmt_noise_channels`, `twin_*` text) are kept out of the numeric `param_dir` dump |

### TWIN mode (`twin_cygno04_mode: True` only)
| Where | Change |
|---|---|
| [TwinPlacement.h:14](../include/TwinPlacement.h#L14) | Deterministic choice of one of 3 cameras per parent track (FNV-1a + mt19937_64), and one rigid XY shift keeping **all** hits inside that FOV and the GEM. Z is moved to a target ionisation-weighted centroid per `variant_id` (0–3). The camera and XY are shared by the 4 variants, so the same track appears at 4 depths |
| [TwinPlacement.h:73](../include/TwinPlacement.h#L73)/[:77](../include/TwinPlacement.h#L77) | `local`/`global`: the camera is rendered in its own frame ([DigitizationRunner.cxx:1071](../src/DigitizationRunner.cxx#L1071)). PMT light goes back to global GEM XY ([:1220](../src/DigitizationRunner.cxx#L1220)), so all 8 PMTs see the event where it really is |
| [TwinML.h:147](../include/TwinML.h#L147), [DigitizationRunner.cxx:819](../src/DigitizationRunner.cxx#L819) | Per-event ML record: placed hits, QF energies, generated direction (joined to `tree1` by `eventnumber`), class, IDs, seeds, reject reason. Output contract: [TWIN_SCHEMA.md](TWIN_SCHEMA.md) |
| [DigitizationRunner.cxx:1029](../src/DigitizationRunner.cxx#L1029) | Response weights are `energyDep_hits_QF` (ER and NR treated alike) |
| [DigitizationRunner.cxx:1198](../src/DigitizationRunner.cxx#L1198), [:1209](../src/DigitizationRunner.cxx#L1209), [:1237](../src/DigitizationRunner.cxx#L1237) | PMT photoelectrons = photons × `photons_per_el` × `twin_pmt_quantum_efficiency` (upstream uses a fixed 0.0136). Signed drift times; t = 0 is the first detected photoelectron, not the smallest z |
| [TwinML.h:134](../include/TwinML.h#L134), [PMTHits.cxx:53](../src/PMTHits.cxx#L53), [PMTSignal.cxx:58](../src/PMTSignal.cxx#L58), [TrackProcessor.cxx:709](../src/TrackProcessor.cxx#L709), [DigitizationRunner.cxx:81](../src/DigitizationRunner.cxx#L81) | Named seed streams (charge, collection, SPE, electronics) and one thread, so a rerun is bit-reproducible |
| [PMTSignal.cxx:183](../src/PMTSignal.cxx#L183), [TwinML.h:171](../include/TwinML.h#L171) | `twin_noise_mode: 'synthetic'`: seeded Gaussian PMT noise and camera read noise, added offline |
| [TrackProcessor.cxx:101](../src/TrackProcessor.cxx#L101), 203/292/408/484, [:571](../src/TrackProcessor.cxx#L571), [:581](../src/TrackProcessor.cxx#L581) | Photon counts stay as `double` (no per-voxel `int` truncation). Legacy keeps the `int` truncation; this is bit-identical (§2) |
| [DigitizationRunner.cxx:509](../src/DigitizationRunner.cxx#L509), [:578](../src/DigitizationRunner.cxx#L578), [:1065](../src/DigitizationRunner.cxx#L1065), [:1258](../src/DigitizationRunner.cxx#L1258) | Required MC branches are checked up front. A failing event is saved as an invalid record instead of aborting the run |
| [CMakeLists.txt:98](../CMakeLists.txt#L98) | The git commit (with `-dirty` if there are local changes) is stored in every output file (`digitizer_commit`) |

## 5. New settings (all optional for the standard digitizer)

| Key | Default | Meaning |
|---|---|---|
| `min_drift`, `max_drift` | 0, 510 mm | Upstream keys. Deposits with z outside the interval are excluded from the response. Rule: `0 <= min_drift < max_drift`. `drift_length` is no longer accepted |
| `pmt_number` | 4 | Now 1–8 |
| `pmt_noise` | `True` | `False` gives noiseless PMT waveforms |
| `pmt_noise_channels` | identity | `';'`-separated PSD template channel per PMT |
| `twin_cygno04_mode` | `False` | Enables TWIN. All `twin_*` keys, `variant_id` and the `REQUIRED` per-run inputs are documented inline in `ConfigFile_TWIN_CYGNO04.txt` |

Each key is commented where it is used: [ConfigFile_new.txt:25](../config/ConfigFile_new.txt#L25), [:102](../config/ConfigFile_new.txt#L102)–104, [:135](../config/ConfigFile_new.txt#L135); [ConfigFile_CYGNO04_8PMT.txt:1](../config/ConfigFile_CYGNO04_8PMT.txt#L1)–12 (how to switch camera) and [:115](../config/ConfigFile_CYGNO04_8PMT.txt#L115)–122.

## 6. CYGNO04 geometry and PMT positions (`CYGNO04_CAD_v1`)

![CYGNO04 PMT positions](cygno04_pmt_positions.png)

Regenerate the figure with `python3 tools/plot_cygno04_geometry.py`.

Cameras, PMTs and the GEM stack come from the **CYGNO04 readout CAD** provided by the CYGNO collaboration
(`CAMERAS+PMT+GEM`, the same assembly as `.FCStd`, `.step` and `.stl`; not stored in the repository, 18–85 MB).
[tools/extract_cygno04_cad.py](../tools/extract_cygno04_cad.py) reads it with FreeCAD and prints every CAD value
in the table:

```
CYGNO04_CAD=/path/to/CAMERAS+PMT+GEM.FCStd freecadcmd tools/extract_cygno04_cad.py
```

**Frames.** In the CAD part `Cygno04 - Detector`, +X is the GEM normal pointing to the optics, Y is the 50 cm
GEM side and Z the 80 cm side. Digitizer: x = CAD Y, y = CAD Z, and z is the drift distance from the
drift-side GEM foil (z > 0 towards the cathode).

| Quantity | Value | Source |
|---|---|---|
| GEM stack | 3 foils at 2.1 mm pitch; the camera-side foil (GEM3) ends 4.3 mm from the drift-side face of GEM1 | CAD |
| GEM foil outline | 529 × 829 mm, centred within 1.2 mm of the middle camera axis | CAD |
| Active GEM | x ∈ ±250, y ∈ ±400 mm | `CYGNUS-RD/geometry` `cygno_04_v3` @ `dbf18a3` |
| Drift | 500 mm (GEM1 at MC z = −502.2 on side L) | `cygno_04_v3`, CYGNO-MC `g4v11.3_cygno04` @ `7464329` |
| Cameras | 3 × ORCA-Quest2 with EHD-25085-C lens (25 mm, f/0.85). Optical axes normal to the GEM at x = 0, **y = +266.7 / 0 / −266.7 mm**. Lens front 666.6 mm from GEM3 | CAD |
| FOV per camera | 532.48 × 299.52 mm (0.130 mm/px), 4096-px axis along x | [arXiv:2510.01646](https://arxiv.org/abs/2510.01646) p.70, consistent with the CAD (below) |
| **PMTs** | 8 × 1-inch tubes (body Ø 26 mm, window radius 11.5 mm). **x = ±150; y = ±133.35, ±400.05 mm; window 505.1 mm from GEM3** (`dist_gem_pmt`) | CAD |
| `pmt_radius` | 11 mm | R7378A effective photocathode; fits inside the 11.5 mm CAD window |
| MC → digitizer | `x,y,z`, `z_offset 502.2` (side L, `/CYGNO/lab/select NoCave`). Side R: `-x`, `-z`, 501.2 | `cygno_04_v3` + CYGNO-MC |

**Layout.** The PMTs form two columns at x = ±150 mm. The four rows sit on the camera FOV seams
(y = ±133.35 mm, half the 266.7 mm camera spacing) and on the outer FOV edges (y = ±400.05 mm, 1.5 × the spacing).

**FOV cross-check.** For a thin 25 mm lens with the Quest2 sensor (4096 × 4.6 µm) at the C-mount flange
distance (17.526 mm behind the lens flange), the CAD sensor–GEM3 distance of 769.6 mm gives a demagnification of
28.8, i.e. **0.132 mm/px**. This agrees with the 0.130 mm/px FOV within 2 %; 0.152 mm/px would need about 880 mm.

**Sensor orientation.** The 4096-px axis must lie along x: with the sensor turned by 90°, the 299.52 mm FOV side
could not cover the 500 mm GEM side.

**Numbering.** PMT 1–8 and camera 0–2 (figure) are the digitizer's convention. The CAD carries no readout channel
numbers, so map them to DAQ channels before comparing with data.

**One-camera 8-PMT config.** PMT XY are given relative to the **simulated** camera centre (central camera →
same as GEM XY). For another camera, subtract its y (±266.7 mm) and set `y_offset` (see the config header).

Other sources:
* [arXiv:2512.01762](https://arxiv.org/abs/2512.01762)
* [arXiv:2603.22595](https://arxiv.org/abs/2603.22595)
* [IDM2026 slides, S. Piacentini](https://indico.capa.unizar.es/event/40/contributions/862/attachments/618/1369/IDM2026_Piacentini.pdf) p.10
* [CYGNUS-RD/WIKI-documentation CYGNO-04](https://github.com/CYGNUS-RD/WIKI-documentation/tree/main/CYGNO-04)

## 7. TWIN pairing rules (summary)

* Run the same inputs 4 times with `variant_id = 0..3` into separate output folders, and change nothing else.
* The parent ID is `twin_source_id/<ROOT file UUID>:eventnumber`. The camera and XY draws depend only on the
  parent, the side and `twin_master_seed`; the depth draw also depends on `variant_id`.
* A shift must keep **all** hits inside the FOV and the GEM; up to 10000 attempts are made. Hits are never
  removed or truncated. Any hit outside 0–500 mm after the Z move rejects that variant, and the record says why.
* `exposure_time_effect` must be `False`. NR-only filtering and the `ion_pot` early exit are bypassed, so
  every parent gives a record.

## 8. Known limits / not tested here

* **Not tested here:** the full CMake build with ROOTANA and OpenCV (pedestal download, measured PSD noise
  IFFT), and real CYGNO-MC files. All tests in §9 and the §2 comparison use stubs for these two libraries only.
* **Camera storage:** TWIN `obs_camera_counts` stores the full 4096 × 2304 noisy image, about 75 MB per event.
  It must be made sparse before production.
* **Calibration gaps:**
  * there is no Quest2 vignetting map;
  * the PMT electronics/SPE constants and noise templates are LIME values;
  * the TWIN PMT trigger is ideal (first photoelectron).
* **Inherited from upstream:** the PMT acceptance r²h²/(4R⁴) has one extra cos θ compared with the exact
  small-disk solid angle, and photon counts are rounded per voxel.
* **Not in the readout CAD:** the cathode and field cage. The drift length, `z_offset` and active GEM area come
  from `cygno_04_v3` and CYGNO-MC. In the CAD, GEM1 lies 510.0 mm from the detector's central plane: if the cathode
  is on that plane, the drift gap is 510 mm rather than 500.
* **Lens aperture:** the CAD lens is f/0.85, while `camera_aperture` is 0.95 (LIME value). The camera light
  factor scales as 1/aperture², so f/0.85 gives 25 % more light; the overall scale `A` still needs calibration.
* **Pedestals with Quest2:** `bckg: True` needs Quest2 pedestal runs. The code stops on a Fusion/Quest mismatch.

## 9. Tests

```
python3 tests/run_pmt_tests.py [--without-opencv]   # 4 and 8 PMTs, noise switch/remap, legacy vs TWIN reset, ROOT round trip
python3 tests/run_twin_ml_tests.py                  # actual runner in TWIN mode, 8 waveforms, reproducibility
c++ -std=c++17 -Iinclude tests/test_twin.cxx src/PMTHits.cxx -o /tmp/t && /tmp/t
c++ -std=c++17 -Iinclude tests/test_cygno04_geometry.cxx src/ConfigManager.cxx src/PMTHits.cxx $(root-config --cflags --libs) -o /tmp/g && /tmp/g   # configs match the CAD values
```
