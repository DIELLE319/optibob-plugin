// Il livellatore, e basta.
//
// Un solo compito: portare qualunque disco al livello di destinazione e
// tenercelo, senza che lo si senta lavorare. Non e' un compressore -- non ha
// soglia ne' rapporto: ha un BERSAGLIO e una velocita' massima con cui puo'
// muoversi, in dB al secondo, come sulle macchine da trasmissione.
//
// Regole di progetto, in ordine:
//  1. A bersaglio raggiunto il guadagno e' 1.000 -- identico al bypass.
//     Si verifica con un null test, non a orecchio.
//  2. Deve costare pochissimo: nessun filtro, nessuna convoluzione. Il livello
//     si misura ogni 32 campioni: per un movimento che dura secondi non cambia
//     niente e costa un trentaduesimo.
//  3. Funziona a qualunque frequenza: i tempi nascono da secondi, non da
//     numeri di campioni.
#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

namespace agc {

struct Params {
    double targetDb  = -18.0;   // dove porta il programma
    double upDbS     =   1.0;   // quanto in fretta puo' ALZARE (dB al secondo)
    double downDbS   =  12.0;   // quanto in fretta puo' ABBASSARE
    double gateDb    = -45.0;   // sotto questo livello il guadagno si congela
    // ⭐ 04/09 — Luca: «devi aumentare il ritardo cosi' hai tempo per
    // correggere». E' come lavora un Optimod vero: il rivelatore guarda il
    // segnale PRIMA che si senta, e quando la botta arriva all'uscita il
    // guadagno e' gia' quello giusto. Misurato al banco: il ballo
    // dell'uscita dell'AGC scende da 4.47 a 3.94 dB fra 0 e 80 ms.
    double anticipoMs =  0.0;
    double maxUpDb   =  18.0;   // quanto puo' alzare al massimo
    double maxDownDb =  18.0;   // quanto puo' abbassare
    double holdMs    = 200.0;   // fermo dopo una discesa, prima di risalire
    double rmsMs     = 300.0;   // finestra del rivelatore
    double hpfHz     =  60.0;   // taglio in ingresso; 0 = spento
    int    hpfSez    =     2;   // 1 = 12 dB/ott, 2 = 24, 3 = 36
    bool   pesato    =  true;   // misura pesata come l'udito invece che a energia
    // ⭐ 15/08/2026 — LE DUE VELOCITA'.
    // Segnalato da Luca: «se prendo la musica da TIDAL il livello e' gia'
    // controllato perche' normalizzano loro, ma con i brani dentro Musica
    // c'e' una tale differenza di livello che e' impossibile controllare».
    // Misurato sul banco: con la salita a 0.35 dB/s servono 41 secondi per
    // recuperare 8 dB. Su un brano di tre minuti e' il primo quarto che esce
    // piano.
    // Alzare la salita e basta pero' rovina l'altra meta': dentro la canzone
    // si sente la macchina respirare. Quindi due velocita': lenta per le
    // correzioni piccole -- quelle che stanno DENTRO un brano -- e veloce
    // quando lo scarto e' grosso e non passa, cioe' quando e' cambiato disco.
    // Misurato: con soglia a 6 dB il modo veloce si accendeva anche sui
    // ritornelli (+-4 dB dentro un brano fanno 8 dB di escursione) e il
    // guadagno si muoveva di 6.5 dB invece di 3.3 -- cioe' si sentiva
    // respirare. A 10 dB passa solo il cambio di disco.
    double saltoDb   =  10.0;   // oltre questo scarto si considera «altro brano»
    double saltoMs   = 800.0;   // ...ma solo se dura: cosi' una pausa non conta
    double saltoX    =   8.0;   // quante volte piu' svelto (1 = spento)
    // ⭐ 18/08/2026 — IL FRENO.
    // Segnalato da Luca: «certe volte quando cambia il brano entrano delle
    // botte e ci impiega a scendere, li' dove devo attaccare».
    // Due difetti sommati, tutti e due misurabili:
    //  · il rivelatore e' una media di 300 ms: un disco nuovo che parte forte
    //    lui lo VEDE dopo quasi un secondo, e per tutto quel secondo suona
    //    col guadagno del disco di prima -- che magari era +9;
    //  · poi scende a 12 dB al secondo, cioe' altri tre quarti di secondo
    //    per recuperarne nove.
    // Il freno e' un secondo rivelatore, rapidissimo, che non guida niente:
    // guarda solo se l'uscita ha scavalcato il bersaglio di piu' di
    // «finestra». Se e' successo, il guadagno scende alla velocita' del
    // freno finche' non rientra, e poi si toglie di mezzo. Dentro il brano
    // non si accende mai, quindi non cambia nulla di come respira.
    double frenoDb   =   6.0;   // di quanto puo' scavalcare prima che intervenga
    double frenoDbS  = 800.0;   // a che velocita' rientra (0 = freno spento)
    double frenoMs   =   3.0;   // quanto e' rapido l'occhio del freno
    // Solo in discesa: non alza mai, abbassa soltanto sopra il bersaglio.
    // Cosi' il Target diventa una soglia nel senso classico -- sotto non
    // tocca niente, sopra interviene -- e il modulo si comporta come un
    // compressore lentissimo invece che come un livellatore.
    bool   soloGiu   = false;
    // --- la forma del basso, PRIMA del livellatore ------------------------
    // Ricetta di Luca (12/08), che lui faceva con un equalizzatore davanti:
    // si alza il sub attorno ai 68 Hz e si toglie una fetta stretta a 117.
    // A 117 sta il rimbombo -- la nota che continua a suonare dopo il colpo --
    // mentre a 68 c'e' il peso che non sporca. Togliendo il primo, il basso
    // pesa senza impastare E il livellatore smette di inseguirlo: e' per
    // questo che il risultato e' piu' morbido e piu' schiacciato.
    double subHz  =  68.0, subDb = 0.0;          // scaffale basso
    double boomHz = 117.0, boomDb = 0.0, boomQ = 2.1;   // la fetta da togliere
};

// La curva con cui si MISURA (non con cui si suona).
//
// L'orecchio non conta i watt: a 2-4 kHz sente molto piu' che a 60 Hz. Se il
// livellatore misura l'energia grezza, un intro fatto di soli medio-alti gli
// sembra piano -- e lui alza, proprio dove l'orecchio e' piu' sensibile: ti
// spacca le orecchie. Al contrario un basso distorto pesa tanta energia e
// conta poco, e lui abbassa tutto per colpa sua.
//
// La correzione e' quella usata nelle trasmissioni: passa-alto attorno ai
// 40 Hz e scaffale di +4 dB sopra i 1500 Hz, applicati SOLO al segnale di
// misura. Il suono non viene toccato.
struct Pesatura {
    void set(double sr) {
        // scaffale alto, +4 dB da 1681 Hz
        const double A = std::pow(10.0, 4.0 / 40.0);
        const double w = 2.0 * M_PI * 1681.0 / sr;
        const double cs = std::cos(w), sn = std::sin(w);
        const double al = sn / 2.0 * std::sqrt((A + 1.0/A) * (1.0/0.70710678 - 1.0) + 2.0);
        const double sq = 2.0 * std::sqrt(A) * al;
        const double b0 =    A*((A+1) + (A-1)*cs + sq);
        const double b1 = -2*A*((A-1) + (A+1)*cs);
        const double b2 =    A*((A+1) + (A-1)*cs - sq);
        const double a0 =       (A+1) - (A-1)*cs + sq;
        const double a1 =    2*((A-1) - (A+1)*cs);
        const double a2 =       (A+1) - (A-1)*cs - sq;
        sb0_=b0/a0; sb1_=b1/a0; sb2_=b2/a0; sa1_=a1/a0; sa2_=a2/a0;
        // passa-alto a 40 Hz, 12 dB per ottava
        const double w2 = 2.0 * M_PI * 40.0 / sr;
        const double c2 = std::cos(w2), s2 = std::sin(w2);
        const double a  = s2 / (2.0 * 0.70710678);
        const double B0 = (1.0 + c2) * 0.5, B1 = -(1.0 + c2), B2 = B0;
        const double A0 = 1.0 + a, A1 = -2.0 * c2, A2 = 1.0 - a;
        hb0_=B0/A0; hb1_=B1/A0; hb2_=B2/A0; ha1_=A1/A0; ha2_=A2/A0;
    }
    void reset() { sx1_=sx2_=sy1_=sy2_=hx1_=hx2_=hy1_=hy2_=0.0; }
    inline double process(double x) {
        const double y1 = sb0_*x + sb1_*sx1_ + sb2_*sx2_ - sa1_*sy1_ - sa2_*sy2_;
        sx2_=sx1_; sx1_=x; sy2_=sy1_; sy1_=y1;
        const double y2 = hb0_*y1 + hb1_*hx1_ + hb2_*hx2_ - ha1_*hy1_ - ha2_*hy2_;
        hx2_=hx1_; hx1_=y1; hy2_=hy1_; hy1_=y2;
        return y2;
    }
    double sb0_=1,sb1_=0,sb2_=0,sa1_=0,sa2_=0,sx1_=0,sx2_=0,sy1_=0,sy2_=0;
    double hb0_=1,hb1_=0,hb2_=0,ha1_=0,ha2_=0,hx1_=0,hx2_=0,hy1_=0,hy2_=0;
};

// Passa-alto Butterworth, da 12 a 36 dB per ottava.
//
// A 12 dB/ottava, tagliando a 60 Hz, a 30 Hz passa ancora un terzo del
// segnale: sembra di non aver tagliato niente. Le macchine da trasmissione
// usano pendenze ben piu' ripide. Qui si mettono in fila fino a tre sezioni,
// con i fattori di merito giusti perche' la somma resti piatta in banda.
struct PassaAlto {
    void set(double fc, double sr, int sezioni) {
        sez_ = std::min(3, std::max(0, sezioni));
        if (fc <= 0.0) { sez_ = 0; return; }
        // fattori di merito di Butterworth per 2, 4 e 6 poli
        static const double Q1[3] = { 0.70710678, 0.54119610, 0.51763809 };
        static const double Q2[3] = { 0.0,        1.30656296, 0.70710678 };
        static const double Q3[3] = { 0.0,        0.0,        1.93185165 };
        const int k = sez_ - 1;
        const double q[3] = { Q1[k], Q2[k], Q3[k] };
        for (int n = 0; n < sez_; ++n) coef(n, fc, sr, q[n]);
    }
    void reset() { for (auto& b : b_) b = Sezione{}; }
    inline double process(double x) {
        for (int n = 0; n < sez_; ++n) x = b_[n].process(x);
        return x;
    }
private:
    struct Sezione {
        double b0=1,b1=0,b2=0,a1=0,a2=0,x1=0,x2=0,y1=0,y2=0;
        inline double process(double x) {
            const double y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;
            x2=x1; x1=x; y2=y1; y1=y; return y;
        }
    };
    void coef(int n, double fc, double sr, double q) {
        const double w = 2.0 * M_PI * fc / sr;
        const double cs = std::cos(w), sn = std::sin(w);
        const double al = sn / (2.0 * q);
        const double B0 = (1.0 + cs) * 0.5, B1 = -(1.0 + cs), B2 = B0;
        const double A0 = 1.0 + al, A1 = -2.0 * cs, A2 = 1.0 - al;
        b_[n].b0 = B0/A0; b_[n].b1 = B1/A0; b_[n].b2 = B2/A0;
        b_[n].a1 = A1/A0; b_[n].a2 = A2/A0;
    }
    Sezione b_[3];
    int sez_ = 1;
};

// Biquad con scaffale basso e campana: serve alla forma del basso.
struct Filtro {
    void reset() { x1=x2=y1=y2=0; }
    inline double process(double x) {
        const double y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;
        x2=x1; x1=x; y2=y1; y1=y; return y;
    }
    void scaffaleBasso(double f, double gDb, double sr) {
        const double A = std::pow(10.0, gDb/40.0);
        const double w = 2*M_PI*std::min(f, sr*0.45)/sr, c = std::cos(w), sn = std::sin(w);
        const double al = sn/2 * std::sqrt((A + 1/A)*(1/0.9 - 1) + 2);
        const double sq = 2*std::sqrt(A)*al;
        const double B0 = A*((A+1) - (A-1)*c + sq), B1 = 2*A*((A-1) - (A+1)*c);
        const double B2 = A*((A+1) - (A-1)*c - sq), A0 =    (A+1) + (A-1)*c + sq;
        const double A1 =  -2*((A-1) + (A+1)*c),    A2 =    (A+1) + (A-1)*c - sq;
        b0=B0/A0; b1=B1/A0; b2=B2/A0; a1=A1/A0; a2=A2/A0;
    }
    void campana(double f, double q, double gDb, double sr) {
        const double A = std::pow(10.0, gDb/40.0);
        const double w = 2*M_PI*std::min(f, sr*0.45)/sr, c = std::cos(w), sn = std::sin(w);
        const double al = sn/(2*q);
        const double A0 = 1 + al/A;
        b0 = (1 + al*A)/A0; b1 = (-2*c)/A0; b2 = (1 - al*A)/A0;
        a1 = (-2*c)/A0;     a2 = (1 - al/A)/A0;
    }
    double b0=1,b1=0,b2=0,a1=0,a2=0,x1=0,x2=0,y1=0,y2=0;
};

class Agc {
public:
    void prepare(double sr) {
        sr_ = (sr > 0.0) ? sr : 48000.0;
        aggiorna();
        gDb_ = 0.0; gLin_ = 1.0; env_ = 1e-20; hold_ = 0; conta_ = 0; attesaSalto_ = 0;
        envF_ = 1e-10; frenoDest_ = 99.0;
        bersaglioDb_ = 0.0; lvl_ = -100.0;
        hpfL_.reset(); hpfR_.reset(); pesa_.reset();
        for (int c = 0; c < 2; ++c) { sub_[c].reset(); boom_[c].reset(); }
    }

