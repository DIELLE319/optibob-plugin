// Il finale: densita' e tetto. Niente FFT, niente FIR.
//
// La catena, nell'ordine:
//   guadagno d'ingresso -> saturazione morbida (SHAPE) a 2x -> limitatore a
//   muro con anticipo -> tetto d'uscita -> miscela col secco.
//
// Perche' il 2x: una curva non lineare genera armoniche sopra la meta' della
// frequenza di campionamento, che si ripiegano dentro la banda e si sentono
// come una patina metallica. Raddoppiando la frequenza prima di deformare, le
// armoniche stanno sopra e il filtro di ritorno se le porta via. Costa quattro
// biquad per campione: niente, rispetto a quello che salva.
//
// Regola, come per l'AGC: a SHAPE 0, LIMIT spento, ingresso e uscita a 0 dB,
// deve essere identico al bypass.
#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

namespace omax {

struct Params {
    // (il muro in cima e' acceso solo fra 100 Hz e 19.5 kHz)
    // ⭐ 30/08/2026 — CLIPPING e FINALE si spengono separatamente.
    // Erano un interruttore solo perche' sono la stessa scatola del motore.
    // Ma sono due mestieri diversi: uno tosa le punte, l'altro dosa e mette
    // il tetto. Adesso ognuno ha il suo, cosi' si puo' sentire chi fa cosa.
    //   tosaOn = rotatore, clipper del basso, pre-enfasi + limitatore acuti
    //            + clipper + de-enfasi, muro a 15 kHz
    //   (il resto — campane, spaziosita', anticipo, limitatore di sicurezza,
    //    guadagni e miscela — sta nel FINALE, che ha gia' il suo)
    bool   tosaOn  = true;
    double inDb    =  0.0;    // spinta in ingresso
    double shape   =  0.0;    // 0..1  quanta saturazione
    double outDb   = -1.0;    // tetto d'uscita
    double mix     =  1.0;    // 0..1  miscela col secco
    bool   limit   = true;    // il muro finale
    // 10 ms di anticipo: sotto, il guadagno si muove DENTRO il periodo di
    // un'onda bassa e quel movimento e' distorsione. Con 10 ms un 60 Hz vede
    // un guadagno praticamente fermo. Sono 10 ms dichiarati all'host, che li
    // compensa: in ascolto non si notano.
    double anticipoMs = 10.0;
    double rilascioMs = 120.0;
    // --- i medio-alti -----------------------------------------------------
    // Una campana attorno ai 3 kHz: una parte sempre inserita, e una che
    // entra solo quando quella zona spinge. E' quello che sull'Optimod fa il
    // limitatore degli acuti: non incupisce il brano, lo tiene a bada quando
    // grida.
    double maHz    = 2400.0;  // dove sta la campana (2400: zona indicata da Luca)
    double maFisso =    0.0;  // dB, sempre inserito (negativo = schiaccia)
    double maDin   =    0.0;  // dB, quanto puo' schiacciare in piu' quando spinge
    double maRel   = 1000.0;  // ms, quanto ci mette a mollare: lungo = distanza
    double maQ     =    0.8;  // larghezza: basso = rientro ampio, alto = buco stretto
    // --- il basso che scoda ----------------------------------------------
    // Non e' un problema di quantita' ma di DURATA: il colpo va bene, e' la
    // coda che resta li' e impasta. Quindi attacco lento a sufficienza da
    // lasciar passare la botta, e rilascio corto che accorcia quello che
    // viene dopo. E' l'opposto dei medio-alti, dove serve lungo.
    double bsHz    =  90.0;
    // ⭐ 16/08/2026 — LA CAMPANA FISSA SUL GRAVE.
    // Luca taglia 6.8 dB a 117 Hz con l'EQ di Logic e non riusciva a
    // ottenere lo stesso taglio dentro OptiBob. Motivo: lo faceva PRIMA
    // delle bande, e il multibanda glielo rimetteva -- la banda che contiene
    // i 117 si trovava con meno energia, comprimeva di meno, e la lasciava
    // uscire piu' forte. Meta' del taglio spariva.
    // Questa campana sta DOPO le bande: quello che toglie, resta tolto.
    double bsFisso =   0.0;   // dB, sempre inserito (negativo = toglie)
    double bsDin   =   0.0;   // dB, quanto accorcia la coda
    double bsRel   = 120.0;   // ms, corto = piu' asciutto
    double bsQ     =   0.9;

    // ======================================================================
    // I QUATTRO STADI DELL'OPTIMOD (14/08/2026)
    // Fin qui il finale era un limitatore: guarda avanti, abbassa in tempo,
    // non distorce. Pulito, e per questo INDIETRO. L'Optimod invece TOSA, e
    // la spinta in avanti viene da li'. Questi quattro stadi sono quello che
    // mancava. A zero sono tutti spenti e la catena resta identica a prima.
    // ======================================================================

    // --- 1. rotatore di fase ---------------------------------------------
    // La voce e' asimmetrica: spinge molto piu' da una parte. Il clipper la
    // tosa tutta da un lato solo. Quattro passa-tutto la raddrizzano senza
    // cambiare una virgola dello spettro: si sente uguale, ma poi si tosa
    // uniforme. Vale 2-3 dB gratis.
    int    rotSez  = 0;       // 0 = spento, 4 = come l'Optimod
    double rotHz   = 200.0;

    // --- 2. clipper del basso, separato ----------------------------------
    // Si divide a bcHz, si tosa SOLO il grave, si risomma. Se si tosa tutto
    // insieme, ogni colpo di cassa si porta dietro i medi e sporca la voce a
    // ogni battuta -- il "fango" che compare quando si spinge.
    double bcHz    = 200.0;
    double bcDb    =   0.0;   // dB di spinta dentro il clipper del basso
    // ⭐⭐ 03/09/2026 — LE ARMONICHE DEL GRAVE.
    // Luca: «non si sente il gonfio delle frequenze basse che pero' non
    // scodano». Misurato: l'Orban tiene la seconda armonica del basso 4.1 dB
    // sotto la fondamentale, noi 7.4. Sotto i 60 Hz l'orecchio a volume basso
    // non sente quasi niente, a 100-200 sente ancora bene: il basso
    // dell'Orban si sente anche piano perche' il cervello ricostruisce la
    // fondamentale dalle sue armoniche.
    // Il clipper del basso le fa TOGLIENDO dalla fondamentale, e paga 4.3 dB
    // di sporco per 1.5 di armoniche: un cambio in perdita, ed e' il motivo
    // per cui e' a zero. Questo stadio invece le SINTETIZZA e le somma,
    // senza toccare la fondamentale.
    // Misurato a 9 dB / 120 Hz: seconda armonica a -3.86 sotto la
    // fondamentale contro i -4.13 dell'Orban, e sopra i 500 Hz aggiunge
    // -24.3 dB, cioe' niente.
    double armDb   =   0.0;   // quanto se ne aggiunge
    double armHz   = 120.0;   // sotto cosa si prende il grave da armonizzare

