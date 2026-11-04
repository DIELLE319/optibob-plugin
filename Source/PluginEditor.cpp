#include "PluginEditor.h"
#include "BinaryData.h"

#if JUCE_MAC && JucePlugin_Build_Standalone
 #include <juce_audio_utils/juce_audio_utils.h>
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
 #include "Cattura.h"
// Solo nell'app (non dentro un DAW): l'audio del Mac entra in OptiBob Live da solo.
static void collegaAudioDelMac() {
    static bool fatto = false;
    if (fatto || juce::StandalonePluginHolder::getInstance() == nullptr) return;
    fatto = true;
    std::string err;
    if (cattura_avvia(err)) {
        // l'audio del Mac arriva dalla presa: niente ingresso, si suona sull'uscita che usa il Mac
        auto& dm = juce::StandalonePluginHolder::getInstance()->deviceManager;
        auto setup = dm.getAudioDeviceSetup();
        const juce::String uscita (cattura_uscitaMac());
        if (uscita.isNotEmpty()) setup.outputDeviceName = uscita;
        setup.inputDeviceName = {};
        setup.useDefaultOutputChannels = true;
        dm.setAudioDeviceSetup(setup, true);
        return;
    }
    // macOS prima del 14.4: ponte BlackHole (lo mette l'installer), tutto impostato da solo
    if (juce::String(err).contains("14.4")) {
        std::string vera;
        if (ponte_blackhole(vera)) {
            auto& dm = juce::StandalonePluginHolder::getInstance()->deviceManager;
            auto setup = dm.getAudioDeviceSetup();
            setup.inputDeviceName  = juce::String(ponte_nome());
            setup.outputDeviceName = juce::String(vera);
            setup.useDefaultInputChannels = setup.useDefaultOutputChannels = true;
            dm.setAudioDeviceSetup(setup, true);
            juce::StandalonePluginHolder::getInstance()->getMuteInputValue().setValue(false);
            return;
        }
    }
    juce::String testo (err);
    if (testo.contains("14.4"))
        testo = "On this macOS OptiBob Live can't take the Mac's sound by itself (it needs macOS 14.4 or later).\n\n"
                "Do it this way:\n"
                "1. Install BlackHole 2ch (free): existential.audio/blackhole\n"
                "2. System Settings > Sound > Output: BlackHole 2ch\n"
                "3. Here, click Options (top left): Input = BlackHole 2ch, Output = your speakers or sound card.";
    juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "OptiBob Live", testo);
}
#endif

namespace {
// i comandi che l'interfaccia muove e mostra
const char* const ID[] = { "g_pre", "a_up", "g_porta", "m_giumax", "m_spinta", "m_indbs", "m_unione", "x_in",
                           "m_gai1", "m_gai2", "m_gai3", "m_gai4", "m_gai5",
                           "m_xo1", "m_xo2", "m_xo3", "m_xo4" };

// la pagina per NOME-FILE originale (non dipende dal nome ribattezzato da JUCE)
const char* trovaPagina(const juce::String& nomeFile, int& dim) {
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
        const char* res = BinaryData::namedResourceList[i];
        if (juce::String(BinaryData::getNamedResourceOriginalFilename(res)) == nomeFile)
            return BinaryData::getNamedResource(res, dim);
    }
    dim = 0;
    return nullptr;
}
}

// riserva: se l'evento JUCE non c'e', la pagina naviga a optibob://set?id=..&n=..
bool UnoEditor::Pagina::pageAboutToLoad(const juce::String& url) {
    if (! url.startsWithIgnoreCase("optibob:")) return true;
    juce::URL u(url);
    auto* o = new juce::DynamicObject();
    const auto& nomi = u.getParameterNames();
    const auto& valori = u.getParameterValues();
    for (int i = 0; i < nomi.size(); ++i) {
        if (nomi[i] == "id") o->setProperty("id", valori[i]);
        else if (nomi[i].isNotEmpty()) o->setProperty(juce::Identifier(nomi[i]), valori[i].getDoubleValue());
    }
    ed.daPagina(juce::var(o));
    return false;
}

