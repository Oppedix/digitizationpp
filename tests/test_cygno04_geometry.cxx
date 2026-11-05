// Consistency checks of the nominal CYGNO04 geometry in config/ConfigFile_TWIN_CYGNO04.txt.
// It checks internal consistency (tiling, containment, symmetry, light sharing), NOT that the
// inferred PMT positions match the as-built detector. See docs/TWIN_DIGI_CHANGES.md.
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
    assert(options.at("twin_geometry_id") == "CYGNO04_nominal_v1");
    assert(options.at("Camera_type") == "Quest2");
    // Test-only sign-off of the gate; the shipped template keeps it False.
    assert(options.at("twin_geometry_verified") == "False");
    options["twin_geometry_verified"] = "True";
    options["twin_source_id"] = "geometry-test";

    TwinPlacement g(options);
    const int npmt = PhotonPropagation::validateGeometry(options);
    assert(npmt == 8);

    // Quest2 aspect ratio (checkDimensionConsistency) and the 0.130 mm effective pixel.
    assert(fabs(g.width / g.height - 4096. / 2304.) < 1e-9);
    assert(fabs(g.width / 4096. - 0.130) < 1e-9);

    // Three FOVs cover the whole 500 x 800 mm GEM, with overlap at both seams.
    for (const auto& c : g.cameras) assert(g.width >= g.xmax - g.xmin && fabs(c.x) < 1e-9 && c.angle == 0.);
    assert(g.cameras[0].y - g.height / 2 < g.cameras[1].y + g.height / 2);
    assert(g.cameras[1].y - g.height / 2 < g.cameras[2].y + g.height / 2);
    assert(g.cameras[0].y + g.height / 2 >= g.ymax && g.cameras[2].y - g.height / 2 <= g.ymin);

    // PMTs: inside the GEM footprint, mirror-symmetric in X and Y, two per seam/edge row.
    vector<pair<double,double>> pmt;
    for (int i = 1; i <= npmt; ++i)
        pmt.push_back({stod(options.at("pmt_" + to_string(i) + "_x")), stod(options.at("pmt_" + to_string(i) + "_y"))});
    for (auto [x, y] : pmt) {
        assert(x > g.xmin && x < g.xmax && y > g.ymin && y < g.ymax);
        bool mx = false, my = false;
        for (auto [u, v] : pmt) { mx |= fabs(u + x) < 1e-9 && fabs(v - y) < 1e-9; my |= fabs(u - x) < 1e-9 && fabs(v + y) < 1e-9; }
        assert(mx && my);
    }

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
    cout << "PASS: CYGNO04_nominal_v1 tiling, containment, PMT symmetry/light sharing, camera balance "
         << counts[0] << "/" << counts[1] << "/" << counts[2] << "\n";
}