    // --- 3. pre-enfasi + limitatore acuti + clipper + de-enfasi ----------
    // L'Optimod non lavora sul segnale com'e': lo porta in un dominio dove
    // gli acuti sono molto piu' alti, lavora LI', e poi rimette a posto.
    // Conseguenza automatica: gli acuti vengono frenati assai piu' dei
    // gravi, sempre. E' il motivo per cui in radio i piatti non escono mai
    // dal quadro.
    // La de-enfasi e' l'inversa ESATTA della pre-enfasi (stessi coefficienti
    // scambiati), quindi a limitatore e clipper fermi il blocco e' trasparente.
    // ⚠️⚠️ 16/08/2026 — IL TAGLIO A 15 kHz, CHE MANCAVA DEL TUTTO.
    // Misurato sull'uscita Orban vera che mi ha passato Luca: -33.8 dB a
    // 15 kHz, -44 a 15.5, -56 a 16, -65 a 17. E' un muro. Non e' una
    // scelta di gusto: in FM sopra i 15 kHz non ci puo' andare NIENTE,
    // perche' li' comincia il pilota stereo a 19 kHz. Ogni Optimod ce
    // l'ha, ed e' parte del suo suono -- non solo una regola.
    // Noi non l'avevamo per niente e lasciavamo passare tutto fino a 20k.
    // ⚠️ 18/08 — Luca: «muro in cima, appena si tocca una virgola cambia
    // tutto». Colpa della scala, non del filtro: il cursore andava da 0 a
    // 20000 a scatti di 250, quindi il primo scatto sopra lo spento era un
    // passa-basso a 250 Hz -- cioe' tutto il suono buttato via. Adesso il
    // cursore vive fra 10 e 20 kHz, dove un muro ha senso, e SPENTO sta in
    // cima: sopra i 19.5 kHz non filtra piu' niente.
    // ⭐ 21/08/2026 — LA SPAZIOSITA'. Luca: «migliorare la stereofonia
    // soprattutto sulle medio alte... così gli dà quel senso di etereo».
    //
    // ⚠️ Detto onestamente: qui NON stiamo piu' inseguendo l'Orban.
    // Misurato su September, dopo le correzioni del 20-21 agosto:
    //                        0.8-3.5k   3.5-16k
    //   quanto L e R si somigliano (1 = mono)
    //   Orban vero              0.399     0.423
    //   noi                     0.409     0.387
    //   code (il riverbero che resta dopo il colpo)
    //   Orban vero              -4.99     -6.08
    //   noi                     -4.43     -5.90
    // Sui medio-alti la nostra immagine e' gia' quella della macchina vera,
    // e sulle code lo battiamo. Allargare non ci avvicina all'Orban: ci
    // porta OLTRE. Ed e' quello che Luca ha chiesto -- «allontanarci dalla
    // realta' per creare un suono nuovo» -- quindi e' una manopola, parte
    // da zero, e chi vuole l'Orban la lascia li'.
    //
    // Come funziona: si separa il segnale in centro (L+R) e lato (L-R), si
    // alza SOLO il lato e SOLO sopra una frequenza. Il centro non si tocca
    // mai: voce e basso restano dove sono, e non si apre quel buco al
    // centro che fa sembrare tutto lontano ma vuoto.
    double spazio   =  0.0;   // 0..1 quanto si allarga il lato
    double spazioHz = 1500.0; // ...e da dove in su
    double topHz   =  0.0;    // 0 o >19500 = spento; 15000 = la macchina vera
    double preUs   =  0.0;    // 0 = spento; 50 (Europa) o 75 (USA) microsecondi
    double hfHz    = 6000.0;  // dove sente gli acuti
    double hfDb    =  0.0;    // 0 = spento; quanto puo' frenarli (negativo)
    double clipDb  =  0.0;    // 0 = spento; spinta dentro il clipper finale
};

// Biquad passa-basso Butterworth, per salire e scendere di frequenza
struct Lp {
    void set(double fc, double sr) {
        const double w = 2.0 * M_PI * std::min(fc, sr * 0.49) / sr;
        const double cs = std::cos(w), sn = std::sin(w);
        const double al = sn / (2.0 * 0.70710678);
        const double b0 = (1.0 - cs) * 0.5, b1 = 1.0 - cs, b2 = b0;
        const double a0 = 1.0 + al, a1 = -2.0 * cs, a2 = 1.0 - al;
        b0_=b0/a0; b1_=b1/a0; b2_=b2/a0; a1_=a1/a0; a2_=a2/a0;
    }
    void reset() { x1_=x2_=y1_=y2_=0.0; }
    inline double process(double x) {
        const double y = b0_*x + b1_*x1_ + b2_*x2_ - a1_*y1_ - a2_*y2_;
        x2_=x1_; x1_=x; y2_=y1_; y1_=y; return y;
    }
    double b0_=1,b1_=0,b2_=0,a1_=0,a2_=0,x1_=0,x2_=0,y1_=0,y2_=0;
};

// Campana (peaking) a guadagno variabile
struct Campana {
    void set(double fc, double q, double gDb, double sr) {
        const double A = std::pow(10.0, gDb / 40.0);
        const double w = 2.0 * M_PI * std::min(fc, sr * 0.45) / sr;
        const double cs = std::cos(w), sn = std::sin(w);
        const double al = sn / (2.0 * q);
        const double b0 = 1.0 + al * A, b1 = -2.0 * cs, b2 = 1.0 - al * A;
        const double a0 = 1.0 + al / A, a1 = -2.0 * cs, a2 = 1.0 - al / A;
        b0_=b0/a0; b1_=b1/a0; b2_=b2/a0; a1_=a1/a0; a2_=a2/a0;
    }
    void reset() { x1_=x2_=y1_=y2_=0.0; }
    inline double process(double x) {
        const double y = b0_*x + b1_*x1_ + b2_*x2_ - a1_*y1_ - a2_*y2_;
        x2_=x1_; x1_=x; y2_=y1_; y1_=y; return y;
    }
    double b0_=1,b1_=0,b2_=0,a1_=0,a2_=0,x1_=0,x2_=0,y1_=0,y2_=0;
};

// Passabanda solo per MISURARE quanto spinge la zona: non sta sul percorso
// del suono, quindi non lo tocca.
struct Banda {
    void set(double fc, double q, double sr) {
        const double w = 2.0 * M_PI * std::min(fc, sr * 0.45) / sr;
        const double cs = std::cos(w), sn = std::sin(w);
        const double al = sn / (2.0 * q);
        const double b0 = al, b1 = 0.0, b2 = -al;
        const double a0 = 1.0 + al, a1 = -2.0 * cs, a2 = 1.0 - al;
        b0_=b0/a0; b1_=b1/a0; b2_=b2/a0; a1_=a1/a0; a2_=a2/a0;
    }
    void reset() { x1_=x2_=y1_=y2_=0.0; }
    inline double process(double x) {
        const double y = b0_*x + b1_*x1_ + b2_*x2_ - a1_*y1_ - a2_*y2_;
        x2_=x1_; x1_=x; y2_=y1_; y1_=y; return y;
    }
    double b0_=1,b1_=0,b2_=0,a1_=0,a2_=0,x1_=0,x2_=0,y1_=0,y2_=0;
};

// Passa-tutto del primo ordine: cambia la fase, non lo spettro. In fila per
// quattro, raddrizza le forme asimmetriche.
struct Ap1 {
    void set(double fc, double sr) {
        const double t = std::tan(M_PI * std::min(fc, sr * 0.45) / sr);
        c_ = (t - 1.0) / (t + 1.0);
    }
    void reset() { x1_ = y1_ = 0.0; }
    inline double process(double x) {
        const double y = c_ * x + x1_ - c_ * y1_;
        x1_ = x; y1_ = y; return y;
    }
    double c_ = 0.0, x1_ = 0.0, y1_ = 0.0;
};

// Filtro del primo ordine generico, uno zero e un polo. Serve per la coppia
// pre-enfasi / de-enfasi: la seconda si costruisce scambiando numeratore e
// denominatore della prima, quindi l'inversione e' esatta per costruzione e
// non "quasi esatta".
struct Uno1 {
    void reset() { x1_ = y1_ = 0.0; }
    inline double process(double x) {
        const double y = b0_ * x + b1_ * x1_ - a1_ * y1_;
        x1_ = x; y1_ = y; return y;
    }
    double b0_ = 1.0, b1_ = 0.0, a1_ = 0.0, x1_ = 0.0, y1_ = 0.0;
};

// Costruisce la coppia. tau in secondi, spinta = quanto sale al massimo.
inline void enfasi(double tau, double spinta, double sr, Uno1& pre, Uno1& de) {
    const double wz = 1.0 / std::max(1e-9, tau);
    const double wp = wz * std::max(1.5, spinta);
    const double K  = 2.0 * sr;
    const double A  = K / wz, B = K / wp;
    const double n  = 1.0 + B;
    pre.b0_ = (1.0 + A) / n; pre.b1_ = (1.0 - A) / n; pre.a1_ = (1.0 - B) / n;
    // l'inversa: si scambiano i due polinomi
    const double m  = 1.0 + A;
    de.b0_ = (1.0 + B) / m;  de.b1_ = (1.0 - B) / m;  de.a1_ = (1.0 - A) / m;
}

// Scaffale alto: il limitatore degli acuti tira giu' di qui.
struct Scaffale {
    void set(double fc, double gDb, double sr) {
        const double A = std::pow(10.0, gDb / 40.0);
        const double w = 2.0 * M_PI * std::min(fc, sr * 0.45) / sr;
        const double cs = std::cos(w), sn = std::sin(w);
        // S = pendenza dello scaffale. 0.7 e' dolce: non fa il gradino.
        const double S  = 0.7;
        const double al = sn * 0.5 * std::sqrt((A + 1.0/A) * (1.0/S - 1.0) + 2.0);
        const double sA = 2.0 * std::sqrt(A) * al;
        const double b0 =      A * ((A+1.0) + (A-1.0)*cs + sA);
        const double b1 = -2.0*A * ((A-1.0) + (A+1.0)*cs);
        const double b2 =      A * ((A+1.0) + (A-1.0)*cs - sA);
        const double a0 =           (A+1.0) - (A-1.0)*cs + sA;
        const double a1 =  2.0 *   ((A-1.0) - (A+1.0)*cs);
        const double a2 =           (A+1.0) - (A-1.0)*cs - sA;
        b0_=b0/a0; b1_=b1/a0; b2_=b2/a0; a1_=a1/a0; a2_=a2/a0;
    }
    void reset() { x1_=x2_=y1_=y2_=0.0; }
    inline double process(double x) {
        const double y = b0_*x + b1_*x1_ + b2_*x2_ - a1_*y1_ - a2_*y2_;
        x2_=x1_; x1_=x; y2_=y1_; y1_=y; return y;
    }
    double b0_=1,b1_=0,b2_=0,a1_=0,a2_=0,x1_=0,x2_=0,y1_=0,y2_=0;
};

// La tosatura. Non un taglio netto -- quello genera armoniche fino a Nyquist
// e oltre, che si ripiegano dentro -- ma un ginocchio corto: fino al 85%
// della soglia e' retta, poi curva e si ferma. Suona come un taglio netto e
// sporca molto meno.
inline double tosa(double x, double t) {
    if (t <= 1e-9) return x;
    const double k = 0.85 * t;          // fin qui e' retta
    const double a = std::abs(x);
    if (a <= k) return x;
    const double d = t - k;
    const double u = (a - k) / d;
    // parabola con pendenza 1 all'inizio e 0 alla fine: a u = 2 tocca il
    // tetto e ci resta. Continua anche nella derivata, che e' quello che
    // conta per non generare armoniche alte.
    const double y = (u >= 2.0) ? t : (k + d * (u - u * u * 0.25));
    return (x < 0.0) ? -y : y;
}

class Max {
public:
    void prepare(double sr, int blocco) {
        sr_ = (sr > 0.0) ? sr : 48000.0;
        for (int c = 0; c < 2; ++c) {
            su_[c][0].set(sr_ * 0.45, sr_ * 2.0);
            giu_[c][0].set(sr_ * 0.45, sr_ * 2.0);
            su_[c][0].reset(); giu_[c][0].reset();
        }
        for (int c = 0; c < 2; ++c) { camp_[c].reset(); }
        bandaL_.reset(); bandaR_.reset(); envMa_ = 1e-12; maDbOra_ = 0.0; contaMa_ = 0;
        bandaBsL_.reset(); bandaBsR_.reset(); campBs_[0].reset(); campBs_[1].reset();
        envBs_ = 1e-12; bsDbOra_ = 0.0; contaBs_ = 0;
        for (int c = 0; c < 2; ++c) {
            for (int k = 0; k < 4; ++k) rot_[c][k].reset();
            bcLp_[c].reset(); pre_[c].reset(); de_[c].reset();
            hfSc_[c].reset(); hfDet_[c].reset();
            for (int k = 0; k < 4; ++k) { topL_[k].reset(); topR_[k].reset(); }
            clSu_[c].set(sr_ * 0.45, sr_ * 2.0); clSu_[c].reset();
            clGiu_[c].set(sr_ * 0.45, sr_ * 2.0); clGiu_[c].reset();
        }
        envHf_ = 1e-12; hfDbOra_ = 0.0; contaHf_ = 0; clipDbOra_ = 0.0; picPrima_ = 0.0; picDopo_ = 0.0; contaClip_ = 0; picco_ = 0.0; picBs_ = 1e-3;
        aggiorna();
        rit_.assign((size_t) std::max(8, anticipo_ + 4) * 2, 0.0f);
        ritS_.assign((size_t) std::max(8, anticipo_ + 4) * 2, 0.0f);
        finestra_.assign((size_t) std::max(1, anticipo_), 1.0f);
        pos_ = 0; posS_ = 0; posF_ = 0; g_ = 1.0; grDb_ = 0.0;
        (void) blocco;
    }

