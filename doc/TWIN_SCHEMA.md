# Using the digitizer with PMTs and TWIN mode, and the TWIN ROOT output (schema 1)

This page has two parts:

* **Usage** (below): how to run the digitizer as upstream, with or without PMTs, with 1–8 PMTs, as CYGNO04
  one camera + 8 PMTs, and in TWIN mode for paired ML datasets.
* **TWIN output contract** (from [File-local MC join](#file-local-mc-join-and-visible-energy) on): every branch
  TWIN mode writes, its type and unit, and which ones may be used as model inputs.

What changed in the code, line by line, is in [TWIN_DIGI_CHANGES.md](TWIN_DIGI_CHANGES.md).

## Usage

### Build and run

The build is the upstream one (see `README.md`):

```bash
cmake -B build-dir -S . && cmake --build build-dir
./build-dir/digitizationpp <config file> -I <folder with MC .root files> -O <output folder>
```

Every mode below is chosen **only by the config file**. There are no extra command-line options.

### Choose a mode

| I want to… | Start from | Keys that matter |
|---|---|---|
| **A. Run the digitizer exactly as upstream** | `config/ConfigFile_new.txt` (LIME) | Nothing to change. Leave `twin_cygno04_mode` absent or `False` and leave out `pmt_noise_channels`. The output is the upstream one, plus one branch `pmt_signal_valid` |
| **B. Camera only, no PMTs** | any config | `pmt_mode: False`. No waveforms, no `pmt_signal_valid`, no PMT noise download |
| **C. Change the number of PMTs (1–8)** | `ConfigFile_new.txt` | `pmt_number: N` and `pmt_1_x/y … pmt_N_x/y`, `dist_gem_pmt`, `pmt_radius` |
| **D. CYGNO04: one camera + all 8 PMTs, ordinary digitization** | `config/ConfigFile_CYGNO04_8PMT.txt` | Ready to run. CYGNO04 PMT positions from the CAD |
| **E. TWIN paired-depth ML datasets** | `config/ConfigFile_TWIN_CYGNO04.txt` | Fill every `REQUIRED` value, then run 4 times with `variant_id` 0–3 (below) |

Modes A–D are the ordinary digitizer: every upstream option keeps working, including the camera pedestal
(`bckg`), `Vignetting`, `redpix_output`, `exposure_time_effect`, random shifts, `fixed_seed`, `digitizers`,
and measured PMT noise. Mode E fixes some of them (see below).

### PMT options in modes A–D

| Key | Values | Effect |
|---|---|---|
| `pmt_mode` | `True` / `False` | Simulate the PMTs or not |
| `pmt_number` | 1–8 | Writes `wav_fast_pmt1..N` and `wav_slow_pmt1..N`. Each PMT needs `pmt_N_x` and `pmt_N_y` (mm, GEM frame) |
| `dist_gem_pmt`, `pmt_radius` | mm | PMT window height above the light-emitting GEM, and photocathode radius |
| `digitizers` | `Fast`, `Slow`, `Both` | Which digitizer waveforms are filled |
| `pmt_noise` | `True` (default) / `False` | `True`: noise from the measured PSD templates `<detector>_{fast,slow}_noise_pmt_N.txt` (downloaded if missing). `False`: clean waveforms, i.e. single-photoelectron pulses and 12-bit quantization only, nothing downloaded |
| `pmt_noise_channels` | e.g. `'1;2;3;4;1;2;3;4'` | Which template each PMT uses. Needed for more than 4 PMTs, because only `LIME_*_noise_pmt_1..4.txt` exist. Each PMT still gets its own random noise |
| `detector` | `LIME`, `GIN`, `MAN`, `CYGNO04`, `CUSTOM` | Prefix of the noise template files |

Common PMT recipes:

* **No PMT noise:** `pmt_noise: False`.
* **8 PMTs with measured noise:** `detector: 'LIME'`, `pmt_noise_channels: '1;2;3;4;1;2;3;4'`.
* **Only the fast digitizer:** `digitizers: 'Fast'`.
* **Camera pedestal on or off:** `bckg: True` or `False`. This is independent of the PMTs. With `Camera_type: 'Quest2'`
  the pedestal run must also be a Quest2 run; the code stops on a mismatch.

**Mode D, other cameras.** `ConfigFile_CYGNO04_8PMT.txt` simulates the central camera. To simulate the upper or
lower camera, subtract its centre (y = +266.7 or −266.7 mm) from every `pmt_N_y` and set `y_offset` to
−266.7 or +266.7, as explained in the config header.

### Mode E: TWIN paired-depth datasets

TWIN places each MC track in front of one of the 3 CYGNO04 cameras and renders it at one of 4 depths.
The same track gets the same camera and the same XY position in all 4 depth variants, so the 4 records
differ only in depth.

**Before the first run, add the vignetting map.** `ConfigFile_TWIN_CYGNO04.txt` has `Vignetting: True` with
`Vig_Map: vignette_ORCA_calibrated.root` (Davide–Melba map). The file is **not in this repository**: it is kept
on the CNAF cluster and must be copied by hand into `VignettingMap/` of the checkout that runs the digitizer.
Without it the run stops at once with `Vignetting map not found`.

1. **Fill the `REQUIRED` values** in a copy of `ConfigFile_TWIN_CYGNO04.txt`:
   * `twin_source_id`: a stable name for this sample;
   * `twin_mc_commit`: the 40-character commit of the CYGNO-MC build that made the input;
   * `twin_primary_pdg`: the generated primary, e.g. 11 for e⁻, 1000020040 for He;
   * `twin_gas_pressure_mbar` and `twin_gas_temperature_k`;
   * `twin_pmt_noise_rms_v`.
2. **Build with CMake from a git checkout.** TWIN refuses an unversioned build, because every output stores
   the digitizer commit.
3. **Run the same input 4 times**, changing only `variant_id`, each into its own output folder:

   ```bash
   for v in 0 1 2 3; do
     sed "s/^'variant_id': 0,/'variant_id': $v,/" my_twin.txt > my_twin_v$v.txt
     ./build-dir/digitizationpp my_twin_v$v.txt -I mc_in -O out_v$v
   done
   ```

4. **Side R:** set `twin_side: 'R'`, `MC_xaxis: '-x'`, `MC_zaxis: '-z'`, `z_offset: 501.2`.
5. **Extra noise replicas** of the same tracks: change `twin_response_id` (or `twin_detector_seed`).
   Camera choice and placement do not change.

TWIN fixes these settings and stops at start-up if any differs:

| Setting | Required value | Why |
|---|---|---|
| `pmt_mode`, `pmt_number` | `True`, 8 | All 8 PMTs are part of every record |
| `digitizers` | `Both` | Fast and slow waveforms in every record |
| `bckg` | `False` | Camera noise is TWIN's own Gaussian noise; a pedestal would add it twice |
| `twin_noise_mode` | `synthetic` | Seeded Gaussian camera and PMT noise, no measured-PSD download |
| `exposure_time_effect` | `False` | Rolling-shutter cuts would remove truth |
| `min_drift`, `max_drift` | 0, 500 | Every hit must lie in the CYGNO04 drift volume |
| `twin_geometry_verified` | `True` | Cameras, PMTs and GEM taken from the CYGNO04 CAD |
| `twin_gas` | `HeCF4_60_40` | The only gas the drift model supports |

`Parallel_threads` is ignored: TWIN runs with one thread, so a rerun is bit-identical.

**How TWIN uses the 8 PMTs.** In every record:
* the light reaches all 8 PMTs from the event's true GEM position, not from the camera's frame;
* photoelectrons = photons × `photons_per_el` × `twin_pmt_quantum_efficiency`;
* time zero is the first photoelectron seen on any of the 8 PMTs;
* each channel gets seeded Gaussian noise of `twin_pmt_noise_rms_v`.

Records without light still have 8 full-length noise-only waveforms. Invalid records have all-zero waveforms.

### Reading TWIN output

Each output file holds the tree `event_info` (one row per MC event) and the file-level objects
`twin_configuration`, `digitizer_commit`, `root_version`, `reproducibility`, `ionization_convention`.
Stack the PMT channels and reshape the camera like this:

```python
import ROOT, numpy as np
f = ROOT.TFile.Open("out_v0/digi_Run00001.root"); t = f.Get("event_info")
for e in t:
    if not e.ml_valid: continue
    fast = np.stack([np.asarray(getattr(e, f"wav_fast_pmt{i}")) for i in range(1, 9)])  # [8, fast_window_len]
    slow = np.stack([np.asarray(getattr(e, f"wav_slow_pmt{i}")) for i in range(1, 9)])  # [8, slow_window_len]
    cam = np.asarray(e.obs_camera_counts).reshape(e.obs_camera_height, e.obs_camera_width)
```

Pair the 4 variants on `ml_parent_id`. `python3 tools/inspect_twin_event.py <file> --entry 0` draws one record:
3D truth, camera image and the 16 waveforms.

---

The rest of this page is the TWIN output contract. Do not select model features by numeric type or by loading
every branch: use the explicit observable allowlist below. The configuration is audit data, not a feature
vector. No clustering or super-hit compression is applied.

## File-local MC join and visible energy

Source inspected: CYGNUS-RD/CYGNO-MC `g4v11.3_cygno04`, revision
7464329b3ed8c99e5e4bdaa84762d7d1db57441a. See
[run schema](https://github.com/CYGNUS-RD/CYGNO-MC/blob/7464329b3ed8c99e5e4bdaa84762d7d1db57441a/src/CYGNORunAction.cc),
[event filling](https://github.com/CYGNUS-RD/CYGNO-MC/blob/7464329b3ed8c99e5e4bdaa84762d7d1db57441a/src/CYGNOEventAction.cc),
[hit quenching](https://github.com/CYGNUS-RD/CYGNO-MC/blob/7464329b3ed8c99e5e4bdaa84762d7d1db57441a/src/CYGNOHit.cc).

`tree1.eventnumber` joins `nTuple.eventnumber` inside one ROOT file. Neither row
index nor event number alone is a global identity. Parent ID is
`twin_source_id/ROOT-file-UUID:eventnumber`; copying the same original ROOT file
retains parent identity. Use a distinct source namespace for separately generated
samples. Duplicate IDs within either tree are fatal file errors. Missing primary
matches become rejected records. A file without `tree1`/required branches fails
before output creation. Only nTuple rows exist in the dataset: upstream events
without sensitive-detector hits have no nTuple row and are not manufactured here.

`tree1` holds Energy (Geant4 internal MeV), vertex (mm), signed unit direction
(dir_x/y/z), and event number. It does **not** hold primary PDG. Declare
`twin_primary_pdg` for a homogeneous primary sample; any recorded trackID=1,
parentID=0 deposits must agree. Mixed-primary files need additional upstream
primary-PDG metadata and are not supported by this declaration. `nTuple.particle_type`
is derived from the first recorded hit, so it is not silently treated as generator truth.

All hit energies are keV:

| Upstream branch | Meaning |
|---|---|
| energyDep_hits | Raw total deposited step energy, saved before quenching |
| energyDep_hits_QF | Full visible response: SRIM/TRIM-quenched ion/proton steps plus raw deposits of electronic secondaries |
| energyDep_hits_NRQF | Quenched ion/proton steps, zero for non-nuclear deposits |
| energyDep_hits_QF_geant | Geant4 total-minus-nonionizing energy for nuclear steps; raw total deposit for electronic steps |
| energyDep_hits_NRQF_geant | Geant4 ionizing energy for nuclear steps, zero for electronic steps |
| energyDep_QF / energyDep / energyDep_NRQF | Sums of the corresponding complete QF / raw / NR-only arrays |

TWIN uses **energyDep_hits_QF** unchanged for centroid weighting and primary
electron generation. No second Lindhard/SRIM factor is applied. The legacy
NR-only convention is unchanged when TWIN is off. Array lengths, finite/nonnegative
values, QF=NRQF on nuclear steps, QF=raw and NRQF=0 on electronic steps, and QF/raw
scalar sums are checked (absolute 1e-6 keV plus relative 1e-6 tolerance). Geant4's
alternative QF_geant array is checked for aligned, finite, nonnegative entries;
it is not used or interpreted as an additional correction.

Upstream ApplyQuenching multiplies steps by dR/dE above 25 eV; below that threshold
ApplyQuenchingAvg uses QF. The imported models cover H, He, C, O and F to 3000 keV
per ion, with a documented low-energy extrapolation. Differential step responses
need not be bounded by raw step energy. No per-step cap is imposed; array/scalar
closure validates the imported convention, **not experimental energy calibration**.
Unsupported upstream ion species may be unquenched; source-MC model limits,
low-energy closure and charge/light yield require CNAF physics validation.

## Frames, truth and identities

All `ml_hit_*_mm`, `ml_initial_u*` and `ml_primary_vertex_mm` use the **common
side-local GEM frame**, before diffusion. Axis permutation/sign follows MC_*axis;
positions additionally receive configured offsets/extras and rigid XYZ placement.
Directions receive only the signed permutation and normalization. Camera roll is
not applied to common-GEM truth. To obtain camera-frame truth, use the inverse
camera transform documented in TWIN_DIGI_CHANGES.md; its Z component is unchanged. Never use
geometric theta/phi or first-hit differences as the generated direction.

| Branch | ROOT/C++ type, shape, unit | Role |
|---|---|---|
| ml_schema_version | int scalar, 1 | Audit |
| ml_parent_id, ml_source_file_id | string scalar | Audit, pairing key |
| ml_variant_id, ml_response_id | int scalars | Audit, configured depth stratum / response replicate |
| ml_seed | ULong64_t scalar | Audit, derived detector seed |
| ml_camera_id | int scalar 0..2, -1 if unplaced | Observable camera selection / audit |
| ml_side_id | int scalar 0=L, 1=R | Observable detector side / audit |
| ml_placement_xyz_mm | vector<double>[3], mm | Truth/audit only |
| ml_hit_x_mm, ml_hit_y_mm, ml_hit_z_mm | vector<double>[H], mm | Complete original ionization-hit truth |
| ml_hit_evis_keV, ml_hit_edep_keV | vector<double>[H], keV | Complete visible/raw deposited truth |
| ml_z_centroid_mm | double scalar, mm | Truth only, weighted by Evis |
| ml_evis_total_keV | double scalar, keV | Truth only |
| ml_primary_pdg | int scalar | Training truth, declared generator species |
| ml_class_nr | bool scalar | Training truth, declared primary is proton/ion |
| ml_initial_ux, ml_initial_uy, ml_initial_uz | double scalars, dimensionless | Generated signed direction truth |
| ml_direction_valid | bool scalar | Truth validity, not an input feature |
| ml_primary_energy_MeV | double scalar, MeV | Generator energy truth |
| ml_primary_vertex_mm | vector<double>[3], mm | Transformed/placed generated vertex truth |
| ml_response_hit_index | vector<unsigned int>[R] | Audit, indices of original deposits passed to response |
| ml_valid | bool scalar | Dataset quality filter, not an input feature |
| ml_reject_reason | string scalar, empty for valid | Audit |
| ml_legacy_sparse_clipped | bool scalar | Audit: old uint16 camera branch saturated |

Pair records on `ml_parent_id`, requiring all variant IDs 0..3 and the intended
response IDs. The position/weight arrays retain all electronic secondaries.
Dataset splits must group by parent. Different detector seeds/response IDs do
not alter camera choice or XY placement; those use the independent placement
master seed `twin_master_seed`. The Z shift changes only absolute depth, not relative
geometry. Original input coordinates/momenta remain in the source_* audit
branches; generated direction comes exclusively from the file-local tree1 join.

## Observable allowlist and array construction

| Branch | Type, shape, unit | Use |
|---|---|---|
| obs_camera_width, obs_camera_height | int scalars W,H | Observable fixed geometry |
| obs_camera_counts | vector<double>[H*W], sensor counts | Noisy camera model input, reshape [H,W] with flat index y*W+x |
| obs_camera_ix, obs_camera_iy | vector<unsigned int>[K], pixel indices | Lossless sparse signal coordinates |
| obs_camera_signal | vector<double>[K], sensor counts | Signal-only audit/simulation view, NOT a noisy pedestal image |
| wav_fast_pmt1 ... wav_fast_pmt8 | vector<double>[fast_window_len], V | Stack channels in numeric order to [8,fast_samples] |
| wav_slow_pmt1 ... wav_slow_pmt8 | vector<double>[slow_window_len], V | Stack to [8,slow_samples] |
| pmt_signal_valid | vector<int>[8], 0/1 | Collected-light indicator; not a hardware trigger bit |

Safe inference features are obs_camera_counts, the sixteen named waveforms,
and fixed camera/side/geometry/sampling identifiers if needed. Do not use
placement, source truth, generated energy, absolute depth, seeds, parent IDs,
validity/rejection labels or response/variant IDs as features. A builder filters
invalid records, then selects the allowlist explicitly. PMT light-validity is a
simulation indicator, not directly observed by hardware: omit it from learned
features unless a corresponding detector-observable mask is supplied.

Sparse signal is saved before Gaussian sensor noise. It uses double values,
including large counts and fractional sensor conversion; reconstruct zeros
elsewhere. Legacy redpix_ix/iy/iz are retained; for TWIN, iz clips at 65535 with
ml_legacy_sparse_clipped set. They must not be used for lossless ML reconstruction.
TWIN voxel-image accumulation and camera Poisson sampling use double counts;
legacy per-voxel integer rounding remains outside TWIN. PMT photon-per-voxel
conversion still has an explicit INT_MAX limit; overflow rejects the event
instead of truncating. TH2I pictures are compatibility views, not lossless ML input.

## Timing, sensor noise and reproducibility

TWIN propagates diffused light voxel Z/v without folding negative diffusion
fluctuations through abs(). After actual geometric Poisson photon collection,
the earliest collected light packet over **all eight** channels defines a shared
ideal-light trigger. Subtract that one arrival time from every collected packet
before SPE transit-time/dispersion draws. No trigger time, event t0, true minimum
Z or absolute centroid is output as an observable feature. Relative delays and
depth-dependent diffusion broadening remain. Existing fast/slow trigger-window
offsets (200/fast_freq and 1466/slow_freq seconds), SPE and quantization stay
unchanged; plotted sample axes are i/Fs relative to that simulated trigger/window.
This is an ideal detected-light trigger, **not a calibrated electronic threshold**.
No collected light means no trigger/pulse, but synthetic baseline noise still fills
all enabled channels and declared sample lengths.

`twin_noise_mode=synthetic` is the required offline TWIN mode. Camera noise is
independent zero-mean Gaussian counts with configured twin_camera_noise_rms_counts.
TWIN applies photons_per_el once to saturated GEM charge, then the explicitly
required twin_pmt_quantum_efficiency before the existing geometric Poisson
collection. This makes the photon-yield convention explicit for both camera and
PMTs; it does not inherit the undocumented legacy combined 0.0136 factor.
Rounding after yield/QE and Poisson geometric thinning follow the existing model;
validate this approximation and the provisional QE on CNAF.
PMT electronics noise is independent zero-mean Gaussian voltage with configured
twin_pmt_noise_rms_v, followed by existing 1/4096 V quantization. All RMS values
are provisional inputs; no measured CYGNO04 noise or inter-channel correlation
is claimed. bckg=False prevents a second pedestal. Outside TWIN, measured-PSD
loading/downloads and existing noise generation are unchanged.

ml_seed hashes parent ID, twin_detector_seed, variant ID and response ID.
Named nonzero seeds drive charge/camera photons, camera Gaussian noise, geometric
collection, SPE and electronics independently. TWIN runs the existing TBB renderer
with one thread for reproducible draw/order behavior. Exact repeatability is
supported for the same binary, libraries, platform and config; standard-library
normal/Poisson algorithms and ROOT versions can differ across platforms. Other
seeds/response IDs vary detector stochasticity without changing original truth.

`twin_configuration` is a ROOT TMap of TObjString key/value pairs containing the
entire configuration: camera centres/roll/FOV, GEM and PMT geometry, gas model,
drift/diffusion/gain/ionization, sampling, provisional RMS values and source MC
commit. `digitizer_commit` is configured from Git (dirty builds marked); re-run
CMake on a clean committed source before production. `root_version`,
`ionization_convention`, and `reproducibility` are TObjString audit keys.
`twin_mc_commit` must be the actual MC build SHA; it cannot be inferred from a
ROOT file and is not silently guessed. The gas identifier HeCF4_60_40 reflects
the existing drift model, not a new gas calibration. Gas pressure (mbar) and
temperature (K) are required audit inputs; they do not automatically rescale
the configured diffusion/drift/ionization constants.

## Rejected records and legacy compatibility

A rejected nTuple row is retained with ml_valid=False and an explicit reason.
Mapped original arrays remain available, even for empty/failed-containment rows;
when XY placement cannot fit, they are mapped **unplaced** coordinates,
ml_camera_id=-1 and zero placement. Drift-containment rejections retain the full
rigidly placed track, including out-of-interval hits, and its placement metadata.
No partial response is generated for TWIN
training. response_hit_index is empty if containment/placement fails. If response fails after
placement, full placed truth survives and the row remains invalid. Invalid rows
have zero observation arrays/waveforms of declared lengths and must be filtered.
No-signal valid rows instead have synthetic sensor baseline noise. Invalid input
schema/duplicate event IDs/configuration fail the file/run, not silent coercion.

Legacy event_info branches retain their names when TWIN is off. Existing
TWIN-only camera_id, selected_side, parent_id, variant_id, target_depth_mm,
actual_depth_mm, visible_ionization_weights and source_x/y/z, source_px/py/pz
are audit branches. Legacy placement_dx/dy/dz_mm are doubles in mm.
Legacy eventnumber, particle_type, nhits_og, N_photons, N_primaries_reaching_GEMs,
nRedpix are ints; energy/cut_energy (keV), theta/phi (rad), track_length_3D,
proj_track_2D, vertices and min/max/cut counterparts are floats with historical
pixel-coordinate XY and mm Z conventions. These are legacy audit quantities,
not canonical generated truth; some retain historical sentinel values.
N_photons is -1 in TWIN to avoid its historical integer-integral overflow.
redpix_ix/iy/iz are vector<uint16_t>. Optional mc_first3_x/y/z are vector<double>
in original MC mm, and exposure fields exist only when enabled outside TWIN.
Legacy waveform branches are still generated for 1..pmt_number and unchanged
in four-PMT mode. param_dir keeps historical numeric histogram settings.

## Validation and CNAF checklist

`python3 tests/run_twin_ml_tests.py` executes the actual runner with tiny synthetic
ROOT inputs and a 32×32 test camera. Throwing stubs replace only unavailable
ROOTANA pedestal loading and OpenCV measured-PSD IFFT; offline TWIN noise,
ROOT I/O, collection, SPE, charge and camera response are real production code.
`python3 tests/run_pmt_tests.py --without-opencv` checks four/eight-channel
legacy paths with an explicit zero-noise IFFT stub. tests/test_twin.cxx covers
paired geometry/depth containment and fixed PMTs. `tools/inspect_twin_event.py`
uses PyROOT to display one event's 3D truth, camera and sixteen waveforms.

Before production: confirm the 500 mm drift interval (reference fiducial gas is
only 470 mm), validate sensor calibration and
ideal-trigger assumptions, build with ROOTANA/OpenCV, run real ER/He/C/F MC files,
inspect all energy/rejection distributions, verify four variants pair completely,
and repeat outputs on the exact CNAF binary/dependency stack. Synthetic fixtures
validate bookkeeping, not measured charge/light response or Geant4 transport.
