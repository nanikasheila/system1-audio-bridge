// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#pragma once
#include <set>
inline const juce::Colour panelGreen{0xff79ed99},panelInk{0xff151917},panelText{0xffd5ddd7};
class PanelLook:public juce::LookAndFeel_V4 {
public:
    juce::Font getComboBoxFont(juce::ComboBox&)override{return juce::FontOptions(12.0f);}
    PanelLook(){
        setColour(juce::Slider::textBoxTextColourId,panelText);setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        setColour(juce::TextButton::buttonColourId,juce::Colour(0xff2c3530));setColour(juce::TextButton::textColourOffId,panelText);
        setColour(juce::ToggleButton::textColourId,panelText);setColour(juce::ToggleButton::tickColourId,panelGreen);
        setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff242e28));setColour(juce::ComboBox::textColourId,panelText);
        setColour(juce::ComboBox::outlineColourId,juce::Colour(0xff4b6152));setColour(juce::PopupMenu::backgroundColourId,panelInk);
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider&)override{
        auto area=juce::Rectangle<float>(float(x),float(y),float(w),float(h)).reduced(1);
        auto r=juce::jmin(area.getWidth(),area.getHeight())*.5f,cx=area.getCentreX(),cy=area.getCentreY();
        for(int i=0;i<11;++i){float a=start+(end-start)*i/10.0f;
            g.setColour(i<=pos*10?panelGreen:juce::Colour(0xff4a544d));
            g.drawLine(cx+std::sin(a)*(r-1),cy-std::cos(a)*(r-1),cx+std::sin(a)*(r-4),cy-std::cos(a)*(r-4),1.3f);
        }
        r-=5;g.setColour(juce::Colours::black.withAlpha(.5f));g.fillEllipse(cx-r,cy-r+3,2*r,2*r);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff525852),cx,cy-r,juce::Colour(0xff232a25),cx,cy+r,false));
        g.fillEllipse(cx-r,cy-r,2*r,2*r);g.setColour(juce::Colour(0xff69726b));g.drawEllipse(cx-r,cy-r,2*r,2*r,1);
        float a=start+pos*(end-start);g.setColour(panelGreen);
        g.drawLine(cx+std::sin(a)*r*.35f,cy-std::cos(a)*r*.35f,cx+std::sin(a)*r*.86f,cy-std::cos(a)*r*.86f,2.7f);
    }
    void drawLinearSlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float,float,juce::Slider::SliderStyle style,juce::Slider&)override{
        const bool vertical=style==juce::Slider::LinearVertical;
        auto cx=x+w*.5f,cy=y+h*.5f;
        g.setColour(juce::Colour(0xff060b08));
        if(vertical){g.fillRoundedRectangle(cx-3,float(y),6,float(h),3);g.setColour(panelGreen.withAlpha(.5f));g.fillRect(cx-1,pos,2.0f,float(y+h)-pos);}
        else{g.fillRoundedRectangle(float(x),cy-3,float(w),6,3);g.setColour(panelGreen.withAlpha(.5f));g.fillRect(float(x),cy-1,pos-x,2.0f);}
        auto cap=vertical?juce::Rectangle<float>(cx-10,pos-5,20,10):juce::Rectangle<float>(pos-5,cy-10,10,20);
        g.setColour(juce::Colour(0xff5e6860));g.fillRoundedRectangle(cap,2);g.setColour(panelGreen);
        if(vertical)g.drawHorizontalLine(int(pos),cx-8,cx+8);else g.drawVerticalLine(int(pos),cy-8,cy+8);
    }
};
class PanelKeyboard:public juce::Component {
public:
    std::function<void(int,bool)> note;
    std::array<bool,128> lit{};
    bool hold=false;
    ~PanelKeyboard()override{releaseAll();}
    void releaseAll(){for(auto n:held)if(note)note(n,false);held.clear();dragNote=-1;repaint();}
    void setHold(bool state){if(hold!=state){releaseAll();hold=state;}}
    void paint(juce::Graphics& g)override{
        for(int black=0;black<2;++black)for(int s=0;s<128;++s)if(isBlack(s)==bool(black)){
            auto r=keyRect(s);int n=s;bool on=held.count(n)||lit[n];
            g.setColour(on?panelGreen:(black?juce::Colour(0xff161d18):juce::Colour(0xffd5dbd5)));
            g.fillRoundedRectangle(r,3);g.setColour(black?juce::Colour(0xff536056):juce::Colour(0xff78867b));g.drawRoundedRectangle(r.reduced(.5f),3,1);
        }
        for(int n=0;n<128;n+=12){g.setColour(juce::Colour(0xff4f6455));g.setFont(10);g.drawText("C"+juce::String(n/12),keyRect(n).toNearestInt().withTop(getHeight()-24).withWidth(36),juce::Justification::centredLeft);}
    }
    void mouseDown(const juce::MouseEvent& e)override{select(hit(e.position),true);}
    void mouseDrag(const juce::MouseEvent& e)override{select(hit(e.position),false);}
    void mouseUp(const juce::MouseEvent&)override{if(!hold)releaseAll();dragNote=-1;}
public:
    static bool isBlack(int s){int n=s%12;return n==1||n==3||n==6||n==8||n==10;}
    juce::Rectangle<float> keyRect(int s)const{
        int white=0;for(int i=0;i<s;++i)if(!isBlack(i))++white;
        float w=getWidth()/75.0f;
        return isBlack(s)?juce::Rectangle<float>(white*w-w*.31f,0,w*.62f,getHeight()*.61f):juce::Rectangle<float>(white*w,0,w-1,float(getHeight()));
    }
    int hit(juce::Point<float> pt)const{for(int b=1;b>=0;--b)for(int s=0;s<128;++s)if(isBlack(s)==bool(b)&&keyRect(s).contains(pt))return s;return -1;}
private:
    std::set<int> held;int dragNote=-1;
    void select(int n,bool down){
        if(n==dragNote)return;
        if(!hold)releaseAll();dragNote=n;
        if(n<0)return;
        if(hold&&held.count(n)){if(down){held.erase(n);if(note)note(n,false);}}
        else{held.insert(n);if(note)note(n,true);}repaint();
    }
};
