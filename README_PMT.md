# PMT Simulation

Here you find information about the implementation of the PMT simulation for the LIME setup.

## Main Features
* **Light Propagation:** Calculation of arrival times and the number of photoelectrons based on the detector geometry.
* **Signal generation:** Signal generation based on the SPE signal and subsequent digitization (ADC).

## File Structure
* `pmt_hits.cxx / .h`: Main logic for Light Propagation and integration with the DigitizationRunner.
* `pmt_signal.cxx / .h`: Main logic for Signal Generation and integration with the DigitizationRunner.

## Data Output and Waveforms
The simulated waveforms are stored within the `event_info` tree in the output ROOT files. Each PMT channel and digitizer type results in a specific branch containing the waveforms.

* **Tree Structure:** The signals are saved as arrays in branches named `wav_[type]_pmt[n]`, where `[type]` is either `fast` or `slow` and `[n]` is the PMT index (1 to `pmt_number`, supported range 1–8).
* **Accessing Data:** You can load these waveforms by accessing the `event_info` tree. A practical example of how to handle the sampling frequencies and plot the signals is available in `pmt/plot_pmt_wf_example.ipynb`.

## Main Configuration Parameters (ConfigFile)
The simulation is activated and controlled via the `# PMT simulation parameters` section in the `ConfigFile.txt`.

* **Activation:**
    * `pmt_mode`: Enables (`True`) or disables (`False`) the PMT signal simulation.

* **Geometry:**
    * `dist_gem_pmt`: Z-axis distance (in mm) from the GEM plane (at z = 0) to the PMTs.
    * `pmt_radius`: Physical radius of the PMT R7378A (default: `11` mm).
    * `pmt_number`: Total number of PMTs used in the simulation (1–8, default: `4`).

* **PMT Positions:**
    The PMT coordinates follow the standard `digitization` reference frame. The positions are defined in millimeters relative to the center of the detector:
    * **PMT 1 (Top-Left)**: Located at `(-142.0, -142.0)`.
    * **PMT 2 (Top-Right)**: Located at `(142.0, -142.0)`.
    * **PMT 3 (Bottom-Right)**: Located at `(142.0, 142.0)`.
    * **PMT 4 (Bottom-Left)**: Located at `(-142.0, 142.0)`.

* **Time Response (Transit Time):**
    * `transit_time`: Average transit time of photoelectrons (default: `17` ns).
    * `transit_time_spread`: Transit time spread (default: `0.9` ns).
    * `exp_dispersion_scale`: Scale parameter for the exponential dispersion of photon arrival (default: `0.079e9` Hz).

* **Pulse Shape and Gain:**
    * `pmt_gain`: Amplification gain of the PMT (default: `0.398e6`).
    * `pmt_sigma`: Standard deviation of the Gaussian component of the Single Photoelectron (SPE) signal (default: `0.516e-9` s).
    * `pmt_lambda`: Decay rate of the exponential tail of the SPE signal (default: `0.658e9` Hz).

* **Digitizers:**
    * `digitizers`: Defines which signals to generate (`'Fast'`, `'Slow'`, or `'Both'`).
    * `fast_freq` / `slow_freq`: Sampling frequencies for the fast (`750e6` Hz) and slow (`250e6` Hz) channels.

## Noise options (fork additions, defaults = upstream behaviour)

* `pmt_noise` (default `True`): add noise generated from the measured PSD templates
  `<detector>_fast_noise_pmt_N.txt` / `<detector>_slow_noise_pmt_N.txt` (downloaded from
  `pmt_noise_cloud_dir` if missing). `False` gives noiseless waveforms (SPE pulses and
  12-bit quantization only) and downloads nothing.
* `pmt_noise_channels` (optional, e.g. `'1;2;3;4;1;2;3;4'`): template channel used by PMT 1..N.
  Only `LIME_*_noise_pmt_1..4.txt` exist on the cloud (checked 2026-11-05), so an 8-PMT run with
  measured noise must reuse them. Every PMT still gets an independent noise realization.

## CYGNO04: one camera + 8 PMTs

`config/ConfigFile_CYGNO04_8PMT.txt` is the ordinary digitizer with `pmt_number: 8`: the
CYGNO04 Quest2 central camera and the 8 nominal PMT positions (inferred, see
`docs/TWIN_DIGI_CHANGES.md`), LIME noise templates remapped to 8 channels. All legacy
options (`bckg`, `Vignetting`, `redpix_output`, `digitizers`, random shifts, `fixed_seed`)
behave as in the LIME template. Electronics/SPE settings are LIME values, not a CYGNO04
calibration.

Output: `wav_fast_pmt1..8`, `wav_slow_pmt1..8`, plus `pmt_signal_valid` (one integer per
PMT: 1 = photoelectrons collected and synthesis ran). As on upstream main, events skipped
or without light keep empty waveforms; only TWIN mode zero-fills them to fixed length.
Fast and slow digitizers sample the same photoelectron transit/dispersion draw.

Run focused tests with `python3 tests/run_pmt_tests.py`. ROOT and OpenCV are
required; `--without-opencv` runs the physics/ROOT tests with only the noise IFFT
replaced by a zero-noise test stub, and explicitly does not test production noise.
