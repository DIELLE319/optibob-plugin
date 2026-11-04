#include "PluginProcessor.h"
#include <cmath>
#include "PluginEditor.h"
#include "PresetDiFabbrica.h"
#include "Licenza.h"
#if JUCE_MAC || JUCE_WINDOWS
 #include "Cattura.h"
#endif

namespace {
void aggiungi(juce::AudioProcessorValueTreeState::ParameterLayout& l, const uno::Voce& v,
              const juce::String& id) {
    l.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ id, 1 }, v.nome,
        juce::NormalisableRange<float>(v.min, v.max, v.passo), v.def));
}

juce::AudioProcessorValueTreeState::ParameterLayout disposizione() {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (const auto& v : uno::vociEq())     aggiungi(l, v, v.id);
    for (const auto& v : uno::vociAgc())    aggiungi(l, v, v.id);
    for (const auto& v : uno::vociMbGlob()) aggiungi(l, v, v.id);
    for (const auto& v : uno::vociMax())    aggiungi(l, v, v.id);
    for (const auto& v : uno::vociTosa())   aggiungi(l, v, v.id);
    // le cinque bande: attacchi e rilasci scalati, basse lente e alte svelte
    const float att[5] = { 25.f, 15.f, 8.f, 4.f, 2.f };
    const float ril[5] = { 250.f, 200.f, 150.f, 250.f, 400.f };
    for (int b = 0; b < 5; ++b)
        for (const auto& v : uno::vociBanda()) {
            uno::Voce c = v;
            if (juce::String(v.id) == "m_att") c.def = att[b];
            if (juce::String(v.id) == "m_ril") c.def = ril[b];
            aggiungi(l, c, juce::String(v.id) + juce::String(b + 1));
        }
    l.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "a_pesato", 1 }, "Ear-weighted detector", true));
    l.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "a_sologiu", 1 }, "Down only", false));
    l.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "a_on", 1 }, "AGC section", true));
    l.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "e_on", 1 }, "EQ section", true));
    l.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "m_on", 1 }, "Multiband section", true));
    l.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "x_on", 1 }, "Final section", true));
    l.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "x_limit", 1 }, "Limiter", true));
    l.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "m_lock", 1 }, "Hold bands at ceiling (radio)", false));
    l.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "a_slope", 1 }, "Low cut slope",
        juce::StringArray{ "12 dB/oct", "24 dB/oct", "36 dB/oct" }, 1));
    return l;
}
}

UnoProcessor::UnoProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("In",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Out", juce::AudioChannelSet::stereo(), true)),
      stato(*this, nullptr, "OPTIBOB", disposizione()) {
    caricaPreset(kPresetDiFabbrica);                 // parte gia' tarato, non coi valori neutri
    stato.state.setProperty("taratura", 1, nullptr);
    lic::segnaPrimoAvvio();
    modoLic.store((int) lic::modo());
    static bool verificata = false;                  // una volta per avvio: chiave scaduta/revocata?
    if (! verificata) {
        verificata = true;
        juce::Thread::launch([] { lic::verifica(); });   // l'editor rilegge il modo ogni 2 secondi
    }
}

void UnoProcessor::prepareToPlay(double sr, int blocco) {
    equalizzatore.prepare(sr);
    livellatore.prepare(sr);
    multibanda.prepare(sr);
    finale.prepare(sr, blocco);
    setLatencySamples(finale.latenza());       // l'unico che ritarda e' il finale
}

bool UnoProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
         || l.getMainInputChannelSet() == juce::AudioChannelSet::mono());
}

void UnoProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buf.getNumSamples();
    if (n <= 0) return;
    // 30/09 - qui il plugin andava in crash, e non da oggi: quattro comandi
    // (a_down, a_frenoms, m_out, x_bsrel) sono citati qui ma non esistono
    // nella lista, e getRawParameterValue restituiva un puntatore nullo che
    // veniva letto subito. Nell'app non succedeva perche' li' la lettura ha
    // sempre avuto un valore di ripiego (q(id, default) in Motore.mm). Adesso
    // ce l'ha anche qui: un comando che manca vale il suo default, non un
    // segnale di terminazione.
    auto g = [this](const char* id, double def = 0.0) {
        auto* p = stato.getRawParameterValue(id);
        return p ? (double) p->load() : def; };
    auto b = [this](const char* id, bool def = false) {
        auto* p = stato.getRawParameterValue(id);
        return p ? p->load() > 0.5f : def; };
    auto gb = [this](const char* id, int i, double def = 0.0) {
        auto* p = stato.getRawParameterValue(juce::String(id) + juce::String(i + 1));
        return p ? (double) p->load() : def; };

    float* L = buf.getWritePointer(0);
    float* R = buf.getNumChannels() > 1 ? buf.getWritePointer(1) : L;
   #if JUCE_MAC || JUCE_WINDOWS
    if (R != L) cattura_leggi(L, R, n, getSampleRate());   // OptiBob Live: l'audio del Mac al posto dell'ingresso
   #endif

    auto picco = [n](const float* a, const float* b) {
        float m = 0.0f;
        for (int i = 0; i < n; ++i) m = std::max(m, std::max(std::abs(a[i]), std::abs(b[i])));
        return m > 1.0e-6f ? 20.0f * std::log10(m) : -100.0f; };
    inDb.store(picco(L, R));
    if (confronto.load() || modoLic.load() == 0) {   // B, o prova finita: si ascolta l'originale
        agcDb.store(0.0f); maxDb.store(0.0f); for (int i = 0; i < 5; ++i) mbDb[i].store(0.0f);
        outDb.store(inDb.load()); return;
    }
    // AGC drive: quanto si spinge il segnale dentro la catena (g_pre, in dB)
    if (const double pre = g("g_pre"); std::abs(pre) > 0.01) {
        const float k = (float) std::pow(10.0, pre * 0.05);
        for (int i = 0; i < n; ++i) { L[i] *= k; if (R != L) R[i] *= k; }
    }

    // ---------------------------------------------------------------- AGC
    if (b("a_on")) {
        agc::Params p;
        p.targetDb = g("a_target"); p.upDbS = g("a_up"); p.downDbS = g("a_down", p.downDbS);
        p.maxUpDb = p.maxDownDb = g("a_win");
        p.gateDb = g("a_gate");  p.holdMs = g("a_hold");  p.rmsMs = g("a_det");
        p.hpfHz = g("a_cut");
        p.anticipoMs = std::max(g("a_ant"), g("g_porta"));   // Delay: guarda avanti, non sfonda
        p.saltoDb=g("a_salto"); p.saltoMs=g("a_saltoms"); p.saltoX=g("a_saltox");
        p.frenoDb=g("a_freno"); p.frenoDbS=g("a_frenov"); p.frenoMs=g("a_frenoms", p.frenoMs);
        if (auto* sl = stato.getRawParameterValue("a_slope")) p.hpfSez = 1 + (int) sl->load();
        p.pesato = b("a_pesato"); p.soloGiu = b("a_sologiu");
        p.subHz = g("a_subhz");  p.subDb = g("a_sub");
        p.boomHz = g("a_boomhz"); p.boomDb = g("a_boom"); p.boomQ = g("a_boomq");
        livellatore.setParams(p);
        livellatore.process(L, R, n);
        agcDb.store((float) livellatore.gainDb());
    } else agcDb.store(0.0f);

    // ------------------------------------------------------------ EQ
    // ⚠️ 18/08 — trovato con l'inventario: i comandi dell'equalizzatore
    // esistevano ma nel PLUGIN non li leggeva nessuno e il filtro non
    // veniva mai chiamato. Nell'app funzionava, nel plugin no.
    if (b("e_on")) {
        oeq::Params pe;
        pe.hpHz=g("e_hp"); pe.hpQ=g("e_hpq"); pe.hpSez=(int) std::lround(g("e_hpsez"));
        pe.lsHz=g("e_lshz"); pe.lsDb=g("e_lsdb");
        pe.b1Hz=g("e_b1hz"); pe.b1Db=g("e_b1db"); pe.b1Q=g("e_b1q");
        pe.b2Hz=g("e_b2hz"); pe.b2Db=g("e_b2db"); pe.b2Q=g("e_b2q");
        pe.b3Hz=g("e_b3hz"); pe.b3Db=g("e_b3db"); pe.b3Q=g("e_b3q");
        pe.hsHz=g("e_hshz"); pe.hsDb=g("e_hsdb");
        pe.lpHz=g("e_lp");   pe.lpQ=g("e_lpq");
        equalizzatore.setParams(pe);
        equalizzatore.process(L, R, n);
    }

    // ---------------------------------------------------------- multibanda
    if (b("m_on")) {
        omb::Params p;
        for (int i = 0; i < 5; ++i) {
            p.b[i].soglia = gb("m_sog", i); p.b[i].tetto = gb("m_tet", i);
            p.b[i].rapporto = gb("m_rap", i);
            p.b[i].attaccoMs = gb("m_att", i); p.b[i].rilascioMs = gb("m_ril", i);
            p.b[i].gainDb = gb("m_gai", i);
            p.b[i].sottoDb = gb("m_sot", i);
        }
        p.xover[0]=g("m_xo1"); p.xover[1]=g("m_xo2"); p.xover[2]=g("m_xo3"); p.xover[3]=g("m_xo4");
        p.inMin=g("m_inmin"); p.inMax=g("m_inmax"); p.inDbS=g("m_indbs");
        p.hpfHz=g("m_hpf"); p.mix=g("m_mix")*0.01; p.outDb=g("m_out", p.outDb);
        p.rivMs = g("m_det");
        p.suMax=g("m_sumax"); p.giuMax=g("m_giumax");
        p.liberta = g("m_lib"); p.sbavatura = g("m_sbav") * 0.01; p.limDb = g("m_lim"); p.limRelMs = g("m_limrel");
        p.unione = g("m_unione") * 0.01;
        p.accBasso=g("m_acc")*0.01; p.acc34=g("m_acc34")*0.01;
        p.pendenza=(g("m_pend") >= 36.0) ? 4 : 2;
        p.modo = (int) std::lround(g("m_modo", 1.0)); p.spinta = g("m_spinta", 15.0); p.lega2 = g("m_lega2", 40.0) * 0.01;
        p.bloccaAlTetto = b("m_lock");
        multibanda.setParams(p);
        multibanda.process(L, R, n);
        for (int i = 0; i < 5; ++i) {
            mbDb[i].store((float) multibanda.riduzione(i));
            mbLv[i].store((float) multibanda.livello(i));
        }
    } else for (int i = 0; i < 5; ++i) mbDb[i].store(0.0f);

    // ------------------------------------------------------------- finale
    if (b("x_on")) {
        omax::Params p;
        p.inDb=g("x_in"); p.shape=g("x_shape")*0.01; p.mix=g("x_mix")*0.01;
        p.outDb=g("x_out"); p.rilascioMs=g("x_rel"); p.anticipoMs=g("x_ant"); p.limit=b("x_limit");
        p.maHz=g("x_mahz"); p.maFisso=g("x_mafix"); p.maDin=g("x_madin");
        p.maRel=g("x_marel"); p.maQ=g("x_maq");
        p.bsHz=g("x_bshz"); p.bsDin=g("x_bsdin"); p.bsFisso=g("x_bsfix"); p.bsRel=g("x_bsrel", p.bsRel); p.bsQ=g("x_bsq");
        p.rotSez=(int) std::lround(g("x_rot"));
        p.bcDb=g("x_bcdb"); p.bcHz=g("x_bchz");
        p.spazio = g("x_spazio") * 0.01; p.spazioHz = g("x_spaziohz");
        p.preUs=g("x_pre"); p.topHz=g("x_top"); p.hfDb=g("x_hfdb"); p.hfHz=g("x_hfhz"); p.clipDb=g("x_clip");
        finale.setParams(p);
        finale.process(L, R, n);
        maxDb.store((float) finale.riduzioneDb());
    } else maxDb.store(0.0f);

    outDb.store(picco(L, R));
}

