// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#pragma once
#include "Scope.h"

namespace graphs {
inline float wave(int kind,float phase,float color=0){
    const float t=phase-std::floor(phase),a=juce::MathConstants<float>::twoPi*t;
    const float c=juce::jlimit(0.f,1.f,color),width=.5f-.46f*c;
    auto basic=[](int shape,float phase){float t=phase-std::floor(phase);return shape==0?2*t-1:shape==1?(t<.5f?1.f:-1.f):1-4*std::abs(t-.5f);};
    // Qualitative COLOR previews, not measured Roland oscillator models.
    // Width, stacked detune, FM and formant examples follow wave families;
    // amount curves, ratios and phases below are illustrative choices.
    switch(kind){
        case 0:{float span=1-.9f*c;return t<span?2*t/span-1:-1;}
        case 1:return t<width?1.f:-1.f;
        case 2:return t<width?-1+2*t/width:1-2*(t-width)/(1-width);
        case 3:case 4:case 5:{float v=0;for(int i=-3;i<=3;++i)v+=basic(kind-3,phase*(1+c*.025f*i)+c*.065f*i);return v/7;}
        case 6:return (1-.5f*c)*(2*t-1)+.5f*c*std::sin(71*a+.7f)*std::sin(33*a);
        case 7:return ((t<.5f)!=(std::fmod((2+5*c)*t,1.f)<width))?.8f:-.8f;
        case 8:return std::sin(a+7*c*std::sin(3*a));
        case 9:{float synced=juce::MathConstants<float>::twoPi*std::fmod((1+4*c)*t,1.f);return std::sin(synced+4*c*std::sin(3*synced));}
        case 10:{
            // Crossfade representative harmonic clusters, without phoneme calibration.
            constexpr int bands[5][2]{{3,9},{4,8},{2,11},{2,5},{1,4}};
            int n=std::min(3,int(c*4));float f=c*4-n;
            auto vowel=[&](int k){return (.65f*std::sin(bands[k][0]*a)+.3f*std::sin(bands[k][1]*a))*std::pow(.5f+.5f*std::cos(a),2.f);};
            return (1-f)*vowel(n)+f*vowel(n+1);
        }
        case 11:return (.5f*std::sin(2*a)+.5f*std::sin((3+3*c)*a))*std::exp(-t*(1+5*c));
        case 12:return std::sin(a);
        case 13:return .7f*std::sin(17*std::floor(t*8)+.7f);
        default:return .55f*std::sin(3*a)+.28f*std::sin(7*a+.6f);
    }
}
inline float filter(float x,float cutoff,float highpass,float resonance,bool steep){
    // Relative frequency / level diagram. No calibrated Hz, dB or measured response.
    float lp=1/(1+std::exp((x-cutoff)*(steep?26.f:13.f)));
    float hp=highpass<=0?1:1/(1+std::exp((highpass-x)*18));
    float peak=resonance*.35f*std::exp(-std::pow((x-cutoff)/.055f,2.f));
    return juce::jlimit(0.f,1.f,(lp*.65f+peak)*hp);
}
inline juce::Rectangle<float> frame(juce::Graphics& g,juce::Rectangle<int> bounds,const juce::String& title){
    g.setColour(juce::Colour(0xff0d1510));g.fillRoundedRectangle(bounds.toFloat(),4);
    g.setColour(panelText.withAlpha(.75f));g.setFont(10);g.drawText(title,bounds.reduced(7,2).withHeight(15),juce::Justification::centredLeft);
    auto plot=bounds.toFloat().reduced(8,5).withTrimmedTop(15).withTrimmedBottom(9);
    g.setColour(panelGreen.withAlpha(.1f));for(int i=0;i<5;++i)g.drawVerticalLine(int(plot.getX()+plot.getWidth()*i/4),plot.getY(),plot.getBottom());
    g.drawHorizontalLine(int(plot.getCentreY()),plot.getX(),plot.getRight());return plot;
}
inline void stroke(juce::Graphics& g,const juce::Path& path,juce::Colour colour=panelGreen){g.setColour(colour);g.strokePath(path,juce::PathStrokeType(1.4f));}
inline void waveGuide(juce::Graphics& g,juce::Rectangle<int> bounds,int kind,bool known,float color=-1,bool colorKnown=true,bool modulated=false){
    auto plot=frame(g,bounds,"WAVE GUIDE");if(!known){g.setFont(11);g.setColour(panelText);g.drawText("Choose / receive",plot.toNearestInt(),juce::Justification::centred);return;}
    juce::Path p;for(int i=0;i<240;++i){float x=i/239.f,y=wave(kind,x*2,colorKnown?std::max(0.f,color):0);auto pt=juce::Point<float>(plot.getX()+x*plot.getWidth(),plot.getCentreY()-y*plot.getHeight()*.44f);if(i==0)p.startNewSubPath(pt);else p.lineTo(pt);}stroke(g,p);
    if(color>=0){g.setFont(9);g.setColour(panelText.withAlpha(.7f));
        juce::String label=colorKnown?"COLOR "+juce::String(juce::roundToInt(color*100))+"%":"COLOR *";
        label+=modulated?" | Depth preview":" | Approx.";
        g.drawText(label,bounds.withTop(bounds.getBottom()-13).reduced(7,0),juce::Justification::centredLeft);}
}
inline void envelope(juce::Graphics& g,juce::Rectangle<int> bounds,const juce::String& title,float attack,float decay,float sustain,float release,bool ad,float depth,bool known){
    auto plot=frame(g,bounds,title+(known?"":" *"));
    const float a=.02f+.25f*attack,d=.02f+.25f*decay,r=ad?0:.02f+.25f*release,hold=ad?0:.12f;
    auto point=[&](float x,float v){return juce::Point<float>(plot.getX()+x*plot.getWidth(),plot.getBottom()-(.08f+(depth<0?.84f:0)+v*depth*.84f)*plot.getHeight());};
    juce::Path p;p.startNewSubPath(point(0,0));
    for(int i=1;i<=30;++i){float t=i/30.f;p.lineTo(point(a*t,t));}
    for(int i=1;i<=30;++i){float t=i/30.f;p.lineTo(point(a+d*t,(ad?0:sustain)+(1-(ad?0:sustain))*std::pow(1-t,2.f)));}
    if(!ad){p.lineTo(point(a+d+hold,sustain));for(int i=1;i<=30;++i){float t=i/30.f;p.lineTo(point(a+d+hold+r*t,sustain*std::pow(1-t,2.f)));}}
    p.lineTo(point(1,0));stroke(g,p);
    g.setFont(9);g.setColour(panelText.withAlpha(.65f));
    g.drawText(ad?"A   >   D":"A  >  D  >  S  >  R",bounds.withTop(bounds.getBottom()-13).reduced(7,0),juce::Justification::centred);
}
inline void filterGuide(juce::Graphics& g,juce::Rectangle<int> bounds,float cutoff,float hp,float res,bool steep,bool known){
    auto plot=frame(g,bounds,juce::String("FILTER GUIDE")+(known?"":" *"));juce::Path p;
    for(int i=0;i<240;++i){float x=i/239.f,y=filter(x,cutoff,hp,res,steep);auto pt=juce::Point<float>(plot.getX()+x*plot.getWidth(),plot.getBottom()-y*plot.getHeight());if(i==0)p.startNewSubPath(pt);else p.lineTo(pt);}stroke(g,p);
    g.setColour(panelText.withAlpha(.6f));g.setFont(9);g.drawText("LOW  >  HIGH",bounds.withTop(bounds.getBottom()-13).reduced(8,0),juce::Justification::centredRight);
}
inline void live(juce::Graphics& g,juce::Rectangle<int> bounds,const OutputScope::Snapshot& s,bool active){
    auto plot=frame(g,bounds,"USB OUTPUT   L / R   |   LIVE   |   Auto scale");
    if(!active||s.count<2){g.setFont(11);g.setColour(panelText);g.drawText("Waiting for audio",plot.toNearestInt(),juce::Justification::centred);return;}
    unsigned count=std::min(s.count,unsigned(s.rate*.020)),start=s.count-count;
    // Positive crossing trigger, only within data that includes a full display window.
    for(unsigned i=1;i+count<=s.count;++i)if(s.left[i-1]<=0&&s.left[i]>.001f){start=i;break;}
    float peak=.001f;for(unsigned i=0;i<count;++i)peak=std::max(peak,std::max(std::abs(s.left[start+i]),std::abs(s.right[start+i])));
    for(int ch=1;ch>=0;--ch){juce::Path p;auto& data=ch?s.right:s.left;
        for(unsigned i=0;i<count;++i){auto pt=juce::Point<float>(plot.getX()+i*plot.getWidth()/float(count-1),plot.getCentreY()-data[start+i]/peak*plot.getHeight()*.46f);if(i==0)p.startNewSubPath(pt);else p.lineTo(pt);}stroke(g,p,ch?juce::Colour(0xff73bcca).withAlpha(.7f):panelGreen);}
    g.setFont(9);g.setColour(panelText.withAlpha(.7f));g.drawText(juce::String(count/s.rate*1000,1)+" ms",bounds.withTop(bounds.getBottom()-13).reduced(8,0),juce::Justification::centredRight);
}
}
