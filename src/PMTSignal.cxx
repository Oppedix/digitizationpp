/*
 * Copyright (C) 2026 CYGNO Collaboration
 *
 *
 * Author: Luan Gomes Mattosinhos de Carvalho
 * Created in 2026
 *
 */

#include "PMTSignal.h"
#include <fstream>
#include <cmath>
#include <random>
#include <complex>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <stdexcept>
#include <sstream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/utility.hpp>
#include <iostream>
#include <cstdlib>
#include <filesystem>

using namespace cv;
using namespace std;
using namespace chrono;

SignalSimulation::SignalSimulation(const map<string, vector<PMTData>>& hits_dict,
                                   const map<string, string>& options,
                                   const string& SOURCE_DIR)
    : ptc_hits_(hits_dict), options_(options) {
        
    num_pmts_ = PhotonPropagation::validateGeometry(options_);
    digitizers_ = options_["digitizers"];
    fast_window_len_ = stoi(options_["fast_window_len"]);
    slow_window_len_ = stoi(options_["slow_window_len"]);
    Fs_fast_ = stod(options_["fast_freq"]);
    Fs_slow_ = stod(options_["slow_freq"]);
    
    pmt_gain_ = stod(options_["pmt_gain"]);
    pmt_sigma_ = stod(options_["pmt_sigma"]);
    pmt_lambda_ = stod(options_["pmt_lambda"]);
    exp_scale_ = stod(options_["exp_dispersion_scale"]);
    
    transit_time_mu_ = stod(options_["transit_time"]);
    double tt_fwhm = stod(options_["transit_time_spread"]);
    transit_time_sigma_ = fwhm2std(tt_fwhm);

    if (fast_window_len_ <= 0 || slow_window_len_ <= 0 ||
        !isfinite(Fs_fast_) || Fs_fast_ <= 0 || !isfinite(Fs_slow_) || Fs_slow_ <= 0)
        throw invalid_argument("PMT waveform lengths and sampling frequencies must be positive and finite");

    random_device rd;
    auto seed=options_.find("twin_spe_seed");
    gen.seed(seed==options_.end() ? rd() : std::stoul(seed->second));
    if(seed!=options_.end()) noise_gen.seed(std::stoul(options_.at("twin_noise_seed")));
    exp_dist_ = std::exponential_distribution<double>(exp_scale_);
    transit_dist_ = std::normal_distribution<double>(transit_time_mu_, transit_time_sigma_);

    f_SOURCE_DIR = SOURCE_DIR;
    f_DETECTOR = options_["detector"];
    f_cloud_dir = options_["pmt_noise_cloud_dir"];
        
    set_t();
    gen_noise();
}

void SignalSimulation::set_t() {
    t_fast_.resize(fast_window_len_);
    t_slow_.resize(slow_window_len_);
    for (int i = 0; i < fast_window_len_; ++i) t_fast_[i] = i / Fs_fast_;
    for (int i = 0; i < slow_window_len_; ++i) t_slow_[i] = i / Fs_slow_;
}

double SignalSimulation::fwhm2std(double fwhm) {
    return fwhm / (2.0 * sqrt(2.0 * log(2.0)));
}

double SignalSimulation::transit_time() {
    return transit_dist_(gen);
}

void SignalSimulation::expgaussian(vector<double>& waveform, const vector<double>& x, double G, double cen, double sig, double lambda) {
    double Q = G * (-1.6e-19);
    double R = 50.0;
    double amp_factor = Q * R * (lambda / 2.0);
    double sqrt2 = sqrt(2.0);
    double sig_sq_lambda = lambda * sig * sig;

    for (size_t i = 0; i < x.size(); ++i) {
        double exp_arg = lambda * (cen - x[i] + (sig_sq_lambda / 2.0));
        double erfc_arg = (cen + sig_sq_lambda - x[i]) / (sig * sqrt2);
    
        if (!isfinite(exp_arg) || !isfinite(erfc_arg) || exp_arg > 700.0 || abs(erfc_arg) > 30.0) {
            continue;
        }

        double pulse = exp(exp_arg) * erfc(erfc_arg);

        if (isfinite(pulse)) {
            waveform[i] += amp_factor * pulse;
        }
    }
}

