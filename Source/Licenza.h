#pragma once
#include <juce_core/juce_core.h>

// I blocchi Basic/Pro + la prova di 7 giorni.
//  - Prova: si salva la data del primo avvio in ~/Library/Application Support/OptiBob.
//  - Pro:   si incolla la license key di Lemon Squeezy, si valida online, si salva.
//  - Sbloccato (bande + incroci) = Pro OPPURE prova ancora valida.
namespace lic {

inline juce::File cartella() {
    auto d = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                 .getChildFile("OptiBob");
    d.createDirectory();
    return d;
}
inline juce::File fProva()   { return cartella().getChildFile("prova.txt"); }
inline juce::File fLicenza() { return cartella().getChildFile("licenza.txt"); }

inline void segnaPrimoAvvio() {
    auto f = fProva();
    if (! f.existsAsFile())
        f.replaceWithText(juce::String(juce::Time::getCurrentTime().toMilliseconds()));
}
inline int giorniProvaRimasti() {
    auto f = fProva();
    if (! f.existsAsFile()) return 7;
    auto t0 = f.loadFileAsString().trim().getLargeIntValue();
    auto giorni = (juce::Time::getCurrentTime().toMilliseconds() - t0) / (1000.0 * 60 * 60 * 24);
    int r = 7 - (int) giorni;
    return r < 0 ? 0 : r;
}
inline bool haPro() {
    auto f = fLicenza();
    return f.existsAsFile() && f.loadFileAsString().trim().isNotEmpty();
}

enum class Modo { Pro, Prova, Bloccato };
inline Modo modo() {
    if (haPro()) return Modo::Pro;
    return giorniProvaRimasti() > 0 ? Modo::Prova : Modo::Bloccato;
}
inline bool sbloccato() { auto m = modo(); return m == Modo::Pro || m == Modo::Prova; }

// Valida la chiave su Lemon Squeezy. SINCRONO: chiamalo in un thread, non sul message thread.
inline bool validaChiave(const juce::String& chiave) {
    juce::URL u("https://api.lemonsqueezy.com/v1/licenses/validate");
    u = u.withParameter("license_key", chiave.trim());
    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                    .withConnectionTimeoutMs(12000)
                    .withExtraHeaders("Accept: application/json");
    std::unique_ptr<juce::InputStream> s(u.createInputStream(opts));
    if (s == nullptr) return false;
    auto risposta = s->readEntireStreamAsString();
    auto j = juce::JSON::parse(risposta);
    return (bool) j.getProperty("valid", false);
}
inline void salvaPro(const juce::String& chiave) { fLicenza().replaceWithText(chiave.trim()); }

} // namespace lic
