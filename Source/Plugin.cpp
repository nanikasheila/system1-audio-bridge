// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Reader.h"
#include "MidiControl.h"
#include "PanelWidgets.h"
#include "ControlDisplay.h"
#include "PanelGraphs.h"
#include "LibraryPanel.h"
#include <atomic>
#include <fstream>

static void startupLog(const juce::String& message){
    if(auto* dir=std::getenv("SYSTEM1_BRIDGE_LOG_DIR")){
        std::ofstream out(std::string(dir)+"/plugin-start.log",std::ios::app);
        out<<GetTickCount64()<<" "<<message<<std::endl;
    }
}

static juce::File moduleFile(){
    HMODULE m=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&moduleFile),&m);
    wchar_t path[32768]{};GetModuleFileNameW(m,path,32768);return juce::File(juce::String(path));
}
class BridgeProcessor;
static thread_local bool receivingControl=false;
class BridgeEditor:public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit BridgeEditor(BridgeProcessor&);
    ~BridgeEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    BridgeProcessor& p;
    juce::TextButton reconnect{"Reconnect"},sendSaved{"Send saved controls"},panic{"Stop notes"};
    juce::TextButton libraryButton{"Preset library"};std::unique_ptr<LibraryPanel> libraryPanel;
    juce::ToggleButton midiEnabled{"MIDI control"},notesEnabled{"DAW notes"};
    PanelLook look;
    juce::Component surface;
    juce::ComboBox channel,clockMode;
    juce::Slider gain,bend;
    juce::TextButton fold{"Hide performance"};bool performanceVisible=true;
    juce::ToggleButton keyHold{"KEY HOLD"};
    PanelKeyboard keyboard;
    OutputScope::Snapshot scopeSnapshot,scopeScratch;
    juce::TooltipWindow tooltips{this,600};
    std::array<std::unique_ptr<juce::Slider>,control::count> knobs;
    std::array<std::unique_ptr<juce::Label>,control::count> labels;
    std::array<std::unique_ptr<juce::ComboBox>,control::count> selectors;
    std::array<std::unique_ptr<juce::TextButton>,control::count> switches;
    std::array<std::unique_ptr<juce::ParameterAttachment>,control::count> attachments;
    std::array<bool,control::count> controlGesture{};bool gainGesture=false;
    std::unique_ptr<juce::ParameterAttachment> gainAttachment;
    std::unique_ptr<juce::ParameterAttachment> clockAttachment;
    void layoutControls();
    void performanceLayout();
    void updateControl(int);
    void timerCallback() override;
};
class BridgeProcessor:public juce::AudioProcessor,private juce::Timer,private juce::AudioProcessorParameter::Listener {
public:
    BridgeProcessor():AudioProcessor(BusesProperties().withOutput("USB Stereo",juce::AudioChannelSet::stereo(),true)) {
        gainParam=new juce::AudioParameterFloat(juce::ParameterID{"gain",1},"Output gain",-60.0f,6.0f,-6.0f);
        addParameter(gainParam);
        for(int i=0;i<control::count;++i){auto spec=control::specs[i];
            auto attr=juce::AudioParameterIntAttributes().withStringFromValueFunction([this,cc=spec.cc](int value,int){
                if(cc==46||cc==61){auto* bank=params[control::indexForCC(cc==46?105:106)];return control::waveText(value,bank?bank->get():0);}return control::display(cc,value);
            }).withValueFromStringFunction([cc=spec.cc](const juce::String& s){return control::parse(cc,s);});
            params[i]=new juce::AudioParameterInt(juce::ParameterID{"cc"+juce::String(spec.cc),1},spec.name,0,127,spec.initial,attr);
            addParameter(params[i]);params[i]->addListener(this);known[i]=false;
        }
        clockParam=new juce::AudioParameterChoice(juce::ParameterID{"clockMode",1},"DAW clock",{"Off","DAW tempo","Tempo + transport"},0);
        addParameter(clockParam);clockParam->addListener(this);
        startTimerHz(30);
    }
    ~BridgeProcessor() override{stopTimer();clockParam->removeListener(this);for(auto* param:params)param->removeListener(this);}
    const juce::String getName() const override{return "SYSTEM-1 Audio Bridge";}
    bool acceptsMidi()const override{return true;}
    bool producesMidi()const override{return false;}
    double getTailLengthSeconds()const override{return std::numeric_limits<double>::infinity();}
    int getNumPrograms() override{return 1;}
    int getCurrentProgram() override{return 0;}
    void setCurrentProgram(int) override{}
    const juce::String getProgramName(int) override{return {};}
    void changeProgramName(int,const juce::String&) override{}
    bool isBusesLayoutSupported(const BusesLayout& l)const override{return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()&&l.getMainInputChannelSet().isDisabled();}
    void prepareToPlay(double rate,int) override{clockPlanner.reset();reader.prepare(rate);scope.prepare(rate);hostRate=rate;lastGain=juce::Decibels::decibelsToGain(gainParam->get());setLatencySamples((int)std::ceil(rate*0.050));if(!isNonRealtime())midiControl.start();}
    void releaseResources() override{midiControl.clockValid=false;midiControl.panic=true;}
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi) override{
        juce::ScopedNoDenormals guard;
        if(!isNonRealtime()){
            const auto now=juce::Time::getMillisecondCounterHiRes();
            double bpm=0,ppq=0;bool playing=false,hasPpq=false;
            if(auto* play=getPlayHead())if(auto pos=play->getPosition()){
                playing=pos->getIsPlaying();
                if(auto tempo=pos->getBpm())bpm=*tempo;
                if(auto position=pos->getPpqPosition()){ppq=*position;hasPpq=std::isfinite(ppq);}
            }
            bool valid=std::isfinite(bpm)&&bpm>=1&&bpm<=1000;
            midiControl.hostBpm=valid?bpm:0;midiControl.hostPlaying=playing;
            midiControl.audioHeartbeat=now;midiControl.clockValid=valid;
            clockPlanner.process(clockParam->getIndex(),valid,bpm,playing,hasPpq,ppq,hostRate,b.getNumSamples(),now,
                [this](uint8_t byte,double due,uint32_t gen){midiControl.scheduleClock(byte,due,gen);});
            midiControl.clockGeneration=clockPlanner.generation;
            for(const auto metadata:midi)midiControl.enqueue(metadata.getMessage(),now+metadata.samplePosition/hostRate*1000.0);
            if(wasPlaying&&!playing)midiControl.panic=true;wasPlaying=playing;
        }else midiControl.clockValid=false;
        midi.clear();
        if(b.getNumChannels()<2){b.clear();return;}
        if(isNonRealtime()){b.clear();streaming=false;peak=0;return;}
        reader.process(shared.load(std::memory_order_acquire),b.getWritePointer(0),b.getWritePointer(1),b.getNumSamples());
        float nextGain=juce::Decibels::decibelsToGain(gainParam->get());
        b.applyGainRamp(0,b.getNumSamples(),lastGain,nextGain);lastGain=nextGain;
        scope.push(b.getReadPointer(0),b.getReadPointer(1),b.getNumSamples());
        peak.store(b.getMagnitude(0,b.getNumSamples()));
        streaming.store(reader.streaming);bufferMs.store(reader.bufferedMs);ppm.store(reader.correctionPpm);
        resyncs.store(reader.resyncs);latency.store(reader.latencySamples);
    }
    bool hasEditor()const override{return true;}
    juce::AudioProcessorEditor* createEditor()override{return new BridgeEditor(*this);}
    void getStateInformation(juce::MemoryBlock& data)override{
        juce::MemoryOutputStream stream(data,false);stream.writeInt(5);stream.writeFloat(gainParam->get());
        stream.writeInt(midiControl.channel.load());stream.writeInt(midiControl.enabled.load());stream.writeInt(midiControl.notesEnabled.load());
        stream.writeInt(control::count);
        for(int i=0;i<control::count;++i){stream.writeInt(control::specs[i].cc);stream.writeInt(params[i]->get());stream.writeInt(known[i].load());}
        stream.writeInt(clockParam->getIndex());stream.writeInt(octave.load());
        stream.writeInt(performanceExpanded.load());
    }
    void setStateInformation(const void* data,int bytes)override{
        if(bytes<8)return;juce::MemoryInputStream stream(data,(size_t)bytes,false);
        int version=stream.readInt();if(version!=1&&version!=3&&version!=4&&version!=5)return;
        auto db=stream.readFloat();if(!std::isfinite(db))return;
        int mode=0,oct=0,expanded=1;
        if(version>=3){
            if(bytes<24)return;
            int ch=stream.readInt(),en=stream.readInt(),notes=stream.readInt(),count=stream.readInt();
            if(ch<1||ch>16||en<0||en>1||notes<0||notes>1||count<0||count>128||bytes!=(version==5?36:version==4?32:24)+12*count)return;
            std::array<int,128> values,flags;values.fill(-1);flags.fill(0);
            for(int i=0;i<count;++i){int cc=stream.readInt(),value=stream.readInt(),flag=stream.readInt();
                if(cc<0||cc>127||value<0||value>127||flag<0||flag>1||values[cc]>=0)return;values[cc]=value;flags[cc]=flag;
            }
            if(version>=4){mode=stream.readInt();oct=stream.readInt();if(mode<0||mode>2||oct< -3||oct>3)return;}
            if(version==5){expanded=stream.readInt();if(expanded<0||expanded>1)return;}
            receivingControl=true;
            midiControl.discardWaveRequests();
            for(int i=0;i<control::count;++i){auto cc=control::specs[i].cc;if(values[cc]>=0)params[i]->setValueNotifyingHost(params[i]->convertTo0to1(float(values[cc])));known[i]=values[cc]>=0&&flags[cc]!=0;midiControl.pending[cc]=-1;}
            receivingControl=false;midiControl.channel=ch;midiControl.enabled=en!=0;midiControl.notesEnabled=notes!=0;
        }
        clockParam->setValueNotifyingHost(clockParam->convertTo0to1(float(mode)));octave=oct;
        performanceExpanded=expanded!=0;
        gainParam->setValueNotifyingHost(gainParam->convertTo0to1(juce::jlimit(-60.0f,6.0f,db)));
    }
    void retry(){lastLaunch=0;midiControl.reconnect=true;timerCallback();}
    presets::Patch capturePreset(){
        presets::Patch patch;
        for(int i=0;i<control::count;++i){int cc=control::specs[i].cc;if(known[i].load()&&cc!=1&&cc!=64&&cc!=11)patch.values[cc]=params[i]->get();}
        for(auto pair:{std::pair<int,int>{46,105},{61,106}})if(patch.values[pair.first]<0||patch.values[pair.second]<0||patch.values[pair.second]>1)patch.values[pair.first]=patch.values[pair.second]=-1;
        return patch;
    }
    void applyPreset(const presets::Patch& patch,bool send){
        receivingControl=true;midiControl.discardWaveRequests();
        for(int i=0;i<control::count;++i){int cc=control::specs[i].cc;midiControl.received[cc]=-1;midiControl.pending[cc]=-1;
            int v=patch.values[cc];if(v<0)continue;auto* param=params[i];param->beginChangeGesture();param->setValueNotifyingHost(param->convertTo0to1(float(v)));param->endChangeGesture();known[i]=true;
        }
        receivingControl=false;
        if(send&&!isNonRealtime()){
            midiControl.panic=true;
            for(int osc=0;osc<2;++osc){int w=osc?61:46,b=osc?106:105;if(patch.values[w]>=0&&patch.values[b]>=0)midiControl.setWave(osc,patch.values[b],patch.values[w]);}
            for(auto spec:control::specs)if(spec.cc!=46&&spec.cc!=61&&spec.cc!=105&&spec.cc!=106&&patch.values[spec.cc]>=0)midiControl.setCC(spec.cc,patch.values[spec.cc]);
        }
    }
    void sendControls(){for(int i=0;i<control::count;++i)if(known[i].load())midiControl.setCC(control::specs[i].cc,params[i]->get());}
    void selectWave(int oscillator,int choice){
        int wave=control::indexForCC(oscillator==0?46:61),bank=control::indexForCC(oscillator==0?105:106);
        choice=juce::jlimit(0,11,choice);int value=control::selectorValues[choice%6],variant=choice/6;
        receivingControl=true;
        for(auto pair:{std::pair<int,int>{bank,variant},{wave,value}}){auto* param=params[pair.first];param->beginChangeGesture();param->setValueNotifyingHost(param->convertTo0to1(float(pair.second)));param->endChangeGesture();known[pair.first]=true;}
        receivingControl=false;
        if(!isNonRealtime())midiControl.setWave(oscillator,variant,value);
    }
    void parameterValueChanged(int index,float)override{
        if(index==control::count+1){midiControl.clockMode=clockParam->getIndex();return;}
        int i=index-1;if(i<0||i>=control::count||receivingControl)return;
        known[i]=true;if(!isNonRealtime())midiControl.setCC(control::specs[i].cc,params[i]->get());
    }
    void parameterGestureChanged(int,bool)override{}
    MidiControl midiControl;
    OutputScope scope;
    std::array<juce::AudioParameterInt*,control::count> params{};
    std::array<std::atomic<bool>,control::count> known;
    std::atomic<float> peak{0};std::atomic<bool> streaming{false};
    std::atomic<double> bufferMs{0},ppm{0};std::atomic<uint64_t>resyncs{0};std::atomic<int> latency{0};
    juce::AudioParameterFloat* gainParam=nullptr;
    juce::AudioParameterChoice* clockParam=nullptr;std::atomic<int> octave{0};
    std::atomic<bool> performanceExpanded{true};
    juce::String statusText="Connecting to SYSTEM-1 USB...";
