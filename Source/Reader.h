// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#pragma once
#include "Shared.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace bridge {
// Consumer-local cursor; several plugin instances can independently read one capture stream.
class Reader {
public:
    static constexpr int taps=64, phases=512;
    void prepare(double outputRate) {hostRate=outputRate;sourceRate=96000;makeKernel();reset();}
    void reset(){position=-1;integral=0;filteredError=0;fade=0;lastGeneration=-1;}
    double bufferedMs=0, correctionPpm=0;
    uint64_t resyncs=0;
    int latencySamples=0;
    bool streaming=false;
    void process(Shared* s,float* l,float* r,int n) {
        std::fill(l,l+n,0.0f);std::fill(r,r+n,0.0f);streaming=false;
        if(n<=0)return;
        if(!s || read(s->version)!=1 || read(s->status)!=1 || GetTickCount64()-(ULONGLONG)read(s->serverHeartbeat)>1000) {position=-1;fade=0;return;}
        const double rate=(double)read(s->sampleRate);
        if(rate<=0 || hostRate<=0)return;
        if(sourceRate!=rate){sourceRate=rate;makeKernel();position=-1;}
        auto generation=read(s->generation);
        if(generation!=lastGeneration){position=-1;fade=0;lastGeneration=generation;}
        const auto w=read(s->writeFrame);
        const double target=std::max(rate*0.04,rate/hostRate*n*3.0+taps);
        latencySamples=(int)std::ceil((target+read(s->inputLatency))/rate*hostRate);
        if(target>capacity/2)return;
        if(position<0) {if(w<target+taps)return;position=w-target;integral=0;filteredError=0;fade=0;}
        double fill=w-position;
        if(fill<taps+rate/hostRate*n*1.01 || fill>capacity-taps-n*rate/hostRate) {position=-1;fade=0;++resyncs;return;}
        double error=(fill-target)/rate;
        double dt=n/hostRate;
        filteredError+=(1-std::exp(-dt/1.0))*(error-filteredError);
        integral=std::clamp(integral+filteredError*dt*0.002,-0.003,0.003);
        double correction=std::clamp(filteredError*0.03+integral,-0.005,0.005);
        double step=rate/hostRate*(1+correction);
        correctionPpm=correction*1e6;bufferedMs=fill/rate*1000;
        for(int i=0;i<n;++i){
            auto base=(LONG64)std::floor(position);
            int phase=std::min(phases-1,(int)((position-base)*phases));
            float a=0,b=0;
            for(int j=0;j<taps;++j){float x,y;unpack(read(s->samples[(base+j-(taps/2-1))&(capacity-1)]),x,y);auto k=kernel[phase*taps+j];a+=x*k;b+=y*k;}
            fade=std::min(1.0f,fade+(float)(1.0/(hostRate*0.01)));
            l[i]=std::isfinite(a)?a*fade:0;r[i]=std::isfinite(b)?b*fade:0;
            position+=step;
        }
        // Discard a block if capture restarted or overtook the reader during processing.
        if(read(s->generation)!=generation || read(s->writeFrame)-position>capacity-taps){
            std::fill(l,l+n,0.0f);std::fill(r,r+n,0.0f);position=-1;fade=0;++resyncs;return;
        }
        streaming=true;
    }
private:
    double hostRate=48000,sourceRate=0,position=-1,integral=0,filteredError=0;
    LONG64 lastGeneration=-1;
    float fade=0;
    std::array<float,taps*phases> kernel{};
    void makeKernel(){
        const double pi=3.14159265358979323846;
        const double cutoff=0.94*std::min(1.0,hostRate/sourceRate);
        for(int p=0;p<phases;++p){double sum=0;for(int j=0;j<taps;++j){
            double x=j-(taps/2-1)-(double)p/phases;
            double v=std::abs(x)<1e-10?cutoff:std::sin(pi*cutoff*x)/(pi*x);
            double window=0.42+0.5*std::cos(2*pi*x/taps)+0.08*std::cos(4*pi*x/taps);
            kernel[p*taps+j]=(float)(v*window);sum+=v*window;
        }for(int j=0;j<taps;++j)kernel[p*taps+j]/=(float)sum;}
    }
};
}
