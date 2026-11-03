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

## CYGNO04 one-side example

`config/ConfigFile_CYGNO04_8PMT.txt` selects eight PMTs. All eight X/Y
coordinates, the GEM-to-PMT distance and PMT radius are required placeholders:
no verified CYGNO04 geometry is supplied here. Replace them with measurements
in the digitization reference frame (mm). Missing, nonnumeric or nonfinite geometry
fails before output files or noise downloads are opened. The copied electronics
settings are LIME examples, not a CYGNO04 calibration. Supply the existing per-channel
`CYGNO04_fast_noise_pmt_N.txt` and `CYGNO04_slow_noise_pmt_N.txt` PSD files for N=1..8
under `pmt/`; the existing download/noise mechanism is unchanged.

The LIME template and legacy four-channel waveform names are unchanged. Every
PMT-enabled event has the declared fast/slow sample counts, including skipped or
no-light events (zero-filled). The additional `pmt_signal_valid` vector has one
integer per channel: 1 means photoelectrons were collected and synthesis ran;
0 means no collected light or a skipped event. It is not a trigger or a guarantee
that a pulse falls inside the sampling window. When synthesis runs, channels
without collected light still receive the existing noise realization.
Fast and slow digitizers sample the same photoelectron transit/dispersion draw.
ROOT branch storage remains alive until branch addresses are reset.

Run focused tests with `python3 tests/run_pmt_tests.py`. ROOT and OpenCV are
required; `--without-opencv` runs the physics/ROOT tests with only the noise IFFT
replaced by a zero-noise test stub, and explicitly does not test production noise.