    void setParams(const Params& p) {
        const int vecchio = anticipo_;
        p_ = p; aggiorna();
        if (anticipo_ != vecchio) {
            rit_.assign((size_t) std::max(8, anticipo_ + 4) * 2, 0.0f);
            ritS_.assign((size_t) std::max(8, anticipo_ + 4) * 2, 0.0f);
            finestra_.assign((size_t) std::max(1, anticipo_), 1.0f);
            pos_ = 0; posS_ = 0; posF_ = 0;
        }
    }

    int  latenza()  const { return anticipo_; }
    double riduzioneDb()  const { return grDb_; }
    double medioAltiDb()  const { return maDbOra_; }
    double bassoDb()      const { return bsDbOra_; }
    double tosaturaDb()   const { return clipDbOra_; }

    inline void process(float* L, float* R, int n) {
        const double gin  = db2lin(p_.inDb);
        const double gout = db2lin(p_.outDb);
        const double tetto = gout;              // il limitatore punta al tetto

        for (int i = 0; i < n; ++i) {
            const double secchiL = L[i], secchiR = R[i];
            double l = secchiL * gin, r = secchiR * gin;

            // --- 1. rotatore di fase, prima di qualsiasi tosatura --------
            // ⭐ 03/09 — le armoniche del grave stanno FUORI dalla
            // tosatura. Ci erano finite dentro per come e' scritto il
            // blocco, e spegnendo il clipping si spegnevano anche loro --
            // ma il gonfio del basso e la tosatura sono due cose diverse, e
            // Luca ha appena scoperto a orecchio che la seconda toglie il
            // primo. Adesso si possono avere le armoniche senza la tosatura.
            // --- le armoniche del grave -------------------------------
            if (p_.armDb > 0.01) {
                const double m = 0.5 * (l + r);
                const double g = armA_.process(armB_.process(m));
                // l'inviluppo rende l'armonica proporzionale al grave e non
                // al suo quadrato: senza, a volume basso sparisce e a volume
                // alto esplode.
                const double ag = std::fabs(g);
                envArm_ += (ag - envArm_) * ((ag > envArm_) ? 0.02 : 0.0006);
                const double n = std::max(1e-4, envArm_);
                const double u = g / n;
                // ⚠️ la terza armonica (u^3) faceva piu' intermodulazione che
                // armonica: portava lo sporco sopra i 500 Hz da -29 a -17 dB.
                // Il grave e' quasi sempre piu' note insieme, e un termine
                // dispari le mescola fra loro. Resta la seconda, che e' anche
                // quella che l'orecchio usa per ricostruire la fondamentale.
                // ⚠️⚠️ 03/09 — APPIATTIVA LE CIME DEL BASSO.
                // u*u ai picchi vale sempre +1: si sommava alla cima
                // dell'onda e la spianava. Misurato sulla coppia vera: le
                // cime del grave passavano da 16.8 (vive, come quelle
                // dell'Orban) a 1.3, piatte come una tavola. Era questo
                // stadio a tosare il basso -- non il muro, non il clipper.
                // La seconda armonica vera e' sin(2x) = 2 sin(x) cos(x), e
                // il coseno si ricava dalla derivata del grave: cosi'
                // l'armonica vale ZERO dove l'onda e' al picco ed e' massima
                // dove passa per lo zero. Gonfia senza tosare.
                const double dg = (g - gPrec_) * derNorm_;
                gPrec_ = g;
                double arm = 2.0 * u * dg * n;
                arm = armC_.process(arm);         // niente acuti
                arm = arm - armD_.process(arm);   // via la fondamentale
                const double k = db2lin(p_.armDb) - 1.0;
                l += arm * k; r += arm * k;
            }

            if (p_.tosaOn) {
            // Non cambia lo spettro di un decimo di dB: cambia solo dove
            // stanno le punte. Ma le mette simmetriche, e da li' in poi
            // ogni clipper lavora uguale sopra e sotto.
            if (p_.rotSez > 0) {
                const int q = std::min(p_.rotSez, 4);
                for (int k = 0; k < q; ++k) { l = rot_[0][k].process(l);
                                              r = rot_[1][k].process(r); }
            }

            if (p_.shape > 1e-6) { l = satura(l, 0); r = satura(r, 1); }

            }

            // --- medio-alti: quanto spinge quella zona, e di quanto la si tiene
            if (p_.maFisso < -0.01 || p_.maDin < -0.01) {
                const double bl = bandaL_.process(l), br = bandaR_.process(r);
                const double m2 = 0.5 * (bl * bl + br * br);
                // attacco svelto, rilascio lento: cosi' prende l'ATTACCO dei
                // suoni, non il loro livello medio. E' l'attacco trattenuto
                // che fa sembrare una cosa lontana, non il volume.
                envMa_ += ((m2 > envMa_) ? aMaSu_ : aMaGiu_) * (m2 - envMa_);
                if (--contaMa_ <= 0) {
                    contaMa_ = 32;
                    const double lvl = 10.0 * std::log10(std::max(envMa_, 1e-20));
                    // sopra -24 dB comincia a intervenire, a fondo scala da' tutto
                    const double quanto = std::min(std::max((lvl + 24.0) / 18.0, 0.0), 1.0);
                    const double voluto = p_.maFisso + p_.maDin * quanto;
                    // scende in fretta, risale col rilascio impostato
                    const double su = 1.0 - std::exp(-32.0 / (std::max(0.05, p_.maRel*0.001) * sr_));
                    maDbOra_ += (voluto - maDbOra_) * ((voluto < maDbOra_) ? 0.6 : su);
                    camp_[0].set(p_.maHz, p_.maQ, maDbOra_, sr_);
                    camp_[1].set(p_.maHz, p_.maQ, maDbOra_, sr_);
                }
                l = camp_[0].process(l);
                r = camp_[1].process(r);
            }

            // --- il basso che scoda: si accorcia la coda, non il colpo ---
            if (p_.bsFisso < -0.01 || p_.bsDin < -0.01) {
                const double bl = bandaBsL_.process(l), br = bandaBsR_.process(r);
                const double m2 = 0.5 * (bl * bl + br * br);
                envBs_ += ((m2 > envBs_) ? aBsSu_ : aBsGiu_) * (m2 - envBs_);
                if (p_.bsDin > -0.01) {          // solo la parte fissa
                    if (std::abs(bsDbOra_ - p_.bsFisso) > 1e-4) {
                        bsDbOra_ = p_.bsFisso;
                        campBs_[0].set(p_.bsHz, p_.bsQ, bsDbOra_, sr_);
                        campBs_[1].set(p_.bsHz, p_.bsQ, bsDbOra_, sr_);
                    }
                } else if (--contaBs_ <= 0) {
                    contaBs_ = 32;
                    const double lvl = 10.0 * std::log10(std::max(envBs_, 1e-20));
                    const double quanto = std::min(std::max((lvl + 26.0) / 16.0, 0.0), 1.0);
                    const double voluto = p_.bsFisso + p_.bsDin * quanto;
                    const double su = 1.0 - std::exp(-32.0 / (std::max(0.02, p_.bsRel*0.001) * sr_));
                    bsDbOra_ += (voluto - bsDbOra_) * ((voluto < bsDbOra_) ? 0.5 : su);
                    campBs_[0].set(p_.bsHz, p_.bsQ, bsDbOra_, sr_);
                    campBs_[1].set(p_.bsHz, p_.bsQ, bsDbOra_, sr_);
                }
                l = campBs_[0].process(l);
                r = campBs_[1].process(r);
            }

            // --- 2. il clipper del basso, per conto suo -----------------
            if (p_.tosaOn) {
            // Si divide in due: sotto bcHz e il resto. Il grave si tosa da
            // solo e si risomma. La somma e' esatta per costruzione (alto =
            // tutto meno basso), quindi a clipper fermo non cambia niente.
            // ⚠️⚠️ 20/08/2026 — IL CLIPPER DEL BASSO NON TOSAVA NIENTE.
            // Tagliava a 1.0, cioe' a fondo scala assoluto. Ma qui dentro il
            // grave sta intorno a 0.1: con sei dB di spinta arrivava a 0.2 e
            // la soglia restava lontana cinque volte tanto. Misurato sul
            // banco: portarlo da 6 a 0 dava un'uscita identica BIT PER BIT
            // (-213 dB). Il comando c'era, il blocco c'era, e non toccava
            // mai il segnale.
            // Adesso la soglia e' relativa al picco del grave, come nel
            // clipper finale: a spinta zero non tocca niente, e ogni dB di
            // spinta e' un dB vero di tosatura.
            if (p_.bcDb > 0.01) {
                const double gb = db2lin(p_.bcDb);
                const double lb = bcLp_[0].process(l), rb = bcLp_[1].process(r);
                const double ora = std::max(std::abs(lb), std::abs(rb));
                if (ora > picBs_) picBs_ = ora;
                else picBs_ += (ora - picBs_) * aPicGiu_;
                if (picBs_ < 1e-3) picBs_ = 1e-3;
                const double sogliaBs = std::max(1e-4, picBs_ * 1.18 / gb);
                l = tosa(lb, sogliaBs) + (l - lb);
                r = tosa(rb, sogliaBs) + (r - rb);
            }

            }

            // --- 3. pre-enfasi -> limitatore acuti -> clipper -> de-enfasi
            if (p_.tosaOn) {
            if (blocco_) {
                if (pre_on_) { l = pre_[0].process(l); r = pre_[1].process(r); }

                if (p_.hfDb < -0.01) {
                    // Il rivelatore sta DOPO la pre-enfasi: sente gli acuti
                    // gia' alzati, ed e' per questo che il freno entra molto
                    // prima sugli acuti che su tutto il resto. E' il
                    // meccanismo per cui in radio i piatti non escono mai.
                    const double dl = hfDet_[0].process(l), dr = hfDet_[1].process(r);
                    const double m2 = 0.5 * (dl * dl + dr * dr);
                    envHf_ += ((m2 > envHf_) ? aHfSu_ : aHfGiu_) * (m2 - envHf_);
                    if (--contaHf_ <= 0) {
                        contaHf_ = 32;
                        const double lvl = 10.0 * std::log10(std::max(envHf_, 1e-20));
                        const double quanto = std::min(std::max((lvl + 20.0) / 14.0, 0.0), 1.0);
                        const double voluto = p_.hfDb * quanto;
                        hfDbOra_ += (voluto - hfDbOra_) * ((voluto < hfDbOra_) ? 0.7 : aHfRis_);
        hfSc_[0].set(p_.hfHz, hfDbOra_, sr_);
                        hfSc_[1].set(p_.hfHz, hfDbOra_, sr_);
                    }
                    l = hfSc_[0].process(l); r = hfSc_[1].process(r);
                }

                if (p_.clipDb > 0.01) {
                    // ⚠️⚠️ 18/08 — «appena tocchi il clipper, gia' da 0.1,
                    // distorce tutto». Vero, ed era un difetto di
                    // costruzione: il clipper tosava a fondo scala fisso,
                    // ma qui arriva un segnale che sta BEN SOPRA fondo scala
                    // (il recupero delle bande piu' l'ingresso del finale).
                    // Quindi accenderlo non voleva dire «tosa un pelo»:
                    // voleva dire «spiana tutto quello che sta sopra lo
                    // zero», e il salto era enorme.
                    //
                    // Adesso la soglia e' RELATIVA al picco del programma:
                    // si segue il picco (attacco svelto, rilascio lento) e
                    // si tosa a picco/guadagno. Cosi' il drive dice quanti
                    // dB di punte togli -- 0.25 dB si sentono, come avverte
                    // il tecnico -- e a zero non succede niente, da
                    // qualunque livello arrivi il segnale.
                    const double gc = db2lin(p_.clipDb);
                    // ⚠️ 18/08 — SECONDO PEZZO DELLA DISTORSIONE IMPROVVISA.
                    // Il picco saliva in 5 ms. Dopo un silenzio scendeva
                    // quasi a zero, e quando la musica ripartiva la soglia
                    // restava minuscola per qualche millisecondo: tosatura
                    // totale, cioe' un colpo di distorsione, poi tornava
                    // tutto a posto. Adesso il picco sale ISTANTANEAMENTE
                    // -- e' un riferimento, non un rivelatore -- e scende
                    // piano. E c'e' un fondo, per non arrivare mai a zero.
                    const double ora = std::max(std::abs(l), std::abs(r));
                    if (ora > picco_) picco_ = ora;
                    else picco_ += (ora - picco_) * aPicGiu_;
                    if (picco_ < 1e-3) picco_ = 1e-3;
                    // Il ginocchio di tosa() comincia all'85% della soglia:
                    // se la soglia fosse il picco, a drive zero si tocherebbe
                    // gia' l'ultimo 15%. Si alza di 1/0.85 = 1.18 cosi' a
                    // drive zero il ginocchio cade ESATTAMENTE sul picco e
                    // non si tocca niente. Misurato: 0.00 dB tosati a 0.
                    const double soglia = std::max(1e-4, picco_ * 1.18 / gc);
                    const double primaL = std::abs(l), primaR = std::abs(r);
                    // niente divisione dopo: qui la spinta DEVE diventare
                    // volume. E' l'unico posto della catena dove si guadagna
                    // livello tosando invece che abbassando.
                    l = tosa2x(l, 0, soglia);
                    r = tosa2x(r, 1, soglia);
                    // ⚠️ 16/08/2026 — quanto sta TOSANDO.
                    // Segnalato da Luca: «la colonna finale non lavora mai».
                    // Vero, ed e' giusto: il clipper ha preso il posto del
                    // limitatore. Ma la spia misurava la cosa sbagliata.
                    //
                    // PRIMO TENTATIVO SBAGLIATO: confrontare i campioni uno
                    // per uno. Vicino agli zeri del segnale il rapporto fra
                    // prima e dopo esplode (i filtri del 2x spostano di un
                    // pelo la forma d'onda) e usciva -35 dB di «tosatura»
                    // dove non si tosava niente. Si confrontano i PICCHI su
                    // una finestra, non i campioni.
                    picPrima_ = std::max(picPrima_, std::max(primaL, primaR));
                    picDopo_  = std::max(picDopo_,  std::max(std::abs(l), std::abs(r)));
                    if (++contaClip_ >= 480) {          // 10 ms a 48 kHz
                        const double t = (picPrima_ > 1e-6)
                            ? 20.0 * std::log10(std::max(picDopo_, 1e-9) / picPrima_) : 0.0;
                        clipDbOra_ += (t < clipDbOra_) ? (t - clipDbOra_) * 0.6
                                                       : (t - clipDbOra_) * 0.08;
                        picPrima_ = 0.0; picDopo_ = 0.0; contaClip_ = 0;
                    }
                }

                if (pre_on_) { l = de_[0].process(l); r = de_[1].process(r); }
            }

            }

            // --- la spaziosita': centro fermo, lato piu' largo in alto ---
            if (spazioOn_) {
                // ⭐⭐ 27/09/2026 — IL BASSO AL CENTRO.
                // Luca, sulla web radio: «il basso picchia ma e' dolce», «come
                // faccio ad allargare i bassi come in quella canzone». Misurato:
                // la' sotto i 150 Hz il basso e' MONO perfetto (lato -33 dB,
                // correlazione 1.00). Da noi era il contrario: correlazione
                // 0.23, lato a -2 dB -- il grave sparpagliato fra le casse, che
                // suona piu' debole e in mono si cancella. Colpa di questa
                // manopola: alzava il lato anche SOTTO la sua frequenza (x1.43).
                // Adesso il lato passa da un passa-alto a 150 Hz (4 poli): sotto
                // il basso va tutto al centro. Al banco: -33.0 / 1.00, identico
                // alla web radio. E sotto spazioHz il lato non si alza piu'.
                const double M = 0.5 * (l + r);
                const double S = spMo2_.process(spMo1_.process(0.5 * (l - r)));
                // il lato si divide in due: sotto resta com'e', sopra cresce
                const double Sb = spLpL_.process(S);
                const double Sa = S - Sb;
                // ⭐⭐ 02/09/2026 — ERA TIMIDA DI TRE VOLTE.
                // Misurato sull'8200 con Stereomaxx che Luca ha mandato: il
                // lato sta 1.73 dB sotto il centro. Da noi, con questa
                // manopola a FONDO CORSA, il lato stava 4.21 dB sotto -- e
                // nel preset, a 47, stava a 7.19, cioe' come se non ci
                // fosse. Il moltiplicatore massimo era 2.5, e solo sulla
                // parte alta del lato: non bastava nemmeno spalancata.
                // Adesso arriva a sei volte in alto e alza poco in basso --
                // il grave largo sfasa, e in mono sparisce.
                // Con 45 a 700 Hz: S/M -1.81 contro -1.73 dell'Orban, e in
                // mono si perde 1.91 dB contro i 2.51 suoi. E' «l'etereo».
                const double S2 = Sb + Sa * (1.0 + p_.spazio * 5.0);
                l = M + S2; r = M - S2;
            }

            // --- il muro a 15 kHz ---------------------------------------
            // In fondo a tutto: cosi' nemmeno le armoniche che genera il
            // clipper possono salire sopra il taglio.
            if (p_.tosaOn && muroAcceso()) {
                for (int k = 0; k < 4; ++k) { l = topL_[k].process(l); r = topR_[k].process(r); }
            }

            // --- il ritardo dell'anticipo, SEMPRE ------------------------
            // ⚠️ 12/08/2026: prima il ritardo si applicava solo con il
            // limitatore acceso, mentre la latenza dichiarata all'host era
            // sempre 10 ms. Risultato: a limitatore spento l'host compensava
            // un ritardo che non c'era, e se nella sessione esisteva un
            // secondo percorso (una mandata, un parallelo) i due si sommavano
            // sfasati -- filtro a pettine, cioe' una risonanza metallica che
            // compare «senza motivo». Il ritardo ora c'e' sempre.
            double yl, yr;
            {
                const size_t k = (size_t) pos_ * 2;
                yl = rit_[k]; yr = rit_[k + 1];
                rit_[k] = (float) l; rit_[k + 1] = (float) r;
                if (++pos_ >= anticipo_) pos_ = 0;
            }

            if (p_.limit) {
                const double picco = std::max(std::abs(l), std::abs(r));
                const double serve = (picco > tetto && picco > 1e-12) ? tetto / picco : 1.0;

                // minimo scorrevole sulla finestra dell'anticipo
                finestra_[(size_t) posF_] = (float) serve;
                if (++posF_ >= (int) finestra_.size()) posF_ = 0;
                double minimo = 1.0;
                for (float v : finestra_) minimo = std::min(minimo, (double) v);

                // Niente gradini: il guadagno scende con una costante di tempo
                // pari a una frazione dell'anticipo e risale col rilascio. Un
                // gradino dentro il periodo di un'onda bassa e' distorsione
                // pura -- misurata al 3% su un 60 Hz prima di questa correzione.
                if (minimo < g_) g_ += (minimo - g_) * attacco_;
                else             g_ += (minimo - g_) * rilascio_;

                // l'audio esce gia' ritardato: il guadagno e' quindi sceso
                // prima che arrivi il picco che lo ha causato
                yl *= g_; yr *= g_;
                // ⚠️ 25/10/2026 — IL TETTO NON TENEVA. Il guadagno scende con una
                // costante di tempo (niente gradini), quindi su un picco ripido
                // arrivava in ritardo: x_out -1.95 dava picchi a -1.38, e con
                // l'ingresso a 30 si sforava di quasi un dB. Qui l'ultima rete:
                // quello che resta sopra il tetto si taglia. Lavora solo sulle
                // poche punte che il limitatore lascia scappare.
                yl = std::max(-tetto, std::min(tetto, yl));
                yr = std::max(-tetto, std::min(tetto, yr));
                grDb_ = 20.0 * std::log10(std::max(g_, 1e-6));
            } else {
                yl *= gout; yr *= gout;      // gia' ritardati
                g_ = 1.0; grDb_ = 0.0;
            }

            // --- miscela col secco --------------------------------------
            // ⚠️ anche il segnale secco della miscela va ritardato dello
            // stesso anticipo: sommare secco e trattato sfasati di 10 ms e'
            // di nuovo un filtro a pettine, stavolta dentro il plugin.
            const size_t ks = (size_t) posS_ * 2;
            const double dL = ritS_[ks], dR = ritS_[ks + 1];
            ritS_[ks] = (float) secchiL; ritS_[ks + 1] = (float) secchiR;
            if (++posS_ >= anticipo_) posS_ = 0;

            L[i] = (float) (yl * p_.mix + dL * (1.0 - p_.mix));
            R[i] = (float) (yr * p_.mix + dR * (1.0 - p_.mix));
        }
    }

private:
    // Il muro in cima e' acceso solo dove ha senso: sotto i 100 Hz e sopra
    // i 19.5 kHz non e' un muro, e' un errore di manovra.
    inline bool muroAcceso() const { return p_.topHz > 100.0 && p_.topHz < 19500.0; }
    static double db2lin(double db) { return std::pow(10.0, db * 0.05); }

