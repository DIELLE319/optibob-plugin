#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// Interfaccia = la pagina HTML del prototipo, dentro un WebView.
// La pagina manda i comandi (evento "set"), l'editor rimanda i valori veri 15 volte al secondo.
class UnoEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit UnoEditor(UnoProcessor&);
    ~UnoEditor() override;
    void resized() override;
    void daPagina(const juce::var& o);
private:
    struct Pagina : public juce::WebBrowserComponent {
        Pagina(UnoEditor& e, const Options& o) : juce::WebBrowserComponent(o), ed(e) {}
        bool pageAboutToLoad(const juce::String& url) override;
        UnoEditor& ed;
    };
    void timerCallback() override;
    UnoProcessor& proc;
    std::unique_ptr<Pagina> web;
    juce::String ultimo;
    int giri = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UnoEditor)
};
