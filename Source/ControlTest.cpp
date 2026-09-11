// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#include <juce_audio_processors/juce_audio_processors.h>
#include "MidiControl.h"
#include "ControlDisplay.h"
#include <iostream>
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
void check(bool ok,const char* message){if(!ok){std::cerr<<"FAIL "<<message<<std::endl;std::exit(1);}}
int main(){
    juce::ScopedJuceInitialiser_GUI init;
    check(control::specs[control::indexForCC(3)].group==0,"SYSTEM-1 cutoff must use CC3");
    check(control::indexForCC(74)==-1,"do not use generic cutoff CC74");
    std::array<bool,128> seen{};for(auto spec:control::specs){check(spec.cc>0&&spec.cc<120&&!seen[spec.cc],"unique safe CC mapping");seen[spec.cc]=true;}
    check(control::matchesDevice("SYSTEM-1")&&!control::matchesDevice("SYSTEM-1 CTRL")&&!control::matchesDevice("SYSTEM-1m"),"exact instrument MIDI endpoint");
    control::Queue<4> queue;check(queue.push({1,1})&&queue.push({2,2})&&queue.push({3,3})&&!queue.push({4,4}),"bounded queue overflow");
    control::Event e;for(int i=1;i<=3;++i){check(queue.peek(e)&&e.bytes==i,"queue FIFO");queue.pop();}check(!queue.peek(e),"queue empty");
    for(int i=0;i<10000;++i){check(queue.push({uint32_t(i),0})&&queue.peek(e)&&e.bytes==i,"queue wrap");queue.pop();}
    control::Queue<4096> backlog;for(int i=0;i<2048;++i)check(backlog.push({uint32_t(i),100000}),"fill future events");
    backlog.discard();check(!backlog.peek(e),"panic discards more than one worker batch");
    check(backlog.push({9999,0})&&backlog.peek(e)&&e.bytes==9999,"queue usable after panic discard");
    MidiControl midi;midi.setCC(3,200);check(midi.pending[3].load()==127,"CC upper clamp");midi.setCC(3,-7);check(midi.pending[3].load()==0,"CC lower clamp");
    std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());auto params=p->getParameters();
    check(params.size()==control::count+2,"all controls and clock exposed to DAW");
    juce::MemoryBlock original;p->getStateInformation(original);
    juce::MemoryInputStream initState(original,false);check(initState.readInt()==5,"versioned state");initState.setPosition(24);
    for(int i=0;i<control::count;++i){initState.readInt();initState.readInt();check(initState.readInt()==0,"new controls remain unknown");}
    auto i=control::indexForCC(3);params[i+1]->setValueNotifyingHost(42.0f/127.0f);
    params.getLast()->setValueNotifyingHost(1.0f);
    juce::MemoryBlock saved;p->getStateInformation(saved);
    std::unique_ptr<juce::AudioProcessor> restored(createPluginFilter());restored->setStateInformation(saved.getData(),int(saved.getSize()));
    check(std::abs(restored->getParameters()[i+1]->getValue()-42.0f/127.0f)<0.0001f,"parameter recall");
    juce::MemoryBlock after;restored->getStateInformation(after);check(after==saved,"complete state round trip including known flags");
    restored->setStateInformation(saved.getData(),int(saved.getSize())-1);restored->getStateInformation(after);check(after==saved,"truncated state is rejected atomically");
    juce::MemoryBlock old;{juce::MemoryOutputStream out(old,false);out.writeInt(1);out.writeFloat(-12);}
    restored->setStateInformation(old.getData(),int(old.getSize()));check(std::abs(restored->getParameters()[0]->getValue()-48.0f/66.0f)<0.0001f,"0.2 gain state compatibility");
    // A malformed complete state must not change even the gain.
    juce::MemoryBlock malformed(saved);static_cast<char*>(malformed.getData())[8]=99;
    restored->getStateInformation(after);restored->setStateInformation(malformed.getData(),int(malformed.getSize()));
    juce::MemoryBlock unchanged;restored->getStateInformation(unchanged);check(after==unchanged,"invalid MIDI channel rejected");
    juce::MemoryBlock invalidClock(saved);static_cast<char*>(invalidClock.getData())[24+12*control::count]=9;
    restored->setStateInformation(invalidClock.getData(),int(invalidClock.getSize()));restored->getStateInformation(unchanged);check(after==unchanged,"invalid clock rejected atomically");
    juce::MemoryBlock v3;{juce::MemoryOutputStream out(v3,false);out.writeInt(3);out.writeFloat(-6.3f);out.writeInt(1);out.writeInt(1);out.writeInt(1);out.writeInt(54);
        for(int j=0;j<54;++j){out.writeInt(control::specs[j].cc);out.writeInt(j==i?97:control::specs[j].initial);out.writeInt(1);}}
    restored->setStateInformation(v3.getData(),int(v3.getSize()));
    check(std::abs(restored->getParameters()[i+1]->getValue()-97.0f/127.0f)<.0001f&&restored->getParameters().getLast()->getValue()==0,"0.3 full state loads with clock off");
    juce::MemoryBlock v4(saved.getData(),saved.getSize()-4);
    static_cast<char*>(v4.getData())[0]=4;restored->setStateInformation(v4.getData(),int(v4.getSize()));
    check(std::abs(restored->getParameters()[i+1]->getValue()-42.0f/127.0f)<.0001f,"0.4 state compatibility");
    juce::MemoryBlock folded(saved);static_cast<char*>(folded.getData())[folded.getSize()-4]=0;restored->setStateInformation(folded.getData(),int(folded.getSize()));
    restored->getStateInformation(after);check(after==folded,"collapsed layout state round trip");
    static_cast<char*>(folded.getData())[folded.getSize()-4]=2;restored->setStateInformation(folded.getData(),int(folded.getSize()));restored->getStateInformation(unchanged);check(after==unchanged,"invalid collapsed flag rejected atomically");
    check(control::display(115,0)=="24 dB"&&control::display(115,127)=="12 dB","filter switch meaning");
    check(control::display(119,0)=="Poly"&&control::display(119,64)=="Mono"&&control::display(119,127)=="Unison","voice modes calibrated against hardware LED and chord playback");
    check(control::parse(119,"Poly")==0&&control::parse(119,"Mono")==64&&control::parse(119,"Unison")==127,"named voice modes send the hardware CC values");
    check(control::display(105,1)=="Extended"&&control::display(105,127).startsWith("Unknown"),"extended bank uses 1, not 127");
    for(int n=0;n<12;++n)check(control::waveText(control::selectorValues[n%6],n/6)==control::waveNames[n],"all twelve waveform names");
    check(control::waveText(118,0).startsWith("~ "),"noncanonical old values are identified, not silently rewritten");
    check(control::display(81,64)=="0%"&&control::display(81,0)=="-100%"&&control::parse(81,"+100%")==127,"centered amount conversion");
    check(control::options(41).size()==24&&control::display(41,0)=="1 st"&&control::display(41,50)=="10 st"&&control::display(41,127)=="24 st","bend semitone range");
    check(control::options(87).size()==23&&control::display(87,0)=="-11 st"&&control::display(87,64)=="0 st"&&control::display(87,127)=="+11 st"&&control::parse(87,"+5 st")==92,"coarse semitone range");
    std::cout<<"PASS: CC mappings, queues, parameters, v5 recall, v1/v3/v4 compatibility, fold state, named switches/selectors, twelve waveforms, normalized amounts. No hardware writes.\n";
}