    // La curva. A shape 0 e' la retta; salendo diventa una tangente iperbolica
    // sempre piu' curva. La spinta viene restituita in uscita, cosi' aumenta la
    // densita' e non il volume.
    inline double satura(double x, int c) {
        const double k = 2.5;      // curvatura fissa: la quantita' la fa shape
        // 2x fatto come si deve: si infila uno zero fra due campioni (e si
        // raddoppia, perche' lo zero dimezza l'energia), UN filtro solo li
        // percorre in fila, si deforma, un secondo filtro li ripulisce e si
        // tiene il secondo dei due. Due passate di biquad per campione.
        double y = 0.0;
        for (int s = 0; s < 2; ++s) {
            const double u = su_[c][0].process(s == 0 ? x * 2.0 : 0.0);
            // Miscela fra la retta e la tangente iperbolica. Diviso k, la
            // pendenza a segnale piccolo resta 1: i passaggi piano non
            // vengono toccati, si curva solo dove il segnale e' grosso.
            // La corsa della manopola non e' lineare: al quadrato. Cosi' la
            // prima meta' e' quasi trasparente e la seconda spinge davvero.
            // Misurato: a meta' corsa 1.4% di armoniche invece di 5.7.
            const double q = p_.shape * p_.shape;
            const double d = (1.0 - q) * u + q * (std::tanh(k * u) / k);
            y = giu_[c][0].process(d);
        }
        return y;
    }