    // ⭐⭐ 19/09/2026 — IL PAVIMENTO SI ACCENDE CON UN INTERRUTTORE, NON
    // RICALCOLANDO I PARAMETRI. Ieri per congelare il livellatore spostavo
    // p_.gateDb e richiamavo setParams() DAL FILO DELL'AUDIO: setParams
    // rifa' tutti i coefficienti, ed e' esattamente il mestiere che questo
    // file (18/08) dice di non fare li' dentro. Un blocco che sfora il suo
    // tempo non produce un errore: produce silenzio.
    bool tieni_ = false;
    void setParams(const Params& p) { p_ = p; aggiorna(); }
    double gainDb()    const { return gDb_; }
    double livelloDb() const { return lvl_; }

    // stereo, in-place. Un guadagno solo per i due canali: se si muovessero
    // separati l'immagine ballerebbe.
    inline void process(float* L, float* R, int n) {
        for (int i = 0; i < n; ++i) {
            // il taglio agisce PRIMA di tutto: sul suono e sulla misura
            double xl = hpfL_.process((double) L[i]);
            double xr = hpfR_.process((double) R[i]);
            if (p_.subDb  >  0.01) { xl = sub_[0].process(xl);  xr = sub_[1].process(xr); }
            if (p_.boomDb < -0.01) { xl = boom_[0].process(xl); xr = boom_[1].process(xr); }
            const double m = 0.5 * (xl + xr);
            // il segnale di MISURA passa dalla pesatura; il suono no
            const double mm = p_.pesato ? pesa_.process(m) : m;
            env_ += aRms_ * (mm * mm - env_);
            // l'occhio del freno: attacco quasi istantaneo, rilascio breve
            const double pk = std::abs(mm);
            envF_ += ((pk > envF_) ? aFreA_ : aFreR_) * (pk - envF_);

            if (--conta_ <= 0) {
                conta_ = kPasso;
                lvl_ = 10.0 * std::log10(std::max(env_, 1e-20));
                if (lvl_ > p_.gateDb && !tieni_) {
                    bersaglioDb_ = std::min(std::max(p_.targetDb - lvl_,
                                                     -p_.maxDownDb), p_.maxUpDb);
                    if (p_.soloGiu && bersaglioDb_ > 0.0) bersaglioDb_ = 0.0;
                } else
                    bersaglioDb_ = gDb_;      // nei silenzi non si tira su niente

                // Il freno guarda l'USCITA: livello rapido piu' il guadagno
                // che sto applicando adesso. Se ha scavalcato il tetto della
                // finestra, il suo bersaglio e' il guadagno che lo rimette
                // esattamente sul bordo. Se non l'ha scavalcato, si mette
                // fuori portata e non conta.
                if (p_.frenoDbS > 0.01) {
                    const double lvlF = 20.0 * std::log10(std::max(envF_, 1e-10));
                    frenoDest_ = std::max(p_.targetDb + p_.frenoDb - lvlF, -p_.maxDownDb);
                } else frenoDest_ = 99.0;
            }

            // --- le due velocita' -------------------------------------
            // Si guarda quanto manca. Se manca tanto e continua a mancare,
            // non e' una sfumatura del brano: e' cambiato disco. Allora si
            // corre. Appena rientrati, si torna lenti.
            const double manca = std::abs(bersaglioDb_ - gDb_);
            if (p_.saltoX > 1.0) {
                if (manca > p_.saltoDb) {
                    if (attesaSalto_ < (int) (p_.saltoMs * 0.001 * sr_)) attesaSalto_ += kPasso;
                } else if (manca < 1.5) {
                    attesaSalto_ = 0;               // rientrati: si rallenta
                }
            } else attesaSalto_ = 0;
            const bool corri = (p_.saltoX > 1.0) &&
                               (attesaSalto_ >= (int) (p_.saltoMs * 0.001 * sr_));
            // ⚠️ 18/08 — la corsa valeva SOLO in salita. Era sbagliato:
            // il disco che entra dieci dB piu' forte e' esattamente il caso
            // per cui la corsa esiste, e li' bisogna SCENDERE. Il timore di
            // «mangiare l'attacco» non regge, perche' la corsa si accende
            // solo dopo che lo scarto e' rimasto sopra i dieci dB per otto
            // decimi di secondo: l'attacco e' gia' passato da un pezzo.
            const double pSu  = corri ? passoSu_  * p_.saltoX : passoSu_;
            const double rSu  = corri ? db2lin(pSu) : ratSu_;
            const double pGiu = corri ? passoGiu_ * p_.saltoX : passoGiu_;
            const double rGiu = corri ? db2lin(-pGiu) : ratGiu_;

            // Dove devo andare, e con che passo. Di norma e' il bersaglio
            // del livellatore; ma se il freno chiede di scendere PIU' in
            // basso, comanda lui e col suo passo -- finche' non e' rientrato.
            // ⚠️ 18/08 (secondo giro) — il freno non serviva a niente sulla
            // taratura vera. Chiedeva di scendere PIU' in basso del
            // bersaglio; ma con la finestra a 4 dB il bersaglio e' gia'
            // attaccato al fondo, quindi il freno non aveva mai niente da
            // aggiungere e il guadagno scendeva alla velocita' lenta --
            // mezzo dB al secondo, otto secondi per quattro dB. La botta
            // restava, ed e' esattamente quello che Luca sentiva.
            // La condizione giusta non e' «piu' in basso del bersaglio»: e'
            // «l'uscita ha scavalcato». Se ha scavalcato, si scende con la
            // velocita' del freno, verso il piu' basso dei due.
            double dest = bersaglioDb_, giu = pGiu, rgiu = rGiu;
            if (frenoDest_ < gDb_ - 1e-9) {
                dest = std::min(dest, frenoDest_);
                giu = passoFreno_; rgiu = ratFreno_;
            }

            if (dest < gDb_ - 1e-9) {                  // scendere
                gDb_ -= giu;  gLin_ *= rgiu;
                if (gDb_ <= dest) { gDb_ = dest; gLin_ = db2lin(gDb_); }
                hold_ = holdCamp_;
            } else if (bersaglioDb_ > gDb_ + 1e-9) {   // salire: lenti, e dopo l'attesa
                if (hold_ > 0) --hold_;
                else {
                    gDb_ += pSu;  gLin_ *= rSu;
                    if (gDb_ >= bersaglioDb_) { gDb_ = bersaglioDb_; gLin_ = db2lin(gDb_); }
                }
            }

            if (rit_ > 0) {
                const int j = (scrivi_ + 1) % rit_;
                const double dl = ritL_[j], dr = ritR_[j];
                ritL_[j] = xl; ritR_[j] = xr; scrivi_ = j;
                L[i] = (float) (dl * gLin_);
                R[i] = (float) (dr * gLin_);
            } else {
                L[i] = (float) (xl * gLin_);
                R[i] = (float) (xr * gLin_);
            }
        }
    }

private:
    static constexpr int kPasso = 32;

