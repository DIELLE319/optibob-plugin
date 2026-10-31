#include "PluginEditor.h"

namespace {
const juce::Colour SFONDO { 0xff12151a }, CARTA { 0xfff7f7f5 },
                   INCHIOSTRO { 0xff111418 }, TENUE { 0xff5b6672 }, BLU { 0xff0b6fcb };
}

Pagina::Pagina(UnoProcessor& p, const std::vector<uno::Voce>& v, const juce::String& suff) {
    for (const auto& c : v) {
        auto m = std::make_unique<Man>();
        m->cursore.setSliderStyle(juce::Slider::LinearHorizontal);
        m->cursore.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        m->cursore.setColour(juce::Slider::trackColourId, BLU);
        m->cursore.setColour(juce::Slider::thumbColourId, BLU);
        addAndMakeVisible(m->cursore);
        m->voce = &c;
        m->nome.setColour(juce::Label::textColourId, INCHIOSTRO);
        m->nome.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        addAndMakeVisible(m->nome);
        // La spiegazione sta di FIANCO al nome, non sotto: il pannello e'
        // largo 680 e in altezza non ci sarebbe stata. Grigia e piccola,
        // cosi' si legge quando serve e sparisce quando non serve.
        m->spiega.setColour(juce::Label::textColourId, juce::Colour(0xff8b97a5));
        m->spiega.setFont(juce::Font(juce::FontOptions(10.5f)));
        addAndMakeVisible(m->spiega);
        m->valore.setColour(juce::Label::textColourId, BLU);
        m->valore.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        m->valore.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(m->valore);
        const juce::String unita = c.unita;
        auto* lab = &m->valore;
        m->cursore.onValueChange = [lab, &s = m->cursore, unita] {
            lab->setText(juce::String(s.getValue(), 2) + unita, juce::dontSendNotification); };
        m->aggancio = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            p.stato, juce::String(c.id) + suff, m->cursore);
        m->cursore.onValueChange();
        man.push_back(std::move(m));
    }
    lingua();
}

void Pagina::lingua() {
    for (auto& m : man) {
        if (m->voce == nullptr) continue;
        m->nome  .setText(uno::nomeDi (*m->voce), juce::dontSendNotification);
        m->spiega.setText(uno::aiutoDi(*m->voce), juce::dontSendNotification);
        m->cursore.setTooltip(uno::aiutoDi(*m->voce));
    }
}

void Pagina::resized() {
    auto r = getLocalBounds().reduced(10, 6);
    const int alt = juce::jmax(30, r.getHeight() / (int) man.size());
    for (auto& m : man) {
        auto cella = r.removeFromTop(alt);
        auto riga  = cella.removeFromTop(16);
        m->valore.setBounds(riga.removeFromRight(110));
        m->nome  .setBounds(riga.removeFromLeft(juce::jmin(190, riga.getWidth())));
        m->spiega.setBounds(riga);
        m->cursore.setBounds(cella.removeFromTop(14));
    }
}

