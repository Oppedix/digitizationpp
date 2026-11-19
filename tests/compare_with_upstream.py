"""Is the standard (non-TWIN) digitizer of this branch still the upstream one?

    python3 tests/compare_with_upstream.py <checkout of CYGNUS-RD/digitizationpp main>

Builds this branch and the upstream checkout with the same two stubs (no ROOTANA, no OpenCV), runs both on the
events of tests/upstream_fixture.cxx with upstream's ConfigFile_new.txt and a fixed seed, with the upstream
options primary_granularity and sigma0_light off and on, and compares every camera image pixel by pixel and
every number of event_info. The only difference allowed is particle_type of the event without hits (upstream
leaves it undefined). The PMTs are off: without a fixed seed of their own they differ from run to run.
Run it after every merge of upstream. About five minutes.
"""
import ast
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

import ROOT

ROOT.gROOT.SetBatch(True)
repositories = {"ours": Path(__file__).resolve().parents[1], "upstream": Path(sys.argv[1]).resolve()}
root_flags = shlex.split(subprocess.check_output(["root-config", "--cflags", "--libs"], text=True))

with tempfile.TemporaryDirectory(prefix="upstream-compare-") as directory:
    work = Path(directory)
    (work/"input").mkdir()
    subprocess.run(["c++", str(repositories["ours"]/"tests/upstream_fixture.cxx"), *root_flags, "-o", str(work/"fixture")], check=True)
    subprocess.run([str(work/"fixture"), str(work/"input/mc.root")], check=True)
    (work/"run.cxx").write_text('#include "DigitizationRunner.h"\n'
                                'int main(int, char** argv) { DigitizationRunner runner(argv[1], argv[2], argv[3]); runner.run(); return 0; }\n')

    for name, repository in repositories.items():   # the same stubs as tests/run_twin_ml_tests.py
        build = work/name
        build.mkdir()
        header = (repository/"include/DigitizationRunner.h").read_text().replace('#include "cygnolib.h"', '''
#include <sstream>
#include <chrono>
#include <unistd.h>
namespace cygnolib { inline void joinMidasPedestals(const std::vector<int>&,const std::string&,const std::string&,const std::string&) {
    throw std::runtime_error("ROOTANA pedestal unavailable in this comparison"); } }''')
        (build/"DigitizationRunner.h").write_text("#include <vector>\n#include <string>\n#include <stdexcept>\n" + header)
        signal = "\n".join(line for line in (repository/"src/PMTSignal.cxx").read_text().splitlines()
                           if "opencv2/" not in line and "using namespace cv;" not in line)
        signal = signal[:signal.index("vector<double> SignalSimulation::compute_noise(")] + '''
vector<double> SignalSimulation::compute_noise(const vector<double>&,const string&) { throw std::runtime_error("OpenCV unavailable in this comparison"); }
'''
        (build/"PMTSignal.cxx").write_text(signal)
        sources = ["DigitizationRunner.cxx", "TrackProcessor.cxx", "ConfigManager.cxx", "Globals.cxx", "Utils.cxx", "PMTHits.cxx"]
        subprocess.run(["c++", "-O2", '-DTWIN_DIGITIZER_COMMIT="COMPARISON"', "-I" + str(build), "-I" + str(repository/"include"), "-I/opt/homebrew/include",
                        str(work/"run.cxx"), *[str(repository/"src"/source) for source in sources], str(build/"PMTSignal.cxx"),
                        *root_flags, "-L/opt/homebrew/lib", "-ltbb", "-o", str(build/"run")], check=True)

    settings = ast.literal_eval((repositories["upstream"]/"config/ConfigFile_new.txt").read_text())
    # Identity axes put the fixture where it was written; lower GEM1 and GEM2 voltages only make the run shorter.
    settings.update(fixed_seed=True, bckg=False, Vignetting=False, pmt_mode=False, redpix_output=False, Parallel_threads=1,
                    MC_xaxis="x", MC_yaxis="y", MC_zaxis="z", x_offset=0., y_offset=0., z_offset=0., GEM1_HV=400., GEM2_HV=400.)
    identical = True
    for granularity in (False, True):
        for blur_mm in (0., 0.2):
            case = f"primary_granularity {granularity}, sigma0_light {blur_mm}"
            settings.update(primary_granularity=granularity, sigma0_light=blur_mm)
            config = work/"config.txt"
            config.write_text("{\n" + "".join(f"{key!r}: {value!r},\n" for key, value in settings.items()) + "}\n")
            files = {}
            for name in repositories:
                output = work/name/f"{granularity}_{blur_mm}"
                output.mkdir()
                log = subprocess.run([str(work/name/"run"), str(config), str(work/"input"), str(output)], text=True, capture_output=True)
                if log.returncode:
                    print(log.stdout[-3000:], log.stderr[-3000:])
                    sys.exit(f"{name} failed with {case}")
                files[name] = ROOT.TFile(str(next(output.rglob("histograms_*.root"))))
            ours, upstream = files["ours"], files["upstream"]
            images = sorted(key.GetName() for key in upstream.GetListOfKeys() if key.GetName().startswith("pic_"))
            assert images and images == sorted(key.GetName() for key in ours.GetListOfKeys() if key.GetName().startswith("pic_"))
            different_images = []
            for image in images:
                difference = upstream.Get(image).Clone("difference")
                difference.Add(ours.Get(image), -1)
                if difference.GetMaximum() != 0 or difference.GetMinimum() != 0:
                    different_images.append(image)
            tree_ours, tree_upstream = ours.Get("event_info"), upstream.Get("event_info")
            assert tree_ours.GetEntries() == tree_upstream.GetEntries() == len(images)
            numbers = [leaf.GetName() for leaf in tree_upstream.GetListOfLeaves()
                       if leaf.GetTypeName() in ("Int_t", "Float_t", "Double_t", "UInt_t") and tree_ours.GetLeaf(leaf.GetName())]
            different_numbers = set()
            for row in range(tree_upstream.GetEntries()):
                tree_ours.GetEntry(row)
                tree_upstream.GetEntry(row)
                for number in numbers:
                    a, b = getattr(tree_ours, number), getattr(tree_upstream, number)
                    allowed = number == "particle_type" and tree_upstream.nhits_og == 0 and a == 0
                    if a != b and not (a != a and b != b) and not allowed:
                        different_numbers.add(number)
            identical &= not different_images and not different_numbers
            print(f"{case}: {len(images)} images, {len(numbers)} numbers per event; "
                  f"different images: {different_images or 'none'}; different numbers: {sorted(different_numbers) or 'none'}", flush=True)
    if not identical:
        sys.exit("FAIL: the standard path differs from upstream")
    print("PASS: standard path identical to upstream: every image pixel and every event_info number, 4 option settings")
