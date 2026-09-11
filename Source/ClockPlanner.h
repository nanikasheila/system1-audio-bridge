// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#pragma once
#include <cmath>
#include <cstdint>

namespace control {
// Audio-thread-only planner. Timestamped events are delivered by the MIDI worker.
// MIDI Clock is 24 pulses per quarter note; SYSTEM-1 does not accept song position.
class ClockPlanner {
public:
    template<class Send> void process(int mode, bool valid, double bpm, bool playing,
            bool hasPpq, double ppq, double rate, int frames, double now, Send send) {
        valid=valid&&std::isfinite(bpm)&&bpm>=1&&bpm<=1000&&std::isfinite(rate)&&rate>0&&frames>0;
        hasPpq=hasPpq&&std::isfinite(ppq);
        const bool running=mode>0&&valid;
        const bool seek=running&&playing&&wasPlaying&&hasPpq&&hadPpq&&std::abs(ppq-expectedPpq)>0.02;
        const bool reset=mode!=lastMode||running!=wasRunning||seek||playing!=wasPlaying;
        if(reset){++generation;freePhase=0;}
        if(transportStarted&&(!running||mode!=2||!playing||seek)){
            send(0xfc,now,generation);transportStarted=false;
        }
        if(running&&mode==2&&playing&&!transportStarted){send(0xfa,now,generation);transportStarted=true;}
        if(running){
            const double ticksPerSample=bpm*24.0/(60.0*rate);
            double phase=playing&&hasPpq&&std::isfinite(ppq)?ppq*24.0:freePhase;
            const double end=phase+frames*ticksPerSample;
            auto tick=std::ceil(phase-1.0e-8);
            for(;tick<end-1.0e-8;tick+=1){
                const double offset=std::fmax(0.0,(tick-phase)/ticksPerSample);
                send(0xf8,now+offset/rate*1000.0,generation);
            }
            freePhase=end-std::floor(end);
        }
        lastMode=mode;wasRunning=running;wasPlaying=playing;hadPpq=hasPpq;
        expectedPpq=ppq+frames*bpm/(60.0*rate);
    }
    void reset(){lastMode=-1;wasRunning=false;wasPlaying=false;hadPpq=false;transportStarted=false;freePhase=0;++generation;}
    uint32_t generation=1;
private:
    int lastMode=0;bool wasRunning=false,wasPlaying=false,hadPpq=false,transportStarted=false;
    double expectedPpq=0,freePhase=0;
};
struct ClockEvent {uint8_t byte;double due;uint32_t generation;};
}