Spie::Spie(UnoProcessor& p) : proc(p) { startTimerHz(20); }
void Spie::timerCallback() {
    bool mo = false;
    const float na = proc.agcDb.load(), nx = proc.maxDb.load();
    if (std::abs(na - a) > 0.05f) { a = na; mo = true; }
    if (std::abs(nx - x) > 0.05f) { x = nx; mo = true; }
    for (int i = 0; i < 5; ++i) { const float v = proc.mbDb[i].load();
        if (std::abs(v - m[i]) > 0.05f) { m[i] = v; mo = true; } }
    if (mo) repaint();
}
void Spie::paint(juce::Graphics& g) {
    // (proc serve anche qui: i riferimenti delle bande vengono dagli incroci)
    auto r = getLocalBounds().toFloat();
    g.setColour(SFONDO); g.fillRoundedRectangle(r, 10.0f);
    g.setColour(juce::Colour(0xffe8e6e1));
    g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
    g.drawText("OPTIBOB", 12, 6, 80, 16, juce::Justification::centredLeft);
    // Il numero di versione non serve a niente: quello che serve e' sapere se
    // quello che sta suonando e' la copia appena costruita. Data e ora del
    // compilatore, grandi e in chiaro, subito accanto al nome.
    g.setColour(juce::Colour(0xff7fd48a));
    g.drawText(juce::String("build ") + __DATE__ + " " + __TIME__,
               92, 6, 300, 16, juce::Justification::centredLeft);

    const float y0 = 30.0f, h = r.getHeight() - y0 - 18.0f;
    auto barra = [&](float x0, float w, double val, bool bidir, const char* et) {
        g.setColour(juce::Colour(0xff2a3038));
        g.fillRoundedRectangle(x0, y0, w, h, 3.0f);
        const float scala = h / (bidir ? 24.0f : 18.0f);
        if (bidir) {
            const float mez = y0 + h * 0.5f;
            const float alt = juce::jlimit(-h*0.5f, h*0.5f, (float) val * scala);
            g.setColour(val >= 0 ? juce::Colour(0xff5fc27e) : juce::Colour(0xffd8a13a));
            if (alt >= 0) g.fillRoundedRectangle(x0, mez - alt, w, alt, 3.0f);
            else          g.fillRoundedRectangle(x0, mez, w, -alt, 3.0f);
        } else {
            const float alt = juce::jlimit(0.0f, h, -(float) val * scala);
            g.setColour(juce::Colour(0xffd8a13a));
            g.fillRoundedRectangle(x0, y0, w, alt, 3.0f);
        }
        g.setColour(juce::Colour(0xff8b97a5));
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.drawText(et, (int) x0 - 14, (int)(y0 + h + 2), (int) w + 28, 14,
                   juce::Justification::centred);
    };
    const float W = r.getWidth() - 28.0f, u = W / 8.0f;
    barra(14.0f, u * 0.7f, a, true, "AGC");

    // i riferimenti delle bande, calcolati dagli incroci impostati adesso:
    // senza, non si sa quale colonna sia quale
    auto val = [this](const char* id) {
        return (double) proc.stato.getRawParameterValue(id)->load(); };
    const double lim[6] = { val("m_hpf"), val("m_xo1"), val("m_xo2"),
                            val("m_xo3"), val("m_xo4"), 16000.0 };
    auto corta = [](double f) {
        return f >= 1000.0 ? juce::String(f / 1000.0, 1) + "k"
                           : juce::String((int) (f + 0.5)); };
    for (int i = 0; i < 5; ++i) {
        barra(14.0f + u * (1.2f + (float) i), u * 0.7f, m[i], false,
              (corta(lim[i]) + "-" + corta(lim[i+1])).toRawUTF8());
    }
    barra(14.0f + u * 6.6f, u * 0.7f, x, false, "FINAL");
}

// ------------------------------------------------------------- Finestre
Finestre::Finestre(UnoProcessor& p) : proc(p) { startTimerHz(20); }

void Finestre::timerCallback() {
    bool m = false;
    for (int i = 0; i < 5; ++i) {
        const float a = proc.mbLv[i].load(), b = proc.mbDb[i].load();
        if (std::abs(a - lv[i]) > 0.3f || std::abs(b - gr[i]) > 0.1f) { lv[i]=a; gr[i]=b; m = true; }
    }
    if (m) repaint();
}

// la scala verticale: 0 dB in cima, -60 in fondo
float Finestre::yDaDb(double db) const {
    const float alto = 22.0f, basso = (float) getHeight() - 18.0f;
    return juce::jlimit(alto, basso, alto + (float) ((0.0 - db) / 60.0) * (basso - alto));
}
double Finestre::dbDaY(float y) const {
    const float alto = 22.0f, basso = (float) getHeight() - 18.0f;
    return juce::jlimit(-60.0, 0.0, -(double)((y - alto) / (basso - alto)) * 60.0);
}

