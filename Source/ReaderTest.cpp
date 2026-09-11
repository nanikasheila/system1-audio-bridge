// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#include "Reader.h"
#include <memory>
#include <vector>
#include <iostream>
int main(){
    bool ok=true;
    for(double rate:{44100.,48000.,96000.,192000.})for(double drift:{-300.,300.}){
        auto s=std::make_unique<bridge::Shared>();
        bridge::put(s->version,1);bridge::put(s->status,1);bridge::put(s->sampleRate,96000);bridge::put(s->generation,1);
        bridge::Reader reader;reader.prepare(rate);
        const int block=256;std::vector<float> l(block),r(block);double count=0,energy=0;int samples=0,gaps=0;bool started=false;
        for(int b=0;b<(int)(rate*90/block);++b){
            count+=block*96000/rate*(1+drift/1e6);
            auto w=bridge::read(s->writeFrame);auto end=(LONG64)count;
            for(auto i=w;i<end;++i){auto x=(float)(0.5*std::sin(i*2*3.141592653589793*1000/96000));bridge::put(s->samples[i&(bridge::capacity-1)],bridge::pack(x,-x));}
            bridge::put(s->writeFrame,end);bridge::put(s->serverHeartbeat,(LONG64)GetTickCount64());
            reader.process(s.get(),l.data(),r.data(),block);
            if(started&&!reader.streaming)++gaps;if(reader.streaming)started=true;
            if(b>rate/block)for(int i=0;i<block;++i){if(!std::isfinite(l[i])||std::abs(l[i]+r[i])>1e-6)ok=false;energy+=l[i]*l[i];++samples;}
        }
        double rms=std::sqrt(energy/samples);
        bool passed=reader.resyncs==0&&gaps==0&&std::abs(rms-0.353553)<0.003&&std::abs(reader.correctionPpm-drift)<200;
        ok&=passed;
        std::cout<<(passed?"PASS ":"FAIL ")<<rate<<" Hz drift "<<drift<<" ppm | correction "<<reader.correctionPpm<<" | buffer "<<reader.bufferedMs<<" ms | resyncs "<<reader.resyncs<<" | gaps "<<gaps<<" | RMS "<<rms<<std::endl;
        bridge::put(s->status,0);reader.process(s.get(),l.data(),r.data(),block);for(auto v:l)if(v!=0)ok=false;
    }
    return ok?0:1;
}
