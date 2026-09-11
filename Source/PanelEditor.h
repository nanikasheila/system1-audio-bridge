// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#pragma once
BridgeEditor::BridgeEditor(BridgeProcessor& proc):AudioProcessorEditor(proc),p(proc){
    setLookAndFeel(&look);addAndMakeVisible(surface);
    auto show=[this](juce::Component& c){surface.addAndMakeVisible(c);};
    show(reconnect);reconnect.onClick=[this]{p.retry();};
    show(sendSaved);sendSaved.setButtonText("Send saved");sendSaved.onClick=[this]{p.sendControls();};
    show(panic);panic.onClick=[this]{keyboard.releaseAll();p.midiControl.panic=true;p.midiControl.setCC(64,0);bend.setValue(0);};
    show(midiEnabled);midiEnabled.onClick=[this]{keyboard.releaseAll();p.midiControl.enabled=midiEnabled.getToggleState();};
    show(notesEnabled);notesEnabled.onClick=[this]{p.midiControl.notesEnabled=notesEnabled.getToggleState();};
    for(int c=1;c<=16;++c)channel.addItem("CH "+juce::String(c),c);
    channel.setSelectedId(p.midiControl.channel.load(),juce::dontSendNotification);show(channel);
    channel.onChange=[this]{keyboard.releaseAll();p.midiControl.channel=channel.getSelectedId();};
    clockMode.addItem("Clock off",1);clockMode.addItem("DAW tempo",2);clockMode.addItem("Tempo + transport",3);show(clockMode);
    clockMode.setTooltip("Set SYSTEM-1 MIDI Clock Source to AUTO. Tempo sends MIDI Clock while audio processing is active. Transport also follows DAW play / stop. Disable other clock senders to SYSTEM-1.");
    clockAttachment=std::make_unique<juce::ParameterAttachment>(*p.clockParam,[this](float v){clockMode.setSelectedId(int(v)+1,juce::dontSendNotification);});
    clockMode.onChange=[this]{clockAttachment->setValueAsCompleteGesture(float(clockMode.getSelectedId()-1));};clockAttachment->sendInitialUpdate();
    gain.setRange(-60,6,.1);gain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);gain.setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,18);gain.setTextValueSuffix(" dB");
    gain.setName("gain");gain.setDoubleClickReturnValue(true,p.gainParam->convertFrom0to1(static_cast<juce::AudioProcessorParameter*>(p.gainParam)->getDefaultValue()));
    gainAttachment=std::make_unique<juce::ParameterAttachment>(*p.gainParam,[this](float v){gain.setValue(v,juce::dontSendNotification);});
    gain.onDragStart=[this]{gainGesture=true;gainAttachment->beginGesture();};gain.onDragEnd=[this]{gainAttachment->endGesture();gainGesture=false;};
    gain.onValueChange=[this]{if(gainGesture)gainAttachment->setValueAsPartOfGesture(float(gain.getValue()));else gainAttachment->setValueAsCompleteGesture(float(gain.getValue()));};
    gainAttachment->sendInitialUpdate();gain.setTooltip("USB bridge output level. Double-click to reset to -6.0 dB (plugin default).");show(gain);
    for(int i=0;i<control::count;++i){
        knobs[i]=std::make_unique<juce::Slider>();labels[i]=std::make_unique<juce::Label>();auto& k=*knobs[i];
        k.setRange(0,127,1);k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,52,17);
        k.setName("cc"+juce::String(control::specs[i].cc));k.setDoubleClickReturnValue(true,control::specs[i].initial);
        k.textFromValueFunction=[cc=control::specs[i].cc](double v){return control::display(cc,int(v));};
        k.valueFromTextFunction=[cc=control::specs[i].cc](const juce::String& s){return double(control::parse(cc,s));};
        k.setTooltip(juce::String(control::specs[i].name)+" | CC "+juce::String(control::specs[i].cc)+" | MIDI 0-127. * means not yet received from hardware. Extended wave / coarse tune / bend range require updated firmware.");
        if(control::specs[i].cc==41||control::specs[i].cc==87)k.setTooltip("st = semitones. Values calibrated from the connected SYSTEM-1. * Unsynced; ~ nearest discrete position.");
        k.setTooltip(k.getTooltip()+" Double-click knob / fader to reset to "+control::display(control::specs[i].cc,control::specs[i].initial)+" (plugin default).");
        labels[i]->setJustificationType(juce::Justification::centred);labels[i]->setFont(juce::FontOptions(10.0f));labels[i]->setColour(juce::Label::textColourId,panelText);
        attachments[i]=std::make_unique<juce::ParameterAttachment>(*p.params[i],[this,i](float v){knobs[i]->setValue(v,juce::dontSendNotification);updateControl(i);});
        k.onDragStart=[this,i]{controlGesture[i]=true;attachments[i]->beginGesture();};k.onDragEnd=[this,i]{attachments[i]->endGesture();controlGesture[i]=false;};
        k.onValueChange=[this,i]{auto& s=*knobs[i];if(controlGesture[i])attachments[i]->setValueAsPartOfGesture(float(s.getValue()));else attachments[i]->setValueAsCompleteGesture(float(s.getValue()));};
        int cc=control::specs[i].cc;auto opts=control::options(cc);
        if(control::isSwitch(cc)||cc==1){
            switches[i]=std::make_unique<juce::TextButton>();auto& button=*switches[i];show(button);
            button.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff447e54));
            if(cc==1){button.setTooltip("Hold to apply modulation; release to return to zero.");button.onStateChange=[this,i,down=false]()mutable{bool next=switches[i]->isDown();if(next!=down){down=next;attachments[i]->setValueAsCompleteGesture(next?127.0f:0.0f);}};}
            else button.onClick=[this,i,cc]{auto opts=control::options(cc);int current=control::closest(opts,p.params[i]->get());attachments[i]->setValueAsCompleteGesture(float(opts[current==0?1:0].value));};
        }else if(!opts.empty()&&cc!=105&&cc!=106){
            selectors[i]=std::make_unique<juce::ComboBox>();auto& combo=*selectors[i];show(combo);
            if(cc==46||cc==61){for(int n=0;n<12;++n){if(n==6)combo.addSectionHeading("Firmware 1.20");combo.addItem(control::waveNames[n],n+1);}
                combo.onChange=[this,i,cc]{int n=selectors[i]->getSelectedId()-1;if(n>=0)p.selectWave(cc==46?0:1,n);};
            }else{for(int n=0;n<int(opts.size());++n)combo.addItem(opts[n].label,n+1);
                combo.onChange=[this,i,opts]{int n=selectors[i]->getSelectedId()-1;if(n>=0)attachments[i]->setValueAsCompleteGesture(float(opts[n].value));};
            }
        }
        attachments[i]->sendInitialUpdate();k.updateText();show(k);show(*labels[i]);k.setVisible(!switches[i]&&!selectors[i]&&cc!=105&&cc!=106);
        if(selectors[i]){selectors[i]->setName("cc"+juce::String(cc));selectors[i]->setTooltip(cc==46||cc==61?"12 waveforms. Firmware 1.20 is required for the six extended waves. Choosing a wave sends its bank first, then its waveform. * Unsynced; ~ nearest named setting for a noncanonical value.":k.getTooltip());}
        if(switches[i]&&cc!=1){switches[i]->setName("cc"+juce::String(cc));switches[i]->setTooltip(k.getTooltip());}
        if(cc==105||cc==106)labels[i]->setVisible(false);
    }
    show(keyboard);keyboard.note=[this](int n,bool on){
        int c=p.midiControl.channel.load();p.midiControl.postUI(on?juce::MidiMessage::noteOn(c,n,juce::uint8(100)):juce::MidiMessage::noteOff(c,n));
    };
    show(fold);show(keyHold);fold.onClick=[this]{p.performanceExpanded=!p.performanceExpanded.load();performanceLayout();};
    keyHold.onClick=[this]{keyboard.setHold(keyHold.getToggleState());};
    keyHold.setTooltip("Hold on-screen notes; click a held key again to release. This is independent of the hardware arpeggiator hold.");
    bend.setRange(-8192,8191,1);bend.setSliderStyle(juce::Slider::LinearHorizontal);bend.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);bend.setDoubleClickReturnValue(true,0);
    bend.onValueChange=[this]{p.midiControl.postUI(juce::MidiMessage::pitchWheel(p.midiControl.channel.load(),int(bend.getValue())+8192));};
    bend.onDragEnd=[this]{bend.setValue(0);};bend.setTooltip("Pitch bend. Springs back to centre after dragging.");show(bend);
    libraryPanel=std::make_unique<LibraryPanel>();surface.addChildComponent(*libraryPanel);
    libraryPanel->capture=[this]{return p.capturePreset();};libraryPanel->apply=[this](const presets::Patch& patch,bool send){keyboard.releaseAll();p.applyPreset(patch,send);};
    libraryPanel->close=[this]{libraryPanel->setVisible(false);};show(libraryButton);libraryButton.onClick=[this]{libraryPanel->open();libraryPanel->setConnected(p.midiControl.state.load()==1);};
    setResizable(true,true);setSize(1600,824);layoutControls();performanceLayout();timerCallback();startTimerHz(24);
}
BridgeEditor::~BridgeEditor(){stopTimer();keyboard.releaseAll();keyboard.note=nullptr;if(bend.getValue()!=0)p.midiControl.postUI(juce::MidiMessage::pitchWheel(p.midiControl.channel.load(),8192));setLookAndFeel(nullptr);}
void BridgeEditor::performanceLayout(){
    const int width=getWidth();
    performanceVisible=p.performanceExpanded.load();
    if(!performanceVisible){keyboard.releaseAll();keyboard.setHold(false);keyHold.setToggleState(false,juce::dontSendNotification);bend.setValue(0);}
    keyboard.setVisible(performanceVisible);keyHold.setVisible(performanceVisible);bend.setVisible(performanceVisible);
    fold.setButtonText(performanceVisible?"Hide performance  -":"Show performance  +");
    int h=performanceVisible?824:714;setResizeLimits(1280,int(h*.8),2080,int(h*1.3));getConstrainer()->setFixedAspectRatio(1600.0/h);
    setSize(width,juce::roundToInt(width*h/1600.0));resized();repaint();
}
void BridgeEditor::resized(){surface.setBounds(0,0,1600,performanceVisible?824:714);surface.setTransform(juce::AffineTransform::scale(getWidth()/1600.0f));}
void BridgeEditor::updateControl(int i){
    int cc=control::specs[i].cc,v=p.params[i]->get();auto opts=control::options(cc);
    if(switches[i]){if(cc!=1)switches[i]->setToggleState(control::closest(opts,v)==1,juce::dontSendNotification);switches[i]->setButtonText(cc==1?(v>0?"ON":"Hold"):control::display(cc,v));}
    if(selectors[i]){
        auto& combo=*selectors[i];
        if(cc==46||cc==61){int b=control::indexForCC(cc==46?105:106),bank=p.params[b]->get();
            bool exact=(bank==0||bank==1)&&control::selectorValues[control::waveSlot(v)]==v;
            if(p.known[i].load()&&p.known[b].load()&&exact)combo.setSelectedId(control::waveSlot(v)+6*bank+1,juce::dontSendNotification);
            else{combo.setSelectedId(0,juce::dontSendNotification);combo.setText(p.known[b].load()?control::waveText(v,bank):"Choose / receive",juce::dontSendNotification);}
        }else{int n=control::closest(opts,v);if(opts[n].value==v)combo.setSelectedId(n+1,juce::dontSendNotification);else{combo.setSelectedId(0,juce::dontSendNotification);combo.setText(control::display(cc,v),juce::dontSendNotification);}}
    }
}
void BridgeEditor::layoutControls(){
    reconnect.setBounds(1468,23,110,28);sendSaved.setBounds(1347,23,108,28);panic.setBounds(1347,67,108,28);
    libraryButton.setBounds(1150,23,180,28);libraryPanel->setBounds(20,120,1560,534);
    midiEnabled.setBounds(500,23,122,28);notesEnabled.setBounds(627,23,110,28);channel.setBounds(745,23,85,28);
    clockMode.setBounds(846,23,180,28);
    gain.setBounds(24,574,106,71);
    auto place=[this](int cc,const char* title,int x,int y,int w,int h=79,bool fader=false){
        int i=control::indexForCC(cc);labels[i]->setName(title);labels[i]->setBounds(x,y,w,17);
        knobs[i]->setBounds(x+2,y+17,w-4,h-17);if(fader)knobs[i]->setSliderStyle(juce::Slider::LinearVertical);
        if(selectors[i])selectors[i]->setBounds(x+2,y+25,w-4,26);
        if(switches[i])switches[i]->setBounds(x+4,y+31,w-8,27);
    };
    place(35,"WAVE",26,170,69);place(29,"RATE",101,170,69);place(27,"FADE TIME",26,263,69);place(26,"PITCH",101,263,69);place(28,"FILTER",26,356,69);place(30,"AMP",101,356,69);
    place(35,"WAVE",26,170,144,54);place(29,"RATE",26,230,69,68);place(27,"FADE",101,230,69,68);place(26,"PITCH",26,304,69,68);place(28,"FILTER",101,304,69,68);place(30,"AMP",26,378,69,68);
    place(46,"WAVE",191,170,188,60);place(47,"RANGE",191,234,188,57);place(50,"COLOR",191,299,88,88);place(52,"CROSS MOD",291,299,88,88);place(60,"MOD SOURCE",191,392,188,55);
    place(61,"WAVE",406,170,188,60);place(62,"RANGE",406,234,188,57);place(55,"COLOR",406,299,88,88);place(56,"FINE TUNE",506,299,88,88);place(63,"MOD SOURCE",406,392,188,55);
    place(87,"COARSE",604,170,74,60);place(111,"RING SW",604,234,74,55);place(112,"SYNC SW",604,299,74,55);
    place(16,"OSC 1",701,170,65);place(17,"OSC 2",772,170,65);place(18,"SUB",701,263,65);place(19,"NOISE",772,263,65);place(113,"SUB TYPE",701,356,65);place(114,"NOISE TYPE",772,356,65);
    place(22,"ENV",875,176,85,95);place(23,"A",861,296,51,139,true);place(24,"D",924,296,51,139,true);
    place(3,"LPF CUTOFF",1002,170,94,91);place(9,"RESONANCE",1105,179,67,82);place(79,"HPF",1179,179,67,82);
    place(81,"ENV",1002,266,77,80);place(82,"KEY",1085,266,77,80);place(115,"TYPE",1170,266,76,80);
    place(83,"A",1002,353,49,82,true);place(84,"D",1067,353,49,82,true);place(85,"S",1132,353,49,82,true);place(86,"R",1197,353,49,82,true);
    place(69,"TONE",1267,174,89,93);place(12,"CRUSHER",1367,174,89,93);
    place(89,"A",1267,296,40,139,true);place(90,"D",1317,296,40,139,true);place(96,"S",1367,296,40,139,true);place(97,"R",1417,296,40,139,true);
    place(13,"DELAY TIME",1477,170,96);place(94,"DELAY LEVEL",1477,263,96);place(91,"REVERB",1477,356,96);
    place(5,"PORTAMENTO",145,565,84);place(116,"LEGATO",232,565,74);place(119,"VOICE MODE",309,565,74);
    place(117,"LFO KEY TRIG",389,565,88);place(118,"TEMPO SYNC",480,565,88);place(11,"EXPRESSION",574,565,84);
    place(64,"HOLD PEDAL",662,565,84);place(41,"BEND RANGE",750,565,84);place(1,"MODULATION",838,565,84);
    keyboard.setBounds(26,718,1552,80);keyHold.setBounds(26,687,117,29);fold.setBounds(1380,660,198,28);
    bend.setBounds(240,687,192,29);
}
void BridgeEditor::timerCallback(){
    // Small panel labels were partially clipped by Direct2D on the test machine.
    // Use JUCE's software renderer for this embedded editor only.
    if(auto* peer=getPeer())if(peer->getCurrentRenderingEngine()!=0)peer->setCurrentRenderingEngine(0);
    for(int i=0;i<control::count;++i){labels[i]->setText(labels[i]->getName()+(p.known[i].load()?"":" *"),juce::dontSendNotification);updateControl(i);}
    channel.setSelectedId(p.midiControl.channel.load(),juce::dontSendNotification);
    midiEnabled.setToggleState(p.midiControl.enabled.load(),juce::dontSendNotification);notesEnabled.setToggleState(p.midiControl.notesEnabled.load(),juce::dontSendNotification);
    bool ready=p.midiControl.state.load()==1;
    if(libraryPanel&&libraryPanel->isVisible())libraryPanel->setConnected(ready);
    sendSaved.setEnabled(ready);keyboard.setEnabled(ready);bend.setEnabled(ready);
    if(!ready)keyboard.releaseAll();
    for(int n=0;n<128;++n)keyboard.lit[n]=p.midiControl.inputNotes[n].load()||p.midiControl.outputNotes[n].load();keyboard.repaint();
    if(p.scope.read(scopeScratch))scopeSnapshot=scopeScratch;
    if(performanceVisible!=p.performanceExpanded.load())performanceLayout();repaint();
}
void BridgeEditor::paint(juce::Graphics& g){
    g.fillAll(panelInk);g.addTransform(juce::AffineTransform::scale(getWidth()/1600.0f));
    auto text=[&g](const juce::String& s,int x,int y,int w,int h,float size,juce::Colour c=panelText){g.setColour(c);g.setFont(size);g.drawText(s,x,y,w,h,juce::Justification::centredLeft);};
    g.setColour(juce::Colour(0xff222b25));g.fillRect(0,0,1600,114);
    text("SYSTEM-1",24,14,285,42,34,panelGreen);text("AUDIO BRIDGE  /  USB AUDIO + MIDI",27,56,425,21,12);
    text(p.statusText,27,81,470,21,11,juce::Colour(0xff9fb4a5));
    auto bpm=p.midiControl.hostBpm.load();
    g.setColour(juce::Colour(0xff0b140e));g.fillRoundedRectangle(1042,15,91,47,4);
    text(bpm>0?juce::String(bpm,1):"---",1050,17,81,28,23,panelGreen);text("DAW BPM",1060,43,75,13,9);
    juce::String status;
    switch(p.midiControl.state.load()){
      case 1:status=p.midiControl.inputState.load()==1?"USB MIDI ready - send / receive":"USB MIDI output ready; input unavailable";break;
      case -1:status="MIDI access failed - check desktop permission prompts";break;
      case -2:status="MIDI is owned by another bridge instance";break;
      case -3:status="SYSTEM-1 MIDI port is not listed by Windows";break;
      case -4:status="Connect one SYSTEM-1 MIDI device";break;
      case -5:status="MIDI control off";break;
      default:status="Looking for SYSTEM-1 USB MIDI...";
    }
    text(status,501,62,637,20,12,p.midiControl.state.load()==1?panelGreen:panelText);
    juce::String clockStatus="Clock off";
    if(p.clockParam->getIndex()>0){
        bool active=p.midiControl.state.load()==1&&p.midiControl.clockValid.load()&&juce::Time::getMillisecondCounterHiRes()-p.midiControl.audioHeartbeat.load()<500;
        clockStatus=active?"Clock sending":"Clock waiting for DAW audio / tempo";
        clockStatus+="  |  SYSTEM-1 Clock Source: AUTO";
    }
    text(clockStatus,501,85,637,18,11,juce::Colour(0xff9fb4a5));
    float db=juce::Decibels::gainToDecibels(p.peak.load(),-60.0f);
    g.setColour(juce::Colour(0xff0d140f));g.fillRect(1470,69,108,8);g.setColour(db>-.1?juce::Colours::orange:panelGreen);
    g.fillRect(1470,69,int(108*juce::jlimit(0.0f,1.0f,(db+60)/60)),8);text("USB  "+juce::String(db,1)+" dBFS",1470,81,108,17,10);
    struct Section{int x,w;const char* name;};
    for(auto s:{Section{20,155,"LFO"},{185,205,"OSCILLATOR 1"},{400,285,"OSCILLATOR 2"},{695,148,"MIXER"},{850,135,"PITCH"},{995,258,"FILTER"},{1260,203,"AMP"},{1470,110,"EFFECTS"}}){
        g.setColour(juce::Colour(0xff263029));g.fillRoundedRectangle(float(s.x),130,float(s.w),404,4);
        g.setColour(panelGreen.withAlpha(.7f));g.drawHorizontalLine(157,float(s.x+7),float(s.x+s.w-7));text(s.name,s.x+8,135,s.w-16,20,12,panelGreen);
    }
    auto value=[this](int cc){return p.params[control::indexForCC(cc)]->get();};
    auto unit=[&](int cc){return value(cc)/127.f;};
    auto signedAmount=[&](int cc){int v=value(cc);return (v-64)/float(v<64?64:63);};
    auto known=[this](std::initializer_list<int> ccs){for(int cc:ccs)if(!p.known[control::indexForCC(cc)].load())return false;return true;};
    constexpr int lfoKinds[]{12,2,0,1,13,14};
    graphs::waveGuide(g,{26,452,144,75},lfoKinds[control::waveSlot(value(35))],known({35}));
    for(int osc=0;osc<2;++osc){int wave=osc?61:46,bank=osc?106:105,color=osc?55:50,mod=osc?63:60;
        graphs::waveGuide(g,osc?juce::Rectangle<int>(406,452,272,75):juce::Rectangle<int>(191,452,188,75),control::waveSlot(value(wave))+6*value(bank),known({wave,bank})&&value(bank)<=1,unit(color),known({color}),!known({mod})||control::waveSlot(value(mod))!=0);}
    graphs::envelope(g,{857,452,121,75},"AD GUIDE",unit(23),unit(24),0,0,true,signedAmount(22),known({22,23,24}));
    graphs::filterGuide(g,{1002,452,118,75},unit(3),unit(79),unit(9),value(115)<64,known({3,79,9,115}));
    graphs::envelope(g,{1126,452,120,75},"ENV GUIDE",unit(83),unit(84),unit(85),unit(86),false,signedAmount(81),known({81,83,84,85,86}));
    graphs::envelope(g,{1267,452,189,75},"ADSR GUIDE",unit(89),unit(90),unit(96),unit(97),false,1,known({89,90,96,97}));
    auto mixPlot=graphs::frame(g,{701,452,136,75},"MIX LEVELS");
    for(int i=0;i<4;++i){float h=unit(16+i)*mixPlot.getHeight();g.setColour(panelGreen.withAlpha(.7f));g.fillRect(mixPlot.getX()+i*30+5,mixPlot.getBottom()-h,16.f,h);}
    text("1     2    SUB   N",708,514,125,12,9);
    text("GUIDE",1480,463,88,17,11,panelGreen);text("Illustrative",1480,485,88,15,10);text("Not measured",1480,503,90,15,10);
    g.setColour(juce::Colour(0xff29382e));g.fillRoundedRectangle(20,550,1560,104,4);
    text("USB LEVEL",32,565,95,17,10);text("SYSTEM-1",1440,574,140,28,23,panelGreen);text("SYNTH CONTROL",1440,606,140,16,10);
    graphs::live(g,{940,559,481,87},scopeSnapshot,p.streaming.load()&&juce::Time::getMillisecondCounterHiRes()-p.midiControl.audioHeartbeat.load()<500);
    text("PERFORMANCE",27,667,147,19,12,panelGreen);
    text("Hold MANUAL to receive controls.  * Unsynced   ~ Nearest named setting   % Normalized amount",190,667,1175,18,11,juce::Colour(0xff9fb4a5));
    if(performanceVisible){
        text("128 KEYS  /  C0 - G10",465,689,360,20,12,panelGreen);text("C0 = MIDI 0    |    Velocity 100",1212,691,366,18,11);
        text("PITCH BEND",155,691,82,17,10);
    }
    int footer=performanceVisible?802:692;
    text("LIVE INPUT  /  Buffer "+juce::String(p.bufferMs.load(),1)+" ms  /  Clock pulses "+juce::String((juce::int64)p.midiControl.clocksSent.load())+"  /  MIDI drops "+juce::String((juce::int64)p.midiControl.overflow.load())+"  /  Late clocks "+juce::String((juce::int64)p.midiControl.clockLate.load()),27,footer,1430,17,10,juce::Colour(0xff829b89));
    text("PROTOTYPE 0.7.1",1475,footer,115,17,10,juce::Colour(0xff829b89));
}