void UnoEditor::daPagina(const juce::var& o) {
    const juce::String id = o.getProperty("id", juce::var()).toString();
    if (id == "_ab") { proc.confronto.store((double) o.getProperty("v", 0.0) > 0.5); return; }
    auto* p = proc.stato.getParameter(id);
    if (p == nullptr) return;
    if (o.hasProperty("g")) {
        if ((double) o.getProperty("g", 0.0) > 0.5) p->beginChangeGesture();
        else                                         p->endChangeGesture();
        return;
    }
    float n;
    if      (o.hasProperty("n")) n = (float) (double) o.getProperty("n", 0.0);
    else if (o.hasProperty("v")) n = p->convertTo0to1((float) (double) o.getProperty("v", 0.0));
    else return;
    p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, n));
}

UnoEditor::UnoEditor(UnoProcessor& p)
    : AudioProcessorEditor(&p), proc(p) {
    auto opz = juce::WebBrowserComponent::Options{}
                  #if JUCE_WINDOWS
                   .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
                   .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2{}
                        .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("OptiBob")))
                  #endif
                   .withNativeIntegrationEnabled()
                   .withEventListener("set", [this](const juce::var& o) { daPagina(o); });
    web = std::make_unique<Pagina>(*this, opz);
    addAndMakeVisible(*web);

    int dim = 0;
    const char* html = trovaPagina("interfaccia-pro.html", dim);
    if (html != nullptr && dim > 0)
        web->goToURL("data:text/html;charset=utf-8;base64," + juce::Base64::toBase64(html, (size_t) dim));
    else
        web->goToURL("data:text/html,<body style='background:%230E1116;color:%23e6ebf2;font-family:system-ui'>"
                     "<h2 style='padding:24px'>OptiBob - pagina non trovata</h2></body>");

    setSize(1120, 850);
    setResizable(true, true);
    setResizeLimits(820, 620, 1920, 1400);
    startTimerHz(15);
   #if JUCE_MAC && JucePlugin_Build_Standalone
    if (juce::JUCEApplicationBase::isStandaloneApp())
        juce::Timer::callAfterDelay(700, [] { collegaAudioDelMac(); });
   #endif
}

UnoEditor::~UnoEditor() { stopTimer(); }

void UnoEditor::resized() { if (web) web->setBounds(getLocalBounds()); }

// rimanda alla pagina i valori veri (solo se cambiati, e comunque ogni tanto)
void UnoEditor::timerCallback() {
    juce::String js = "window.obStato&&obStato({";
    bool primo = true;
    for (auto* id : ID) {
        if (auto* q = proc.stato.getParameter(id)) {
            const float n = q->getValue();
            const float v = q->convertFrom0to1(n);
            js << (primo ? "" : ",") << "\"" << id << "\":[" << juce::String(v, 4) << "," << juce::String(n, 5) << "]";
            primo = false;
        }
    }
    // meter: AGC, 5 bande, finale (quanto schiaccia, dB positivi) + livelli in/out
    js << ",\"_m\":[" << juce::String(std::abs(proc.agcDb.load()), 2);
    for (int i = 0; i < 5; ++i) js << "," << juce::String(-proc.mbDb[i].load(), 2);
    js << "," << juce::String(-proc.maxDb.load(), 2) << "]";
    js << ",\"_io\":[" << juce::String(proc.inDb.load(), 1) << "," << juce::String(proc.outDb.load(), 1) << "]";
   #if JUCE_MAC && JucePlugin_Build_Standalone
    if (auto* h = juce::StandalonePluginHolder::getInstance())
        if (auto* d = h->deviceManager.getCurrentAudioDevice()) {
            const auto ch = d->getActiveInputChannels();
            juce::String c;
            for (int i = 0; i < ch.getHighestBit() + 1; ++i) if (ch[i]) c << (c.isEmpty() ? "" : "+") << (i + 1);
            juce::String info = (cattura_attiva() ? juce::String("IN: Mac sound") : "IN: " + c)
                              + "  |  OUT: " + d->getName()
                              + "  |  " + juce::String((int) d->getCurrentSampleRate()) + " Hz";
            js << ",\"_info\":\"" << info.replace("\"", "'") << "\"";
        }
   #endif
    js << "})";
    if (js != ultimo || (++giri % 8) == 0) { ultimo = js; web->evaluateJavascript(js); }
}
