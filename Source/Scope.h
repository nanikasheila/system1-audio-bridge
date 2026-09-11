// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>

// Audio writes bounded atomic storage; the editor reads without blocking audio.
struct OutputScope {
    static constexpr unsigned capacity=16384, snapshotSize=8192;
    static_assert(std::atomic<float>::is_always_lock_free);
    struct Snapshot {std::array<float,snapshotSize> left{},right{};unsigned count=0;double rate=48000;};
    std::array<std::atomic<float>,capacity> left{},right{};
    std::atomic<uint64_t> published{0};std::atomic<double> rate{48000};
    std::atomic<unsigned> generation{0};
    uint64_t cursor=0;
    void prepare(double sampleRate){generation.fetch_add(1,std::memory_order_acq_rel);rate=sampleRate;cursor=0;published.store(0,std::memory_order_release);generation.fetch_add(1,std::memory_order_release);}
    void push(const float* l,const float* r,int n){
        generation.fetch_add(1,std::memory_order_acq_rel);
        for(int i=0;i<n;++i){auto slot=unsigned(cursor++%capacity);left[slot].store(std::isfinite(l[i])?l[i]:0,std::memory_order_relaxed);right[slot].store(std::isfinite(r[i])?r[i]:0,std::memory_order_relaxed);}
        published.store(cursor,std::memory_order_release);
        generation.fetch_add(1,std::memory_order_release);
    }
    bool read(Snapshot& out)const{
        auto before=generation.load(std::memory_order_acquire);if(before%2)return false;
        auto end=published.load(std::memory_order_acquire);out.rate=rate.load();
        out.count=unsigned(std::min<uint64_t>(end,std::min<unsigned>(snapshotSize,unsigned(out.rate*.04))));
        for(unsigned i=0;i<out.count;++i){auto slot=unsigned((end-out.count+i)%capacity);out.left[i]=left[slot].load(std::memory_order_relaxed);out.right[i]=right[slot].load(std::memory_order_relaxed);}
        auto after=published.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_acquire);
        if(generation.load(std::memory_order_relaxed)!=before||after<end||after-end>capacity-out.count)return false;
        return out.count>1;
    }
};
