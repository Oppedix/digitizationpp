// Checks config/ConfigFile_TWIN_CYGNO04.txt against the CYGNO04 readout CAD values
// (tools/extract_cygno04_cad.py, doc/TWIN_DIGI_CHANGES.md §7) and for internal consistency
// (FOV tiling, placement containment, light sharing).
#include "ConfigManager.h"
#include "TwinPlacement.h"
#include "PMTHits.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
using namespace std;

int main(int argc, char** argv) {
    ConfigManager config;
    if (!config.loadConfig(argc > 1 ? argv[1] : "config/ConfigFile_TWIN_CYGNO04.txt")) return 1;
    auto options = config.getOptions();
    assert(options.at("twin_geometry_id") == "CYGNO04_CAD_v1");
    assert(options.at("Camera_type") == "Quest2");
    assert(options.at("twin_geometry_verified") == "True");
    options["twin_source_id"] = "geometry-test";

    TwinPlacement g(options);
    const int npmt = PhotonPropagation::validateGeometry(options);
    assert(npmt == 8);

    // Quest2 aspect ratio (checkDimensionConsistency) and the 0.1247 mm effective pixel (FoV scan report).
    assert(fabs(g.width / g.height - 4096. / 2304.) < 1e-9);
    assert(fabs(g.width / 4096. - 0.1247) < 1e-9);

    // Three FOVs cover the whole 500 x 800 mm GEM, with overlap at both seams.
    for (const auto& c : g.cameras) assert(g.width >= g.xmax - g.xmin && fabs(c.x) < 1e-9 && c.angle == 0.);
    assert(g.cameras[0].y - g.height / 2 < g.cameras[1].y + g.height / 2);
    assert(g.cameras[1].y - g.height / 2 < g.cameras[2].y + g.height / 2);
    assert(g.cameras[0].y + g.height / 2 >= g.ymax && g.cameras[2].y - g.height / 2 <= g.ymin);

    // CAD values: camera axes 266.7 mm apart, PMT rows on the FOV seams/outer edges, window-GEM3 distance.
    const double cad_pmt[8][2] = {{-150, 400.05}, {150, 400.05}, {-150, 133.35}, {150, 133.35},
                                  {-150, -133.35}, {150, -133.35}, {-150, -400.05}, {150, -400.05}};
    vector<pair<double,double>> pmt;
    for (int i = 1; i <= npmt; ++i) {
        pmt.push_back({stod(options.at("pmt_" + to_string(i) + "_x")), stod(options.at("pmt_" + to_string(i) + "_y"))});
        assert(fabs(pmt.back().first - cad_pmt[i-1][0]) < 1e-9 && fabs(pmt.back().second - cad_pmt[i-1][1]) < 1e-9);
    }
    assert(fabs(stod(options.at("dist_gem_pmt")) - 505.1) < 1e-9);
    for (int c = 0; c < 3; ++c) assert(fabs(g.cameras[c].y - 266.7 * (1 - c)) < 1e-9);

    // Light sharing: a source under a PMT gives that PMT the largest mean collection.
    for (int j = 0; j < npmt; ++j) {
        vector<double> mean(npmt, 0.);
        for (int k = 0; k < npmt; ++k) {
            const double dx = pmt[k].first - pmt[j].first, dy = pmt[k].second - pmt[j].second;
            const double h2 = pow(stod(options.at("dist_gem_pmt")), 2), R2 = dx*dx + dy*dy + h2;
            mean[k] = h2 / (R2 * R2);
        }
        for (int k = 0; k < npmt; ++k) assert(k == j || mean[j] > mean[k]);
    }

    // Placement inside every camera keeps the track in FOV and active GEM.
    const vector<double> w = {1, 2, 3};
    int counts[3] = {0, 0, 0};
    for (int parent = 0; parent < 300; ++parent) {
        vector<double> x = {-20, 0, 20}, y = {-5, 0, 5}, z = {200, 210, 220};
        auto r = g.place("geometry-test:" + to_string(parent), x, y, z, w);
        ++counts[r.camera];
        for (size_t i = 0; i < x.size(); ++i) {
            auto l = g.local(r.camera, x[i], y[i]);
            assert(fabs(l[0]) <= g.width / 2 && fabs(l[1]) <= g.height / 2);
            assert(x[i] >= g.xmin && x[i] <= g.xmax && y[i] >= g.ymin && y[i] <= g.ymax);
        }
    }
    for (int c : counts) assert(c > 60);

    if (argc > 2) {  // Optional SVG: GEM (black), FOVs (blue), PMTs (red), lens axes (+).
        ofstream svg(argv[2]);
        svg << "<svg xmlns='http://www.w3.org/2000/svg' viewBox='-300 -460 600 920'>"
            << "<rect x='-250' y='-400' width='500' height='800' fill='none' stroke='black' stroke-width='2'/>";
        for (const auto& c : g.cameras)
            svg << "<rect x='" << c.x - g.width/2 << "' y='" << -c.y - g.height/2 << "' width='" << g.width
                << "' height='" << g.height << "' fill='blue' fill-opacity='0.08' stroke='blue'/>";
        for (auto [x, y] : pmt) svg << "<circle cx='" << x << "' cy='" << -y << "' r='11' fill='red'/>";
        svg << "</svg>";
    }
    cout << "PASS: CYGNO04_CAD_v1 CAD values, tiling, containment, light sharing, camera balance "
         << counts[0] << "/" << counts[1] << "/" << counts[2] << "\n";
}