void SignalSimulation::spe_signal(double arr_time, vector<double>& fast_wf_aux, vector<double>& slow_wf_aux) {
    double disp = exp_dist_(gen) * 1e9;
    // One photoelectron realization is sampled by both digitizers.
    double mean = (transit_time() + arr_time + disp) * 1e-9;
    
    float shift_fast = 200.0 / Fs_fast_;
    float shift_slow = 1466.0 / Fs_slow_;

    if (digitizers_ == "Both" || digitizers_ == "Fast") {
        expgaussian(fast_wf_aux, t_fast_, pmt_gain_, mean + shift_fast, pmt_sigma_, pmt_lambda_);
    }
    if (digitizers_ == "Both" || digitizers_ == "Slow") {
        expgaussian(slow_wf_aux, t_slow_, pmt_gain_, mean + shift_slow, pmt_sigma_, pmt_lambda_);
    }
}

void SignalSimulation::gen_signal(int nr_photons, double arr_time, vector<double>& fast_wf_aux, vector<double>& slow_wf_aux) {
    for (int i = 0; i < nr_photons; ++i)
        spe_signal(arr_time, fast_wf_aux, slow_wf_aux);
}

vector<double> SignalSimulation::quantization(const vector<double>& signal) {
    vector<double> result(signal.size());
    const double levels = 4096.0;

    for (size_t i = 0; i < signal.size(); ++i) {
        result[i] = round(signal[i] * levels) / levels;
    }
    return result;
}

void SignalSimulation::pmt_signal(const string& pmt,
                                   map<string, vector<double>>& fast_signal,
                                   map<string, vector<double>>& slow_signal) {
    vector<double> fast_wf_aux(fast_window_len_, 0.0);
    vector<double> slow_wf_aux(slow_window_len_, 0.0);
                                    
    auto it = ptc_hits_.find(pmt);
    if (it != ptc_hits_.end()) {
        for (const auto& data : it->second) {
            gen_signal(data.hits, data.arrival_time, fast_wf_aux, slow_wf_aux);
        }
    }

    for (size_t i = 0; i < fast_wf_aux.size(); ++i) fast_wf_aux[i] += fast_noise_[pmt][i];
    for (size_t i = 0; i < slow_wf_aux.size(); ++i) slow_wf_aux[i] += slow_noise_[pmt][i];

    fast_signal[pmt] = quantization(fast_wf_aux);
    slow_signal[pmt] = quantization(slow_wf_aux);
}

void SignalSimulation::simulated_signals(map<string, vector<double>>& fast_signal,
                                         map<string, vector<double>>& slow_signal) {                                                      
    fast_signal.clear();
    slow_signal.clear();
    for (int i = 1; i <= num_pmts_; ++i)
        pmt_signal("pmt_" + to_string(i), fast_signal, slow_signal);

    fast_signal["time"] = t_fast_;
    slow_signal["time"] = t_slow_;
}

vector<double> SignalSimulation::load_txt_array(const string& filename) {
    ifstream file(filename);
    vector<double> data;
    double val;
    while (file >> val) {
        data.push_back(val);
    }
    return data;
}

void SignalSimulation::gen_noise() {
    if (options_.count("twin_spe_seed") && options_.at("twin_noise_mode")=="synthetic") {
        const double rms=std::stod(options_.at("twin_pmt_noise_rms_v"));
        if(!std::isfinite(rms) || rms<0) throw std::invalid_argument("Invalid TWIN PMT noise RMS");
        std::normal_distribution<double> gaussian(0,1);
        for(int i=1;i<=num_pmts_;++i) {
            auto& fast=fast_noise_["pmt_"+to_string(i)];
            auto& slow=slow_noise_["pmt_"+to_string(i)];
            fast.resize(fast_window_len_);slow.resize(slow_window_len_);
            for(auto& value:fast) value=rms*gaussian(noise_gen);
            for(auto& value:slow) value=rms*gaussian(noise_gen);
        }
        return; // Offline provisional white voltage noise, no PSD download.
    }
    if (!noiseEnabled(options_)) {
        for (int i = 1; i <= num_pmts_; ++i) {
            fast_noise_["pmt_"+to_string(i)].assign(fast_window_len_, 0.);
            slow_noise_["pmt_"+to_string(i)].assign(slow_window_len_, 0.);
        }
        return; // pmt_noise False: SPE pulses and ADC quantization only, no PSD download.
    }
    const vector<int> templates = noiseTemplateChannels(options_, num_pmts_);

    for (int i = 1; i <= num_pmts_; ++i) {
        const string pmt = "pmt_" + to_string(i);
        const string source = "pmt_" + to_string(templates[i-1]); // == pmt unless remapped
        string fname = f_DETECTOR+"_fast_noise_"+source+".txt";
        string localname_fast = f_SOURCE_DIR+"pmt/"+fname;

        if(!std::filesystem::exists(localname_fast.c_str())) {
            int ret;
            string downloadname = "https://s3.cr.cnaf.infn.it:7480/cygno:"+ f_cloud_dir + "/" + fname;      //maybe to be fixed with s3, but needs to change s3
            ret=system(("wget "+ downloadname).c_str());
            ret=system(("mv "+fname + " " + localname_fast).c_str());
            if(ret!=0) cout<<"Ouch error using system moving file "<<fname<<endl;
        }

        fname = f_DETECTOR+"_slow_noise_"+source+".txt";
        string localname_slow = f_SOURCE_DIR+"pmt/"+fname;

        if(!std::filesystem::exists(localname_slow.c_str())) {
            int ret;
            string downloadname = "https://s3.cr.cnaf.infn.it:7480/cygno:"+ f_cloud_dir + "/" + fname;      //maybe to be fixed with s3, but needs to change s3
            ret=system(("wget "+ downloadname).c_str());
            ret=system(("mv "+fname + " " + localname_slow).c_str());
            if(ret!=0) cout<<"Ouch error using system moving file "<<fname<<endl;
        }

        vector<double> fast_psd = load_txt_array(localname_fast);
        vector<double> slow_psd = load_txt_array(localname_slow);

        fast_noise_[pmt] = compute_noise(fast_psd, "Fast");
        slow_noise_[pmt] = compute_noise(slow_psd, "Slow");
    }
}

