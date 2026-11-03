#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// Geometry and truth placement only: no detector-response RNG is consumed here.
struct TwinPlacement {
    struct Camera { double x, y, angle; };
    struct Result { int camera; double dx, dy, dz, target, actual; bool drift_contained; };
    std::array<Camera,3> cameras;
    std::array<std::array<double,2>,4> depths;
    double width, height, xmin, xmax, ymin, ymax;
    std::uint64_t seed;
    int variant;
    std::string side, source;

    explicit TwinPlacement(const std::map<std::string,std::string>& options) {
        auto text = [&](const std::string& key) {
            auto it=options.find(key);
            if(it==options.end() || it->second.empty() || it->second=="REQUIRED")
                throw std::invalid_argument("TWIN requires " + key);
            return it->second;
        };
        auto number = [&](const std::string& key) {
            const auto s=text(key); size_t used=0; double v=std::stod(s,&used);
            if(used!=s.size() || !std::isfinite(v)) throw std::invalid_argument("Invalid TWIN value: "+key);
            return v;
        };
        if(text("twin_geometry_verified")!="True")
            throw std::invalid_argument("Verify required CYGNO04 geometry before enabling TWIN");
        side=text("twin_side"); source=text("twin_source_id");
        if(side!="L" && side!="R") throw std::invalid_argument("twin_side must be L or R");
        const auto seed_text=text("twin_master_seed"); size_t used=0;
        if(seed_text.find_first_not_of("0123456789")!=std::string::npos)
            throw std::invalid_argument("twin_master_seed must be an unsigned integer");
        seed=std::stoull(seed_text,&used);
        const double v=number("variant_id");
        if(v<0 || v>3 || v!=std::floor(v)) throw std::invalid_argument("variant_id must be 0..3");
        variant=static_cast<int>(v);
        if(number("drift_length")!=500.) throw std::invalid_argument("TWIN requires drift_length=500 mm");
        if(number("pmt_number")!=8 || text("pmt_mode")!="True") throw std::invalid_argument("TWIN requires eight enabled PMTs");
        if(text("exposure_time_effect")!="False") throw std::invalid_argument("TWIN requires exposure_time_effect=False to retain truth");
        width=number("x_dim"); height=number("y_dim");
        xmin=number("twin_gem_xmin"); xmax=number("twin_gem_xmax");
        ymin=number("twin_gem_ymin"); ymax=number("twin_gem_ymax");
        if(width<=0 || height<=0 || xmin>=xmax || ymin>=ymax) throw std::invalid_argument("Invalid TWIN XY bounds");
        for(int i=0;i<3;++i) {
            const auto key="twin_camera_"+std::to_string(i);
            cameras[i]={number(key+"_x"),number(key+"_y"),number(key+"_angle_rad")};
        }
        for(int i=0;i<4;++i) {
            const auto key="twin_depth_"+std::to_string(i);
            depths[i]={number(key+"_min"),number(key+"_max")};
            if(depths[i][0]<0 || depths[i][1]>500 || depths[i][0]>depths[i][1])
                throw std::invalid_argument("Invalid TWIN depth stratum: "+key);
        }
    }
    static std::uint64_t hash(const std::string& key) {
        std::uint64_t h=14695981039346656037ULL;
        for(unsigned char c:key) { h^=c; h*=1099511628211ULL; }
        return h; // FNV-1a, specified integer arithmetic; never std::hash.
    }
    static double uniform(std::mt19937_64& rng) { return (rng()>>11)*0x1.0p-53; }
    std::array<double,2> local(int id,double x,double y) const {
        const auto& c=cameras.at(id); const double a=std::cos(c.angle), b=std::sin(c.angle);
        return {a*(x-c.x)+b*(y-c.y),-b*(x-c.x)+a*(y-c.y)};
    }
    std::array<double,2> global(int id,double x,double y) const {
        const auto& c=cameras.at(id); const double a=std::cos(c.angle), b=std::sin(c.angle);
        return {c.x+a*x-b*y,c.y+b*x+a*y};
    }
    Result place(const std::string& parent, std::vector<double>& x,std::vector<double>& y,
                 std::vector<double>& z,const std::vector<double>& weights,bool require_drift_containment=true) const {
        if(x.empty() || x.size()!=y.size() || x.size()!=z.size() || x.size()!=weights.size())
            throw std::invalid_argument("TWIN empty or mismatched parent hits: "+parent);
        double total=0, weighted=0;
        for(size_t i=0;i<x.size();++i) {
            if(!std::isfinite(x[i]) || !std::isfinite(y[i]) || !std::isfinite(z[i]) ||
               !std::isfinite(weights[i]) || weights[i]<0) throw std::invalid_argument("Invalid TWIN hit: "+parent);
            total+=weights[i]; weighted+=weights[i]*z[i];
        }
        if(!std::isfinite(total) || total<=0 || !std::isfinite(weighted)) throw std::invalid_argument("Invalid TWIN ionization weight: "+parent);
        const std::string key=parent+"|"+side+"|"+std::to_string(seed);
        std::mt19937_64 xy(hash(key));
        const int id=xy()%3;
        double lx=INFINITY,hx=-INFINITY,ly=INFINITY,hy=-INFINITY;
        for(size_t i=0;i<x.size();++i) {
            auto p=local(id,x[i],y[i]);
            lx=std::min(lx,p[0]); hx=std::max(hx,p[0]); ly=std::min(ly,p[1]); hy=std::max(hy,p[1]);
        }
        const double ax=-width/2-lx,bx=width/2-hx,ay=-height/2-ly,by=height/2-hy;
        if(ax>bx || ay>by) throw std::runtime_error("TWIN parent cannot fit camera: "+parent);
        double dx=0,dy=0; bool contained=false;
        const double ca=std::cos(cameras[id].angle),sa=std::sin(cameras[id].angle);
        // Sample camera-local translations, rejecting overlap with the active GEM boundary.
        for(int trial=0;trial<10000 && !contained;++trial) {
            double u=ax+(bx-ax)*uniform(xy),v=ay+(by-ay)*uniform(xy);
            dx=ca*u-sa*v; dy=sa*u+ca*v; contained=true;
            for(size_t i=0;i<x.size();++i)
                if(x[i]+dx<xmin || x[i]+dx>xmax || y[i]+dy<ymin || y[i]+dy>ymax) { contained=false; break; }
        }
        if(!contained) throw std::runtime_error("TWIN camera/GEM placement failed after 10000 trials: "+parent);
        std::mt19937_64 depth(hash(key+"|depth|"+std::to_string(variant)));
        const auto range=depths[variant];
        const double target=range[0]+(range[1]-range[0])*uniform(depth), dz=target-weighted/total;
        const bool contained_z=std::all_of(z.begin(),z.end(),[&](double hit){return hit+dz>=0 && hit+dz<=500;});
        if(require_drift_containment && !contained_z)
            throw std::runtime_error("TWIN full-track drift containment failed: "+parent+" variant "+std::to_string(variant));
        double actual=0;
        for(size_t i=0;i<x.size();++i) { x[i]+=dx; y[i]+=dy; z[i]+=dz; actual+=weights[i]*z[i]; }
        return {id,dx,dy,dz,target,actual/total,contained_z};
    }
};
