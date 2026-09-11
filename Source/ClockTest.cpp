// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#include "ClockPlanner.h"
#include <vector>
#include <iostream>
#include <cstdlib>
#include <limits>
void check(bool ok,const char* why){if(!ok){std::cerr<<"FAIL "<<why<<std::endl;std::exit(1);}}
int main(){
    for(double rate:{44100.,48000.,192000.})for(int block:{64,256,1024})for(double bpm:{90.,120.,150.}){
        control::ClockPlanner p;std::vector<control::ClockEvent> events;
        for(int frame=0;frame<int(rate*2);frame+=block){int n=std::min(block,int(rate*2)-frame);
            p.process(1,true,bpm,true,true,frame*bpm/(60*rate),rate,n,frame/rate*1000,[&](uint8_t b,double t,uint32_t g){events.push_back({b,t,g});});}
        check(events.size()==size_t(bpm*24/30),"24 pulses per quarter at multiple rates and block sizes");
        for(size_t i=0;i<events.size();++i)check(events[i].byte==0xf8&&std::abs(events[i].due-i*60000/(bpm*24))<.00001,"clock timestamps and no duplicated boundaries");
    }
    control::ClockPlanner p;std::vector<control::ClockEvent> e;
    auto run=[&](int mode,bool valid,double bpm,bool playing,double ppq){e.clear();p.process(mode,valid,bpm,playing,true,ppq,48000,1000,0,[&](uint8_t b,double t,uint32_t g){e.push_back({b,t,g});});};
    run(2,true,120,true,0);check(e.size()==2&&e[0].byte==0xfa&&e[1].byte==0xf8,"start precedes first clock");auto generation=p.generation;
    run(2,true,120,true,4);check(e.size()==3&&e[0].byte==0xfc&&e[1].byte==0xfa&&p.generation!=generation,"seek invalidates scheduled events and restarts transport");
    run(2,true,120,false,4);check(e.size()==2&&e[0].byte==0xfc&&e[1].byte==0xf8,"stop transport retains tempo clock");
    run(0,true,120,false,4);check(e.empty(),"off sends no clock");run(1,false,120,true,0);check(e.empty(),"missing host tempo sends no clock");
    run(1,true,std::numeric_limits<double>::quiet_NaN(),true,0);check(e.empty(),"nonfinite tempo rejected");
    run(1,true,120,false,0);check(e.size()==1&&e[0].byte==0xf8,"tempo mode works while stopped");
    run(1,true,90,false,0);check(e.size()==1&&e[0].byte==0xf8,"tempo change continues free clock");
    run(2,true,120,true,0);run(0,true,120,true,0);check(e.size()==1&&e[0].byte==0xfc,"turning clock off stops owned transport");
    std::cout<<"PASS: 24 PPQN, 27 rate/block/tempo combinations, timestamps, seek, transport, stopped clock, invalid tempo, off.\n";
}
