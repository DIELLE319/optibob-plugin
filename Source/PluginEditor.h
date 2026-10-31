#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Una pagina di cursori, costruita da un elenco di comandi.
class Pagina : public juce::Component {
public:
    Pagina(UnoProcessor& p, const std::vector<uno::Voce>& v, const juce::String& suffisso = {});
    void resized() override;
    void lingua();          // rilegge i nomi nella lingua scelta
private:
    struct Man {
        juce::Slider cursore; juce::Label nome, spiega, valore;
        const uno::Voce* voce = nullptr;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aggancio;
    };
    std::vector<std::unique_ptr<Man>> man;
};

// Le cinque bande DISEGNATE: per ognuna la finestra fra il minimo (Threshold)
// e il massimo (Ceiling), i due cursori da trascinare, e dentro il livello
// della banda in tempo reale. Si vede dove sta suonando e dove le e' concesso
// stare: e' il modo in cui si guida una macchina cosi', invece che a numeri.
class Finestre : public juce::Component, private juce::Timer {
public:
    explicit Finestre(UnoProcessor& p);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    void muovi(const juce::MouseEvent&);
    float yDaDb(double db) const;
    double dbDaY(float y) const;
    UnoProcessor& proc;
    float lv[5] { -100,-100,-100,-100,-100 }, gr[5] { 0,0,0,0,0 };
    int bandaPresa = -1, qualeCursore = 0;   // 0 = minimo, 1 = massimo
};

// La striscia delle frequenze: i quattro incroci si trascinano col mouse.
class Incroci : public juce::Component {
public:
    explicit Incroci(UnoProcessor& p);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
private:
    void muovi(const juce::MouseEvent&);
    float xDaHz(double hz) const;
    double hzDaX(float x) const;
    UnoProcessor& proc;
    int preso = -1;
};

// Le spie in cima: AGC, le cinque bande, il finale.
class Spie : public juce::Component, private juce::Timer {
public:
    explicit Spie(UnoProcessor& p);
    void paint(juce::Graphics&) override;
private:
    void timerCallback() override;
    UnoProcessor& proc;
    float a = 0, m[5] { 0,0,0,0,0 }, x = 0;
};

class UnoEditor : public juce::AudioProcessorEditor {
public:
    explicit UnoEditor(UnoProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    UnoProcessor& proc;
    Spie spie;
    Finestre finestre;
    Incroci incroci;
    juce::TabbedComponent schede { juce::TabbedButtonBar::TabsAtTop };
    std::vector<std::unique_ptr<Pagina>> pagine;
    // ⚠️ niente inizializzazione con le graffe: il costruttore di
    // ToggleButton e' esplicito. Il testo si mette nel costruttore.
    juce::ToggleButton acc[3];
    juce::ToggleButton blocca { "hold bands at ceiling  (the radio sound: always the same)" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aggBlocca;
    juce::TextButton salva { "save preset" }, carica { "load preset" };
    juce::TextButton lingua { "EN" };      // ribalta italiano <-> inglese
    void applicaLingua();
    std::unique_ptr<juce::FileChooser> scelta;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> agg[3];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UnoEditor)
};
