#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <variant>
#include <vector>
#include "PMTHits.h"
// Seed stochastic physics for reproducible comparisons without a production API.
#define private public
#include "PMTSignal.h"
#undef private
#include "ConfigManager.h"
#include "TFile.h"
#include "TTree.h"
using namespace std;

int main(int argc, char** argv) {
    assert(argc == 3);
    const string repo = argv[1], temp = argv[2];
    for (int count : {4,8}) {
        ConfigManager config;
        assert(config.loadConfig(repo+"/config/ConfigFile_new.txt"));
        const string settings = temp+"/config.txt";
        {
            ofstream file(settings);
            file << "'pmt_number': " << count << ",\n'fast_window_len': 2048,\n'slow_window_len': 2048,\n"
                    "'fast_freq': 1e9,\n'slow_freq': 1e9,\n'detector': 'TEST',\n";
            for (int i=1;i<=count;++i) {
                // Synthetic geometry: no claim about physical CYGNO04 positions.
                file << "'pmt_"<<i<<"_x': "<<(i-1)*50<<",\n'pmt_"<<i<<"_y': 0,\n";
            }
        }
        assert(config.loadConfig(settings));
        auto options = config.getOptions();
        assert(PhotonPropagation::validateGeometry(options) == count);
        for (const string bad : {"0","9","4.5","nan"}) {
            auto invalid = options; invalid["pmt_number"] = bad;
            bool rejected=false;
            try { PhotonPropagation::validateGeometry(invalid); } catch(const invalid_argument&) { rejected=true; }
            assert(rejected);
        }
        auto invalid=options; invalid.erase("pmt_"+to_string(count)+"_x");
        bool rejected=false;
        try { PhotonPropagation::validateGeometry(invalid); } catch(const invalid_argument&) { rejected=true; }
        assert(rejected);
        invalid=options; invalid["pmt_1_x"]="REQUIRED";
        rejected=false;
        try { PhotonPropagation::validateGeometry(invalid); } catch(const invalid_argument&) { rejected=true; }
        assert(rejected);

        // Large counts make Poisson fluctuations small relative to the analytic mean.
        PhotonPropagation propagation({0.},{0.},{100000000},{17.},options);
        auto hits=propagation.pmt_hits();
        assert(hits.size()==static_cast<size_t>(count));
        for(int i=1;i<=count;++i) {
            const double dx=(i-1)*50., z=stod(options.at("dist_gem_pmt")), r=stod(options.at("pmt_radius"));
            const double mean=1e8*r*r*z*z/(4*pow(dx*dx+z*z,2));
            assert(abs(hits.at("pmt_"+to_string(i))[0].hits-mean)<8*sqrt(mean));
            assert(hits.at("pmt_"+to_string(i))[0].arrival_time==17.);
        }
        assert(PhotonPropagation({0.},{0.},{0},{0.},options).pmt_hits().empty());
        // Zero PSD fixtures avoid downloads; real OpenCV mode still executes its IFFT.
        filesystem::create_directories(temp+"/pmt");
        for(int i=1;i<=count;++i) for(const string type : {"fast","slow"}) {
            ofstream file(temp+"/pmt/TEST_"+type+"_noise_pmt_"+to_string(i)+".txt");
            for(int n=0;n<2001;++n) file << "0\n";
        }
        map<string,vector<PMTData>> light;
        for(int i=1;i<=count;++i) light["pmt_"+to_string(i)]={{100,17.}};
        SignalSimulation signal(light,options,temp+"/");
        rejected=false;
        try { signal.compute_noise({}, "Fast"); } catch(const runtime_error&) { rejected=true; }
        assert(rejected);
        for (const string key : {"fast_window_len", "slow_window_len"}) {
            auto bad=options; bad[key]="0"; rejected=false;
            try { SignalSimulation invalid_signal({},bad,temp+"/"); } catch(const invalid_argument&) { rejected=true; }
            assert(rejected);
        }
        signal.gen.seed(42);
        map<string,vector<double>> fast_signals,slow_signals;
        signal.simulated_signals(fast_signals,slow_signals);
        assert(fast_signals.size()==static_cast<size_t>(count+1));
        assert(slow_signals.size()==static_cast<size_t>(count+1));
        for(int i=1;i<=count;++i) {
            const auto& fast=fast_signals.at("pmt_"+to_string(i));
            const auto& slow=slow_signals.at("pmt_"+to_string(i));
            assert(fast.size()==2048 && slow.size()==2048);
            assert(any_of(fast.begin(),fast.end(),[](double x){return x!=0.;}));
            // Equal sampling frequencies: only the unchanged trigger offsets differ.
            for(size_t n=0;n<700;++n) assert(fast[n]==slow[n+1266]);
        }
        if(count==4) {
            SignalSimulation legacy(light,options,temp+"/"); legacy.gen.seed(42);
            map<string,vector<double>> old_fast,old_slow;
            for(const string pmt : {"pmt_1","pmt_2","pmt_3","pmt_4"}) legacy.pmt_signal(pmt,old_fast,old_slow);
            for(const auto& [channel,wave] : old_fast) assert(wave==fast_signals.at(channel));
            for(const auto& [channel,wave] : old_slow) assert(wave==slow_signals.at(channel));
        }
        const string output=temp+"/output.root";
        {
            TFile file(output.c_str(),"RECREATE");
#include "storage.inc"
            auto outtree=make_unique<TTree>("event_info","event_info");
#include "branches.inc"
            assert(outtree->GetListOfBranches()->GetEntries()==2*count+1);
            for(int i=1;i<=count;++i) {
                assert(outtree->GetBranch(("wav_fast_pmt"+to_string(i)).c_str()));
                assert(outtree->GetBranch(("wav_slow_pmt"+to_string(i)).c_str()));
            }
            auto pmt_hits_map=light;
#include "assign.inc"
            outtree->Fill();
            {   // Legacy mode: skipped/no-light events keep empty waveforms, as on upstream main.
                const bool twin_mode=false;
#include "reset.inc"
                for(int i=0;i<count;++i) assert(wav_fast[i].empty() && wav_slow[i].empty() && pmt_signal_valid[i]==0);
            }
            outtree->Fill();
            {   // TWIN mode: fixed-length zero waveforms for every ML record.
                const bool twin_mode=true;
#include "reset.inc"
                for(int i=0;i<count;++i) {
                    assert(wav_fast[i].size()==2048 && wav_slow[i].size()==2048);
                    assert(all_of(wav_fast[i].begin(),wav_fast[i].end(),[](double x){return x==0.;}));
                    assert(pmt_signal_valid[i]==0);
                }
            }
            outtree->Fill(); outtree->Write();
            outtree->ResetBranchAddresses(); outtree.reset();
        }
        {
            TFile file(output.c_str()); auto tree=file.Get<TTree>("event_info");
            vector<double>* wave=nullptr;
            tree->SetBranchAddress(("wav_fast_pmt"+to_string(count)).c_str(),&wave);
            tree->GetEntry(0); assert(any_of(wave->begin(),wave->end(),[](double x){return x!=0.;}));
            tree->GetEntry(1); assert(wave->empty());
            tree->GetEntry(2); assert(wave->size()==2048);
            assert(all_of(wave->begin(),wave->end(),[](double x){return x==0.;}));
            tree->ResetBranchAddresses(); delete wave;
        }
        SignalSimulation empty({},options,temp+"/");
        empty.simulated_signals(fast_signals,slow_signals);
        for(const auto& [key,wave] : fast_signals) if(key!="time")
            assert(wave.size()==2048 && all_of(wave.begin(),wave.end(),[](double x){return x==0.;}));
        empty.ptc_hits_["pmt_1"]={{100,17.}};
        empty.simulated_signals(fast_signals,slow_signals);
        assert(any_of(fast_signals.at("pmt_1").begin(),fast_signals.at("pmt_1").end(),[](double x){return x!=0.;}));
        assert(all_of(fast_signals.at("pmt_"+to_string(count)).begin(),fast_signals.at("pmt_"+to_string(count)).end(),[](double x){return x==0.;}));
        for(const string digitizer : {"Fast","Slow"}) {
            empty.digitizers_=digitizer;
            empty.simulated_signals(fast_signals,slow_signals);
            const auto& disabled = digitizer=="Fast" ? slow_signals : fast_signals;
            assert(all_of(disabled.at("pmt_1").begin(),disabled.at("pmt_1").end(),[](double x){return x==0.;}));
        }
        // pmt_noise switch and template remapping (legacy default = identity, noise on).
        assert(SignalSimulation::noiseEnabled(options));
        assert(SignalSimulation::noiseTemplateChannels(options,count)==[&]{vector<int> v; for(int i=1;i<=count;++i) v.push_back(i); return v;}());
        for (const string bad : {"0;1","a","1;2","1.5"}) {
            auto invalid=options; invalid["pmt_noise_channels"]=bad; rejected=false;
            try { SignalSimulation::noiseTemplateChannels(invalid,count); } catch(const invalid_argument&) { rejected=true; }
            assert(rejected);
        }
        {   // Templates exist only for channel 1: every PMT reuses it, no other file is read.
            auto mapped=options; mapped["detector"]="ONLY1"; mapped["pmt_noise_channels"]="1";
            for(int i=2;i<=count;++i) mapped["pmt_noise_channels"]+=";1";
            for(const string type : {"fast","slow"}) {
                ofstream file(temp+"/pmt/ONLY1_"+type+"_noise_pmt_1.txt");
                for(int n=0;n<2001;++n) file << "0\n";
            }
            SignalSimulation reused({},mapped,temp+"/");
            assert(reused.fast_noise_.size()==static_cast<size_t>(count));
            auto silent=options; silent["detector"]="NOFILES"; silent["pmt_noise"]="False";
            SignalSimulation quiet(light,silent,temp+"/"); quiet.gen.seed(42);
            map<string,vector<double>> qf,qs; quiet.simulated_signals(qf,qs);
            for(int i=1;i<=count;++i) assert(qf.at("pmt_"+to_string(i)).size()==2048 &&
                all_of(quiet.fast_noise_.at("pmt_"+to_string(i)).begin(),quiet.fast_noise_.at("pmt_"+to_string(i)).end(),[](double x){return x==0.;}));
            auto bad=options; bad["pmt_noise"]="maybe"; rejected=false;
            try { SignalSimulation::noiseEnabled(bad); } catch(const invalid_argument&) { rejected=true; }
            assert(rejected);
        }
        auto defaults=options;
        defaults["fast_window_len"]="1024"; defaults["slow_window_len"]="4000";
        SignalSimulation lengths({},defaults,temp+"/");
        lengths.simulated_signals(fast_signals,slow_signals);
        assert(fast_signals.at("pmt_1").size()==1024 && slow_signals.at("pmt_1").size()==4000);
        cout << "PASS: "<<count<<" PMTs, geometry, shared fast/slow realization, lengths, ROOT roundtrip, empty/consecutive events\n";
    }
    // The CYGNO04 one-camera template is now complete and runnable with 8 PMTs.
    ConfigManager production;
    assert(production.loadConfig(repo+"/config/ConfigFile_CYGNO04_8PMT.txt"));
    assert(PhotonPropagation::validateGeometry(production.getOptions())==8);
    assert(SignalSimulation::noiseTemplateChannels(production.getOptions(),8)==(vector<int>{1,2,3,4,1,2,3,4}));
    assert(production.get("twin_cygno04_mode").empty() && production.get("min_drift")=="0." && production.get("max_drift")=="500.");
    cout << "PASS: CYGNO04 8-PMT template, noise switch and template remapping\n";
}