    static double db2lin(double db) { return std::pow(10.0, db * 0.05); }

    void aggiorna() {
        const double tau = std::max(1.0, p_.rmsMs) * 0.001;
        aRms_     = 1.0 - std::exp(-1.0 / (tau * sr_));
        {
            const int n = (int) (std::max(0.0, p_.anticipoMs) * 0.001 * sr_);
            if (n != rit_) {
                rit_ = n;
                ritL_.assign(std::max(1, n), 0.0);
                ritR_.assign(std::max(1, n), 0.0);
                scrivi_ = 0;
            }
        }
        passoSu_  = std::max(0.0, p_.upDbS)   / sr_;
        passoGiu_ = std::max(0.0, p_.downDbS) / sr_;
        ratSu_    = db2lin( passoSu_);
        ratGiu_   = db2lin(-passoGiu_);
        holdCamp_ = (long long) (std::max(0.0, p_.holdMs) * 0.001 * sr_);
        passoFreno_ = std::max(0.0, p_.frenoDbS) / sr_;
        ratFreno_   = db2lin(-passoFreno_);
        aFreA_ = 1.0 - std::exp(-1.0 / (std::max(0.2, p_.frenoMs) * 0.001 * sr_));
        aFreR_ = 1.0 - std::exp(-1.0 / (0.150 * sr_));
        hpfL_.set(p_.hpfHz, sr_, p_.hpfSez); hpfR_.set(p_.hpfHz, sr_, p_.hpfSez);
        pesa_.set(sr_);
        for (int c = 0; c < 2; ++c) {
            sub_[c].scaffaleBasso(p_.subHz, p_.subDb, sr_);
            boom_[c].campana(p_.boomHz, p_.boomQ, p_.boomDb, sr_);
        }
    }

    Params p_;
    double sr_ = 48000.0, aRms_ = 0.0, env_ = 1e-20;
    double gDb_ = 0.0, gLin_ = 1.0, bersaglioDb_ = 0.0, lvl_ = -100.0;
    std::vector<double> ritL_, ritR_;
    int rit_ = 0, scrivi_ = 0;
    double passoSu_ = 0.0, passoGiu_ = 0.0, ratSu_ = 1.0, ratGiu_ = 1.0;
    double envF_ = 1e-10, frenoDest_ = 99.0, passoFreno_ = 0.0, ratFreno_ = 1.0;
    double aFreA_ = 1.0, aFreR_ = 0.0;
    long long hold_ = 0, holdCamp_ = 0;
    int conta_ = 0, attesaSalto_ = 0;
    PassaAlto hpfL_, hpfR_;
    Pesatura  pesa_;
    Filtro    sub_[2], boom_[2];
};

} // namespace agc
