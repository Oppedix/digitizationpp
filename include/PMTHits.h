/*
 * Copyright (C) 2026 CYGNO Collaboration
 *
 * Author: Luan Gomes Mattosinhos de Carvalho
 * Created in 2026
 *
 */

/**
 * @file pmt_hits.h
 * @brief Header for Photon Propagation and PMT hit simulation.
 */

#ifndef PMT_HITS_H
#define PMT_HITS_H

#include <string>
#include <vector>
#include <map>
#include <random>

using namespace std;

/**
 * @struct PMTData
 * @brief Structure to store the number of hits and arrival time for a specific PMT.
 */
struct PMTData {
    int hits;                /**< Number of detected photons (hits) */
    double arrival_time;     /**< Mean arrival time of the photons at the PMT [ns] */
};

/**
 * @class PhotonPropagation
 * @brief Class responsible for simulating the light propagation from track voxels to PMTs.
 *
 * This class calculates the solid angle and photon detection probability for each PMT
 * based on the track geometry and detector configuration. It uses cached parameters
 * for optimized performance during high-volume voxel processing.
 */
class PhotonPropagation {
public:
    // Validate production geometry before opening output files or loading noise.
    static int validateGeometry(const map<string, string>& options);

    /**
     * @brief Constructor for PhotonPropagation.
     * * Initializes the propagation module and pre-calculates geometric constants
     * to avoid redundant computations during the simulation loop.
     *
     * @param x_0 Vector of X coordinates of track voxels [mm].
     * @param y_0 Vector of Y coordinates of track voxels [mm].
     * @param n_photons Vector containing the number of photons produced in each voxel.
     * @param arr_times Vector containing the time information for each voxel [ns].
     * @param options Map containing detector configuration and PMT positions.
     */
    PhotonPropagation(const vector<double>& x_0,
                      const vector<double>& y_0,
                      const vector<int>& n_photons,
                      const vector<double>& arr_times,
                      const map<string, string>& options);

    /**
     * @brief Main execution function to process all voxels and generate PMT data.
     * * Iterates through the provided track voxels and accumulates hits for each PMT
     * using a Poisson distribution.
     *
     * @return A nested map structure: voxel_id -> pmt_name -> PMTData.
     */
    map<string, vector<PMTData>> pmt_hits();

private:
    // --- Input Data ---
    vector<double> x_0_;           /**< X coordinates of voxels. */
    vector<double> y_0_;           /**< Y coordinates of voxels. */
    vector<int> n_photons_;        /**< Produced photons per voxel. */
    vector<double> arr_times_;     /**< Arrival times per voxel. */
    map<string, string> options_;  /**< Configuration options from the config file. */

    // --- Pre-calculated values ---
    double r_pmt_sq;               /**< Squared radius of the PMT [mm^2]. */
    double z_pmt_sq;               /**< Squared distance from GEM to PMT [mm^2]. */
    double z_pmt;                  /**< Distance from GEM to PMT [mm]. */
    int num_pmts;                  /**< Total number of PMTs. */
    
    /** * @brief PMT positions
     * Stores pairs of (x, y) coordinates for each PMT.
     */
    vector<pair<double, double>> pmt_positions; 
    
    /** * @brief PMT names corresponding to the positions vector.
     */
    vector<string> pmt_names;

    // --- Random Number Generation ---
    random_device rd;   /**< Non-deterministic random number generator for seeding. */
    mt19937 gen;        /**< Mersenne Twister engine. */
};

#endif