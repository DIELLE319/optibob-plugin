#pragma once
#include <juce_core/juce_core.h>

// Licenze OptiLive (Lemon Squeezy) + prova di 7 giorni.
//  - prova: data del primo avvio in <Application Support>/OptiBob/prova.txt, 7 giorni tutto sbloccato
//  - Basic / Pro: chiave Lemon Squeezy attivata su questo computer (max 2), salvata in licenza.json
//  - prova finita e nessuna chiave: OptiBob non lavora (passa il suono com'e')
namespace lic {

enum Modo { Finita = 0, Basic = 1, Pro = 2, Prova = 3 };

// ultimo esito dell'attivazione, da mostrare nel pannello (scritto da un thread)
inline juce::CriticalSection esitoLock;
inline juce::String esitoMsg;
inline bool esitoOk = false;

inline juce::File cartella() {
    auto d = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("OptiBob");
    d.createDirectory();
    return d;
}
inline juce::File fProva()   { return cartella().getChildFile("prova.txt"); }
inline juce::File fLicenza() { return cartella().getChildFile("licenza.json"); }

inline void segnaPrimoAvvio() {
    if (! fProva().existsAsFile())
        fProva().replaceWithText(juce::String(juce::Time::getCurrentTime().toMilliseconds()));
}
inline int giorniProvaRimasti() {
    if (! fProva().existsAsFile()) return 7;
    const auto t0 = fProva().loadFileAsString().trim().getLargeIntValue();
    const double giorni = (juce::Time::getCurrentTime().toMilliseconds() - t0) / 86400000.0;
    return juce::jlimit(0, 7, 7 - (int) std::floor(giorni));
}
inline juce::var letta() { return juce::JSON::parse(fLicenza()); }

inline Modo modo() {
    const auto l = letta();
    const auto prod = l.getProperty("prodotto", "").toString();
    if (l.getProperty("chiave", "").toString().isNotEmpty()) {
        if (prod.containsIgnoreCase("Basic")) return Basic;
        return Pro;
    }
    return giorniProvaRimasti() > 0 ? Prova : Finita;
}

inline juce::var chiama(const juce::String& cosa, const juce::StringPairArray& par) {
    juce::URL u("https://api.lemonsqueezy.com/v1/licenses/" + cosa);
    for (auto& k : par.getAllKeys()) u = u.withParameter(k, par[k]);
    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                    .withConnectionTimeoutMs(12000)
                    .withExtraHeaders("Accept: application/json");
    std::unique_ptr<juce::InputStream> s(u.createInputStream(opts));
    if (s == nullptr) return {};
    return juce::JSON::parse(s->readEntireStreamAsString());
}

// SINCRONO (in un thread). true = attivata; msg = cosa dire all'utente.
inline bool attiva(const juce::String& chiave, juce::String& msg) {
    juce::StringPairArray p;
    p.set("license_key", chiave.trim());
    p.set("instance_name", juce::SystemStats::getComputerName() + " - OptiBob");
    const auto r = chiama("activate", p);
    if (r.isVoid()) { msg = "No connection: check the internet and try again."; return false; }
    const auto prod = r["meta"]["product_name"].toString();
    if (! (bool) r.getProperty("activated", false)) {
        const auto e = r.getProperty("error", "").toString();
        msg = e.containsIgnoreCase("limit") ? "This key is already active on 2 computers. Deactivate one from your order email, or write to us."
            : e.isNotEmpty() ? "Key not accepted: " + e : "Key not accepted.";
        return false;
    }
    if (! prod.containsIgnoreCase("OptiLive")) { msg = "This key is not for OptiLive."; return false; }
    auto* o = new juce::DynamicObject();
    o->setProperty("chiave", chiave.trim());
    o->setProperty("prodotto", prod);
    o->setProperty("istanza", r["instance"]["id"].toString());
    fLicenza().replaceWithText(juce::JSON::toString(juce::var(o)));
    msg = prod + " activated. Thank you!";
    return true;
}

// All'avvio: se la chiave e' scaduta o revocata la si toglie. Senza rete non si tocca niente.
inline void verifica() {
    const auto l = letta();
    const auto k = l.getProperty("chiave", "").toString();
    if (k.isEmpty()) return;
    juce::StringPairArray p;
    p.set("license_key", k);
    p.set("instance_id", l.getProperty("istanza", "").toString());
    const auto r = chiama("validate", p);
    if (r.isVoid()) return;
    const auto stato = r["license_key"]["status"].toString();
    if (! (bool) r.getProperty("valid", false) && (stato == "expired" || stato == "disabled" || r.getProperty("error", "").toString().isNotEmpty()))
        fLicenza().deleteFile();
}

} // namespace lic
