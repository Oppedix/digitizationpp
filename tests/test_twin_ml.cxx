#include "TwinML.h"
#include "PMTSignal.h"
#include "DigitizationRunner.h"
#include "TFile.h"
#include <cassert>
#include <fstream>
using namespace std;

void fixture(const string& name) {
    TFile f(name.c_str(),"RECREATE");
    int species=name.find("he.root")!=string::npos ? 1000020040 : name.find("carbon.root")!=string::npos ? 1000060120 : name.find("fluorine.root")!=string::npos ? 1000090190 : 11;
    const bool nuclear=species!=11;
    int eventnumber=7,numhits=3,particle_type=species;
    double energyDep=4,energyDep_NR=nuclear ? 2 : 0,energyDep_QF=nuclear ? 2.8 : 4;
    vector<double> x={-1,0,1},y={-2,0,2},z={10,20,30},raw={1,2,1},nr={0,0,0},mom={0,1,0};
    vector<double> visible=nuclear ? vector<double>{.4,2,.4} : raw;
    if(nuclear) nr={.4,0,.4};
    vector<int> pdg={species,11,species},parent={0,1,0},track={1,2,1};
    TTree hits("nTuple","nTuple");
#define B(name) hits.Branch(#name,&name)
    B(eventnumber);B(numhits);B(particle_type);B(energyDep);B(energyDep_NR);B(energyDep_QF);
#undef B
    for(const auto& axis:vector<pair<string,vector<double>*>>{{"x_hits",&x},{"y_hits",&y},{"z_hits",&z},
        {"energyDep_hits",&raw},{"energyDep_hits_QF",&visible},{"energyDep_hits_QF_geant",&raw},
        {"energyDep_hits_NRQF",&nr},{"px_particle",&mom},{"py_particle",&mom},{"pz_particle",&mom},{"tracklen_hits",&raw}})
        hits.Branch(axis.first.c_str(),axis.second);
    hits.Branch("pdgID_hits",&pdg);hits.Branch("parentID_hits",&parent);hits.Branch("trackID_hits",&track);
    hits.Fill();
    eventnumber=8;numhits=0;x.clear();y.clear();z.clear();raw.clear();visible.clear();nr.clear();pdg.clear();parent.clear();track.clear();
    energyDep=energyDep_QF=0;hits.Fill();
    eventnumber=9;numhits=3;x={-1,0,1};y={-2,0,2};z={0,10,600};raw={1,2,1};visible=nuclear ? vector<double>{.4,2,.4} : raw;nr=nuclear ? vector<double>{.4,0,.4} : vector<double>{0,0,0};pdg={species,11,species};parent={0,1,0};track={1,2,1};energyDep=4;energyDep_QF=nuclear ? 2.8 : 4;hits.Fill();
    eventnumber=10;z={10,20,30};raw={1e-9,2e-9,1e-9};visible=nuclear ? vector<double>{.4e-9,2e-9,.4e-9} : raw;
    nr=nuclear ? vector<double>{.4e-9,0,.4e-9} : vector<double>{0,0,0};energyDep=4e-9;energyDep_QF=nuclear ? 2.8e-9 : 4e-9;hits.Fill();
    eventnumber=11;hits.Fill(); // Deliberately no matching tree1 row.
    hits.Write();
    TTree primary("tree1","tree1");
    double Energy=.1,xpos_vertex=0,ypos_vertex=0,zpos_vertex=0,dir_x=0,dir_y=name.find("one.root")!=string::npos ? 1 : -1,dir_z=0;
#define B(name) primary.Branch(#name,&name)
    B(eventnumber);B(Energy);B(xpos_vertex);B(ypos_vertex);B(zpos_vertex);B(dir_x);B(dir_y);B(dir_z);
#undef B
    // Reversed order is intentional: a row-index join must fail this test.
    for(int id:{10,9,8,7}) {eventnumber=id;primary.Fill();} primary.Write();
    primary.ResetBranchAddresses();hits.ResetBranchAddresses();
}
int main(int argc,char** argv) {
    assert(argc==5);const string mode=argv[1];
    if(mode=="fixture") {fixture(argv[2]);if(string(argv[3])!="unused") fixture(argv[3]);return 0;}
    if(mode=="run") {
        DigitizationRunner runner(argv[2],argv[3],argv[4]);
        x_pix=y_pix=32; // Test-only small camera, same production renderer.
        runner.run();return 0;
    }
    if(mode=="check") {
        TFile first(argv[2]),repeat(argv[3]),different(argv[4]);
        auto a=first.Get<TTree>("event_info"),b=repeat.Get<TTree>("event_info"),c=different.Get<TTree>("event_info");
        assert(a && b && c && a->GetEntries()==5 && b->GetEntries()==5);
        TwinML::Record r,s,t;
        auto bind=[](TTree* tree,TwinML::Record& record) {
            tree->SetBranchAddress("ml_primary_pdg",&record.primary_pdg);tree->SetBranchAddress("ml_class_nr",&record.class_nr);tree->SetBranchAddress("ml_valid",&record.valid);tree->SetBranchAddress("ml_initial_uy",&record.initial_uy);
            tree->SetBranchAddress("ml_evis_total_keV",&record.evis_total_keV);
        };
        bind(a,r);bind(b,s);bind(c,t);
        vector<double>* image=nullptr;vector<double>* other=nullptr;vector<double>* changed=nullptr;vector<double>* truth_z=nullptr;
        string* parent_id=nullptr;string* reason=nullptr;
        a->SetBranchAddress("ml_hit_z_mm",&truth_z);a->SetBranchAddress("obs_camera_counts",&image);b->SetBranchAddress("obs_camera_counts",&other);c->SetBranchAddress("obs_camera_counts",&changed);
        a->SetBranchAddress("ml_parent_id",&parent_id);a->SetBranchAddress("ml_reject_reason",&reason);
        vector<int>* light_valid=nullptr;
        a->SetBranchAddress("pmt_signal_valid",&light_valid);
        vector<vector<double>*> fast(8,nullptr),slow(8,nullptr),fast_b(8,nullptr),slow_b(8,nullptr);
        for(int i=0;i<8;++i) {
            a->SetBranchAddress(("wav_fast_pmt"+to_string(i+1)).c_str(),&fast[i]);
            b->SetBranchAddress(("wav_fast_pmt"+to_string(i+1)).c_str(),&fast_b[i]);
            b->SetBranchAddress(("wav_slow_pmt"+to_string(i+1)).c_str(),&slow_b[i]);
            a->SetBranchAddress(("wav_slow_pmt"+to_string(i+1)).c_str(),&slow[i]);
        }
        for(int entry=0;entry<5;++entry) {
            a->GetEntry(entry);b->GetEntry(entry);c->GetEntry(entry);
            assert(*image==*other && image->size()==1024);
            for(int i=0;i<8;++i) {assert(fast[i]->size()==1024 && slow[i]->size()==4000);assert(*fast[i]==*fast_b[i] && *slow[i]==*slow_b[i]);}
            if(entry==0) {assert(r.valid && r.initial_uy==(string(argv[2]).find("/one/")!=string::npos ? -1 : 1) && abs(r.evis_total_keV-(r.primary_pdg==11 ? 4 : 2.8))<1e-10);assert(*image!=*changed);assert(parent_id->find(":7")!=string::npos);}
            else if(entry==3) {assert(all_of(light_valid->begin(),light_valid->end(),[](int v){return v==0;}));assert(any_of(fast[0]->begin(),fast[0]->end(),[](double v){return v!=0.;}));assert(r.valid && r.evis_total_keV>0 && r.evis_total_keV<1e-8);assert(any_of(image->begin(),image->end(),[](double x){return x!=0.;}));}
            else {if(entry==2) assert(truth_z->size()==3 && any_of(truth_z->begin(),truth_z->end(),[](double z){return z<0 || z>500;}));assert(!r.valid && !reason->empty());for(const auto& wave:fast) assert(all_of(wave->begin(),wave->end(),[](double v){return v==0.;}));}
        }
        a->ResetBranchAddresses();b->ResetBranchAddresses();c->ResetBranchAddresses();
        delete light_valid;delete truth_z;delete image;delete other;delete changed;delete parent_id;delete reason;
        for(auto wave:fast) delete wave;for(auto wave:fast_b) delete wave;for(auto wave:slow) delete wave;for(auto wave:slow_b) delete wave;
        return 0;
    }
    assert(mode=="unit");
    for(int pdg:{11,1000020040,1000060120,1000090190}) {
        const bool nr=pdg!=11;
        assert(TwinML::energies({2,1},{nr ? .5 : 2.,1},{pdg,11},{nr ? .5 : 0.,0},{.3,1},3,nr ? 1.5 : 3)==(nr ? 1.5 : 3));
    }
    bool rejected=false;
    try {TwinML::energies({1},{2},{11},{0},{1},1,2);}catch(const invalid_argument&){rejected=true;}assert(rejected);
    TwinML::Record record;record.seed=123;record.image({{70000.25,0},{1e12,3}},0);
    assert(record.camera_signal[0]==70000.25 && record.camera_signal[1]==1e12);
    assert(record.legacy_sparse_clipped);
    auto obs=record.camera_counts;record.initial_ux=-1;record.z_centroid_mm=432;record.evis_total_keV=999;record.image({{70000.25,0},{1e12,3}},0);assert(obs==record.camera_counts);
    map<string,vector<PMTData>> hits={{"pmt_1",{{1,120},{1,140}}},{"pmt_8",{{1,130}}}};
    TwinML::trigger(hits);assert(hits["pmt_1"][0].arrival_time==0 && hits["pmt_8"][0].arrival_time==10);
    auto shifted=hits;
    for(auto& [channel,data]:shifted) for(auto& hit:data) hit.arrival_time+=12345;
    TwinML::trigger(shifted);assert(shifted["pmt_8"][0].arrival_time==hits["pmt_8"][0].arrival_time);
    TwinML::Primary primary{.1,0,0,0,1,2,3};
    auto direction=TwinML::direction(primary,{{"MC_xaxis","-z"},{"MC_yaxis","y"},{"MC_zaxis","x"}});
    assert(direction[0]>0 && direction[1]>0 && direction[2]<0);
    // Two independent source files may reuse event numbers; maps never cross file scope.
    TFile one(argv[2]),two(argv[3]);auto p=TwinML::primaries(one.Get<TTree>("tree1"));auto q=TwinML::primaries(two.Get<TTree>("tree1"));
    assert(p.size()==4 && q.size()==4 && p.at(7).uy==1 && q.at(7).uy==-1 && one.GetUUID()!=two.GetUUID());
    { // A duplicate event ID must not silently overwrite a primary map entry.
        TTree duplicate("tree1","duplicate");int eventnumber=7;double value=1;
        duplicate.Branch("eventnumber",&eventnumber);
        for(const string key:{"Energy","xpos_vertex","ypos_vertex","zpos_vertex","dir_x","dir_y","dir_z"})
            duplicate.Branch(key.c_str(),&value);
        duplicate.Fill();duplicate.Fill();rejected=false;
        try {TwinML::primaries(&duplicate);}catch(const runtime_error&){rejected=true;}assert(rejected);
        duplicate.ResetBranchAddresses();
    }
    cout<<"PASS: ML schema, file-local join, energies, direction, lossless signal and detected-light trigger\n";
}
