// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include "Shared.h"
#include <fstream>
struct DiagnosticLogger:juce::Logger {
    std::ofstream& stream;
    explicit DiagnosticLogger(std::ofstream& s):stream(s){if(stream.is_open())setCurrentLogger(this);}
    ~DiagnosticLogger(){if(getCurrentLogger()==this)setCurrentLogger(nullptr);}
    void logMessage(const juce::String& s)override{if(stream.is_open())stream<<s<<std::endl;}
};

struct Capture : juce::AudioIODeviceCallback {
    bridge::Shared& s;
    explicit Capture(bridge::Shared& shared):s(shared){}
    void audioDeviceAboutToStart(juce::AudioIODevice* d) override {
        bridge::put(s.status,0);
        bridge::put(s.sampleRate,(LONG64)d->getCurrentSampleRate());
        bridge::put(s.blockSize,d->getCurrentBufferSizeSamples());
        bridge::put(s.inputLatency,d->getInputLatencyInSamples());
        bridge::put(s.writeFrame,0);
        InterlockedIncrement64(&s.generation);
        bridge::put(s.status,1);
    }
    void audioDeviceStopped() override {bridge::put(s.status,0);}
    void audioDeviceError(const juce::String&) override {bridge::put(s.status,-2);}
    void audioDeviceIOCallbackWithContext(const float* const* in,int ni,float* const* out,int no,int n,const juce::AudioIODeviceCallbackContext&) override {
        for(int c=0;c<no;++c)if(out[c])juce::FloatVectorOperations::clear(out[c],n);
        auto w=bridge::read(s.writeFrame);
        for(int i=0;i<n;++i) bridge::put(s.samples[(w+i)&(bridge::capacity-1)],bridge::pack(ni>0&&in[0]?in[0][i]:0,ni>1&&in[1]?in[1][i]:0));
        bridge::put(s.writeFrame,w+n);
        InterlockedIncrement64(&s.callbacks);
        bridge::put(s.serverHeartbeat,(LONG64)GetTickCount64());
    }
};
int main(int argc,char** argv) {
    std::ofstream diagnostic;if(argc>2)diagnostic.open(argv[2]);
    if(!diagnostic.is_open())if(auto* dir=std::getenv("SYSTEM1_BRIDGE_LOG_DIR"))diagnostic.open(std::string(dir)+"/capture-"+std::to_string(GetCurrentProcessId())+".log");
    DiagnosticLogger logger(diagnostic);
    auto log=[&](const char* text){if(diagnostic.is_open())diagnostic<<text<<std::endl;};
    log("initializing");
    if(diagnostic.is_open()){wchar_t user[256]{};DWORD size=256;GetUserNameW(user,&size);diagnostic<<"user "<<juce::String(user)<<std::endl;}
    juce::ScopedJuceInitialiser_GUI init;
    HANDLE mutex=CreateMutexW(nullptr,TRUE,bridge::mutexName);
    if(!mutex)return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(mutex);return 0;}
    bridge::Mapping mapping;
    if(!mapping.open(true)){CloseHandle(mutex);return 2;}
    auto& s=*mapping.data;
    bridge::put(s.version,1);bridge::put(s.status,0);
    bridge::put(s.clientHeartbeat,(LONG64)GetTickCount64());
    // Capture only the named SYSTEM-1 Windows endpoint. The DAW keeps its own ASIO device.
    // Avoid loading Roland's ASIO DLL here: its close/reopen failed on the test machine.
    auto type=std::unique_ptr<juce::AudioIODeviceType>(juce::AudioIODeviceType::createAudioIODeviceType_WASAPI(juce::WASAPIDeviceMode::sharedLowLatency));
    type->scanForDevices();
    juce::String endpoint;
    for(const auto& name:type->getDeviceNames(true))if(name.containsIgnoreCase("SYSTEM-1")){
        if(endpoint.isNotEmpty()){bridge::put(s.status,-3);log("ambiguous SYSTEM-1 inputs");CloseHandle(mutex);return 5;}
        endpoint=name;
    }
    if(endpoint.isEmpty()){bridge::put(s.status,-1);log("SYSTEM-1 endpoint missing");CloseHandle(mutex);return 3;}
    log("creating Windows capture device");
    auto device=std::unique_ptr<juce::AudioIODevice>(type->createDevice({},endpoint));
    if(!device){bridge::put(s.status,-1);CloseHandle(mutex);return 3;}
    auto rate=device->getCurrentSampleRate();
    if(rate<=0){auto rates=device->getAvailableSampleRates();rate=rates.isEmpty()?96000:rates[0];}
    juce::BigInteger inputs;inputs.setRange(0,2,true);
    if(diagnostic.is_open())diagnostic<<"opening "<<rate<<" Hz "<<device->getDefaultBufferSize()<<" frames"<<std::endl;
    auto err=device->open(inputs,{},rate,device->getDefaultBufferSize());
    if(err.isNotEmpty()){bridge::put(s.status,-1);if(diagnostic.is_open())diagnostic<<err<<std::endl;CloseHandle(mutex);return 4;}
    log("capture opened");
    Capture capture(s);device->start(&capture);
    const auto start=GetTickCount64();
    auto seconds=argc>1?juce::String(argv[1]).getIntValue():0;
    while(seconds>0 ? GetTickCount64()-start<(ULONGLONG)seconds*1000 : GetTickCount64()-(ULONGLONG)bridge::read(s.clientHeartbeat)<15000) {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        if(GetTickCount64()-start>3000 && (bridge::read(s.status)!=1 || GetTickCount64()-(ULONGLONG)bridge::read(s.serverHeartbeat)>2000))break;
    }
    log("stopping");device->stop();log("closing");device->close();log("closed");bridge::put(s.status,0);
    ReleaseMutex(mutex);CloseHandle(mutex);log("returning");return 0;
}
