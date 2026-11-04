"""Dependency-isolated ROOT integration checks; NOT a full production build."""
import ast
from pathlib import Path
import shlex
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='twin-ml-') as directory:
    tmp = Path(directory)
    print('LIMIT: legacy ROOTANA pedestal loading and OpenCV measured-noise IFFT are replaced by throwing stubs.\n'
          'Actual ROOT, TBB, charge/camera rendering, offline PMT noise, collection and SPE code execute.', flush=True)
    header = (repo/'include/DigitizationRunner.h').read_text().replace('#include "cygnolib.h"', '''
#include <sstream>
#include <chrono>
#include <unistd.h>
namespace cygnolib { inline void joinMidasPedestals(const std::vector<int>&,const std::string&,const std::string&,const std::string&) {
    throw std::runtime_error("ROOTANA pedestal unavailable in isolated test");
} }
''')
    # The replacement needs standard types before its inline declaration.
    (tmp/'DigitizationRunner.h').write_text('#include <vector>\n#include <string>\n#include <stdexcept>\n'+header)
    signal = (repo/'src/PMTSignal.cxx').read_text()
    signal = '\n'.join(line for line in signal.splitlines() if 'opencv2/' not in line and 'using namespace cv;' not in line)
    signal = signal[:signal.index('vector<double> SignalSimulation::compute_noise(')] + '''
vector<double> SignalSimulation::compute_noise(const vector<double>&,const string&) {
    throw std::runtime_error("OpenCV measured PSD unavailable in isolated test");
}
'''
    (tmp/'PMTSignal.cxx').write_text(signal)
    flags = shlex.split(subprocess.check_output(['root-config','--cflags','--libs'],text=True))
    sources = ['DigitizationRunner.cxx','TrackProcessor.cxx','ConfigManager.cxx','Globals.cxx','Utils.cxx','PMTHits.cxx']
    subprocess.run(['c++','-DTWIN_DIGITIZER_COMMIT="TEST-DEPENDENCY-STUB"','-Wall','-Wextra','-I'+str(tmp),'-I'+str(repo/'include'),'-I/opt/homebrew/include',
        str(repo/'tests/test_twin_ml.cxx'),*[str(repo/'src'/s) for s in sources],str(tmp/'PMTSignal.cxx'),
        *flags,'-L/opt/homebrew/lib','-ltbb','-o',str(tmp/'check')],check=True)
    inp=tmp/'input';inp.mkdir()
    one,two=inp/'one.root',inp/'two.root'
    subprocess.run([str(tmp/'check'),'fixture',str(one),str(two),'unused'],check=True)
    subprocess.run([str(tmp/'check'),'unit',str(one),str(two),'unused'],check=True)
    settings=ast.literal_eval((repo/'config/ConfigFile_TWIN_CYGNO04.txt').read_text())
    settings.update(twin_geometry_verified=True,twin_source_id='fixture',twin_side='L',
        twin_mc_commit='7464329b3ed8c99e5e4bdaa84762d7d1db57441a',twin_primary_pdg=11,
        twin_gas_pressure_mbar=900,twin_gas_temperature_k=293.15,twin_detector_seed=123,twin_response_id=0,twin_pmt_quantum_efficiency=.2,twin_pmt_noise_rms_v=.001,twin_camera_noise_rms_counts=1.,
        Camera_type='Fusion',x_dim=400,y_dim=400,MC_xaxis='x',MC_yaxis='-y',MC_zaxis='z',x_offset=0,y_offset=0,z_offset=0,
        twin_gem_xmin=-500,twin_gem_xmax=500,twin_gem_ymin=-500,twin_gem_ymax=500,
        pmt_radius=11,dist_gem_pmt=190,Vignetting=False,c_G=5,alpha_G=0,GEM1_HV=100,GEM2_HV=100,GEM3_HV=100,
        z_vox_dim=2.,Parallel_threads=1)
    for i in range(3): settings[f'twin_camera_{i}_angle_rad']=0
    for i in range(1,9): settings[f'pmt_{i}_x']=(-100 if i%2 else 100);settings[f'pmt_{i}_y']=-150+100*((i-1)//2)
    for run in ('first','repeat','different'):
        settings['twin_detector_seed']=124 if run=='different' else 123
        config=tmp/(run+'.txt')
        config.write_text('{\n'+''.join(f'{key!r}: {value!r},\n' for key,value in settings.items())+'}\n')
        out=tmp/run;out.mkdir()
        result=subprocess.run([str(tmp/'check'),'run',str(config),str(inp),str(out)],text=True,capture_output=True)
        if result.returncode:
            print(result.stdout);print(result.stderr);result.check_returncode()
    # Files receive consecutive run IDs; filesystem traversal is intentionally not assumed.
    for name in ('one','two'):
        paths=[next((tmp/run/name).glob('digi_*.root')) for run in ('first','repeat','different')]
        subprocess.run([str(tmp/'check'),'check',*map(str,paths)],check=True)
    for name,pdg in [('he',1000020040),('carbon',1000060120),('fluorine',1000090190)]:
        sample=tmp/name;sample.mkdir();mc=sample/(name+'.root')
        subprocess.run([str(tmp/'check'),'fixture',str(mc),'unused','unused'],check=True)
        settings['twin_primary_pdg']=pdg
        paths=[]
        for run in ('first','repeat','different'):
            settings['twin_detector_seed']=124 if run=='different' else 123
            cfg=tmp/(name+run+'.txt')
            cfg.write_text('{\n'+''.join(f'{key!r}: {value!r},\n' for key,value in settings.items())+'}\n')
            out=tmp/(name+run);out.mkdir()
            result=subprocess.run([str(tmp/'check'),'run',str(cfg),str(sample),str(out)],text=True,capture_output=True)
            if result.returncode:
                print(result.stdout);print(result.stderr);result.check_returncode()
            paths.append(next((out/name).glob('digi_*.root')))
        subprocess.run([str(tmp/'check'),'check',*map(str,paths)],check=True)
    subprocess.run(['python3',str(repo/'tools/inspect_twin_event.py'),str(paths[0]),'--output',str(tmp/'diagnostic.png')],check=True)
    assert (tmp/'diagnostic.png').stat().st_size>0
    print('PASS: actual runner with synthetic fixtures, rejected/empty records, eight waveforms, same-seed repeat and changed-seed independence')