void Finestre::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    g.setColour(SFONDO); g.fillRoundedRectangle(r, 10.0f);

    auto val = [this](const juce::String& id) {
        return (double) proc.stato.getRawParameterValue(id)->load(); };
    const double lim[6] = { val("m_hpf"), val("m_xo1"), val("m_xo2"),
                            val("m_xo3"), val("m_xo4"), 16000.0 };
    auto corta = [](double f) {
        return f >= 1000.0 ? juce::String(f / 1000.0, 1) + "k" : juce::String((int)(f + 0.5)); };

    // righe della scala
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    for (int db = 0; db >= -60; db -= 12) {
        const float y = yDaDb(db);
        g.setColour(juce::Colour(0xff232830));
        g.fillRect(34.0f, y, r.getWidth() - 44.0f, 1.0f);
        g.setColour(juce::Colour(0xff5b6672));
        g.drawText(juce::String(db), 4, (int) y - 7, 28, 14, juce::Justification::centredRight);
    }

    const float x0 = 38.0f, larg = (r.getWidth() - x0 - 10.0f) / 5.0f;
    for (int i = 0; i < 5; ++i) {
        const float x = x0 + i * larg + 4.0f, w = larg - 8.0f;
        const double minimo = val("m_sog" + juce::String(i + 1));
        const double massimo = val("m_tet" + juce::String(i + 1));
        const float yMin = yDaDb(minimo), yMax = yDaDb(massimo);

        // la finestra: fra minimo e massimo e' dove la banda puo' stare
        g.setColour(juce::Colour(0x22ffffff));
        g.fillRect(x, yMax, w, yMin - yMax);

        // Il livello della banda, e cosa le sta succedendo.
        //   grigio  = il livello che entra
        //   VERDE   = la banda viene alzata (guadagno positivo)
        //   ROSSO   = la banda viene abbassata
        // Cosi' si vede a colpo d'occhio chi sta tirando su e chi sta domando.
        const float basso = (float) getHeight() - 18.0f;
        const float yIn  = yDaDb(lv[i]);
        const float yOut = yDaDb(lv[i] + gr[i]);

        g.setColour(juce::Colour(0xff39424d));                 // il segnale in ingresso
        g.fillRect(x + 6.0f, yIn, w - 12.0f, basso - yIn);

        if (gr[i] > 0.05f) {                                    // sta guadagnando
            g.setColour(juce::Colour(0xff4ecb7a));
            g.fillRect(x + 6.0f, yOut, w - 12.0f, yIn - yOut);
        } else if (gr[i] < -0.05f) {                            // sta abbassando
            g.setColour(juce::Colour(0xffd4574f));
            g.fillRect(x + 6.0f, yIn, w - 12.0f, yOut - yIn);
        }

        // quanti dB, scritti: verde se alza, rosso se abbassa
        g.setColour(gr[i] > 0.05f ? juce::Colour(0xff4ecb7a)
                  : gr[i] < -0.05f ? juce::Colour(0xffd4574f)
                                   : juce::Colour(0xff5b6672));
        g.setFont(juce::Font(juce::FontOptions(10.5f)).boldened());
        g.drawText((gr[i] > 0 ? "+" : "") + juce::String(gr[i], 1),
                   (int) x, (int) basso - 14, (int) w, 13, juce::Justification::centred);

        // i due cursori, col valore scritto sopra: si legge mentre si trascina
        g.setColour(juce::Colour(0xffe8e6e1));
        g.fillRect(x, yMax - 1.5f, w, 3.0f);
        g.setFont(juce::Font(juce::FontOptions(9.5f)).boldened());
        g.drawText(juce::String(massimo, 0), (int) x, (int) yMax - 13, (int) w, 12,
                   juce::Justification::centred);
        g.setColour(juce::Colour(0xff0b6fcb));
        g.fillRect(x, yMin - 1.5f, w, 3.0f);
        g.setColour(juce::Colour(0xff7fb6f0));
        g.drawText(juce::String(minimo, 0), (int) x, (int) yMin + 2, (int) w, 12,
                   juce::Justification::centred);

        g.setColour(juce::Colour(0xff8b97a5));
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.drawText(corta(lim[i]) + "-" + corta(lim[i+1]),
                   (int) x, (int) r.getHeight() - 15, (int) w, 12, juce::Justification::centred);
    }
    g.setColour(juce::Colour(0xff8b97a5));
    g.setFont(juce::Font(juce::FontOptions(9.5f)));
    g.drawText("bianco = massimo   ·   blu = minimo   ·   verde = sta alzando   ·   rosso = sta abbassando",
               38, 6, (int) r.getWidth() - 48, 14, juce::Justification::centredLeft);
}

