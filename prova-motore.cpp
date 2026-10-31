// Prova di correttezza del motore su Windows (31/10/2026).
// Compila il motore VERO (gli stessi header del plugin) con un segnale di
// prova deterministico e stampa alcune misure. La build lo fa girare su
// Windows e su Mac/Linux: se i numeri coincidono, su Windows suona identico.
// Non fa parte del prodotto: e' solo il collaudo automatico.
#define ESPORTA(n) extern "C"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <memory>
#include <algorithm>
#include "Engine/Eq.h"
#include "Engine/Agc.h"
#include "Engine/Mb.h"
#include "Engine/Max.h"
#include "Engine/catena.inc"
#include "Engine/applica.inc"

int main() {
    const double sr = 48000.0;
    const int N = (int)(sr * 2.0);               // 2 secondi
    std::vector<float> L(N), R(N);
    // segnale deterministico: tre sinusoidi + una cassa ogni mezzo secondo
    for (int k = 0; k < N; ++k) {
        double t = k / sr;
        double s = 0.30 * std::sin(2*M_PI*110.0*t)
                 + 0.20 * std::sin(2*M_PI*440.0*t)
                 + 0.12 * std::sin(2*M_PI*3000.0*t);
        if (k % (int)(sr*0.5) < 60) s += 0.6 * std::sin(2*M_PI*60.0*t);  // colpo
        L[k] = (float)s; R[k] = (float)(s * 0.98);
    }
    Catena c;
    c.val.clear();                                // valori di fabbrica
    auto i = c.val.find("x_top"); if (i!=c.val.end() && i->second<100.0) i->second=20000.0;
    c.prepara(sr);
    c.applica();
    const int B = 512;
    for (int p = 0; p < N; p += B) c.elabora(L.data()+p, R.data()+p, std::min(B, N-p));
    double picco = 0, somma = 0; long double checksum = 0;
    for (int k = 0; k < N; ++k) {
        double a = std::fabs(L[k]), b = std::fabs(R[k]);
        picco = std::max(picco, std::max(a,b));
        somma += (double)L[k]*L[k] + (double)R[k]*R[k];
        checksum += (long double)L[k] + (long double)R[k];
    }
    double rms = std::sqrt(somma / (2.0*N));
    std::printf("picco %.6f  rms %.6f  checksum %.3Lf\n", picco, rms, checksum);
    return 0;
}
