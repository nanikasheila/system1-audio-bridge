// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 nanikasheila and contributors

#pragma once
#include "Presets.h"
class LibraryPanel:public juce::Component,private juce::ListBoxModel {
public:
    std::function<presets::Patch()> capture;
    std::function<void(const presets::Patch&,bool)> apply;
    std::function<void()> close;
    presets::Library library;
    LibraryPanel(){
        setName("Preset library");setOpaque(true);
        for(auto* c:std::initializer_list<juce::Component*>{&search,&bank,&favorites,&list,&details,&name,&status,&load,&save,&rename,&favorite,&archive,&import,&exportFile,&refresh,&dismiss,&send})addAndMakeVisible(c);
        list.setModel(this);list.setRowHeight(31);list.setColour(juce::ListBox::backgroundColourId,panelInk);list.setMultipleSelectionEnabled(false);
        search.setTextToShowWhenEmpty("Search presets / banks",panelText.withAlpha(.5f));search.onTextChange=[this]{filter();};
        search.setName("librarySearch");send.setName("librarySend");bank.setName("libraryBank");
        name.setTextToShowWhenEmpty("Preset name for Save current / Rename",panelText.withAlpha(.5f));
        bank.onChange=[this]{filter();};favorites.onClick=[this]{filter();};send.setToggleState(true,juce::dontSendNotification);
        details.setMultiLine(true);details.setReadOnly(true);details.setScrollbarsShown(true);details.setColour(juce::TextEditor::backgroundColourId,panelInk);
        status.setColour(juce::Label::textColourId,panelGreen);status.setFont(juce::FontOptions(12));
        dismiss.onClick=[this]{if(close)close();};refresh.onClick=[this]{reload();};
        load.onClick=[this]{auto* e=selected();if(!e||!apply)return;apply(e->patch,send.getToggleState());status.setText(send.getToggleState()?"Loaded available controls; see retained fields below.":"Loaded in editor. Use Send saved when ready.",juce::dontSendNotification);};
        save.onClick=[this]{if(!capture)return;auto p=capture();p.name=name.getText();juce::String error;if(library.save(p,error)){reload();status.setText("Saved a new user preset.",juce::dontSendNotification);}else status.setText(error,juce::dontSendNotification);};
        rename.onClick=[this]{auto* e=selected();if(e&&name.getText().trim().isNotEmpty()){bool ok=library.editMetadata(*e,name.getText(),e->favorite);reload();status.setText(ok?"Display name updated; original file preserved.":"Could not rename.",juce::dontSendNotification);}};
        favorite.onClick=[this]{auto* e=selected();if(e){library.editMetadata(*e,e->patch.name,!e->favorite);reload();}};
        archive.onClick=[this]{auto* e=selected();if(e){bool ok=library.archive(*e);reload();status.setText(ok?"Moved to library Archive (recoverable on disk).":"Could not archive.",juce::dontSendNotification);}};
        import.onClick=[this]{chooser=std::make_unique<juce::FileChooser>("Import a folder of SYSTEM-1 PRM or bridge presets");juce::Component::SafePointer<LibraryPanel> safe(this);
            chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe](const juce::FileChooser& c){if(!safe)return;auto f=c.getResult();if(f.isDirectory()){auto message=safe->library.importFolder(f);safe->reload();safe->status.setText(message,juce::dontSendNotification);}});};
        exportFile.onClick=[this]{auto* e=selected();if(!e)return;auto source=e->file;
            chooser=std::make_unique<juce::FileChooser>(e->original?"Export unchanged original PRM":"Export bridge preset",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(source.getFileName()),e->original?"*.PRM":"*.s1preset");juce::Component::SafePointer<LibraryPanel> safe(this);
            chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe,source](const juce::FileChooser& c){if(!safe)return;auto f=c.getResult();if(f!=juce::File()){bool ok=f==source||source.copyFileTo(f);safe->status.setText(ok?"Exported original file (no parameter conversion).":"Export failed.",juce::dontSendNotification);}});};
    }
    ~LibraryPanel()override{list.setModel(nullptr);}
    void open(){reload();setVisible(true);toFront(false);}
    void setConnected(bool ready){load.setEnabled(selected()!=nullptr&&(!send.getToggleState()||ready));send.setTooltip(ready?"Load available controls into the connected synth.":"Synth offline. Uncheck to load in the editor only.");}
    void reload(){
        juce::String previous;if(auto* e=selected())previous=e->file.getFullPathName();auto chosen=bank.getText();library.scan();
        juce::StringArray groups;for(auto& e:library.entries)groups.addIfNotAlreadyThere(e.patch.bank);
        bank.clear(juce::dontSendNotification);bank.addItem("All banks",1);for(int i=0;i<groups.size();++i)bank.addItem(groups[i],i+2);
        int id=groups.indexOf(chosen);bank.setSelectedId(id<0?1:id+2,juce::dontSendNotification);filter();
        for(int i=0;i<int(rows.size());++i)if(library.entries[rows[i]].file.getFullPathName()==previous){list.selectRow(i);break;}
        status.setText(juce::String(library.entries.size())+" presets in library",juce::dontSendNotification);
    }
    void paint(juce::Graphics& g)override{
        g.fillAll(juce::Colour(0xff19241d));g.setColour(panelGreen);g.setFont(22);g.drawText("PRESET LIBRARY",20,12,300,35,juce::Justification::centredLeft);
        g.setColour(panelText.withAlpha(.7f));g.setFont(12);g.drawText("PRM originals + editable bridge snapshots",315,17,550,25,juce::Justification::centredLeft);
        g.drawText("Name",810,375,70,22,juce::Justification::centredLeft);
        g.drawText("Save current stores known synth controls; USB level, MIDI routing and DAW clock are kept separate.",810,460,720,36,juce::Justification::topLeft);
    }
    void resized()override{
        dismiss.setBounds(getWidth()-110,15,90,30);search.setBounds(20,61,420,30);bank.setBounds(453,61,330,30);favorites.setBounds(805,61,150,30);refresh.setBounds(973,61,100,30);
        list.setBounds(20,105,763,getHeight()-171);details.setBounds(810,105,getWidth()-830,205);
        send.setBounds(810,320,210,30);load.setBounds(1030,320,145,30);favorite.setBounds(1187,320,150,30);archive.setBounds(1349,320,170,30);
        name.setBounds(870,373,getWidth()-890,28);save.setBounds(810,416,200,30);rename.setBounds(1023,416,160,30);exportFile.setBounds(1196,416,323,30);
        import.setBounds(20,getHeight()-52,215,30);status.setBounds(253,getHeight()-52,getWidth()-273,30);
    }