    // Il clipper finale a 2x: stesso impianto della saturazione, filtri
    // suoi. Tosare a frequenza semplice ripiega le armoniche dentro la
    // banda e si sente come una patina; a 2x stanno sopra e il filtro di
    // ritorno se le porta via.
    inline double tosa2x(double x, int c, double soglia) {
        double y = 0.0;
        for (int s = 0; s < 2; ++s) {
            const double u = clSu_[c].process(s == 0 ? x * 2.0 : 0.0);
            y = clGiu_[c].process(tosa(u, soglia));
        }
        return y;
    }

    void aggiorna() {
        // ⚠️ 25/10/2026 — LA SPAZIOSITA' ERA SPENTA (revisione di tutti i cursori).
        // Queste due righe erano finite DENTRO il blocco del limitatore acuti:
        // la spaziosita' si accendeva solo con 6.13 sotto zero. Con il limitatore
        // acuti a 0 il lato restava a -7.6 dB qualunque valore avesse 6.6.
        spazioOn_ = (p_.spazio > 0.001);
        spLpL_.set(std::max(200.0, p_.spazioHz), sr_); spMo1_.hp(150.0, sr_); spMo2_.hp(150.0, sr_);
        anticipo_ = std::max(1, (int) (p_.anticipoMs * 0.001 * sr_));
        rilascio_ = 1.0 - std::exp(-1.0 / (std::max(1.0, p_.rilascioMs) * 0.001 * sr_));
        attacco_  = 1.0 - std::exp(-1.0 / (std::max(0.2, p_.anticipoMs * 0.5) * 0.001 * sr_));
        aMaSu_  = 1.0 - std::exp(-1.0 / (0.004 * sr_));   // 4 ms  in attacco
        // Rilascio lungo = distanza. Se il guadagno risale in fretta la zona
        // torna avanti fra un colpo e l'altro e si sente respirare; se resta
        // giu' un secondo o due, quella regione sta semplicemente dietro.
        aMaGiu_ = 1.0 - std::exp(-1.0 / (std::max(0.05, p_.maRel * 0.001) * sr_));
        // il basso: attacco 15 ms -- lascia passare il colpo -- e rilascio corto
        aBsSu_  = 1.0 - std::exp(-1.0 / (0.015 * sr_));
        aBsGiu_ = 1.0 - std::exp(-1.0 / (std::max(0.02, p_.bsRel * 0.001) * sr_));
        bandaBsL_.set(p_.bsHz, std::max(0.5, p_.bsQ), sr_);
        bandaBsR_.set(p_.bsHz, std::max(0.5, p_.bsQ), sr_);
        campBs_[0].set(p_.bsHz, p_.bsQ, bsDbOra_, sr_);
        campBs_[1].set(p_.bsHz, p_.bsQ, bsDbOra_, sr_);
        bandaL_.set(p_.maHz, std::max(0.5, p_.maQ), sr_);
        bandaR_.set(p_.maHz, std::max(0.5, p_.maQ), sr_);
        camp_[0].set(p_.maHz, p_.maQ, maDbOra_, sr_);
        camp_[1].set(p_.maHz, p_.maQ, maDbOra_, sr_);

        // --- i quattro stadi nuovi ---------------------------------------
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 4; ++k) rot_[c][k].set(p_.rotHz, sr_);
        bcLp_[0].set(std::max(50.0, p_.bcHz), sr_);
        armA_.set(std::max(40.0, p_.armHz), sr_);
        armB_.set(std::max(40.0, p_.armHz), sr_);
        armC_.set(400.0, sr_);
        armD_.set(std::max(40.0, p_.armHz), sr_);
        derNorm_ = sr_ / (2.0 * M_PI * std::max(30.0, p_.armHz * 0.6));
        bcLp_[1].set(std::max(50.0, p_.bcHz), sr_);
        pre_on_ = (p_.preUs > 1.0);
        if (pre_on_) {
            // spinta 5 = circa +14 dB in cima. Piu' di cosi' e il clipper
            // non lavora piu' sul programma, lavora solo sui piatti.
            enfasi(p_.preUs * 1e-6, 5.0, sr_, pre_[0], de_[0]);
            enfasi(p_.preUs * 1e-6, 5.0, sr_, pre_[1], de_[1]);
        }
        blocco_ = pre_on_ || (p_.clipDb > 0.01) || (p_.hfDb < -0.01);
        hfDet_[0].set(p_.hfHz, 0.6, sr_);
        hfDet_[1].set(p_.hfHz, 0.6, sr_);
        // acuti: attacco 1 ms -- devono essere presi al volo -- e rilascio
        // 30 ms, corto, perche' un freno lungo sugli acuti si sente come un
        // buco che si apre e si chiude.
        aHfSu_  = 1.0 - std::exp(-1.0 / (0.001 * sr_));
        aHfGiu_ = 1.0 - std::exp(-1.0 / (0.030 * sr_));
        aHfRis_ = 1.0 - std::exp(-32.0 / (0.030 * sr_));
        // il picco del programma: sale in 5 ms, scende in un secondo e mezzo
        aPicSu_  = 1.0 - std::exp(-1.0 / (0.005 * sr_));
        aPicGiu_ = 1.0 - std::exp(-1.0 / (1.500 * sr_));
        hfSc_[0].set(p_.hfHz, hfDbOra_, sr_);
        hfSc_[1].set(p_.hfHz, hfDbOra_, sr_);
        if (muroAcceso()) {
            const double f = std::min(p_.topHz, sr_ * 0.47);
            for (int k = 0; k < 4; ++k) { topL_[k].set(f, sr_); topR_[k].set(f, sr_); }
        }
    }

