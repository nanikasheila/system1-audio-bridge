// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include "PanelWidgets.h"
#include "Controls.h"
#include "ControlDisplay.h"
#include "PanelGraphs.h"
#include "LibraryPanel.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI init;
    auto outputDirectory=juce::File::getCurrentWorkingDirectory().getChildFile("work");
    if(outputDirectory.createDirectory().failed()){std::cerr<<"Cannot create editor test output directory.";return 2;}
    std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
    std::unique_ptr<juce::AudioProcessorEditor> e(p->createEditorIfNeeded());
    auto save=[&](const char* name){auto image=e->createComponentSnapshot(e->getLocalBounds());auto f=juce::File::getCurrentWorkingDirectory().getChildFile(name);auto out=f.createOutputStream();if(!out)return false;out->setPosition(0);out->truncate();juce::PNGImageFormat png;return png.writeImageToStream(image,*out);};
    if(!save("work/editor-layout.png"))return 2;
    juce::TextButton* fold=nullptr;
    for(auto* root:e->getChildren())for(auto* c:root->getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(c))if(b->getButtonText().startsWith("Hide performance"))fold=b;
    if(!fold)return 3;fold->onClick();if(e->getHeight()!=714||!save("work/editor-folded.png"))return 4;
    fold->onClick();if(e->getHeight()!=824)return 5;
    PanelKeyboard keys;keys.setSize(1552,80);
    for(int n=0;n<128;++n){auto rect=keys.keyRect(n);auto pt=rect.getCentre();if(!keys.isBlack(n))pt.y=70;if(keys.hit(pt)!=n){std::cerr<<"Bad key "<<n;return 6;}}
    if(keys.hit({-1,50})!=-1||keys.hit({1554,50})!=-1)return 7;
    int tested=0;
    for(auto* root:e->getChildren())for(auto* c:root->getChildren())if(auto* combo=dynamic_cast<juce::ComboBox*>(c))if(combo->getName()=="cc46"||combo->getName()=="cc61"){
        int wave=combo->getName()=="cc46"?46:61,bank=wave==46?105:106;
        for(int choice=0;choice<12;++choice){combo->setSelectedId(choice+1,juce::sendNotificationSync);
            int actualWave=juce::roundToInt(p->getParameters()[control::indexForCC(wave)+1]->getValue()*127);
            int actualBank=juce::roundToInt(p->getParameters()[control::indexForCC(bank)+1]->getValue()*127);
            constexpr int values[]{0,25,51,76,102,127};if(actualBank!=choice/6||actualWave!=values[choice%6])return 8;++tested;
        }
    }
    if(tested!=24)return 9;
    for(int kind=0;kind<15;++kind)for(int color=0;color<128;++color)for(int step=0;step<500;++step){auto v=graphs::wave(kind,step/247.f,color/127.f);if(!std::isfinite(v)||std::abs(v)>1.01f)return 10;}
    for(int kind=0;kind<12;++kind){float difference=0;for(int step=0;step<500;++step)difference+=std::abs(graphs::wave(kind,step/247.f,0)-graphs::wave(kind,step/247.f,1));if(difference<10)return 15;}
    // Pulse width should narrow monotonically, not just change the guide label.
    int previous=1001;for(int color=0;color<128;++color){int positive=0;for(int s=0;s<1000;++s)positive+=graphs::wave(1,s/1000.f,color/127.f)>0;if(positive>previous)return 16;previous=positive;}
    if(graphs::filter(.8f,.3f,0,0,true)>=graphs::filter(.8f,.7f,0,0,true))return 11;
    if(graphs::filter(.15f,1,.7f,0,true)>=graphs::filter(.15f,1,0,0,true))return 12;
    if(graphs::filter(.5f,.5f,0,1,true)<=graphs::filter(.5f,.5f,0,0,true))return 13;
    for(auto pair:{std::pair<int,int>{22,110},{23,60},{24,90},{81,110},{83,20},{84,78},{85,60},{86,80},{89,45},{90,65},{96,83},{97,70},{3,85},{9,96},{79,10},{105,0},{46,0},{106,0},{61,127},{35,25}}){auto* param=p->getParameters()[control::indexForCC(pair.first)+1];param->setValueNotifyingHost(pair.second/127.f);}
    for(auto pair:{std::pair<int,int>{50,50},{55,100}})p->getParameters()[control::indexForCC(pair.first)+1]->setValueNotifyingHost(pair.second/127.f);
    if(!save("work/editor-graphs.png"))return 14;
    // Confirm common OSC controls have identical row positions, sizes and relative x.
    auto visible=[&](const juce::String& name)->juce::Component*{for(auto* root:e->getChildren())for(auto* c:root->getChildren())if(c->isVisible()&&c->getName()==name)return c;return nullptr;};
    for(auto pair:{std::pair<int,int>{46,61},{47,62},{50,55},{60,63},{52,56}}){
        auto* a=visible("cc"+juce::String(pair.first));auto* b=visible("cc"+juce::String(pair.second));
        if(!a||!b||a->getBounds().translated(215,0)!=b->getBounds())return 17;
    }
    // Exercise actual JUCE double-click callbacks and the attached DAW parameter.
    struct Monitor:juce::AudioProcessorParameter::Listener{
        int starts=0,ends=0,values=0;void parameterValueChanged(int,float)override{++values;}
        void parameterGestureChanged(int,bool start)override{if(start)++starts;else ++ends;}
    };
    int resets=0;
    for(auto* root:e->getChildren())for(auto* c:root->getChildren())if(auto* s=dynamic_cast<juce::Slider*>(c))if(s->isVisible()&&(s->getName()=="gain"||s->getName().startsWith("cc"))){
        bool gain=s->getName()=="gain";int index=gain?0:control::indexForCC(s->getName().substring(2).getIntValue())+1;
        auto* param=p->getParameters()[index];double expected=gain?-6:control::specs[index-1].initial;
        s->setValue(expected==s->getMaximum()?s->getMinimum():s->getMaximum(),juce::sendNotificationSync);
        Monitor monitor;param->addListener(&monitor);
        auto now=juce::Time::getCurrentTime();juce::Point<float> pt{15,15};
        juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(),pt,juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,s,s,now,pt,now,2,false);
        s->mouseDoubleClick(event);param->removeListener(&monitor);
        float raw=gain?dynamic_cast<juce::RangedAudioParameter*>(param)->convertFrom0to1(param->getValue()):param->getValue()*127;
        if(std::abs(s->getValue()-expected)>.01||std::abs(raw-expected)>.01||monitor.starts!=1||monitor.ends!=1||monitor.values!=1){std::cerr<<"Reset failed: "<<s->getName()<<" gestures "<<monitor.starts<<"/"<<monitor.ends;return 18;}
        ++resets;
    }
    if(resets<30)return 19;
    // Both oscillator guides must respond to their own parameter automation.
    for(int osc=0;osc<2;++osc){int cc=osc?55:50;auto* param=p->getParameters()[control::indexForCC(cc)+1];
        param->setValueNotifyingHost(0);auto low=e->createComponentSnapshot({osc?406:191,452,osc?272:188,75});
        param->setValueNotifyingHost(1);auto high=e->createComponentSnapshot({osc?406:191,452,osc?272:188,75});
        int pixels=0;for(int y=20;y<60;++y)for(int x=8;x<low.getWidth()-8;++x)if(low.getPixelAt(x,y)!=high.getPixelAt(x,y))++pixels;
        if(pixels<20)return 20;
    }
    // A comparison sheet documents the guide at three COLOR positions, all waves.
    juce::Image sheet(juce::Image::RGB,900,12*100,true);
    {juce::Graphics g(sheet);g.fillAll(panelInk);
    for(int kind=0;kind<12;++kind)for(int c=0;c<3;++c){g.setColour(panelText);g.setFont(12);g.drawText(control::waveNames[kind],c*300+10,kind*100,280,20,juce::Justification::centredLeft);graphs::waveGuide(g,{c*300+10,kind*100+20,280,75},kind,true,c*.5f);}}
    auto out=juce::File::getCurrentWorkingDirectory().getChildFile("work/editor-color-guides.png").createOutputStream();if(!out)return 21;out->setPosition(0);out->truncate();juce::PNGImageFormat png;if(!png.writeImageToStream(sheet,*out))return 22;
    LibraryPanel* library=nullptr;for(auto* root:e->getChildren())for(auto* c:root->getChildren())if(auto* panel=dynamic_cast<LibraryPanel*>(c))library=panel;
    if(!library)return 23;library->library.root=juce::File::getCurrentWorkingDirectory().getChildFile("work/editor-library-"+juce::Uuid().toString());
    presets::Patch sample;sample.name="Library recall check";sample.values[3]=37;sample.values[50]=91;juce::String error;
    if(!library->library.save(sample,error))return 24;library->open();
    juce::TextButton* load=nullptr;for(auto* c:library->getChildren()){if(auto* t=dynamic_cast<juce::ToggleButton*>(c))if(t->getName()=="librarySend")t->setToggleState(false,juce::dontSendNotification);if(auto* b=dynamic_cast<juce::TextButton*>(c))if(b->getButtonText()=="Load preset")load=b;}
    auto gainBefore=p->getParameters()[0]->getValue(),clockBefore=p->getParameters().getLast()->getValue();if(!load)return 25;load->onClick();
    if(juce::roundToInt(p->getParameters()[control::indexForCC(3)+1]->getValue()*127)!=37||p->getParameters()[0]->getValue()!=gainBefore||p->getParameters().getLast()->getValue()!=clockBefore)return 26;
    for(int i=1;i<argc;++i)library->library.importFolder(juce::File(juce::String(argv[i])));
    library->reload();
    if(argc>1){juce::TextEditor* search=nullptr;juce::ListBox* list=nullptr;
        for(auto* c:library->getChildren()){if(auto* t=dynamic_cast<juce::TextEditor*>(c))if(t->getName()=="librarySearch")search=t;if(auto* l=dynamic_cast<juce::ListBox*>(c))list=l;}
        if(!search||!list)return 28;search->setText("Volume 4",false);search->onTextChange();if(list->getModel()->getNumRows()!=8)return 29;
        search->setText("no such preset xyz",false);search->onTextChange();if(list->getModel()->getNumRows()!=0)return 30;
        search->setText("",false);search->onTextChange();list->selectRow(1);
    }
    if(!save("work/editor-library.png"))return 27;
    std::cout<<"PASS: library recall preserves gain / clock; library overlay rendered; OSC alignment; "<<resets<<" double-click resets with one host gesture each; COLOR guides / both OSC automation images; full/collapsed rendering and all 128 keys. No MIDI ports opened.\n";return 0;
}
