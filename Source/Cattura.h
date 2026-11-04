#pragma once
#include <string>
// Audio del Mac dentro OptiBob Live, senza driver (macOS 14.4+). Stessa strada del vecchio OptiBob Live:
// una presa su tutto l'audio di sistema (zittisce la strada normale), letta da noi in un anello;
// il motore la pesca da li' a ogni blocco e JUCE suona solo l'uscita.
bool cattura_avvia(std::string& errore);
void cattura_ferma();
bool cattura_attiva();
// riempie L/R con n campioni d'audio del Mac alla frequenza srUscita. false = cattura spenta.
bool cattura_leggi(float* L, float* R, int n, double srUscita);
// nome dell'uscita che il Mac sta usando (es. la scheda audio): OptiBob Live suona li'
std::string cattura_uscitaMac();
// macOS prima del 14.4: il Mac suona su BlackHole, OptiBob lo legge e suona sull'uscita vera (rimessa all'uscita)
bool ponte_blackhole(std::string& uscitaVera);
std::string ponte_nome();
