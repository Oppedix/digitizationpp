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
#include "DriftRange.h"

// Geometry and truth placement only: no detector-response RNG is consumed here.
struct TwinPlacement {
    struct Camera { double x, y, angle; };
    struct Result { int camera; double dx, dy, dz, target, actual; bool drift_contained; };
    std::array<Camera,3> cameras;
    double fixed_depth; // NaN: one uniform depth per parent. A number: depth of the charge centroid [mm]
    double width, height, xmin, xmax, ymin, ymax, drift_length;
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
        if(v<0 || v!=std::floor(v)) throw std::invalid_argument("variant_id must be a non-negative integer");
        variant=static_cast<int>(v);
        const auto drift=activeDriftRange(options);
        if(drift.first!=0.) throw std::invalid_argument("TWIN requires min_drift=0");
        drift_length=drift.second; // every track is placed inside 0..max_drift
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
        fixed_depth=NAN; // optional key 'twin_depth_mm': fixed depth for the diagnostic set
        if(options.count("twin_depth_mm")) {
            fixed_depth=number("twin_depth_mm");
            if(fixed_depth<0 || fixed_depth>drift_length)
                throw std::invalid_argument("twin_depth_mm must lie between 0 and max_drift");
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
        const double centroid=weighted/total;
        const auto [lowest,highest]=std::minmax_element(z.begin(),z.end());
        const double dz_min=-*lowest, dz_max=drift_length-*highest;   // shifts that keep the whole track in the drift
        const bool contained_z = std::isnan(fixed_depth) ? dz_min<=dz_max
                               : fixed_depth-centroid>=dz_min && fixed_depth-centroid<=dz_max;
        const double dz = !std::isnan(fixed_depth) ? fixed_depth-centroid          // fixed depth of the centroid
                        : contained_z ? dz_min+(dz_max-dz_min)*uniform(depth)      // one uniform depth per parent
                        : (dz_min+dz_max)/2;                                       // longer than the drift: rejected
        const double target=centroid+dz;
        if(require_drift_containment && !contained_z)
            throw std::runtime_error("TWIN full-track drift containment failed: "+parent+" variant "+std::to_string(variant));
        double actual=0;
        for(size_t i=0;i<x.size();++i) { x[i]+=dx; y[i]+=dy; z[i]+=dz; actual+=weights[i]*z[i]; }
        return {id,dx,dy,dz,target,actual/total,contained_z};
    }
};
