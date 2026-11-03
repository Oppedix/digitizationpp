#pragma once
#include "TwinPlacement.h"
#include "PMTHits.h"
#include "TTree.h"
#include "TTreeReader.h"
#include "TTreeReaderValue.h"
#include "TObjString.h"
#include "TMap.h"
#include "TROOT.h"
#ifndef TWIN_DIGITIZER_COMMIT
#define TWIN_DIGITIZER_COMMIT "unversioned"
#endif
#include <set>
#include <limits>
#include <numeric>

namespace TwinML {
struct Primary { double energy,x,y,z,ux,uy,uz; };
inline void require(TTree* tree,const std::vector<std::string>& names) {
    if(!tree) throw std::runtime_error("Missing required MC tree");
    for(const auto& name:names) if(!tree->GetBranch(name.c_str()))
        throw std::runtime_error("Missing MC branch: "+name);
}
inline std::map<int,Primary> primaries(TTree* tree) {
    require(tree,{"eventnumber","Energy","xpos_vertex","ypos_vertex","zpos_vertex","dir_x","dir_y","dir_z"});
    TTreeReader reader(tree);
    TTreeReaderValue<int> event(reader,"eventnumber");
    TTreeReaderValue<double> energy(reader,"Energy"),x(reader,"xpos_vertex"),y(reader,"ypos_vertex"),z(reader,"zpos_vertex");
    TTreeReaderValue<double> ux(reader,"dir_x"),uy(reader,"dir_y"),uz(reader,"dir_z");
    std::map<int,Primary> result;
    while(reader.Next()) if(!result.emplace(*event,Primary{*energy,*x,*y,*z,*ux,*uy,*uz}).second)
        throw std::runtime_error("Ambiguous tree1 eventnumber within source file");
    if(reader.GetEntryStatus()!=TTreeReader::kEntryBeyondEnd) throw std::runtime_error("Cannot read tree1 schema");
    return result;
}
inline void uniqueEvents(TTree* tree) {
    require(tree,{"eventnumber"}); TTreeReader reader(tree); TTreeReaderValue<int> event(reader,"eventnumber");
    std::set<int> seen;
    while(reader.Next()) if(!seen.insert(*event).second) throw std::runtime_error("Duplicate nTuple eventnumber within source file");
    if(reader.GetEntryStatus()!=TTreeReader::kEntryBeyondEnd) throw std::runtime_error("Cannot read nTuple eventnumber");
}
inline unsigned stream(std::uint64_t seed,const std::string& key) {
    return static_cast<unsigned>(TwinPlacement::hash(std::to_string(seed)+"|"+key)%4294967295ULL)+1U;
}
inline std::array<double,3> axes(const std::array<double,3>& input,const std::map<std::string,std::string>& options) {
    std::array<double,3> out{}; std::set<int> axes;
    for(int i=0;i<3;++i) {
        const auto& key=options.at(std::string("MC_")+char('x'+i)+"axis");
        int axis=key.back()-'x';
        if(axis<0 || axis>2 || !axes.insert(axis).second) throw std::invalid_argument("Invalid MC direction axes");
        out[axis]=(key.front()=='-' ? -1. : 1.)*input[i];
    }
    return out;
}
inline std::array<double,3> direction(const Primary& p,const std::map<std::string,std::string>& options) {
    if(!std::isfinite(p.energy) || p.energy<0 || !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
        throw std::invalid_argument("Invalid generated primary energy/vertex");
    auto out=axes({p.ux,p.uy,p.uz},options);
    const double norm=std::hypot(out[0],out[1],out[2]);
    if(!std::isfinite(norm) || norm<=0) throw std::invalid_argument("Invalid generated primary direction");
    for(auto& v:out) v/=norm;
    return out; // Translations never enter a direction transform. Frame = common GEM XY/Z.
}
inline std::vector<double> vertex(const Primary& p,const std::map<std::string,std::string>& options,
                                  const std::vector<double>& shift) {
    auto out=axes({p.x,p.y,p.z},options);
    for(int i=0;i<3;++i) out[i]+=std::stod(options.at(std::string(1,char('x'+i))+"_offset"))+
        std::stod(options.at(std::string(1,char('x'+i))+"_extra"))+shift.at(i);
    return {out[0],out[1],out[2]};
}
inline bool close(double a,double b) { return std::isfinite(a) && std::isfinite(b) && std::abs(a-b)<=1e-6+1e-6*std::max(std::abs(a),std::abs(b)); }
inline double energies(const std::vector<double>& raw,const std::vector<double>& visible,
                       const std::vector<int>& pdg,const std::vector<double>& nr,
                       const std::vector<double>& geant,double rawTotal,double visibleTotal) {
    if(raw.size()!=visible.size() || raw.size()!=pdg.size() || raw.size()!=nr.size() || raw.size()!=geant.size())
        throw std::invalid_argument("MC energy array size mismatch");
    for(size_t i=0;i<raw.size();++i) {
        if(!std::isfinite(raw[i]) || !std::isfinite(visible[i]) || !std::isfinite(nr[i]) || !std::isfinite(geant[i]) ||
           raw[i]<0 || visible[i]<0 || nr[i]<0 || geant[i]<0)
            throw std::invalid_argument("Nonphysical MC energy deposit");
        const bool nuclear=pdg[i]==2212 || pdg[i]>1000000000;
        if(!close(visible[i],nuclear ? nr[i] : raw[i]) || (!nuclear && !close(nr[i],0.)))
            throw std::invalid_argument("MC QF/NRQF convention mismatch");
    }
    double sum=std::accumulate(visible.begin(),visible.end(),0.);
    if(!std::isfinite(rawTotal) || !std::isfinite(visibleTotal) ||
       !close(std::accumulate(raw.begin(),raw.end(),0.),rawTotal) || !close(sum,visibleTotal))
        throw std::invalid_argument("MC energy total mismatch");
    return sum;
}
inline void configuration(const std::map<std::string,std::string>& options) {
    if(std::string(TWIN_DIGITIZER_COMMIT)=="unversioned")
        throw std::invalid_argument("TWIN ML requires a versioned CMake build");
    auto required=[&](const std::string& key) {
        const auto& v=options.at(key);
        if(v.empty() || v=="REQUIRED") throw std::invalid_argument("TWIN requires "+key);
        return v;
    };
    auto number=[&](const std::string& key) {
        const auto text=required(key);size_t used=0;const double value=std::stod(text,&used);
        if(used!=text.size() || !std::isfinite(value)) throw std::invalid_argument("Invalid physical setting: "+key);
        return value;
    };
    for(const std::string key:{"twin_camera_noise_rms_counts","twin_pmt_noise_rms_v"})
        if(number(key)<0) throw std::invalid_argument("Negative noise RMS: "+key);
    for(const std::string key:{"twin_gas_pressure_mbar","twin_gas_temperature_k","ion_pot","drift_field",
        "pmt_gain","pmt_sigma","pmt_lambda","exp_dispersion_scale","transit_time_spread","photons_per_el"})
        if(number(key)<=0) throw std::invalid_argument("Nonpositive physical setting: "+key);
    if(required("twin_noise_mode")!="synthetic") throw std::invalid_argument("TWIN ML requires explicit offline synthetic noise");
    if(required("bckg")!="False") throw std::invalid_argument("TWIN ML camera noise replaces bckg; set bckg=False");
    if(required("digitizers")!="Both") throw std::invalid_argument("TWIN ML requires both digitizers");
    if(required("twin_gas")!="HeCF4_60_40") throw std::invalid_argument("Current drift model supports HeCF4_60_40 only");
    const double qe=number("twin_pmt_quantum_efficiency");
    if(!std::isfinite(qe) || qe<0 || qe>1) throw std::invalid_argument("Invalid PMT quantum efficiency");
    const auto seed=required("twin_detector_seed");
    if(seed.find_first_not_of("0123456789")!=std::string::npos) throw std::invalid_argument("Invalid detector seed");
    (void)std::stoull(seed);
    const auto response=required("twin_response_id");
    if(response.find_first_not_of("0123456789")!=std::string::npos || std::stoi(response)<0) throw std::invalid_argument("Negative response ID");
    if(std::stoi(required("twin_primary_pdg"))==0) throw std::invalid_argument("Declare generated primary PDG");
    const auto commit=required("twin_mc_commit");
    if(commit.size()!=40 || commit.find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::invalid_argument("twin_mc_commit must identify the source MC build");
}
inline void metadata(const std::map<std::string,std::string>& options) {
    TMap values; values.SetOwnerKeyValue();
    for(const auto& [key,value]:options) values.Add(new TObjString(key.c_str()),new TObjString(value.c_str()));
    values.Write("twin_configuration",TObject::kSingleKey);
    TObjString(TWIN_DIGITIZER_COMMIT).Write("digitizer_commit");
    TObjString(gROOT->GetVersion()).Write("root_version");
    TObjString("TWIN detector response runs with one TBB thread").Write("reproducibility");
    TObjString("energyDep_hits_QF: upstream SRIM/TRIM for ions, raw electronic deposits; no second QF").Write("ionization_convention");
}
inline std::map<std::string,std::string> responseOptions(std::map<std::string,std::string> options,std::uint64_t seed) {
    options["twin_collection_seed"]=std::to_string(stream(seed,"collection"));
    options["twin_spe_seed"]=std::to_string(stream(seed,"spe"));
    options["twin_noise_seed"]=std::to_string(stream(seed,"electronics"));
    return options;
}
inline void trigger(std::map<std::string,std::vector<PMTData>>& hits) {
    double first=std::numeric_limits<double>::infinity();
    for(const auto& [channel,data]:hits) for(const auto& hit:data)
        if(hit.hits>0) first=std::min(first,hit.arrival_time);
    if(!std::isfinite(first)) return;
    for(auto& [channel,data]:hits) for(auto& hit:data) hit.arrival_time-=first;
}
struct Record {
    int schema_version=1,variant_id=-1,camera_id=-1,side_id=-1,primary_pdg=0,response_id=0;
    ULong64_t seed=0;
    bool class_nr=false,direction_valid=false,valid=false,legacy_sparse_clipped=false;
    std::string parent_id,reject_reason,source_file_id;
    std::vector<double> placement_xyz_mm{0,0,0},hit_x_mm,hit_y_mm,hit_z_mm,hit_evis_keV,hit_edep_keV;
    std::vector<unsigned> response_hit_index;
    double z_centroid_mm=0,evis_total_keV=0,initial_ux=0,initial_uy=0,initial_uz=0;
    double primary_energy_MeV=0;
    std::vector<double> primary_vertex_mm, camera_signal, camera_counts;
    std::vector<unsigned> camera_ix,camera_iy;
    void branches(TTree& tree) {
#define ML_BRANCH(name) tree.Branch("ml_" #name,&name)
        ML_BRANCH(schema_version);ML_BRANCH(parent_id);ML_BRANCH(variant_id);ML_BRANCH(seed);
        ML_BRANCH(camera_id);ML_BRANCH(side_id);ML_BRANCH(placement_xyz_mm);
        ML_BRANCH(hit_x_mm);ML_BRANCH(hit_y_mm);ML_BRANCH(hit_z_mm);ML_BRANCH(hit_evis_keV);ML_BRANCH(hit_edep_keV);
        ML_BRANCH(z_centroid_mm);ML_BRANCH(evis_total_keV);ML_BRANCH(primary_pdg);ML_BRANCH(class_nr);
        ML_BRANCH(initial_ux);ML_BRANCH(initial_uy);ML_BRANCH(initial_uz);ML_BRANCH(direction_valid);
        ML_BRANCH(response_id);ML_BRANCH(valid);ML_BRANCH(reject_reason);ML_BRANCH(source_file_id);
        ML_BRANCH(legacy_sparse_clipped);ML_BRANCH(primary_energy_MeV);ML_BRANCH(primary_vertex_mm);ML_BRANCH(response_hit_index);
#undef ML_BRANCH
        tree.Branch("obs_camera_ix",&camera_ix);tree.Branch("obs_camera_iy",&camera_iy);
        tree.Branch("obs_camera_signal",&camera_signal);tree.Branch("obs_camera_counts",&camera_counts);
    }
    void image(const std::vector<std::vector<double>>& signal,double rms) {
        std::mt19937 rng(stream(seed,"camera-noise"));std::normal_distribution<double> gaussian(0,1);
        camera_ix.clear();camera_iy.clear();camera_signal.clear();camera_counts.clear();legacy_sparse_clipped=false;
        if(signal.empty()) return;
        for(size_t y=0;y<signal[0].size();++y) for(size_t x=0;x<signal.size();++x) {
            const double value=signal[x][y];
            if(!std::isfinite(value) || value<0) throw std::runtime_error("Invalid camera signal");
            if(value>65535.) legacy_sparse_clipped=true;
            if(value!=0) {camera_ix.push_back(x);camera_iy.push_back(y);camera_signal.push_back(value);}
            const double observed=value+rms*gaussian(rng);
            if(!std::isfinite(observed)) throw std::runtime_error("Nonfinite camera observation");
            camera_counts.push_back(observed);
        }
    }
};
}
