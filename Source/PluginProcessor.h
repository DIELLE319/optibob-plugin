#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "../Engine/Eq.h"
#include "../Engine/Agc.h"
#include "../Engine/Mb.h"
#include "../Engine/Max.h"
#include "Comandi.h"

class UnoProcessor : public juce::AudioProcessor {
public:
    UnoProcessor();
    ~UnoProcessor() override = default;
    void prepareToPlay(double sr, int blocco) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "OptiBob"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState stato;

    // Preset in JSON piatto: nome del comando -> valore. Leggibile a occhio,
    // scambiabile per posta, e lo stesso formato lo capisce il prototipo.
    juce::String presetInJson() const;
    bool caricaPreset(const juce::String& json);
    juce::String nomePreset { "default" };

    // spie: guadagno dell'AGC, riduzione delle cinque bande, riduzione del finale
    std::atomic<float> agcDb { 0.0f }, mbDb[5] { 0,0,0,0,0 }, maxDb { 0.0f };
    // livello di ciascuna banda: serve a disegnare dove suona, dentro la finestra
    std::atomic<float> mbLv[5] { -100,-100,-100,-100,-100 };
    // sezioni accese o spente, per confrontare a orecchio
    std::atomic<bool> vivo { true };

private:
    oeq::Eq   equalizzatore;
    agc::Agc  livellatore;
    omb::Mb   multibanda;
    omax::Max finale;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UnoProcessor)
};
