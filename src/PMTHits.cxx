/*
 * Copyright (C) 2026 CYGNO Collaboration
 *
 *
 * Author: Luan Gomes Mattosinhos de Carvalho
 * Created in 2026
 *
 */

#include "PMTHits.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace std;

int PhotonPropagation::validateGeometry(const map<string, string>& options) {
    auto number = [&](const string& key) {
        auto it = options.find(key);
        size_t used = 0;
        double value;
        try {
            if (it == options.end()) throw invalid_argument("missing");
            value = stod(it->second, &used);
            if (used != it->second.size() || !isfinite(value)) throw invalid_argument("invalid");
        } catch (const exception&) {
            throw invalid_argument("Missing or invalid PMT geometry: " + key + " (supply a finite value in mm)");
        }
        return value;
    };
    const double count = number("pmt_number");
    if (count < 1 || count > 8 || count != floor(count))
        throw invalid_argument("pmt_number must be an integer from 1 to 8");
    if (number("pmt_radius") <= 0 || number("dist_gem_pmt") <= 0)
        throw invalid_argument("pmt_radius and dist_gem_pmt must be positive (mm)");
    for (int i = 1; i <= count; ++i) {
        number("pmt_" + to_string(i) + "_x");
        number("pmt_" + to_string(i) + "_y");
    }
    return static_cast<int>(count);
}

PhotonPropagation::PhotonPropagation(const vector<double>& x_0,
                                     const vector<double>& y_0,
                                     const vector<int>& n_photons,
                                     const vector<double>& arr_times,
                                     const map<string, string>& options)
    : x_0_(x_0), y_0_(y_0), n_photons_(n_photons), arr_times_(arr_times), options_(options) {
    
    num_pmts = validateGeometry(options_);
    if (x_0_.size() != y_0_.size() || x_0_.size() != n_photons_.size() || x_0_.size() != arr_times_.size())
        throw invalid_argument("PMT voxel coordinate, photon and arrival-time sizes must match");
    auto seed=options_.find("twin_collection_seed");
    gen.seed(seed==options_.end() ? rd() : std::stoul(seed->second));

    r_pmt_sq = std::pow(std::stod(options_.at("pmt_radius")), 2);
    z_pmt_sq = std::pow(std::stod(options_.at("dist_gem_pmt")), 2);
    z_pmt = std::stod(options_.at("dist_gem_pmt"));

    for (int i = 1; i <= num_pmts; ++i) {
        string pmt_name = "pmt_" + to_string(i);
        pmt_positions.push_back({std::stod(options_.at(pmt_name + "_x")), 
                                 std::stod(options_.at(pmt_name + "_y"))});
        pmt_names.push_back(pmt_name);
    }
}

map<string, vector<PMTData>> PhotonPropagation::pmt_hits() {
    map<string, vector<PMTData>> all_hits;
    
    for (size_t i = 0; i < x_0_.size(); ++i) {
        const double x = x_0_[i];
        const double y = y_0_[i];
        const int n_pho = n_photons_[i];

        for (int j = 0; j < num_pmts; ++j) {
            const double dx = pmt_positions[j].first - x;
            const double dy = pmt_positions[j].second - y;
            
            const double R2 = dx*dx + dy*dy + z_pmt_sq;
            const double R4 = R2 * R2;

            const double mean = n_pho * r_pmt_sq * z_pmt_sq / (4.0 * R4);
            
            std::poisson_distribution<int> dist(mean);
            
            PMTData data;
            data.hits = dist(gen);
            
            if (data.hits > 0) {
                data.arrival_time = arr_times_[i];
                all_hits[pmt_names[j]].push_back(data);
            }
        }
    }
    return all_hits;
}