    Params p_;
    double sr_ = 48000.0, g_ = 1.0, grDb_ = 0.0, rilascio_ = 0.01, attacco_ = 0.5;
    int anticipo_ = 72, pos_ = 0, posS_ = 0, posF_ = 0;
    std::vector<float> rit_, ritS_, finestra_;
    Lp su_[2][2], giu_[2][2];
    Campana camp_[2];
    Banda bandaL_, bandaR_;
    double envMa_ = 1e-12, aMaSu_ = 0.0, aMaGiu_ = 0.0, maDbOra_ = 0.0;
    int contaMa_ = 0;
    Campana campBs_[2];
    Banda bandaBsL_, bandaBsR_;
    double envBs_ = 1e-12, aBsSu_ = 0.0, aBsGiu_ = 0.0, bsDbOra_ = 0.0;
    int contaBs_ = 0;

    // --- i quattro stadi nuovi ------------------------------------------
    Ap1 rot_[2][4];
    Lp  bcLp_[2];
    Uno1 pre_[2], de_[2];
    bool pre_on_ = false, blocco_ = false;
    Scaffale hfSc_[2];
    Banda hfDet_[2];
    double clipDbOra_ = 0.0, picPrima_ = 0.0, picDopo_ = 0.0;
    double picco_ = 0.0, aPicSu_ = 0.0, aPicGiu_ = 0.0;
    // la spaziosita': un passa-basso del primo ordine sul lato
    struct Uno { double a=0, z=0;
        void set(double f, double sr){ a = std::exp(-2.0*M_PI*std::min(f, sr*0.45)/sr); }
        inline double process(double x){ z = x*(1.0-a) + z*a; return z; } };
    Uno spLpL_;
    // passa-alto a 2 poli (Butterworth) per il lato: due in fila = 4 poli
    struct Bq { double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
        void hp(double f, double sr){ const double w=2*M_PI*f/sr, c=std::cos(w), al=std::sin(w)/(2*0.70710678);
            const double a0=1+al; b0=(1+c)/2/a0; b1=-(1+c)/a0; b2=(1+c)/2/a0; a1=-2*c/a0; a2=(1-al)/a0; }
        inline double process(double x){ const double y=b0*x+z1; z1=b1*x-a1*y+z2; z2=b2*x-a2*y; return y; } };
    Bq spMo1_, spMo2_;
    bool spazioOn_ = false;
    double picBs_ = 1e-3;   // il picco del grave, per il clipper del basso
    Uno armA_, armB_, armC_, armD_;
    double envArm_ = 1e-4, gPrec_ = 0.0, derNorm_ = 1.0;
    int contaClip_ = 0;
    double envHf_ = 1e-12, aHfSu_ = 0.0, aHfGiu_ = 0.0, aHfRis_ = 0.0, hfDbOra_ = 0.0;
    int contaHf_ = 0;
    Lp clSu_[2], clGiu_[2];
    // quattro sezioni = 48 dB/ottava: sotto, il muro non e' un muro
    Lp topL_[4], topR_[4];
};

} // namespace omax
