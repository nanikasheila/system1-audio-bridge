// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>
namespace control {
struct Option {int value;juce::String label;};
inline constexpr int selectorValues[]{0,25,51,76,102,127};
inline constexpr const char* waveNames[]{"Saw", "Square", "Triangle", "Saw 2", "Square 2", "Triangle 2",
    "Noise Saw", "Logic Operation", "FM", "FM + Sync", "Vowel", "CB (Cowbell)"};
inline std::vector<Option> options(int cc){
    // Hardware sweep, 2026-09-11: discrete positions span the full CC range.
    if(cc==41||cc==87){std::vector<Option> result;int steps=cc==41?23:22;
        for(int i=0;i<=steps;++i){int semitones=cc==41?i+1:i-11;
            result.push_back({juce::roundToInt(i*127.0/steps),juce::String(cc==87&&semitones>0?"+":"")+juce::String(semitones)+" st"});}
        return result;
    }
    switch(cc){
        case 35:return {{0,"Sine"},{25,"Triangle"},{51,"Saw"},{76,"Square"},{102,"Sample & Hold"},{127,"Random"}};
        case 46:case 61:return {{0,"Saw"},{25,"Square"},{51,"Triangle"},{76,"Saw 2"},{102,"Square 2"},{127,"Triangle 2"}};
        case 47:case 62:return {{0,"64'"},{25,"32'"},{51,"16'"},{76,"8'"},{102,"4'"},{127,"2'"}};
        case 60:case 63:return {{0,"Manual"},{25,"LFO"},{51,"Pitch Env"},{76,"Filter Env"},{102,"Amp Env"},{127,"Sub OSC"}};
        // Connected SYSTEM-1, 2026-09-11: CC0 = LED off / Poly,
        // CC64 = LED lit / Mono, CC127 = Unison. Raw CC states stay unchanged.
        case 119:return {{0,"Poly"},{64,"Mono"},{127,"Unison"}};
        case 113:return {{0,"-2 oct"},{127,"-1 oct"}};
        case 114:return {{0,"Pink"},{127,"White"}};
        case 115:return {{0,"24 dB"},{127,"12 dB"}};
        case 105:case 106:return {{0,"Standard"},{1,"Extended"}};
        case 64:case 111:case 112:case 116:case 117:case 118:return {{0,"OFF"},{127,"ON"}};
        default:return {};
    }
}
inline bool isSwitch(int cc){switch(cc){case 64:case 111:case 112:case 113:case 114:case 115:case 116:case 117:case 118:return true;default:return false;}}
inline bool bipolar(int cc){switch(cc){case 22:case 26:case 28:case 30:case 56:case 69:case 81:case 82:case 87:return true;default:return false;}}
inline int closest(const std::vector<Option>& opts,int value){int best=0;for(int i=1;i<int(opts.size());++i)if(std::abs(opts[i].value-value)<std::abs(opts[best].value-value))best=i;return best;}
inline int waveSlot(int value){int best=0;for(int i=1;i<6;++i)if(std::abs(selectorValues[i]-value)<std::abs(selectorValues[best]-value))best=i;return best;}
inline juce::String waveText(int value,int bank){
    if(bank!=0&&bank!=1)return "Unknown bank ("+juce::String(bank)+")";
    int slot=waveSlot(value);return juce::String(selectorValues[slot]==value?"":"~ ")+waveNames[slot+6*bank];
}
// No frequency / time calibration is published: continuous values are shown as
// normalized amounts, never invented Hz, milliseconds or semitone ranges.
inline juce::String display(int cc,int value){
    auto list=options(cc);if(!list.empty()){
        if((cc==105||cc==106)&&value>1)return "Unknown ("+juce::String(value)+")";
        auto i=closest(list,value);bool exact=list[i].value==value||cc==64;
        return juce::String(exact?"":"~ ")+list[i].label;
    }
    double percent=bipolar(cc)?(value-64)*100.0/(value<64?64:63):value*100.0/127;
    return juce::String(percent>0&&bipolar(cc)?"+":"")+juce::String(juce::roundToInt(percent))+"%";
}
inline int parse(int cc,juce::String text){
    if(cc==46||cc==61)for(int n=0;n<12;++n)if(text.trim().equalsIgnoreCase(waveNames[n]))return selectorValues[n%6];
    auto list=options(cc);for(auto o:list)if(text.trim().equalsIgnoreCase(o.label))return o.value;
    auto v=text.getDoubleValue();if(text.contains("%")){v=bipolar(cc)?64+v*(v<0?64:63)/100:v*127/100;}
    return juce::jlimit(0,127,int(std::round(v)));
}
}
