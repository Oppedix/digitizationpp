"""Focused PMT checks; optional fallback stubs ONLY OpenCV noise IFFT."""
from pathlib import Path
import argparse
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--without-opencv', action='store_true')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='pmt-tests-') as directory:
    tmp = Path(directory)
    runner = (repo/'src/DigitizationRunner.cxx').read_text()
    # Compile the actual runner's storage/branch/reset/assignment blocks with ROOT.
    spans = {
        'storage': ('            // Never resize these outer containers:', '            // ROOT TTree'),
        'branches': ('                for (int i = 0; i < pmt_count; ++i) {', '\n            }\n\n            if (save_mc_first3_hits)'),
        'reset': ('                    // Skipped/no-light events', '\n                }\n\n                inputtree->GetEntry'),
        'assign': ('                        for (int i = 0; i < pmt_count; ++i) {', '\n                    }\n'),
    }
    for name, (start, end) in spans.items():
        a = runner.index(start)
        (tmp/(name+'.inc')).write_text(runner[a:runner.index(end,a)])
    source = repo/'src/PMTSignal.cxx'
    extra = []
    if args.without_opencv:
        print('LIMIT: OpenCV noise IFFT replaced by zero-noise stub; production noise NOT tested.', flush=True)
        text = source.read_text()
        text = '\n'.join(line for line in text.splitlines() if 'opencv2/' not in line and 'using namespace cv;' not in line)
        text = text[:text.index('    vector<double> PSD = psd;')] + '''
    (void)fs; // Used by the production IFFT, omitted only in this test stub.
    return vector<double>(N, 0.);
}
'''
        source = tmp/'PMTSignal.cxx'
        source.write_text(text)
    else:
        extra = shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','opencv4'],text=True))
    flags = shlex.split(subprocess.check_output(['root-config','--cflags','--libs'],text=True))
    command = ['c++','-Wall','-Wextra','-I'+str(repo/'include'),'-I'+str(tmp),
               str(repo/'tests/test_pmt.cxx'),str(source),str(repo/'src/PMTHits.cxx'),
               str(repo/'src/ConfigManager.cxx'),*flags,*extra,'-o',str(tmp/'check')]
    subprocess.run(command,check=True)
    subprocess.run([str(tmp/'check'),str(repo),str(tmp)],check=True)
