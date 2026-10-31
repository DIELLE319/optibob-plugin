struct Catena {
    oeq::Eq   equalizzatore;      // davanti a tutto
    agc::Agc  livellatore;
    omb::Mb   multibanda;
    omax::Max finale;
    oeq::Params pe; agc::Params pa; omb::Params pm; omax::Params px;
    // ⚠️ 14/08/2026 — I VALORI STANNO QUI, non nei tre Params.
    // Prima il ponte sapeva solo caricare un preset intero. Per avere le
    // manopole del Plus serve poter toccare UN comando alla volta, e per
    // farlo serve un posto solo dove sta la verita': questo dizionario,
    // con le stesse chiavi del plugin (a_ m_ x_). I tre Params si
    // ricostruiscono da qui ogni volta -- costa niente e non si disallinea.
    std::map<std::string,double> val;
    double gPre_ = 1.0;          // la preamplificazione, gia' in lineare

    // ⭐⭐ 18/09/2026 — IL CANCELLO DI PROGRAMMA.
    // Luca: «quando non c'e' sufficiente volume tira su tutto, fruscio e
    // rumori di fondo». Misurato al banco su un passaggio quieto a -55 dBFS:
    // musica e silenzio distavano 41.7 dB in ingresso e **1.3 dB in uscita**.
    // Il fruscio usciva forte quanto la musica.
    //
    // Perche' i cancelli che c'erano non lo prendevano: stanno tutti DOPO i
    // guadagni. Il preamplificatore a +40 porta un fruscio da -55 a -15, e
    // a quel punto nessuna soglia lo riconosce piu' come silenzio -- infatti
    // muovere `a_gate` da -45 a -20 dava un'uscita IDENTICA. E le bande, sotto
    // la loro soglia, RILASCIANO: la banda 2 passava da -14.3 a -0.4, cioe'
    // quattordici dB regalati al fruscio.
    //
    // Questo guarda l'ingresso GREZZO, prima di ogni guadagno, e sotto la
    // soglia congela tutto quello che alza: Porta, AGC e bande.
    // La finestra corta conta piu' della soglia: a 300 ms il cancello arriva
    // quando i guadagni sono gia' saliti (distanza 8.9 dB), a 60 ms li prende
    // in tempo (15.4 dB). Sulla musica vera non interviene mai: verificato,
    // uscita identica campione per campione.
    // ⚠️ 19/09 — si parte da 1.0, cioe' 0 dB, non da 1e-9. Partendo dal
    // silenzio assoluto il pavimento si chiudeva al PRIMO blocco, prima
    // ancora che arrivasse un campione di musica, e la catena nasceva
    // congelata. Si parte aperti: chiudere e' una decisione che si prende
    // dopo aver ascoltato, non prima.
    double livIn_ = 1.0;
    bool   chiuso_ = false;
    // ⚠️ 19/09 — letti in applica(), non nel blocco audio: q() costruisce una
    // std::string e cerca in una mappa, e allocare dentro il filo dell'audio
    // e' proprio quello che non si fa.
    double cancDb_ = -200.0, cancMs_ = 60.0;
    bool   finOn_ = false;
    double finMin_ = -20.0, finMax_ = -20.0, finVel_ = 1.0;
    void guardaIngresso(const float* L, const float* R, int n, double sogliaDb, double finestraMs) {
        double e = 0.0;
        for (int i = 0; i < n; ++i) { const double m = 0.5 * (L[i] + R[i]); e += m * m; }
        e /= std::max(1, n);
        const double k = 1.0 - std::exp(-(double) n / (std::max(0.005, finestraMs * 0.001) * sr_));
        livIn_ += (e - livIn_) * k;
        const double db = 10.0 * std::log10(std::max(livIn_, 1e-20));
        if (chiuso_) { if (db > sogliaDb + 3.0) chiuso_ = false; }   // isteresi 3 dB
        else         { if (db < sogliaDb)       chiuso_ = true;  }
    }

    // ⭐⭐ 04/09 — Luca: «bisogna fare un limitatore, un'applicazione a monte
    // che regola il livello prendendosi un secondo di ritardo e poi entra
    // nell'Opti». L'idea e' giusta, il posto no: un'altra app vorrebbe dire
    // dirottare l'audio del Mac dentro un cavo virtuale, e sarebbe fragile.
    // Il pezzo invece serve, ed e' questo: LA PORTA. Ritarda il suono di
    // quanto le dici, e intanto GUARDA AVANTI -- conosce il picco del pezzo
    // di suono che deve ancora uscire, quindi mette il guadagno giusto
    // PRIMA che arrivi. Non insegue e non sfonda mai: la catena riceve
    // sempre il livello per cui e' tarata, -12 dBFS sui picchi.
    // Non e' un compressore: il guadagno si muove piano e uguale su tutto,
    // quindi non tocca la dinamica -- sposta solo il punto di lavoro.
    struct Porta {
        std::vector<float> bufL, bufR;
        std::vector<float> picchi;        // il massimo di ogni blocchetto
        int nBlocchi = 0, blocco = 0, scritti = 0, pos = 0;
        double g = 1.0, sr = 48000.0;
        static const int K = 128;         // campioni per blocchetto

        void prepara(double s) { sr = s; imposta(0.0); }
        void imposta(double ms) {
            const int n = (int) (std::max(0.0, ms) * 0.001 * sr);
            if ((int) bufL.size() == n) return;
            bufL.assign(std::max(1, n), 0.0f);
            bufR.assign(std::max(1, n), 0.0f);
            nBlocchi = std::max(1, (n + K - 1) / K);
            picchi.assign(nBlocchi, 0.0f);
            blocco = scritti = pos = 0;
        }
        bool accesa() const { return bufL.size() > 1; }

        // bersaglio in dB, velocita' in dB/s
        void passa(float* L, float* R, int n, double bersaglioDb, double dbS, bool tieni = false) {
            if (!accesa()) return;
            const int cap = (int) bufL.size();
            for (int i = 0; i < n; ++i) {
                const float dl = bufL[pos], dr = bufR[pos];
                bufL[pos] = L[i]; bufR[pos] = R[i];
                const float a = std::max(std::fabs(L[i]), std::fabs(R[i]));
                if (a > picchi[blocco]) picchi[blocco] = a;
                if (++scritti >= K) {
                    scritti = 0;
                    blocco = (blocco + 1) % nBlocchi;
                    picchi[blocco] = 0.0f;
                }
                if (++pos >= cap) pos = 0;
                L[i] = (float) (dl * g);
                R[i] = (float) (dr * g);
            }
            // il picco di TUTTO quello che sta in canna
            float pic = 0.0f;
            for (int b = 0; b < nBlocchi; ++b) if (picchi[b] > pic) pic = picchi[b];
            if (pic < 1e-6f) return;                   // silenzio: si sta fermi
            const double picDb = 20.0 * std::log10((double) pic);
            const double voluto = std::pow(10.0, (bersaglioDb - picDb) * 0.05);
            const double passo = std::pow(10.0, (dbS * n / sr) * 0.05);
            if (tieni) return;                                // cancello chiuso: si sta fermi
            if (voluto > g) g = std::min(voluto, g * passo);   // su: piano, col passo
            else            g = voluto;                       // giu: subito, il ritardo copre
            g = std::min(100.0, std::max(0.06, g));    // +40 / -24 dB
        }
    } porta_;

    // ⭐⭐ 19/09/2026 — LA FINESTRA DEL PROGRAMMA.
    // Luca: «io voglio un livello minimo e un livello max sull'input».
    // Aveva ragione, e l'ha avuta prima della misura. Quattro dischi diversi,
    // quattordici dB di scarto in ingresso, misurati in uscita come li sente
    // l'orecchio:
    //
    //     AGC acceso (com'era)   2.92 dB di scarto
    //     AGC spento             1.21
    //     questa finestra        0.49
    //
    // Perche' funziona dove l'AGC falliva: misura la LOUDNESS (pesata K, come
    // la BS.1770), non l'energia. Due dischi con la stessa energia e spettro
    // diverso non suonano uguali -- ed era esattamente quello che ci sfuggiva,
    // perche' il livellatore inseguiva un numero che non e' quello che senti.
    //
    // E va LENTA: 1 dB/s da' 0.49, 3 dB/s da' 0.63, perche' sopra il decibel
    // al secondo comincia a inseguire dentro al brano invece di spostare il
    // punto di lavoro fra un disco e l'altro. Livellare non e' inseguire.
    struct Finestra {
        double b1[3], a1[3], b2[3], a2[3];
        double x1a=0,x2a=0,y1a=0,y2a=0, x1b=0,x2b=0,y1b=0,y2b=0;
        double env = 1e-9, g = 0.0, sr = 48000.0;
        void prepara(double s) {
            sr = s;
            double f0=1681.97, G=3.9998, Q=0.7071;
            double K=std::tan(M_PI*f0/sr), Vh=std::pow(10.0,G/20.0), Vb=std::pow(Vh,0.4996);
            double a0=1+K/Q+K*K;
            b1[0]=(Vh+Vb*K/Q+K*K)/a0; b1[1]=2*(K*K-Vh)/a0; b1[2]=(Vh-Vb*K/Q+K*K)/a0;
            a1[0]=1; a1[1]=2*(K*K-1)/a0; a1[2]=(1-K/Q+K*K)/a0;
            f0=38.13; Q=0.5003; K=std::tan(M_PI*f0/sr); a0=1+K/Q+K*K;
            b2[0]=1/a0; b2[1]=-2/a0; b2[2]=1/a0;
            a2[0]=1; a2[1]=2*(K*K-1)/a0; a2[2]=(1-K/Q+K*K)/a0;
            x1a=x2a=y1a=y2a=x1b=x2b=y1b=y2b=0.0; env=1e-9; g=0.0;
        }
        void passa(float* L, float* R, int n, double minDb, double maxDb, double dbS) {
            double acc = 0.0;
            for (int i = 0; i < n; ++i) {
                const double x = 0.5 * ((double) L[i] + (double) R[i]);
                const double y = b1[0]*x + b1[1]*x1a + b1[2]*x2a - a1[1]*y1a - a1[2]*y2a;
                x2a=x1a; x1a=x; y2a=y1a; y1a=y;
                const double z = b2[0]*y + b2[1]*x1b + b2[2]*x2b - a2[1]*y1b - a2[2]*y2b;
                x2b=x1b; x1b=y; y2b=y1b; y1b=z;
                acc += z*z;
            }
            acc /= std::max(1, n);
            const double k = 1.0 - std::exp(-(double) n / (0.400 * sr));   // 400 ms
            env += (acc - env) * k;
            const double lv = -0.691 + 10.0*std::log10(std::max(env, 1e-12));
            double voluto = g;
            if      (lv + g < minDb) voluto = minDb - lv;   // sotto la finestra: alza
            else if (lv + g > maxDb) voluto = maxDb - lv;   // sopra: abbassa
            if (lv < -60.0) voluto = g;                     // silenzio: non si insegue
            const double passo = dbS * n / sr;
            g += std::min(std::max(voluto - g, -passo), passo);
            g  = std::min(24.0, std::max(-40.0, g));
            const double lin = std::pow(10.0, g * 0.05);
            for (int i = 0; i < n; ++i) { L[i] = (float)(L[i]*lin); R[i] = (float)(R[i]*lin); }
        }
        double db() const { return g; }
    } finestra_;
    double portaGuadagnoDb() const {
        return porta_.accesa() ? 20.0 * std::log10(std::max(porta_.g, 1e-6)) : 0.0;
    }
    double q(const std::string& k, double def) const {
        auto i = val.find(k); return (i == val.end()) ? def : i->second;
    }
    void applica();
    std::atomic<bool> acceso { true };
    std::atomic<float> spiaAgc { 0 }, spiaBanda[5] { 0,0,0,0,0 }, spiaFin { 0 }, spiaTosa { 0 };
    std::atomic<float> spiaLiv[5] { -100,-100,-100,-100,-100 };
    std::atomic<float> spiaUsc[5] { -100,-100,-100,-100,-100 };

    double sr_ = 48000.0;
    void prepara(double sr) {
        sr_ = (sr > 0.0) ? sr : 48000.0;
        porta_.prepara(sr_);
        finestra_.prepara(sr_);
        equalizzatore.prepare(sr); livellatore.prepare(sr); multibanda.prepare(sr);
        finale.prepare(sr, 1024);
        equalizzatore.setParams(pe); livellatore.setParams(pa);
        multibanda.setParams(pm); finale.setParams(px);
    }
    // ⚠️ 16/08/2026 — IL CONFRONTO A PARI VOLUME.
    // Segnalato da Luca: «spento e' molto piu' alto di volume, acceso lo
    // abbassa molto, ho sbagliato?». No: la catena lascia per strada dei dB
    // e non c'e' nessun recupero. Ma il guaio piu' grosso e' che con una
    // differenza di volume NON SI PUO' GIUDICARE: piu' forte suona sempre
    // meglio, anche quando e' peggio.
    // Quindi: mentre lavoriamo teniamo il conto di quanto stiamo
    // guadagnando o perdendo, e quando si spegne l'interruttore applichiamo
    // lo stesso identico guadagno al segnale pulito. Cosi' l'interruttore
    // confronta il SUONO, non il volume.
    double eIn_ = 0.0, eOut_ = 0.0, comp_ = 1.0;
    // ⚠️⚠️ 17/08/2026 — IL BACO PIU' GRAVE DI TUTTI.
    // Segnalato da Luca: «se spegni AGC, bande e finale, acceso e spento
    // devono essere IDENTICI, altrimenti e' tutto sbagliato». Aveva ragione:
    // il plugin controllava a_on / m_on / x_on, il ponte dell'app NO. Il
    // segnale passava dentro tutti e tre gli stadi comunque, sempre. Quindi
    // ogni giudizio a orecchio fatto con gli interruttori era falsato.
    std::atomic<bool> onEq { true }, onAgc { true }, onMb { true }, onMax { true };
    std::atomic<bool> inCoda { false };

    // ⭐ 18/08 — I METRI D'INGRESSO E D'USCITA.
    // Luca: «manca livello di ingresso e livello di uscita, che puoi mettere
    // agli estremi della app». Servono anche a una cosa che ci ha gia' fatto
    // sbagliare: senza vederli, acceso e spento si confrontano a volumi
    // diversi e si conclude il contrario di quello che e' vero.
    // Picco per canale, con la riga che resta tre secondi, piu' un RMS lento
    // per giudicare il volume percepito invece dei picchi.
    void misura(const float* L, const float* R, int n, bool ingresso) {
        double pl = 0, pr = 0, e = 0;
        for (int i = 0; i < n; ++i) {
            const double a = std::abs((double) L[i]), b = std::abs((double) R[i]);
            if (a > pl) pl = a;
            if (b > pr) pr = b;
            e += a * a + b * b;
        }
        const double rms = std::sqrt(e / std::max(1, 2 * n));
        double* pk   = ingresso ? picIn_  : picOut_;
        double* ten  = ingresso ? tenIn_  : tenOut_;
        long long* q = ingresso ? scadIn_ : scadOut_;
        double* rm   = ingresso ? &rmsIn_ : &rmsOut_;
        const double p[2] = { pl, pr };
        for (int c = 0; c < 2; ++c) {
            if (p[c] > pk[c]) pk[c] = p[c]; else pk[c] += (p[c] - pk[c]) * 0.25;
            if (p[c] >= ten[c]) { ten[c] = p[c]; q[c] = (long long)(sr_ * 3.0); }
            else { q[c] -= n; if (q[c] <= 0) ten[c] += (p[c] - ten[c]) * 0.05; }
        }
        *rm += (rms - *rm) * 0.06;
        std::atomic<float>* d = ingresso ? mIn : mOut;
        d[0].store((float) lin2db(pk[0]));  d[1].store((float) lin2db(pk[1]));
        d[2].store((float) lin2db(ten[0])); d[3].store((float) lin2db(ten[1]));
        d[4].store((float) lin2db(*rm));
    }
    static double lin2db(double v) { return 20.0 * std::log10(std::max(v, 1e-7)); }

    void elabora(float* L, float* R, int n) {
        misura(L, R, n, true);
        if (!acceso.load()) {
            // spento: si applica il guadagno medio che la catena avrebbe
            // dato, cosi' il confronto e' onesto
            const float g = (float) comp_;
            if (std::abs(g - 1.0f) > 1e-4f)
                for (int i = 0; i < n; ++i) { L[i] *= g; R[i] *= g; }
            misura(L, R, n, false);
            return;
        }
        // Se sono spenti tutti e tre non si tocca NIENTE: nessun guadagno,
        // nessun filtro, nessun ritardo. Bit per bit come e' entrato.
        const bool a = onAgc.load(), b = onMb.load(), x = onMax.load();
        if (!a && !b && !x && !onEq.load()) {
            comp_ = 1.0; eIn_ = eOut_ = 0.0; misura(L, R, n, false); return;
        }

        double eI = 0.0;
        for (int i = 0; i < n; ++i) eI += (double) L[i] * L[i] + (double) R[i] * R[i];

        // ⭐ la porta: guarda avanti e mette il livello giusto
        guardaIngresso(L, R, n, cancDb_, cancMs_);
        porta_.passa(L, R, n, q("g_pbers", -12.0), q("g_pvel", 3.0), chiuso_);
        // la finestra del programma: minimo e massimo, e in mezzo non tocca
        if (finOn_ && !chiuso_) finestra_.passa(L, R, n, finMin_, finMax_, finVel_);
        // ⭐ la preamplificazione a mano: prima di ogni altra cosa
        if (gPre_ > 1.0001 || gPre_ < 0.9999) {
            for (int i = 0; i < n; ++i) { L[i] *= (float) gPre_; R[i] *= (float) gPre_; }
        }
        // ⚠️⚠️ 19/09/2026 — QUI CHIAMAVO setParams() SUL FILO DELL'AUDIO.
        // Luca: «non funziona audio, ho acceso ma non arriva segnale
        // all'apollo». Per congelare il livellatore spostavo p_.gateDb e
        // richiamavo setParams(), che rifa' TUTTI i coefficienti -- dentro
        // il blocco audio, che ha qualche millesimo di tempo e basta. E'
        // lo stesso errore contro cui questo file mette in guardia dal
        // 18/08, fatto di nuovo. Un blocco che sfora non da' errore: da'
        // silenzio. Adesso sono due interruttori e nient'altro.
        livellatore.tieni_ = chiuso_;
        multibanda.tieni_  = chiuso_;
        if (onEq.load()) equalizzatore.process(L, R, n);
        if (a) livellatore.process(L, R, n);
        if (b) multibanda.process(L, R, n);
        if (x) finale.process(L, R, n);
        // ⭐⭐ 05/10/2026 — LA RETE D'ACCIAIO.
        // Fondo scala e' fondo scala, con qualunque taratura. Misurato stasera:
        // alzando il muro di banda l'uscita arrivava a 1.41 lineare (+3 dB
        // oltre lo zero) e nessuno la fermava -- il limitatore di sicurezza e'
        // spento fuori dal modo RADIO, i due clipper hanno la soglia relativa
        // al picco del programma e il muro di banda e' relativo al tetto della
        // BANDA, non all'uscita. Quei campioni arrivavano al driver cosi' come
        // sono, e li tosava lui: distorsione fuori dal nostro controllo,
        // diversa da scheda a scheda, e invisibile ai nostri metri.
        // Questa non e' un suono, e' il contratto con il convertitore: finche'
        // si sta sotto non tocca un bit (una tosatura a 1.0 su un segnale che
        // non arriva a 1.0 e' l'identita'), e non c'e' taratura che possa
        // passarci sopra. Sta qui e non in Max.h perche' deve valere anche col
        // FINALE spento, quando il tetto non ha nessun proprietario.
        for (int i = 0; i < n; ++i) {
            if (L[i] >  1.0f) L[i] =  1.0f; else if (L[i] < -1.0f) L[i] = -1.0f;
            if (R[i] >  1.0f) R[i] =  1.0f; else if (R[i] < -1.0f) R[i] = -1.0f;
        }
        double eO = 0.0;
        for (int i = 0; i < n; ++i) eO += (double) L[i] * L[i] + (double) R[i] * R[i];
        // medie lente: si vuole il rapporto medio del brano, non del blocco
        eIn_  += (eI - eIn_)  * 0.02;
        eOut_ += (eO - eOut_) * 0.02;
        if (eIn_ > 1e-12 && eOut_ > 1e-12) {
            const double r = std::sqrt(eOut_ / eIn_);
            // fra -18 e +18 dB, e si muove piano
            comp_ += (std::min(std::max(r, 0.125), 8.0) - comp_) * 0.05;
        }

        spiaAgc.store(a ? (float) livellatore.gainDb() : 0.0f);
        for (int i = 0; i < 5; ++i) {
            spiaBanda[i].store(b ? (float) multibanda.riduzione(i) : 0.0f);
            spiaLiv[i].store((float) multibanda.livello(i));
            spiaUsc[i].store((float) multibanda.uscita(i));
        }
        spiaFin.store(x ? (float) finale.riduzioneDb() : 0.0f);
        spiaTosa.store(x ? (float) finale.tosaturaDb() : 0.0f);
        misura(L, R, n, false);
    }
    std::atomic<float> mIn[5]  { -100,-100,-100,-100,-100 };
    std::atomic<float> mOut[5] { -100,-100,-100,-100,-100 };
private:
    double picIn_[2]={0,0}, picOut_[2]={0,0}, tenIn_[2]={0,0}, tenOut_[2]={0,0};
    double rmsIn_=0, rmsOut_=0;
    long long scadIn_[2]={0,0}, scadOut_[2]={0,0};
};