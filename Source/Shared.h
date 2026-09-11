// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#pragma once
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace bridge {
constexpr int capacity=131072;
constexpr wchar_t mappingName[]=L"Local\\System1AudioBridge_v1";
constexpr wchar_t mutexName[]=L"Local\\System1AudioBridgeCapture_v1";
struct alignas(64) Shared {
    volatile LONG64 version, generation, writeFrame, sampleRate, blockSize, inputLatency;
    volatile LONG64 clientHeartbeat, serverHeartbeat, status, callbacks;
    alignas(64) volatile LONG64 samples[capacity];
};
inline LONG64 read(volatile LONG64& x) { return InterlockedCompareExchange64(&x,0,0); }
inline void put(volatile LONG64& x,LONG64 v) { InterlockedExchange64(&x,v); }
inline LONG64 pack(float l,float r) { float v[2]{l,r}; LONG64 bits;std::memcpy(&bits,v,8);return bits; }
inline void unpack(LONG64 bits,float& l,float& r) { float v[2];std::memcpy(v,&bits,8);l=v[0];r=v[1]; }
struct Mapping {
    HANDLE handle=nullptr; Shared* data=nullptr;
    bool open(bool create) {
        if(data) return true;
        handle=create ? CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Shared),mappingName)
                      : OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,mappingName);
        if(!handle)return false;
        data=static_cast<Shared*>(MapViewOfFile(handle,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
        if(!data){CloseHandle(handle);handle=nullptr;return false;}
        return true;
    }
    ~Mapping(){if(data)UnmapViewOfFile(data);if(handle)CloseHandle(handle);}
};
}
