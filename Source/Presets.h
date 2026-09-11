// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#pragma once
#include "Controls.h"
#include "ControlDisplay.h"
#include <map>
#include <regex>

namespace presets {
struct Patch {
    juce::String name,bank,notice;
    std::array<int,128> values;
    Patch(){values.fill(-1);}
    int count()const{int n=0;for(int v:values)n+=v>=0;return n;}
};
struct Field {const char* name;int cc;int steps;};
// steps=255: 8-bit amount -> 7-bit CC; 5: six-position selector;
// 1: boolean; 0: extended-wave bank. Unverified enums are deliberately retained only.
inline constexpr Field fields[]{
    {"LFO_WAVE",35,5},{"LFO_FADE",27,255},{"LFO_RATE",29,255},{"LFO_PITCH",26,255},{"LFO_FLT",28,255},{"LFO_AMP",30,255},{"LFO_TRIG",117,1},
    {"OSC1_WAVE",46,5},{"OSC1_COLOR",50,255},{"OSC1_MOD",60,5},{"OSC1_RANGE",47,5},{"OSC1_CRS_MOD",52,255},
    {"OSC2_WAVE",61,5},{"OSC2_COLOR",55,255},{"OSC2_MOD",63,5},{"OSC2_RANGE",62,5},{"OSC2_TUNE",56,255},{"OSC2_RING",111,1},{"OSC2_SYNC",112,1},
    {"MIX_OSC1",16,255},{"MIX_OSC2",17,255},{"MIX_SUB_OSC",18,255},{"MIX_NOISE",19,255},
    {"PIT_ENV",22,255},{"PIT_ATK",23,255},{"PIT_DCY",24,255},
    {"FLT_LPF",3,255},{"FLT_RESO",9,255},{"FLT_HPF",79,255},{"FLT_ENV",81,255},{"FLT_ATK",83,255},{"FLT_DCY",84,255},{"FLT_SUS",85,255},{"FLT_REL",86,255},{"FLT_KEY",82,255},
    {"AMP_ATK",89,255},{"AMP_DCY",90,255},{"AMP_SUS",96,255},{"AMP_REL",97,255},{"AMP_TONE",69,255},
    {"CRUSHER",12,255},{"REVERB",91,255},{"DELAY",94,255},{"DELAY_TM",13,255},{"PORTAMENTO",5,255},{"LEGATO",116,1},{"TEMPO_SYNC",118,1},
    {"OSC1_EX",105,0},{"OSC2_EX",106,0}
};
inline bool parsePRM(const juce::String& text,Patch& result,juce::String& error){
    if(text.getNumBytesAsUTF8()>65536){error="PRM is too large.";return false;}
    std::map<juce::String,juce::String> data;
    static const std::regex linePattern(R"(^([A-Z][A-Z0-9_]*)\(([^()]*)\);$)");
    for(auto line:juce::StringArray::fromLines(text)){line=line.trim();if(line.isEmpty())continue;
        std::smatch match;auto raw=line.toStdString();
        if(!std::regex_match(raw,match,linePattern)){error="Invalid PRM line.";return false;}
        juce::String key(match[1].str()),value(match[2].str());
        if(!data.emplace(key,value.trim()).second){error="Duplicate PRM field: "+key;return false;}
        if(key!="PATCH_NAME"&&!std::regex_match(match[2].str(),std::regex(R"(\s*-?[0-9]+\s*)"))){error="Invalid number: "+key;return false;}
    }
    for(auto key:{"OSC1_WAVE","OSC2_WAVE","FLT_LPF","AMP_SUS","OSC1_EX","OSC2_EX"})if(!data.count(key)){error="Not a SYSTEM-1 v1.20 patch: missing "+juce::String(key);return false;}
    Patch candidate;candidate.name=data["PATCH_NAME"].substring(0,128);
    for(auto f:fields){auto it=data.find(f.name);if(it==data.end())continue;
        if(it->second.length()>4){error="Out of range: "+juce::String(f.name);return false;}
        auto raw=it->second.getLargeIntValue();int maximum=f.steps==0?1:f.steps;
        if(raw<0||raw>maximum){error="Out of range: "+juce::String(f.name);return false;}
        int v=int(raw);candidate.values[f.cc]=f.steps==255?v/2:f.steps==5?control::selectorValues[v]:f.steps==1?v*127:v;
    }
    if(candidate.count()<30){error="Incomplete SYSTEM-1 patch.";return false;}
    candidate.notice="PRM: CC preview ("+juce::String(candidate.count())+" controls). 8-bit amounts reduced to 7-bit.\n"
        "Retained in original only: ARP / SCATTER, patch level, MOD assignments, octave / hold, PLUG-OUT fields.\n"
        "PRM enum mapping not yet verified: MONO, FILTER TYPE, SUB TYPE, NOISE TYPE, COARSE, BEND RANGE. These stay unchanged on the synth.";
    result=std::move(candidate);error.clear();return true;
}
inline juce::String encode(const Patch& patch){
    juce::DynamicObject::Ptr obj=new juce::DynamicObject;obj->setProperty("format","system1-bridge-preset");obj->setProperty("version",1);obj->setProperty("name",patch.name);
    juce::Array<juce::var> values;
    for(auto spec:control::specs)if(patch.values[spec.cc]>=0){juce::DynamicObject::Ptr item=new juce::DynamicObject;item->setProperty("cc",spec.cc);item->setProperty("value",patch.values[spec.cc]);values.add(juce::var(item.get()));}
    obj->setProperty("controls",values);return juce::JSON::toString(juce::var(obj.get()),false);
}
inline bool parseNative(const juce::String& text,Patch& result,juce::String& error){
    if(text.getNumBytesAsUTF8()>65536){error="Preset is too large.";return false;}
    auto obj=juce::JSON::parse(text);auto* list=obj["controls"].getArray();
    if(obj["format"].toString()!="system1-bridge-preset"||!obj["version"].isInt()||int(obj["version"])!=1||!list||list->size()>control::count){error="Invalid bridge preset.";return false;}
    Patch p;p.name=obj["name"].toString().trim().substring(0,128);
    for(auto& item:*list){auto c=item["cc"],v=item["value"];int cc=int(c),value=int(v);
        if(!c.isInt()||!v.isInt()||cc<0||cc>127||control::indexForCC(cc)<0||value<0||value>127||p.values[cc]>=0||((cc==105||cc==106)&&value>1)){error="Invalid or duplicate preset control.";return false;}
        p.values[cc]=value;
    }
    if(p.count()==0){error="Preset has no known controls.";return false;}
    for(auto pair:{std::pair<int,int>{46,105},{61,106}})if((p.values[pair.first]>=0)!=(p.values[pair.second]>=0)){error="Waveform and bank must be stored together.";return false;}
    p.notice="Bridge snapshot: "+juce::String(p.count())+" known controls. Unstored controls remain unchanged. USB level, clock and MIDI routing remain unchanged.";
    result=std::move(p);error.clear();return true;
}
struct Entry {juce::File file;Patch patch;bool favorite=false,original=false;};
class Library {
public:
    juce::File root;std::vector<Entry> entries;
    explicit Library(juce::File folder=defaultRoot()):root(folder){}
    static juce::File defaultRoot(){return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("System1Bridge/Library");}
    static bool read(const juce::File& f,Patch& p,juce::String& error){
        if(!f.existsAsFile()||f.getSize()>65536){error="Missing or oversized preset.";return false;}
        return f.hasFileExtension("prm")?parsePRM(f.loadFileAsString(),p,error):parseNative(f.loadFileAsString(),p,error);
    }
    static juce::File metadata(const juce::File& f){return f.getSiblingFile(f.getFileName()+".meta.json");}
    void scan(){
        entries.clear();
        for(auto section:{"Imported","User"})for(auto f:root.getChildFile(section).findChildFiles(juce::File::findFiles,true,"*")){
            if(!f.hasFileExtension("prm;s1preset"))continue;Patch p;juce::String error;if(!read(f,p,error))continue;
            auto meta=metadata(f);auto m=meta.getSize()<8192?juce::JSON::parse(meta.loadFileAsString()):juce::var();
            p.bank=juce::String(section)=="User"?"User":f.getParentDirectory().getFileName().replace("SYSTEM-1_Sound_Bank_For_120_","Volume ").replace("Volume Volume_","Volume ");
            if(p.name.isEmpty()){p.name=f.getFileNameWithoutExtension();if(p.name.startsWith("SYSTEM1_PATCH"))p.name="Patch "+juce::String(p.name.substring(13).getIntValue()).paddedLeft('0',2);}
            if(m["name"].isString()&&m["name"].toString().trim().isNotEmpty())p.name=m["name"].toString().substring(0,128);
            entries.push_back({f,p,bool(m["favorite"]),f.hasFileExtension("prm")});
        }
        std::sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){int bank=a.patch.bank.compareNatural(b.patch.bank);return bank?bank<0:a.patch.name.compareNatural(b.patch.name)<0;});
    }
    bool owned(const juce::File& f)const{return f.isAChildOf(root.getChildFile("Imported"))||f.isAChildOf(root.getChildFile("User"));}
    bool editMetadata(const Entry& e,const juce::String& name,bool favorite){
        if(!owned(e.file))return false;juce::DynamicObject::Ptr o=new juce::DynamicObject;o->setProperty("name",name.trim().substring(0,128));o->setProperty("favorite",favorite);
        return metadata(e.file).replaceWithText(juce::JSON::toString(juce::var(o.get())));
    }
    bool save(Patch p,juce::String& error){
        p.name=p.name.trim().substring(0,128);if(p.name.isEmpty()||p.count()==0){error="Enter a name and receive or edit some controls first.";return false;}
        auto folder=root.getChildFile("User");if(folder.createDirectory().failed()){error="Cannot create user library.";return false;}
        auto file=folder.getChildFile(juce::Uuid().toString()+".s1preset");auto text=encode(p);Patch check;if(!parseNative(text,check,error))return false;
        if(!file.replaceWithText(text)){error="Could not save preset.";return false;}scan();return true;
    }
    juce::String importFolder(const juce::File& folder){
        if(!folder.isDirectory()||folder==root||folder.isAChildOf(root)||root.isAChildOf(folder))return "Choose a folder outside this library.";
        int added=0,skipped=0,invalid=0;auto files=folder.findChildFiles(juce::File::findFiles,true,"*");
        for(auto source:files){if(!source.hasFileExtension("prm;s1preset"))continue;if(added+skipped+invalid>=4096)break;
            Patch p;juce::String error;if(!read(source,p,error)){++invalid;continue;}
            auto bank=juce::File::createLegalFileName(source.getParentDirectory().getFileName());auto dir=root.getChildFile("Imported").getChildFile(bank);
            if(dir.createDirectory().failed()){++invalid;continue;}auto target=dir.getChildFile(source.getFileName());
            if(target.existsAsFile()&&target.loadFileAsString()==source.loadFileAsString()){++skipped;continue;}
            if(target.exists())target=target.getNonexistentSibling();
            if(source.copyFileTo(target))++added;else ++invalid;
        }
        scan();return "Imported "+juce::String(added)+" | Already present "+juce::String(skipped)+" | Invalid / failed "+juce::String(invalid);
    }
    bool archive(const Entry& e){
        if(!owned(e.file))return false;auto dir=root.getChildFile("Archive").getChildFile(juce::Uuid().toString());if(dir.createDirectory().failed())return false;
        if(!e.file.moveFileTo(dir.getChildFile(e.file.getFileName())))return false;
        auto meta=metadata(e.file);if(meta.existsAsFile())meta.moveFileTo(dir.getChildFile(meta.getFileName()));scan();return true;
    }
};
}