void Finestre::mouseDown(const juce::MouseEvent& ev) {
    const float x0 = 38.0f, larg = ((float) getWidth() - x0 - 10.0f) / 5.0f;
    const int b = juce::jlimit(0, 4, (int) (((float) ev.x - x0) / larg));
    const double minimo = (double) proc.stato.getRawParameterValue("m_sog" + juce::String(b + 1))->load();
    const double massimo = (double) proc.stato.getRawParameterValue("m_tet" + juce::String(b + 1))->load();
    bandaPresa = b;
    qualeCursore = (std::abs((float) ev.y - yDaDb(massimo)) < std::abs((float) ev.y - yDaDb(minimo))) ? 1 : 0;
    muovi(ev);
}

void Finestre::mouseDrag(const juce::MouseEvent& ev) { muovi(ev); }

void Finestre::muovi(const juce::MouseEvent& ev) {
    if (bandaPresa < 0) return;
    const juce::String id = (qualeCursore == 1 ? "m_tet" : "m_sog") + juce::String(bandaPresa + 1);
    if (auto* p = proc.stato.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1((float) dbDaY((float) ev.y)));
    repaint();
}

// ------------------------------------------------------------- Incroci
Incroci::Incroci(UnoProcessor& p) : proc(p) {}

float Incroci::xDaHz(double hz) const {
    const double a = std::log10(30.0), b = std::log10(16000.0);
    return (float) ((std::log10(juce::jlimit(30.0, 16000.0, hz)) - a) / (b - a)) * (float) getWidth();
}
double Incroci::hzDaX(float x) const {
    const double a = std::log10(30.0), b = std::log10(16000.0);
    return std::pow(10.0, a + (double)(juce::jlimit(0.0f, (float) getWidth(), x) / getWidth()) * (b - a));
}

void Incroci::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    g.setColour(SFONDO); g.fillRoundedRectangle(r, 8.0f);
    auto val = [this](const char* id) {
        return (double) proc.stato.getRawParameterValue(id)->load(); };

    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 }) {
        const float x = xDaHz(f);
        g.setColour(juce::Colour(0xff232830));
        g.fillRect(x, 0.0f, 1.0f, r.getHeight());
        g.setColour(juce::Colour(0xff5b6672));
        g.drawText(f >= 1000 ? juce::String(f/1000.0, 0) + "k" : juce::String((int) f),
                   (int) x - 18, (int) r.getHeight() - 13, 36, 12, juce::Justification::centred);
    }
    // le curve delle cinque bande: si vede dove si intersecano
    const double bordi[6] = { 30.0, val("m_xo1"), val("m_xo2"), val("m_xo3"), val("m_xo4"), 22000.0 };
    const juce::Colour tinte[5] = { juce::Colour(0xff4b8fd4), juce::Colour(0xff5fc27e),
                                    juce::Colour(0xffd8c93a), juce::Colour(0xffd8a13a),
                                    juce::Colour(0xffd46a6a) };
    for (int b = 0; b < 5; ++b) {
        juce::Path pth;
        for (int px = 0; px <= (int) r.getWidth(); ++px) {
            const double f = hzDaX((float) px);
            const double lo = bordi[b], hi = bordi[b+1];
            const double hp = (b == 0) ? 1.0 : std::pow(f/lo,4.0) / (1.0 + std::pow(f/lo,4.0));
            const double lp = (b == 4) ? 1.0 : 1.0 / (1.0 + std::pow(f/hi,4.0));
            const double a = hp * lp;
            const float y = r.getHeight() - 14.0f - (float) a * (r.getHeight() - 20.0f);
            if (px == 0) pth.startNewSubPath((float) px, y); else pth.lineTo((float) px, y);
        }
        g.setColour(tinte[b].withAlpha(0.85f));
        g.strokePath(pth, juce::PathStrokeType(1.4f));
    }

    const char* ids[4] = { "m_xo1", "m_xo2", "m_xo3", "m_xo4" };
    for (int i = 0; i < 4; ++i) {
        const double f = val(ids[i]);
        const float x = xDaHz(f);
        g.setColour(juce::Colour(0xff0b6fcb));
        g.fillRect(x - 1.0f, 2.0f, 2.0f, r.getHeight() - 16.0f);
        g.setColour(juce::Colour(0xffe8e6e1));
        g.setFont(juce::Font(juce::FontOptions(9.5f)).boldened());
        g.drawText(f >= 1000 ? juce::String(f/1000.0, 2) + "k" : juce::String((int)(f+0.5)),
                   (int) x - 26, 2, 52, 12, juce::Justification::centred);
    }
}