private:
    std::vector<size_t> rows;juce::ListBox list;
    juce::TextEditor search,details,name;juce::ComboBox bank;juce::Label status;
    juce::ToggleButton favorites{"Favorites only"},send{"Send to SYSTEM-1"};
    juce::TextButton load{"Load preset"},save{"Save current as new"},rename{"Rename selected"},favorite{"Toggle favorite"},archive{"Archive selected"},import{"Import folder..."},exportFile{"Export original file..."},refresh{"Refresh"},dismiss{"Close"};
    std::unique_ptr<juce::FileChooser> chooser;
    presets::Entry* selected(){int n=list.getSelectedRow();return n>=0&&n<int(rows.size())&&rows[n]<library.entries.size()?&library.entries[rows[n]]:nullptr;}
    void filter(){rows.clear();for(size_t i=0;i<library.entries.size();++i){auto& e=library.entries[i];if(favorites.getToggleState()&&!e.favorite)continue;if(bank.getSelectedId()>1&&bank.getText()!=e.patch.bank)continue;if(!(e.patch.name+" "+e.patch.bank).containsIgnoreCase(search.getText().trim()))continue;rows.push_back(i);}list.deselectAllRows();list.updateContent();if(!rows.empty())list.selectRow(0);else details.setText("No presets match. Import a folder or save the current controls.");}
    int getNumRows()override{return int(rows.size());}
    void paintListBoxItem(int n,juce::Graphics& g,int w,int h,bool selectedRow)override{if(n<0||n>=int(rows.size()))return;auto& e=library.entries[rows[n]];
        if(selectedRow)g.fillAll(juce::Colour(0xff365340));g.setColour(e.favorite?panelGreen:panelText);g.setFont(14);g.drawText((e.favorite?"*  ":"    ")+e.patch.name,8,0,w/2,h,juce::Justification::centredLeft);
        g.setColour(panelText.withAlpha(.7f));g.setFont(11);auto group=e.patch.bank.replace("SYSTEM-1_Sound_Bank_For_120_","").replaceCharacter('_',' ');g.drawText(group+(e.original?"  |  PRM preview":"  |  Snapshot"),w/2,0,w/2-10,h,juce::Justification::centredRight);}
    void selectedRowsChanged(int)override{auto* e=selected();if(!e)return;name.setText(e->patch.name,false);details.setText(e->patch.name+"\n"+e->patch.bank+"\n\n"+e->patch.notice+"\n\n"+juce::String(e->patch.count())+" available controls. Selecting a row does not change the sound.");}
};
