/*
 * Copyright (C) 2026 CYGNO Collaboration
 *
 *
 * Author: Luan Gomes Mattosinhos de Carvalho
 * Created in 2026
 *
 */

/**
 * @file pmt_signal.h
 * @brief Header for PMT signal waveform simulation and digitization.
 */

#ifndef PMT_SIGNAL_H
#define PMT_SIGNAL_H

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <random>
#include "PMTHits.h"

using namespace std;

/**
 * @class SignalSimulation
 * @brief Class responsible for generating electronic signals and waveforms for PMTs.
 *
 * This class simulates the full electronic chain, including single photoelectron (SPE) 
 * response using exponentially modified Gaussian functions, noise generation from 
 * Power Spectral Density (PSD), and signal quantization.
 */
class SignalSimulation {
public:
    /**
     * @brief Constructor for SignalSimulation.
     * @param hits_dict Dictionary containing hit data per voxel and per PMT.
     * @param options Map containing configuration parameters (gains, frequencies, paths).
     * @param string containing the path position of the main folder.
     */
    SignalSimulation(const map<string, vector<PMTData>>& hits_dict,
                     const map<string, string>& options,
                     const string& SOURCE_DIR);

    /**
     * @brief Initializes the time vectors for fast and slow digitizers based on sampling frequencies.
     */
    void set_t();

    /**
     * @brief Utility to load PSD or noise data from a text file.
     * @param filename Path to the .txt file.
     * @return Vector of doubles containing the file data.
     */
    vector<double> load_txt_array(const string& filename);

    /**
     * @brief Computes stochastic noise in the time domain from a frequency domain PSD.
     * @param psd Vector containing the Power Spectral Density.
     * @param digitizer String identifier ("Fast" or "Slow") to set the correct time window.
     * @return Time-domain noise waveform.
     */
    vector<double> compute_noise(const vector<double>& psd, const string& digitizer);

    /**
     * @brief Generates and stores noise waveforms for all PMTs and digitizers.
     */
    void gen_noise();

    /**
     * @brief Optional 'pmt_noise' switch (default True). False gives noiseless waveforms.
     */
    static bool noiseEnabled(const map<string, string>& options);

    /**
     * @brief Noise template channel used by each PMT (index 0 -> pmt_1).
     *
     * Default: PMT i uses <detector>_{fast,slow}_noise_pmt_i.txt. The optional
     * 'pmt_noise_channels' setting ('1;2;3;4;1;2;3;4') reuses existing templates,
     * e.g. the four LIME PSDs for eight PMTs. Each PMT still gets independent phases.
     */
    static vector<int> noiseTemplateChannels(const map<string, string>& options, int num_pmts);

    /**
     * @brief Converts Full Width at Half Maximum (FWHM) to standard deviation (sigma).
     * @param fwhm The FWHM value.
     * @return Corresponding sigma value for a Gaussian distribution.
     */
    double fwhm2std(double fwhm);

    /**
     * @brief Simulates the PMT transit time using a normal distribution.
     * @return Randomly sampled transit time [ns].
     */
    double transit_time();

    /**
     * @brief Computes an exponentially modified Gaussian (ExpGaussian) pulse shape and adds it to the buffer.
     * @param waveform Reference to the waveform accumulator vector.
     * @param x Time vector.
     * @param G PMT Gain.
     * @param cen Pulse center time.
     * @param sig Gaussian width (sigma).
     * @param lambda Exponential decay rate.
     */
    void expgaussian(vector<double>& waveform, const vector<double>& x, double G, double cen, double sig, double lambda);

    /**
     * @brief Generates a Single Photoelectron (SPE) signal.
     * @param arr_time Original photon arrival time.
     * @param fast_wf_aux Reference to the fast waveform vector to accumulate the signal.
     * @param slow_wf_aux Reference to the slow waveform vector to accumulate the signal.
     */
    void spe_signal(double arr_time, vector<double>& fast_wf_aux, vector<double>& slow_wf_aux);

    /**
     * @brief Generates signals for a multi-photon event (pile-up).
     * @param nr_photons Number of photons in the event.
     * @param arr_time Arrival time of the voxel photons.
     * @param fast_wf_aux Reference to the fast waveform vector.
     * @param slow_wf_aux Reference to the slow waveform vector.
     */
    void gen_signal(int nr_photons, double arr_time, vector<double>& fast_wf_aux, vector<double>& slow_wf_aux);

    /**
     * @brief Simulates the ADC quantization process.
     * @param signal Continuous analog signal.
     * @return Quantized digital signal.
     */
    vector<double> quantization(const vector<double>& signal);

    /**
     * @brief Simulates the full signal for a specific PMT across all voxels.
     * @param pmt Name of the PMT.
     * @param fast_signal Map to store the resulting fast waveform.
     * @param slow_signal Map to store the resulting slow waveform.
     */
    void pmt_signal(const string& pmt,
                map<string, vector<double>>& fast_signal,
                map<string, vector<double>>& slow_signal);

    /**
     * @brief Orchestrates the simulation for all PMTs and digitizers for the current event.
     * @param fast_signal Output map for all fast digitizer channels.
     * @param slow_signal Output map for all slow digitizer channels.
     */
    void simulated_signals(map<string, vector<double>>& fast_signal,
                           map<string, vector<double>>& slow_signal);

private:
    std::mt19937 gen, noise_gen;
    std::exponential_distribution<double> exp_dist_; /**< Distribution for exponential dispersion. */
    std::normal_distribution<double> transit_dist_;  /**< Distribution for transit time spread. */

    map<string, vector<PMTData>> ptc_hits_;       /**< Input hit data from PhotonPropagation. */
    map<string, string> options_;                 /**< Simulation configuration options. */
    int num_pmts_;
    string digitizers_;                           /**< Selection of active digitizers (Fast/Slow/Both). */
    int fast_window_len_;                         /**< Fast digitizer window size (samples). */
    int slow_window_len_;                         /**< Slow digitizer window size (samples). */
    double Fs_fast_;                              /**< Fast digitizer sampling frequency [Hz]. */
    double Fs_slow_;                              /**< Slow digitizer sampling frequency [Hz]. */
    
    // Cached Parameters for Optimization
    double pmt_gain_;                             /**< PMT Gain. */
    double pmt_sigma_;                            /**< PMT Sigma. */
    double pmt_lambda_;                           /**< PMT Lambda. */
    double exp_scale_;                            /**< Exponential Scale. */
    double transit_time_mu_;                      /**< Transit Time Mean. */
    double transit_time_sigma_;                   /**< Transit Time Sigma. */

    vector<int> sample_fast_, sample_slow_;       /**< Sample indices. */
    vector<double> t_fast_, t_slow_;              /**< Time vectors for both digitizers. */
    map<string, vector<double>> fast_noise_;      /**< Generated fast noise per PMT channel. */
    map<string, vector<double>> slow_noise_;      /**< Generated slow noise per PMT channel. */

    std::string f_SOURCE_DIR = "";    /**< Path to event source directory*/
    std::string f_DETECTOR = "";      /**< Detector name taken from options*/
    std::string f_cloud_dir = "";     /**< Directory inside the cloud bucket where the noise files are located*/
};

#endif