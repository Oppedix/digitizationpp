#include "TwinPlacement.h"
#include "PMTHits.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <set>
using namespace std;

int main(int argc,char** argv) {
    // SYNTHETIC geometry only: not verified CYGNO04 optical/PMT measurements.
    map<string,string> options={{"twin_geometry_verified","True"},{"twin_side","L"},
        {"twin_source_id","synthetic"},{"twin_master_seed","2026"},{"variant_id","0"},
        {"min_drift","0"},{"max_drift","500"},{"pmt_number","8"},{"pmt_mode","True"},{"exposure_time_effect","False"},
        {"x_dim","400"},{"y_dim","250"},{"twin_gem_xmin","-250"},{"twin_gem_xmax","250"},
        {"twin_gem_ymin","-400"},{"twin_gem_ymax","400"},{"pmt_radius","11"},{"dist_gem_pmt","190"}};
    for(int i=0;i<3;++i) {
        string k="twin_camera_"+to_string(i);
        options[k+"_x"]="0"; options[k+"_y"]=to_string(265*(1-i)); options[k+"_angle_rad"]="0";
    }
    for(int i=0;i<4;++i) {
        string k="twin_depth_"+to_string(i);
        options[k+"_min"]=options[k+"_max"]=to_string(75+100*i);
    }
    for(int i=1;i<=8;++i) {
        options["pmt_"+to_string(i)+"_x"]=to_string(i%2 ? -200 : 200);
        options["pmt_"+to_string(i)+"_y"]=to_string(-300+200*((i-1)/2));
    }
    const auto original_options=options;
    const vector<double> sx={-5,0,5},sy={-10,0,10},sz={10,20,40},w={1,2,3};
    set<int> seen;
    ofstream svg(argc>1 ? argv[1] : "twin-synthetic.svg");
    svg<<"<svg xmlns='http://www.w3.org/2000/svg' viewBox='-300 -450 600 900'><rect x='-250' y='-400' width='500' height='800' fill='white' stroke='black'/>";
    TwinPlacement base(options);
    for(const auto& c:base.cameras) svg<<"<rect x='"<<c.x-base.width/2<<"' y='"<<c.y-base.height/2<<"' width='"<<base.width<<"' height='"<<base.height<<"' fill='none' stroke='blue'/>";
    for(int i=1;i<=8;++i) svg<<"<circle cx='"<<options.at("pmt_"+to_string(i)+"_x")<<"' cy='"<<options.at("pmt_"+to_string(i)+"_y")<<"' r='11' fill='red'/>";
    for(int parent=0;parent<100;++parent) {
        int camera=-1; vector<double> paired_x,paired_y;
        for(int variant=0;variant<4;++variant) {
            options["variant_id"]=to_string(variant);
            TwinPlacement placement(options);
            auto x=sx,y=sy,z=sz;
            auto result=placement.place("synthetic/file:"+to_string(parent),x,y,z,w);
            seen.insert(result.camera);
            assert(abs(result.actual-(75+100*variant))<1e-10);
            assert(w==vector<double>({1,2,3}));
            if(variant==0) {camera=result.camera;paired_x=x;paired_y=y;}
            else {assert(camera==result.camera);assert(x==paired_x && y==paired_y);}
            for(size_t i=0;i<x.size();++i) {
                auto local=placement.local(result.camera,x[i],y[i]);
                auto global=placement.global(result.camera,local[0],local[1]);
                assert(abs(global[0]-x[i])<1e-10 && abs(global[1]-y[i])<1e-10);
                assert(abs(local[0])<=placement.width/2 && abs(local[1])<=placement.height/2);
                assert(x[i]>=placement.xmin && x[i]<=placement.xmax && y[i]>=placement.ymin && y[i]<=placement.ymax);
                assert(z[i]>=0 && z[i]<=500);
                for(size_t j=0;j<x.size();++j) {
                    double before=pow(sx[i]-sx[j],2)+pow(sy[i]-sy[j],2)+pow(sz[i]-sz[j],2);
                    double after=pow(x[i]-x[j],2)+pow(y[i]-y[j],2)+pow(z[i]-z[j],2);
                    assert(abs(before-after)<1e-8);
                }
                if(parent<12 && variant==0) svg<<"<circle cx='"<<x[i]<<"' cy='"<<y[i]<<"' r='2' fill='green'/>";
            }
        }
    }
    assert(seen.size()==3);
    // PMT geometry stays identical when the light source moves; compare collection means statistically.
    double near=0,far=0;
    for(int i=0;i<30;++i) {
        auto a=PhotonPropagation({-200.},{-300.},{10000000},{0.},original_options).pmt_hits();
        auto b=PhotonPropagation({200.},{300.},{10000000},{0.},original_options).pmt_hits();
        near+=a.at("pmt_1")[0].hits; far+=b.at("pmt_1")[0].hits;
    }
    assert(near>far*10);
    for(int i=1;i<=8;++i) for(const string axis:{"x","y"}) {
        auto key="pmt_"+to_string(i)+"_"+axis; assert(options.at(key)==original_options.at(key));
    }
    auto x=sx,y=sy,z=sz; bool rejected=false;
    try { base.place("too-deep",x,y,z={0,20,600},w); } catch(const runtime_error&) {rejected=true;}
    assert(rejected && x==sx && y==sy); // Failure cannot partially mutate truth.
    x={-1000,0,1000};y=sy;z=sz;rejected=false;
    try {base.place("too-wide",x,y,z,w);} catch(const runtime_error&) {rejected=true;} assert(rejected);
    options["variant_id"]="0";
    options["twin_depth_0_min"]="50"; options["twin_depth_0_max"]="100";
    for(int i=0;i<3;++i) options["twin_camera_"+to_string(i)+"_angle_rad"]="0.2";
    TwinPlacement rotated(options);
    for(int n=0;n<10;++n) {
        x=sx;y=sy;z=sz;const auto r=rotated.place("rotated:"+to_string(n),x,y,z,w);
        assert(r.target>=50 && r.target<=100 && abs(r.actual-r.target)<1e-10);
        for(size_t i=0;i<x.size();++i) {
            auto q=rotated.local(r.camera,x[i],y[i]);
            assert(abs(q[0])<=rotated.width/2+1e-10 && abs(q[1])<=rotated.height/2+1e-10);
        }
    }
    options["twin_geometry_verified"]="False";rejected=false;
    try {TwinPlacement invalid(options);} catch(const invalid_argument&) {rejected=true;} assert(rejected);
    svg<<"<text x='-280' y='-420' font-size='12'>SYNTHETIC ONLY: blue cameras, red PMTs, green tracks</text></svg>";
    cout<<"PASS: paired placement, centroid, containment, rigid truth, fixed PMTs and XY collection\n";
}
