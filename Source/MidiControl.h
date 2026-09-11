// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include "Controls.h"
#include "Shared.h"
#include "ClockPlanner.h"
#include <fstream>

// The worker owns the Windows MIDI ports. The audio callback only writes a bounded
// SPSC queue; parameter changes are coalesced separately and never call a driver.
class MidiControl:private juce::Thread,private juce::MidiInputCallback {
public:
    MidiControl():Thread("SYSTEM-1 MIDI") {for(auto& x:pending)x=-1;for(auto& x:received)x=-1;for(auto& x:inputNotes)x=false;for(auto& x:outputNotes)x=false;}
    ~MidiControl()override{signalThreadShouldExit();notify();stopThread(-1);}
    std::atomic<int> channel{1},state{0},inputState{0},rxChannel{0};
    std::atomic<uint64_t> sent{0},receivedCount{0},overflow{0};
    std::atomic<bool> enabled{true},notesEnabled{true},reconnect{false},panic{false};
    std::array<std::atomic<int>,128> pending,received;
    std::array<std::atomic<bool>,128> inputNotes,outputNotes;
    std::atomic<int> clockMode{0};std::atomic<bool> clockValid{false},hostPlaying{false};
    std::atomic<double> hostBpm{0},audioHeartbeat{0};std::atomic<uint32_t> clockGeneration{1};
    std::atomic<uint64_t> clocksSent{0},clockLate{0};
    void scheduleClock(uint8_t byte,double due,uint32_t generation){
        clockGeneration=generation;if(!clockEvents.push({byte,due,generation}))++clockLate;
    }
    void postUI(const juce::MidiMessage& m){
        if(!enabled.load()||state.load()!=1)return;
        auto size=m.getRawDataSize();if(size<1||size>3)return;
        const auto* raw=m.getRawData();uint32_t bytes=uint32_t(size)<<24;
        for(int i=0;i<size;++i)bytes|=uint32_t(raw[i])<<(8*i);
        if(!uiEvents.push({bytes,juce::Time::getMillisecondCounterHiRes()})){++overflow;panic=true;}
    }
    void start(){if(!isThreadRunning())startThread();}
    void setCC(int cc,int value){if(cc>=0&&cc<128)pending[cc].store(juce::jlimit(0,127,value));}
    void setWave(int osc,int bank,int wave){if(osc>=0&&osc<2){pending[osc?61:46]=-1;pending[osc?106:105]=-1;wavePairs[osc]=juce::jlimit(0,1,bank)*128+juce::jlimit(0,127,wave);}}
    void discardWaveRequests(){for(auto& pair:wavePairs)pair=-1;}
    void enqueue(const juce::MidiMessage& m,double due){
        auto size=m.getRawDataSize();if(size<1||size>3||m.isSysEx()||!notesEnabled.load())return;
        auto* raw=m.getRawData();auto status=raw[0]&0xf0;
        if(status!=0x80&&status!=0x90&&status!=0xa0&&status!=0xb0&&status!=0xd0&&status!=0xe0)return;
        auto bytes=uint32_t(status|((channel.load()-1)&15))|(uint32_t(size)<<24);
        for(int i=1;i<size;++i)bytes|=uint32_t(raw[i])<<(8*i);
        if(!events.push({bytes,due})){++overflow;panic=true;}
    }
private:
    control::Queue<4096> events;
    control::Queue<1024> uiEvents;
    control::Queue<4096,control::ClockEvent> clockEvents;
    bool transportStarted=false;
    std::array<std::atomic<int>,2> wavePairs{{-1,-1}};
    uint64_t startsSent=0,stopsSent=0;
    std::unique_ptr<juce::MidiInput> input;std::unique_ptr<juce::MidiOutput> output;
    HANDLE owner=nullptr;
    std::array<std::array<bool,128>,16> active{};
    std::array<bool,16> usedChannels{};
    void send(const juce::MidiMessage& m){if(output){output->sendMessageNow(m);++sent;}}
    void releaseNotes(){
        if(output)for(int c=0;c<16;++c){for(int n=0;n<128;++n)if(active[c][n]){send(juce::MidiMessage::noteOff(c+1,n));active[c][n]=false;}if(usedChannels[c]){send(juce::MidiMessage::controllerEvent(c+1,64,0));send(juce::MidiMessage::pitchWheel(c+1,8192));usedChannels[c]=false;}}
        for(auto& n:outputNotes)n=false;
    }
    void sendPerformance(const juce::MidiMessage& m){
        send(m);const int c=m.getChannel()-1;
        if(c>=0&&(m.isNoteOn()||m.isPitchWheel()||(m.isController()&&m.getControllerNumber()==64)))usedChannels[c]=true;
        if(c>=0&&m.isNoteOn()){active[c][m.getNoteNumber()]=true;outputNotes[m.getNoteNumber()]=true;}
        if(c>=0&&m.isNoteOff()){active[c][m.getNoteNumber()]=false;outputNotes[m.getNoteNumber()]=false;}
    }
    void stopClock(){if(transportStarted){send(juce::MidiMessage::midiStop());++stopsSent;transportStarted=false;}}
    void close(){stopClock();releaseNotes();events.discard();uiEvents.discard();clockEvents.discard();if(input)input->stop();input.reset();for(auto& n:inputNotes)n=false;output.reset();if(owner){ReleaseMutex(owner);CloseHandle(owner);owner=nullptr;}state=0;inputState=0;}
    void open(){
        owner=CreateMutexW(nullptr,FALSE,L"Local\\System1AudioBridgeMidi_v1");
        if(!owner){state=-1;return;}
        auto lock=WaitForSingleObject(owner,0);
        if(lock!=WAIT_OBJECT_0&&lock!=WAIT_ABANDONED){CloseHandle(owner);owner=nullptr;state=-2;return;}
        auto outputs=juce::MidiOutput::getAvailableDevices();juce::String outId,inId;int count=0;
        for(auto d:outputs)if(control::matchesDevice(d.name)){outId=d.identifier;++count;}
        if(count!=1){state=count==0?-3:-4;ReleaseMutex(owner);CloseHandle(owner);owner=nullptr;return;}
        output=juce::MidiOutput::openDevice(outId);
        if(!output){state=-1;ReleaseMutex(owner);CloseHandle(owner);owner=nullptr;return;}
        for(auto d:juce::MidiInput::getAvailableDevices())if(control::matchesDevice(d.name))inId=d.identifier;
        if(inId.isNotEmpty())input=juce::MidiInput::openDevice(inId,this);
        if(input)input->start();inputState=input?1:-1;state=1;
    }
    void handleIncomingMidiMessage(juce::MidiInput*,const juce::MidiMessage& m)override{
        if(m.getChannel()==channel.load()){
            if(m.isNoteOn())inputNotes[m.getNoteNumber()]=true;
            if(m.isNoteOff())inputNotes[m.getNoteNumber()]=false;
            if(m.isAllNotesOff()||m.isAllSoundOff())for(auto& n:inputNotes)n=false;
        }
        if(!m.isController())return;rxChannel=m.getChannel();
        if(m.getChannel()!=channel.load())return;
        auto cc=m.getControllerNumber();if(control::indexForCC(cc)>=0){received[cc]=m.getControllerValue();++receivedCount;}
    }
    void run()override {
        std::ofstream diagnostic;if(auto* dir=std::getenv("SYSTEM1_BRIDGE_LOG_DIR"))diagnostic.open(std::string(dir)+"/midi-session-"+std::to_string(GetCurrentProcessId())+".log");
        double reportAt=0;
        double nextOpen=0,nextScan=0,nextCC=0;int lastChannel=channel.load();bool wasNotes=notesEnabled.load();
        while(!threadShouldExit()){
            auto now=juce::Time::getMillisecondCounterHiRes();
            if(diagnostic.is_open()&&now>=reportAt){reportAt=now+1000;diagnostic<<now<<" state="<<state.load()<<" mode="<<clockMode.load()<<" bpm="<<hostBpm.load()<<" playing="<<hostPlaying.load()<<" valid="<<clockValid.load()<<" clocks="<<clocksSent.load()<<" late="<<clockLate.load()<<" starts="<<startsSent<<" stops="<<stopsSent<<" total="<<sent.load()<<std::endl;}
            if(reconnect.exchange(false)){close();nextOpen=0;}
            if(!enabled.load()){if(output)close();state=-5;nextOpen=0;}
            else if(!output&&now>=nextOpen){open();nextOpen=now+3000;}
            if(output&&now>=nextScan){
                nextScan=now+3000;bool found=false;for(auto d:juce::MidiOutput::getAvailableDevices())if(d.identifier==output->getIdentifier())found=true;
                if(!found)close();
            }
            bool flush=panic.exchange(false)||lastChannel!=channel.load()||(wasNotes&&!notesEnabled.load());
            lastChannel=channel.load();wasNotes=notesEnabled.load();
            if(flush){releaseNotes();events.discard();uiEvents.discard();}
            const bool clockLive=output&&enabled.load()&&clockMode.load()>0&&clockValid.load()&&now-audioHeartbeat.load()<500;
            if(!clockLive||clockMode.load()!=2||!hostPlaying.load())stopClock();
            control::ClockEvent ce;int clockBudget=128;
            while(clockBudget-->0&&clockEvents.peek(ce)){
                if(ce.generation!=clockGeneration.load()||!clockLive){clockEvents.pop();continue;}
                if(ce.due>now)break;
                clockEvents.pop();
                if(now-ce.due>100){++clockLate;continue;}
                if(ce.byte==0xfc){stopClock();continue;}
                if(ce.byte==0xfa){if(clockMode.load()==2&&hostPlaying.load()){stopClock();send(juce::MidiMessage::midiStart());++startsSent;transportStarted=true;}continue;}
                if(clockMode.load()==2&&hostPlaying.load()&&!transportStarted){send(juce::MidiMessage::midiStart());++startsSent;transportStarted=true;}
                send(juce::MidiMessage::midiClock());++clocksSent;
            }
            control::Event ue;int uiBudget=128;
            while(uiBudget-->0&&uiEvents.peek(ue)){
                uiEvents.pop();if(!output||!enabled.load()||flush)continue;
                uint8_t raw[]{uint8_t(ue.bytes),uint8_t(ue.bytes>>8),uint8_t(ue.bytes>>16)};
                sendPerformance(juce::MidiMessage(raw,int(ue.bytes>>24)));
            }
            control::Event e;int budget=512;
            while(budget-->0&&events.peek(e)){
                if(!flush&&output&&enabled.load()&&notesEnabled.load()&&e.due>now)break;
                events.pop();
                if(flush||!output||!enabled.load()||!notesEnabled.load())continue;
                uint8_t raw[]{uint8_t(e.bytes),uint8_t(e.bytes>>8),uint8_t(e.bytes>>16)};
                juce::MidiMessage m(raw,int(e.bytes>>24));
                sendPerformance(m);
            }
            if(now>=nextCC){nextCC=now+10;
                for(int osc=0;osc<2;++osc){int pair=wavePairs[osc].exchange(-1),bankCC=osc?106:105,waveCC=osc?61:46;
                    int bank=pending[bankCC].exchange(-1),wave=pending[waveCC].exchange(-1);
                    if(output&&enabled.load()){
                        if(pair>=0){sendPerformance(juce::MidiMessage::controllerEvent(channel.load(),bankCC,pair/128));sendPerformance(juce::MidiMessage::controllerEvent(channel.load(),waveCC,pair%128));}
                        if(bank>=0)sendPerformance(juce::MidiMessage::controllerEvent(channel.load(),bankCC,bank));
                        if(wave>=0)sendPerformance(juce::MidiMessage::controllerEvent(channel.load(),waveCC,wave));
                    }
                }
                // At most one value per CC per 10 ms. Do not accumulate stale changes while disconnected.
                for(int cc=0;cc<128;++cc){if(cc==46||cc==61||cc==105||cc==106)continue;int v=pending[cc].exchange(-1);if(v>=0&&output&&enabled.load())sendPerformance(juce::MidiMessage::controllerEvent(channel.load(),cc,v));}
            }
            wait(1);
        }
        close();
    }
};
