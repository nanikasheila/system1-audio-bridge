// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#include <juce_audio_processors/juce_audio_processors.h>
#include "Presets.h"
#include <iostream>
void require(bool b,const char* message){if(!b){std::cerr<<"FAIL: "<<message<<std::endl;std::exit(1);}}
int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI init;
    if(argc>=3&&juce::String(argv[1])=="--import"){
        presets::Library library{juce::File(juce::String(argv[2]))};
        for(int i=3;i<argc;++i)std::cout<<library.importFolder(juce::File(juce::String(argv[i])))<<std::endl;
        library.scan();std::cout<<"Library count: "<<library.entries.size()<<std::endl;return library.entries.empty()?1:0;
    }
    auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("work/library-test-"+juce::Uuid().toString());
    presets::Library library(folder);presets::Patch p;juce::String error;
    juce::String prm;for(auto f:presets::fields)prm+=juce::String(f.name)+"("+juce::String(f.steps==255?128:0)+");\n";
    prm+="MONO(2);\nOSC2_COARSE(-4);\nARP_SW(1);\nPATCH_NAME(Test Patch);\n";
    require(presets::parsePRM(prm,p,error),"PRM parse");require(p.values[50]==64&&p.values[46]==0&&p.values[105]==0,"PRM 8-bit / selector / bank mapping");
    require(p.values[119]==-1&&p.values[87]==-1,"unverified enum must not be guessed");
    auto before=presets::encode(p);require(!presets::parsePRM(prm+"OSC1_COLOR(9);\n",p,error)&&presets::encode(p)==before,"duplicates rejected atomically");
    require(!presets::parsePRM(prm.replace("OSC1_COLOR(128)","OSC1_COLOR(256)"),p,error),"out of range rejected");
    require(!presets::parsePRM("OSC1_WAVE(0);",p,error),"incomplete PRM rejected");
    p.name="Saved name";require(library.save(p,error),"save user snapshot");require(library.entries.size()==1,"one snapshot");
    auto e=library.entries.front();presets::Patch loaded;require(presets::Library::read(e.file,loaded,error)&&loaded.values==p.values,"snapshot round trip");
    require(library.editMetadata(e,"Renamed",true),"rename / favorite");library.scan();require(library.entries[0].favorite&&library.entries[0].patch.name=="Renamed","metadata round trip");
    require(library.archive(library.entries[0])&&library.entries.empty(),"archive hides item");require(folder.getChildFile("Archive").findChildFiles(juce::File::findFiles,true,"*.s1preset").size()==1,"archive preserves file");
    auto external=folder.getSiblingFile(folder.getFileName()+"-source");external.createDirectory();auto source=external.getChildFile("SYSTEM1_PATCH1.PRM");require(source.replaceWithText(prm),"fixture write");
    require(library.importFolder(external).startsWith("Imported 1"),"import original");require(library.importFolder(external).startsWith("Imported 0 | Already present 1"),"idempotent import");
    juce::MemoryBlock sourceBytes,importedBytes;source.loadFileAsData(sourceBytes);require(library.entries.size()==1,"single imported original");library.entries[0].file.loadFileAsData(importedBytes);
    require(sourceBytes==importedBytes,"original bytes retained");
    require(!library.owned(source),"external originals cannot be archived or renamed");
    int corpus=0;
    for(int i=1;i<argc;++i){auto directory=juce::File(juce::String(argv[i]));for(auto f:directory.findChildFiles(juce::File::findFiles,true,"*.PRM")){
        presets::Patch patch;require(presets::Library::read(f,patch,error),error.toRawUTF8());require(patch.count()==int(std::size(presets::fields)),"official patch coverage");++corpus;
    }}
    std::cout<<"PASS: parser validation, native round trip, import deduplication, original preservation, rename, favorite, recoverable archive. Official PRMs checked: "<<corpus<<"; mapped controls per patch: "<<std::size(presets::fields)<<std::endl;
}
