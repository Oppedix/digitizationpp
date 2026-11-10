/*
 * Copyright (C) 2025 CYGNO Collaboration
 *
 *
 * Author: Stefano Piacentini
 * Created in 2025
 *
 */
 
#include "Utils.h"
#include <sstream>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <filesystem>

namespace Utils {

std::vector<std::string> splitString(const std::string& input, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream stream(input);
    std::string token;
    while (getline(stream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

double angleBetween(const std::vector<double>& v1, const std::vector<double>& v2) {
    if (v1.size() != 3 || v2.size() != 3) throw std::invalid_argument("Both vectors must be 3D.");
    double dot = std::inner_product(v1.begin(), v1.end(), v2.begin(), 0.0);
    double len1 = std::sqrt(std::inner_product(v1.begin(), v1.end(), v1.begin(), 0.0));
    double len2 = std::sqrt(std::inner_product(v2.begin(), v2.end(), v2.begin(), 0.0));
    return std::acos(dot / (len1 * len2));
}

std::vector<double> crossProduct(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.size() != 3 || b.size() != 3) throw std::invalid_argument("Both vectors must be 3D.");
    std::vector<double> result(3);
    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];
    return result;
}

std::vector<double> rotateByAngleAndAxis(const std::vector<double>& vec, double angle, const std::vector<double>& axis) {
    if (vec.size() != 3 || axis.size() != 3) throw std::invalid_argument("Both vectors must be 3D.");

    // v_rot = (costheta)v + (sintheta)(axis x v) + (1-cos(theta)) (axis dot v) axis
    std::vector<double> result(3);
    
    std::vector<double> axisXvec = crossProduct(axis, vec);
    double axisDOTvec       = std::inner_product(axis.begin(), axis.end(), vec.begin(), 0.0);
    
    for(int i = 0; i < 3; i++) {
        result[i] = std::cos(angle) * vec[i] + std::sin(angle) * axisXvec[i] + (1.-std::cos(angle))*axisDOTvec*axis[i];
    }
    return result;
}

double roundUpToEven(double value) {
    int intVal = static_cast<int>(std::ceil(value));
    return (intVal % 2 == 0) ? intVal : intVal + 1;
}

std::string resolvePath(const std::string& relativePath) {
    return std::filesystem::absolute(std::filesystem::path(relativePath)).string();
}

std::vector<double> arange(double start, double stop, double step) {
    
    int length = (stop - start) / step;
    std::vector<double> result(length+1);
    double value = start;
    std::generate(result.begin(), result.end(), [&value, step]() mutable {
        double current = value;
        value += step;
        return current;
    });
    return result;
}

// Computes electron drift velocity (cm/us) for He/CF4 60/40 based on electric field data.
// Source: Data points extracted from 'HeCF4_60_40.csv'.
double compute_drift_velocity(double electric_field) {
    // Data pairs: (Electric Field [kV/cm], Drift Velocity [cm/us])
    std::vector<std::pair<double, double>> data = {
        {0.0, 0.00},  {0.5, 4.06},  {1.0, 6.12},  {1.5, 7.42},
        {2.0, 8.30},  {2.5, 8.84},  {3.0, 9.08},  {3.5, 9.06},
        {4.0, 8.84},  {4.5, 8.49},  {5.0, 8.11},  {5.5, 7.75},
        {6.0, 7.46},  {6.5, 7.25},  {7.0, 7.11},  {7.5, 7.03},
        {8.0, 7.00},  {8.5, 7.00},  {9.0, 7.04},  {9.5, 7.10},
        {10.0, 7.18}, {10.5, 7.28}, {11.0, 7.39}, {11.5, 7.51}
    };

    // Lower bound check: return minimum velocity if field is below range
    if (electric_field <= data.front().first) return data.front().second;
    
    // Upper bound check: return saturation velocity if field exceeds range
    if (electric_field >= data.back().first) return data.back().second;

    // Linear search and interpolation
    for (size_t i = 0; i < data.size() - 1; ++i) {
        if (electric_field >= data[i].first && electric_field <= data[i+1].first) {
            double x0 = data[i].first;
            double y0 = data[i].second;
            double x1 = data[i+1].first;
            double y1 = data[i+1].second;
            
            // Linear Interpolation: y = y0 + (x - x0) * (y1 - y0) / (x1 - x0)
            return y0 + (electric_field - x0) * (y1 - y0) / (x1 - x0);
        }
    }
    return 0.0; // Safety fallback
}

// Drift velocity (cm/us): the config key 'drift_velocity' when set, otherwise the table above.
double drift_velocity(const std::string& configured, double electric_field) {
    if (configured.empty()) return compute_drift_velocity(electric_field);
    size_t used = 0;
    double value = 0.;
    try { value = std::stod(configured, &used); } catch (const std::exception&) { used = 0; }
    if (used != configured.size() || !std::isfinite(value) || value <= 0.)
        throw std::invalid_argument("drift_velocity must be a positive number [cm/us]: '" + configured + "'");
    return value;
}


}
