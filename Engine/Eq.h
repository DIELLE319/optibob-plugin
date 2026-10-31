// L'equalizzatore davanti a tutto.
//
// Nasce da una frase di Luca del 17/08: «se il suono giusto lo ottenevo
// banalmente con un equalizzatore di Logic, perche' a monte non gli mettiamo
// un EQ cosi'?». Giusto — e per giunta la macchina vera ce l'ha: Low bass
// boost, Mid bass boost, Presence e Brilliance dell'8200 sono esattamente
// un equalizzatore davanti al processore.
//
// I comandi sono quelli del Channel EQ di Logic, uno per uno, cosi' i valori
// si travasano senza conversioni:
//   passa-alto (con pendenza) · scaffale basso · tre campane · scaffale alto
//   · passa-basso
//
// ⚠️ Sta PRIMA dell'AGC, quindi il rivelatore sente il segnale gia'
// equalizzato: e' come lo vuole la macchina, e come lo fa Luca in Logic.
// Attenzione pero': quello che si taglia qui, il multibanda dietro tende a
// rimetterlo (la banda con meno energia comprime di meno). Per i tagli che
// devono restare c'e' la campana fissa nel FINALE.
#pragma once
#include <cmath>
#include <algorithm>   // std::min / std::max

namespace oeq {

struct Bq {
    void reset() { x1=x2=y1=y2=0; }
    inline double process(double x) {
        const double y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;
        x2=x1; x1=x; y2=y1; y1=y; return y;
    }
    void passaAlto(double f, double q, double sr) {
        const double w = 2*M_PI*std::min(f, sr*0.45)/sr, c = std::cos(w), s = std::sin(w);
        const double al = s/(2*std::max(0.05, q));
        const double a0 = 1+al;
        b0=(1+c)/2/a0; b1=-(1+c)/a0; b2=b0; a1=(-2*c)/a0; a2=(1-al)/a0;
    }
    void passaBasso(double f, double q, double sr) {
        const double w = 2*M_PI*std::min(f, sr*0.45)/sr, c = std::cos(w), s = std::sin(w);
        const double al = s/(2*std::max(0.05, q));
        const double a0 = 1+al;
        b0=(1-c)/2/a0; b1=(1-c)/a0; b2=b0; a1=(-2*c)/a0; a2=(1-al)/a0;
    }
    void campana(double f, double q, double g, double sr) {
        const double A = std::pow(10.0, g/40.0);
        const double w = 2*M_PI*std::min(f, sr*0.45)/sr, c = std::cos(w), s = std::sin(w);
        const double al = s/(2*std::max(0.1, q));
        const double a0 = 1+al/A;
        b0=(1+al*A)/a0; b1=(-2*c)/a0; b2=(1-al*A)/a0; a1=(-2*c)/a0; a2=(1-al/A)/a0;
    }
    void scaffaleBasso(double f, double g, double sr) {
        const double A = std::pow(10.0, g/40.0);
        const double w = 2*M_PI*std::min(f, sr*0.45)/sr, c = std::cos(w), s = std::sin(w);
        const double al = s/2*std::sqrt(2.0), sA = 2*std::sqrt(A)*al;
        const double a0 = (A+1)+(A-1)*c+sA;
        b0=A*((A+1)-(A-1)*c+sA)/a0;  b1=2*A*((A-1)-(A+1)*c)/a0;
        b2=A*((A+1)-(A-1)*c-sA)/a0;  a1=-2*((A-1)+(A+1)*c)/a0;
        a2=((A+1)+(A-1)*c-sA)/a0;
    }
    void scaffaleAlto(double f, double g, double sr) {
        const double A = std::pow(10.0, g/40.0);
        const double w = 2*M_PI*std::min(f, sr*0.45)/sr, c = std::cos(w), s = std::sin(w);
        const double al = s/2*std::sqrt(2.0), sA = 2*std::sqrt(A)*al;
        const double a0 = (A+1)-(A-1)*c+sA;
        b0=A*((A+1)+(A-1)*c+sA)/a0;  b1=-2*A*((A-1)+(A+1)*c)/a0;
        b2=A*((A+1)+(A-1)*c-sA)/a0;  a1=2*((A-1)-(A+1)*c)/a0;
        a2=((A+1)-(A-1)*c-sA)/a0;
    }
    double b0=1,b1=0,b2=0,a1=0,a2=0,x1=0,x2=0,y1=0,y2=0;
};

struct Params {
    double hpHz = 0.0,  hpQ = 0.707;  int hpSez = 1;   // 1 = 12 dB/ott, 2 = 24
    double lsHz = 68.0, lsDb = 0.0;                    // scaffale basso
    double b1Hz = 117.0, b1Db = 0.0, b1Q = 2.1;        // campana 1
    double b2Hz = 250.0, b2Db = 0.0, b2Q = 1.0;
    double b3Hz = 2500.0, b3Db = 0.0, b3Q = 1.0;
    double hsHz = 10000.0, hsDb = 0.0;                 // scaffale alto
    double lpHz = 0.0,  lpQ = 0.707;  int lpSez = 2;
};

class Eq {
public:
    void prepare(double sr) { sr_ = sr > 0 ? sr : 48000.0; azzera(); aggiorna(); }
    void setParams(const Params& p) { p_ = p; aggiorna(); }