void Incroci::mouseDown(const juce::MouseEvent& ev) {
    const char* ids[4] = { "m_xo1", "m_xo2", "m_xo3", "m_xo4" };
    float d = 1e9f; preso = -1;
    for (int i = 0; i < 4; ++i) {
        const float x = xDaHz((double) proc.stato.getRawParameterValue(ids[i])->load());
        if (std::abs((float) ev.x - x) < d) { d = std::abs((float) ev.x - x); preso = i; }
    }
    muovi(ev);
}
void Incroci::mouseDrag(const juce::MouseEvent& ev) { muovi(ev); }
void Incroci::muovi(const juce::MouseEvent& ev) {
    if (preso < 0) return;
    const char* ids[4] = { "m_xo1", "m_xo2", "m_xo3", "m_xo4" };
    if (auto* p = proc.stato.getParameter(ids[preso]))
        p->setValueNotifyingHost(p->convertTo0to1((float) hzDaX((float) ev.x)));
    repaint();
    if (auto* g = getParentComponent()) g->repaint();
}

UnoEditor::UnoEditor(UnoProcessor& p)
    : AudioProcessorEditor(&p), proc(p), spie(p), finestre(p), incroci(p) {
    addAndMakeVisible(spie);
    addAndMakeVisible(finestre);
    addAndMakeVisible(incroci);

    auto agg1 = std::make_unique<Pagina>(p, uno::vociAgc());
    schede.addTab("AGC", CARTA, agg1.get(), false);
    pagine.push_back(std::move(agg1));

    for (int b = 0; b < 5; ++b) {
        auto pg = std::make_unique<Pagina>(p, uno::vociBanda(), juce::String(b + 1));
        schede.addTab("BAND " + juce::String(b + 1), CARTA, pg.get(), false);
        pagine.push_back(std::move(pg));
    }
    auto glo = std::make_unique<Pagina>(p, uno::vociMbGlob());
    schede.addTab("BANDS: setup", CARTA, glo.get(), false);
    pagine.push_back(std::move(glo));

    auto tos = std::make_unique<Pagina>(p, uno::vociTosa());
    schede.addTab("CLIPPING", CARTA, tos.get(), false);
    pagine.push_back(std::move(tos));
    auto fin = std::make_unique<Pagina>(p, uno::vociMax());
    schede.addTab("FINAL", CARTA, fin.get(), false);
    pagine.push_back(std::move(fin));

    schede.setOutline(0);
    addAndMakeVisible(schede);

    const char* ids[3] = { "a_on", "m_on", "x_on" };
    const char* eti[3] = { "AGC", "multiband", "final" };
    for (int i = 0; i < 3; ++i) {
        acc[i].setButtonText(eti[i]);
        acc[i].setColour(juce::ToggleButton::textColourId, INCHIOSTRO);
        acc[i].setColour(juce::ToggleButton::tickColourId, BLU);
        acc[i].setTooltip("Turns the whole section on or off, to compare by ear.");
        addAndMakeVisible(acc[i]);
        agg[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            p.stato, ids[i], acc[i]);
    }

    for (auto* b : { &salva, &carica }) {
        b->setColour(juce::TextButton::buttonColourId, CARTA);
        b->setColour(juce::TextButton::textColourOffId, INCHIOSTRO);
        addAndMakeVisible(*b);
    }
    salva.onClick = [this] {
        scelta = std::make_unique<juce::FileChooser>("Where do I save it?",
            juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                .getChildFile("OptiBob.json"), "*.json");
        scelta->launchAsync(juce::FileBrowserComponent::saveMode
                          | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f != juce::File{}) f.replaceWithText(proc.presetInJson());
            });
    };
    carica.onClick = [this] {
        scelta = std::make_unique<juce::FileChooser>("Which preset?",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                .getChildFile("CLAUDE/OPTIBOB/STATUS-CORRENTE"), "*.json");
        scelta->launchAsync(juce::FileBrowserComponent::openMode
                          | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f.existsAsFile() && proc.caricaPreset(f.loadFileAsString())) repaint();
            });
    };

    blocca.setColour(juce::ToggleButton::textColourId, INCHIOSTRO);
    blocca.setColour(juce::ToggleButton::tickColourId, BLU);
    blocca.setTooltip("Above the minimum, each band is brought UP to its ceiling and kept "
                      "there. Not 'do not exceed' but 'stay there': it is what makes radio "
                      "sound the same from record to record.");
    addAndMakeVisible(blocca);
    aggBlocca = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.stato, "m_lock", blocca);

    // IT / EN. Il prodotto esce in inglese, ma chi lo tara lavora in
    // italiano: il tasto dice la lingua verso cui si va, non quella corrente.
    addAndMakeVisible(lingua);
    lingua.onClick = [this] { uno::italiano() = ! uno::italiano(); applicaLingua(); };
    applicaLingua();

    setSize(680, 930);
    setResizable(true, true);
    setResizeLimits(600, 700, 1200, 1300);
}

