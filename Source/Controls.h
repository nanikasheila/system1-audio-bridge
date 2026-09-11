// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace control {
struct Spec {int cc; const char* name; int group; int initial;};
// Stable raw CC parameters preserve existing DAW automation and saved states.
// Named selector values and display conversion live in ControlDisplay.h.
inline constexpr Spec specs[]{
 {3,"LPF Cutoff",0,127},{9,"Resonance",0,0},{79,"HPF Cutoff",0,0},
 {81,"Filter Envelope",0,64},{82,"Filter Key Follow",0,64},{115,"Filter Type",0,0},
 {83,"Filter Attack",0,0},{84,"Filter Decay",0,64},{85,"Filter Sustain",0,127},{86,"Filter Release",0,0},
 {89,"Amp Attack",0,0},{90,"Amp Decay",0,64},{96,"Amp Sustain",0,127},{97,"Amp Release",0,0},{69,"Amp Tone",0,64},
 {46,"OSC1 Wave",1,0},{47,"OSC1 Range",1,64},{50,"OSC1 Color",1,0},{52,"OSC1 Cross Mod",1,0},{60,"OSC1 Mod Source",1,0},
 {61,"OSC2 Wave (CC61)",1,0},{62,"OSC2 Range",1,64},{55,"OSC2 Color",1,0},{56,"OSC2 Tune",1,64},{63,"OSC2 Mod Source",1,0},
 {111,"OSC2 Ring",1,0},{112,"OSC2 Sync",1,0},
 {16,"OSC1 Level",2,100},{17,"OSC2 Level",2,100},{18,"Sub Level",2,0},{19,"Noise Level",2,0},
 {113,"Sub OSC Type",2,0},{114,"Noise Type",2,0},{12,"Crusher",2,0},{13,"Delay Time",2,64},{94,"Delay Level",2,0},{91,"Reverb",2,0},
 {35,"LFO Wave",3,0},{29,"LFO Rate",3,64},{27,"LFO Fade",3,0},{26,"LFO Pitch",3,0},{28,"LFO Filter",3,0},{30,"LFO Amp",3,0},
 {117,"LFO Key Trigger",3,0},{118,"Tempo Sync",3,0},{22,"Pitch Envelope",3,64},{23,"Pitch Attack",3,0},{24,"Pitch Decay",3,0},
 {5,"Portamento",3,0},{116,"Legato",3,0},{119,"Mono",3,0},{1,"Modulation",3,0},{11,"Expression",3,127},{64,"Hold Pedal",3,0},
 // Roland firmware update sheets v1.11 / v1.20. Append to retain existing parameter indices.
 {41,"Bend Range",3,2},{87,"OSC2 Coarse Tune",1,64},{105,"OSC1 Extended Wave",1,0},{106,"OSC2 Extended Wave",1,0}
};
inline constexpr int count=sizeof(specs)/sizeof(specs[0]);
inline int indexForCC(int cc){for(int i=0;i<count;++i)if(specs[i].cc==cc)return i;return -1;}
inline bool matchesDevice(const juce::String& name){
    // Exclude SYSTEM-1 CTRL / SYSTEM-1m and other similarly named endpoints.
    return name.trim().equalsIgnoreCase("SYSTEM-1");
}
struct Event {uint32_t bytes=0; double due=0;};
template<int N,class T=Event> struct Queue {
    std::array<T,N> data{}; std::atomic<unsigned> read{0},write{0};
    bool push(T e){auto w=write.load(std::memory_order_relaxed);auto next=(w+1)%N;if(next==read.load(std::memory_order_acquire))return false;data[w]=e;write.store(next,std::memory_order_release);return true;}
    bool peek(T& e){auto r=read.load(std::memory_order_relaxed);if(r==write.load(std::memory_order_acquire))return false;e=data[r];return true;}
    void pop(){auto r=read.load(std::memory_order_relaxed);read.store((r+1)%N,std::memory_order_release);}
    // Consumer only: discard the queued snapshot, including more than one worker batch.
    void discard(){read.store(write.load(std::memory_order_acquire),std::memory_order_release);}
};
}
