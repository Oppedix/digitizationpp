// MC events for tests/compare_with_upstream.py: together they take every route of TrackProcessor::computeWithSaturation.
#include "TFile.h"
#include "TTree.h"
#include "TRandom3.h"
#include <vector>
#include <string>
using namespace std;
int main(int, char** argv) {
    TFile f(argv[1], "RECREATE");
    int eventnumber = 0, numhits = 0, particle_type = 11;
    double energyDep = 0, energyDep_NR = 0, energyDep_QF = 0;
    vector<double> x, y, z, raw, visible, nr, mom, len;
    vector<int> pdg, parent, track;
    TTree hits("nTuple", "nTuple");
#define B(name) hits.Branch(#name, &name)
    B(eventnumber); B(numhits); B(particle_type); B(energyDep); B(energyDep_NR); B(energyDep_QF);
#undef B
    for (const auto& b : vector<pair<string, vector<double>*>>{{"x_hits", &x}, {"y_hits", &y}, {"z_hits", &z},
        {"energyDep_hits", &raw}, {"energyDep_hits_QF", &visible}, {"energyDep_hits_QF_geant", &raw},
        {"energyDep_hits_NRQF", &nr}, {"px_particle", &mom}, {"py_particle", &mom}, {"pz_particle", &mom}, {"tracklen_hits", &len}})
        hits.Branch(b.first.c_str(), b.second);
    hits.Branch("pdgID_hits", &pdg); hits.Branch("parentID_hits", &parent); hits.Branch("trackID_hits", &track);
    TRandom3 r(1);
    // energy [keV], hits, extent in x, y, z [mm], first z [mm], nuclear recoil
    auto event = [&](double energy, int n, double dx, double dy, double dz, double z0, bool nuclear) {
        ++eventnumber; numhits = n; particle_type = nuclear ? 1000020040 : 11;
        x.clear(); y.clear(); z.clear(); raw.clear(); visible.clear(); nr.clear(); mom.clear(); len.clear(); pdg.clear(); parent.clear(); track.clear();
        for (int i = 0; i < n; ++i) {
            const double t = n > 1 ? double(i) / (n - 1) : 0.;
            x.push_back(-0.5 * dx + t * dx + r.Gaus(0, 0.2)); y.push_back(-0.5 * dy + t * dy + r.Gaus(0, 0.2)); z.push_back(z0 + t * dz);
            raw.push_back(energy / n); visible.push_back(energy / n); nr.push_back(nuclear ? 0.6 * energy / n : 0.);
            mom.push_back(0); len.push_back(1); pdg.push_back(particle_type); parent.push_back(0); track.push_back(1);
        }
        energyDep = energy; energyDep_NR = nuclear ? energy : 0.; energyDep_QF = energy;
        hits.Fill();
    };
    event(6, 5, 1, 1, 1, 100, false);          // sparse map, one batch
    event(30, 30, 20, 2, 10, 200, false);      // dense, one slab
    event(300, 200, 100, 50, 30, 150, false);  // dense, several slabs
    event(110, 40, 150, 150, 60, 300, true);   // nuclear recoil above 100 keV: sparse map in WID batches
    event(0, 0, 0, 0, 0, 0, false);            // no hits
    event(8, 12, 3, 3, 40, -15, false);        // crosses the GEM plane
    event(20, 20, 5, 5, 60, 480, false);       // crosses max_drift
    event(1e-9, 3, 1, 1, 1, 50, false);        // below ion_pot
    hits.Write();
}