private:
    bridge::Mapping mapping;std::atomic<bridge::Shared*> shared{nullptr};bridge::Reader reader;
    ULONGLONG lastLaunch=0;float lastGain=0.5f;double hostRate=48000;bool wasPlaying=false;
    control::ClockPlanner clockPlanner;
    void timerCallback()override{
        for(int i=0;i<control::count;++i){int value=midiControl.received[control::specs[i].cc].exchange(-1);if(value>=0){
            receivingControl=true;params[i]->setValueNotifyingHost(params[i]->convertTo0to1(float(value)));receivingControl=false;known[i]=true;
        }}
        mapping.open(false);auto* s=mapping.data;
        if(s){shared.store(s,std::memory_order_release);bridge::put(s->clientHeartbeat,(LONG64)GetTickCount64());}
        bool live=s && bridge::read(s->status)==1 && GetTickCount64()-(ULONGLONG)bridge::read(s->serverHeartbeat)<2000;
        if(live){statusText="SYSTEM-1 USB connected | "+juce::String(bridge::read(s->sampleRate))+" Hz";}
        else if(GetTickCount64()-lastLaunch>5000){
            lastLaunch=GetTickCount64();
            // Windows rejected audio capture for this executable inside the .vst3 bundle.
            // Keep the helper next to the bundle in the VST3 installation directory.
            auto exe=moduleFile().getParentDirectory().getParentDirectory().getParentDirectory().getSiblingFile("System1Capture.exe");
            startupLog("launch "+exe.getFullPathName());
            if(!exe.existsAsFile()){statusText="System1Capture.exe is missing beside the VST3 bundle.";return;}
            auto cmd=exe.getFullPathName().quoted();std::wstring wcmd=cmd.toWideCharPointer();
            STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;
            PROCESS_INFORMATION pi{};
            if(CreateProcessW(exe.getFullPathName().toWideCharPointer(),wcmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,exe.getParentDirectory().getFullPathName().toWideCharPointer(),&si,&pi)){
                startupLog("created PID "+juce::String((int)pi.dwProcessId));
                CloseHandle(pi.hThread);CloseHandle(pi.hProcess);statusText="Connecting... Check USB and Roland driver if this persists.";
            }else{statusText="Could not launch the USB capture helper. Error "+juce::String((int)GetLastError());startupLog(statusText);}
        }
        auto samples=latency.load();if(samples>0 && samples!=getLatencySamples())setLatencySamples(samples);
    }
};
#include "PanelEditor.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new BridgeProcessor();}