juce::String UnoProcessor::presetInJson() const {
    juce::String s = "{\n";
    const auto& par = getParameters();
    for (auto* p : par)
        if (auto* q = dynamic_cast<juce::AudioProcessorParameterWithID*>(p))
            s << "  \"" << q->paramID << "\": "
              << juce::String((double) stato.getRawParameterValue(q->paramID)->load(), 4)
              << ",\n";
    s << "  \"_nome\": \"" << nomePreset << "\"\n}";
    return s;
}

bool UnoProcessor::caricaPreset(const juce::String& json) {
    auto v = juce::JSON::parse(json);
    if (!v.isObject()) return false;
    const auto& par = getParameters();
    for (auto* p : par)
        if (auto* q = dynamic_cast<juce::AudioProcessorParameterWithID*>(p)) {
            auto x = v.getProperty(juce::Identifier(q->paramID), juce::var());
            if (x.isVoid()) continue;
            if (auto* r = stato.getParameter(q->paramID))
                r->setValueNotifyingHost(r->convertTo0to1((float) (double) x));
        }
    auto nm = v.getProperty("_nome", juce::var());
    nomePreset = nm.isVoid() ? "senza nome" : nm.toString();
    return true;
}

juce::AudioProcessorEditor* UnoProcessor::createEditor() { return new UnoEditor(*this); }
void UnoProcessor::getStateInformation(juce::MemoryBlock& m) {
    if (auto x = stato.copyState().createXml()) copyXmlToBinary(*x, m);
}
void UnoProcessor::setStateInformation(const void* d, int n) {
    // i salvataggi vecchi (valori neutri, senza "taratura") si ignorano: resta la taratura di fabbrica
    if (auto x = getXmlFromBinary(d, n))
        if (x->getIntAttribute("taratura", 0) == 1) stato.replaceState(juce::ValueTree::fromXml(*x));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new UnoProcessor(); }