void UnoEditor::applicaLingua() {
    const bool it = uno::italiano();
    lingua.setButtonText(it ? "EN" : "IT");
    salva .setButtonText(it ? "salva preset" : "save preset");
    carica.setButtonText(it ? "carica preset" : "load preset");
    blocca.setButtonText(it ? "tieni le bande al tetto  (il suono da radio: sempre uguale)"
                            : "hold bands at ceiling  (the radio sound: always the same)");
    blocca.setTooltip(it ? "Sopra il minimo, ogni banda viene portata SU al suo tetto e tenuta "
                           "li'. Non 'non superare' ma 'stai li'': e' quello che fa suonare la "
                           "radio sempre uguale da un disco all'altro."
                         : "Above the minimum, each band is brought UP to its ceiling and kept "
                           "there. Not 'do not exceed' but 'stay there': it is what makes radio "
                           "sound the same from record to record.");
    for (auto& pg : pagine) pg->lingua();
    schede.setTabName(0, "AGC");
    for (int b = 0; b < 5; ++b)
        schede.setTabName(1 + b, (it ? "BANDA " : "BAND ") + juce::String(b + 1));
    schede.setTabName(6, it ? "BANDE: assetto" : "BANDS: setup");
    schede.setTabName(7, "CLIPPING");
    schede.setTabName(8, it ? "FINALE" : "FINAL");
    repaint();
}

void UnoEditor::paint(juce::Graphics& g) {
    g.fillAll(CARTA);
    g.setColour(TENUE);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText("preset: " + proc.nomePreset, 16, getHeight() - 18, 500, 14,
               juce::Justification::centredLeft);
}

void UnoEditor::resized() {
    auto r = getLocalBounds().reduced(14);
    spie.setBounds(r.removeFromTop(120));
    r.removeFromTop(8);
    finestre.setBounds(r.removeFromTop(190));
    r.removeFromTop(4);
    incroci.setBounds(r.removeFromTop(64));
    r.removeFromTop(8);
    auto tasti = r.removeFromTop(26);
    salva.setBounds(tasti.removeFromLeft(140).reduced(2));
    carica.setBounds(tasti.removeFromLeft(140).reduced(2));
    lingua.setBounds(tasti.removeFromRight(52).reduced(2));
    r.removeFromTop(4);
    blocca.setBounds(r.removeFromTop(22));
    r.removeFromTop(4);
    auto fila = r.removeFromTop(24);
    const int w = fila.getWidth() / 3;
    for (int i = 0; i < 3; ++i) acc[i].setBounds(fila.removeFromLeft(w));
    r.removeFromTop(6);
    r.removeFromBottom(18);
    schede.setBounds(r);
}