bool SignalSimulation::noiseEnabled(const map<string, string>& options) {
    const auto it = options.find("pmt_noise");
    if (it == options.end() || it->second == "True" || it->second == "true" || it->second == "1") return true;
    if (it->second == "False" || it->second == "false" || it->second == "0") return false;
    throw invalid_argument("pmt_noise must be True or False");
}

vector<int> SignalSimulation::noiseTemplateChannels(const map<string, string>& options, int num_pmts) {
    vector<int> channels;
    const auto it = options.find("pmt_noise_channels");
    if (it == options.end() || it->second.empty()) {
        for (int i = 1; i <= num_pmts; ++i) channels.push_back(i);
        return channels;
    }
    string item;
    stringstream list(it->second);
    while (getline(list, item, ';')) {
        size_t used = 0;
        int channel = 0;
        try { channel = stoi(item, &used); } catch (const exception&) { used = 0; }
        if (used == 0 || used != item.size() || channel < 1)
            throw invalid_argument("pmt_noise_channels must be ';'-separated positive integers, e.g. '1;2;3;4;1;2;3;4'");
        channels.push_back(channel);
    }
    if (static_cast<int>(channels.size()) != num_pmts)
        throw invalid_argument("pmt_noise_channels needs exactly one template channel per PMT (pmt_number)");
    return channels;
}

vector<double> SignalSimulation::compute_noise(const vector<double>& psd, const string& digitizer) {
    int N = (digitizer == "Fast") ? fast_window_len_ : slow_window_len_;
    double fs = (digitizer == "Fast") ? Fs_fast_ : Fs_slow_;

    if (psd.size() < 2 || 2 * (psd.size() - 1) < static_cast<size_t>(N))
        throw runtime_error("Missing or too short PMT noise PSD for " + digitizer);
    for (double value : psd)
        if (!isfinite(value) || value < 0.) throw runtime_error("Invalid PMT noise PSD");

    vector<double> PSD = psd;
    for (int i = psd.size() - 2; i > 0; --i) {
        PSD.push_back(psd[i]);
    }

    size_t len = PSD.size();
    vector<complex<double>> Nf(len);
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<> phase_dist(-M_PI, M_PI);

    // Positive spectrum
    for (size_t i = 0; i < psd.size(); ++i) {
        double phase = phase_dist(gen);
        double magnitude = sqrt(psd[i] * fs / N);
        Nf[i] = polar(magnitude, phase);
    }

    // Negative spectrum
    for (size_t i = psd.size(); i < len; ++i) {
        Nf[i] = conj(Nf[len - i]);
    }

    // Convert Nf to cv::Mat for OpenCV processing
    Mat complexInput(len, 1, CV_64FC2);
    for (size_t i = 0; i < len; ++i) {
        complexInput.at<Vec2d>(i)[0] = Nf[i].real();
        complexInput.at<Vec2d>(i)[1] = Nf[i].imag();
    }

    // Perform IFFT using OpenCV
    Mat complexOutput;
    dft(complexInput, complexOutput, DFT_INVERSE | DFT_REAL_OUTPUT);

    // Convert the IFFT result back to a vector of doubles
    vector<double> noise(N);
    for (int n = 0; n < N; ++n) {
        noise[n] = complexOutput.at<double>(n);
    }

    return noise;
}