    inline void process(float* L, float* R, int n) {
        if (!attivo_) return;                 // tutto a zero: non si tocca niente
        for (int i = 0; i < n; ++i) {
            double l = L[i], r = R[i];
            if (p_.hpHz > 20.0)  for (int k=0;k<p_.hpSez;++k){ l=hp_[0][k].process(l); r=hp_[1][k].process(r); }
            if (std::abs(p_.lsDb) > 0.01) { l=ls_[0].process(l); r=ls_[1].process(r); }
            if (std::abs(p_.b1Db) > 0.01) { l=c1_[0].process(l); r=c1_[1].process(r); }
            if (std::abs(p_.b2Db) > 0.01) { l=c2_[0].process(l); r=c2_[1].process(r); }
            if (std::abs(p_.b3Db) > 0.01) { l=c3_[0].process(l); r=c3_[1].process(r); }
            if (std::abs(p_.hsDb) > 0.01) { l=hs_[0].process(l); r=hs_[1].process(r); }
            if (p_.lpHz > 1000.0 && p_.lpHz < sr_*0.45)
                for (int k=0;k<p_.lpSez;++k){ l=lp_[0][k].process(l); r=lp_[1][k].process(r); }
            L[i] = (float) l; R[i] = (float) r;
        }
    }

private:
    void azzera() {
        for (int c=0;c<2;++c) {
            for (int k=0;k<2;++k) { hp_[c][k].reset(); lp_[c][k].reset(); }
            ls_[c].reset(); c1_[c].reset(); c2_[c].reset(); c3_[c].reset(); hs_[c].reset();
        }
    }
    void aggiorna() {
        for (int c=0;c<2;++c) {
            for (int k=0;k<2;++k) {
                hp_[c][k].passaAlto(std::max(20.0,p_.hpHz), p_.hpQ, sr_);
                lp_[c][k].passaBasso(p_.lpHz > 1000 ? p_.lpHz : 20000.0, p_.lpQ, sr_);
            }
            ls_[c].scaffaleBasso(p_.lsHz, p_.lsDb, sr_);
            c1_[c].campana(p_.b1Hz, p_.b1Q, p_.b1Db, sr_);
            c2_[c].campana(p_.b2Hz, p_.b2Q, p_.b2Db, sr_);
            c3_[c].campana(p_.b3Hz, p_.b3Q, p_.b3Db, sr_);
            hs_[c].scaffaleAlto(p_.hsHz, p_.hsDb, sr_);
        }
        // Se non c'e' niente da fare, l'equalizzatore si toglie di mezzo del
        // tutto: a comandi a zero il segnale deve uscire identico.
        attivo_ = (p_.hpHz > 20.0) || (p_.lpHz > 1000.0 && p_.lpHz < 19999.0)
               || std::abs(p_.lsDb) > 0.01 || std::abs(p_.b1Db) > 0.01
               || std::abs(p_.b2Db) > 0.01 || std::abs(p_.b3Db) > 0.01
               || std::abs(p_.hsDb) > 0.01;
    }
    Params p_;
    double sr_ = 48000.0;
    bool attivo_ = false;
    Bq hp_[2][2], lp_[2][2], ls_[2], c1_[2], c2_[2], c3_[2], hs_[2];
};

} // namespace oeq
