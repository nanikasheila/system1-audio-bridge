// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#include "Scope.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <limits>
#include <cstdlib>
void require(bool ok){if(!ok){std::cerr<<"Scope consistency failure\n";std::exit(1);}}
int main(){
    OutputScope scope;OutputScope::Snapshot s;
    scope.prepare(192000);require(!scope.read(s));
    std::array<float,257> l{},r{};
    for(int b=0;b<200;++b){for(int i=0;i<257;++i){l[i]=float(b*257+i);r[i]=-l[i];}scope.push(l.data(),r.data(),257);}
    require(scope.read(s)&&s.count==7680&&s.rate==192000);
    for(unsigned i=0;i<s.count;++i)require(s.left[i]==float(51400-s.count+i)&&s.right[i]==-s.left[i]);
    scope.prepare(48000);require(!scope.read(s));
    l.fill(std::numeric_limits<float>::quiet_NaN());r.fill(std::numeric_limits<float>::infinity());scope.push(l.data(),r.data(),257);
    require(scope.read(s)&&s.count==257);for(unsigned i=0;i<s.count;++i)require(s.left[i]==0&&s.right[i]==0);
    scope.prepare(192000);std::atomic<bool> done{false};int reads=0;
    std::thread writer([&]{for(int b=0;b<12000;++b){for(int i=0;i<257;++i){l[i]=float(b*257+i);r[i]=-l[i];}scope.push(l.data(),r.data(),257);if(b%32==0)std::this_thread::sleep_for(std::chrono::milliseconds(1));}done=true;});
    while(!done){if(scope.read(s)){for(unsigned i=0;i<s.count;++i){require(s.right[i]==-s.left[i]);if(i)require(s.left[i]==s.left[i-1]+1);}++reads;}}
    writer.join();require(reads>0&&scope.read(s));
    std::cout<<"PASS: scope wraparound, stereo samples, sample-rate reset, nonfinite input and concurrent snapshots ("<<reads<<").\n";
}
