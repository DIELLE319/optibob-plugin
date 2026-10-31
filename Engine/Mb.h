// Il multibanda, con le manopole di un compressore.
//
// Concetto, come l'ha detto Luca: per ogni banda si segna il MINIMO sotto cui
// non si tocca niente e il MASSIMO oltre cui non la si lascia andare; dentro
// quella finestra si schiaccia. E' esattamente il principio dell'Optimod --
// loro lo chiamano bersaglio, qui si chiama soglia -- ma detto con le parole
// di chi ha sempre usato compressori.
//
//   sotto Threshold      -> non fa niente
//   sopra Threshold      -> comprime col Ratio
//   sopra Ceiling        -> non passa: quella banda non sale oltre
//
// Cinque bande, incroci Linkwitz-Riley del 4o ordine (24 dB/ottava). Sotto la
// prima banda c'e' un taglio: li' non c'e' musica.
#pragma once
#include <cmath>
#include <algorithm>

namespace omb {

constexpr int kB = 5;

struct Banda {
    // ⭐ 30/08/2026 — ogni banda si spegne per conto suo. Spenta non sparisce
    // (se sparisse mancherebbe quella fetta di spettro): passa e basta, col
    // suo guadagno fermo a zero. Serve a sentire cosa fa UNA banda, che e'
    // l'unico modo per tararla davvero.
    bool   accesa   = true;
    double soglia   = -40.0;   // Threshold: sotto, non tocca
    double tetto    =  -6.0;   // Ceiling: oltre non la lascia salire
    double rapporto =   4.0;   // Ratio
    double attaccoMs =  20.0;
    double rilascioMs = 150.0;
    double gainDb   =   0.0;   // trucco d'uscita della banda
    // ⭐ 26/09 — Luca: «manca per ogni banda il guadagno nel caso sia sotto
    // soglia». Sotto il cancello la banda andava a 0 dB: una banda che
    // lavorava a -15 risaliva di quindici dB appena la musica calava, ed e'
    // uno dei motivi per cui le bande ballano. Adesso sotto il cancello va
    // QUI, col suo rilascio. 0 = come prima.
    double sottoDb  =   0.0;
};

struct Params {
    Banda b[kB];
    double xover[kB - 1] = { 100.0, 400.0, 1500.0, 5000.0 };
    // La finestra d'ingresso, come la vede Luca: «mai sotto X, mai sopra Y».
    // DENTRO la finestra non si tocca niente -- ed e' la differenza con un
    // livellatore a bersaglio, che invece tira sempre verso un punto.
    double inMin  = -30.0;
    double inMax  = -12.0;
    double inDbS  =   6.0;     // quanto in fretta rientra nella finestra
    double hpfHz = 36.0;       // sotto la prima banda
    // ⭐ IL MODO RADIO. Con soglia e rapporto il livello continua a respirare:
    // si comprime di piu' quando entra tanto e di meno quando entra poco. Per
    // la radio e per il dj serve l'altra logica -- sopra il minimo la banda
    // viene PORTATA al massimo e ci resta. Non «non superare», ma «stai li'».
    // E' il motivo per cui l'Optimod suona sempre uguale.
    bool   bloccaAlTetto = false;
    // ⚠️ 14/08/2026 — LA CORREZIONE CHE CONTA.
    // Prima la soglia faceva DUE mestieri: cancello del silenzio e limite di
    // quanto si poteva alzare. Con le soglie messe vicino ai tetti succedeva
    // esattamente quello che Luca ha sentito: «quando non c'e' il basso lui
    // non fa il guadagno, lo limita solo perche' non superi». Giusto: sotto
    // soglia il guadagno restava a zero.
    // Ora i due mestieri sono separati. La soglia e' SOLO il cancello, sta
    // molto in basso e serve a non tirare su il silenzio fra un pezzo e
    // l'altro. Quanto puo' alzare e quanto puo' abbassare sono due comandi
    // loro. E' esattamente come e' fatto l'AGC, applicato per banda -- che
    // poi e' quello che un Optimod E'.
    // ⭐ 16/08/2026 — L'ACCOPPIAMENTO, rifatto come lo descrive il tecnico
    // Orban nel video sul bass coupling (trascritto in
    // STATUS-CORRENTE/8200-PAROLE-DEL-TECNICO.md, sezione «EL BASS
    // COUPLING — il brevetto Orban»).
    //
    // PRIMA VERSIONE SBAGLIATA: tiravo tutte e cinque le bande verso la
    // media. Non e' quello. Il comando dell'8200 dice «quanto la riduzione
    // di guadagno dei GRAVI deve seguire quella della MASTER» -- in una
    // direzione sola: il grave insegue il resto, non viceversa.
    //
    //   0%   = bande indipendenti, l'Orban «aggiunge il suo tocco di gravi»
    //   100% = i tempi di rilascio diventano gli stessi, e ci si avvicina
    //          al suono che arriva dal mixer
    //
    // Raccomandazione del tecnico: 70-100% SOLO per la classica; per pop,
    // rock, soul, oldies si sta BASSI, cosi' la macchina puo' metterci il
    // suo. E la coppia che decide tutto: accoppiamento basso + rilascio AGC
    // lento = aperto, caldo, poca fatica, l'ascoltatore resta. Accoppiamento
    // alto + rilascio veloce = freddo, denso, stanca.
    // ⭐ 21/08/2026 — L'UNIONE. Luca: «come faccio a renderlo ancora piu'
    // morbido, a fare entrare le bande una nell'altra».
    // Non e' l'accoppiamento del grave: quello e' il brevetto Orban, va in
    // una direzione sola e riguarda solo la banda 1. Questo e' l'altra cosa,
    // ed e' la differenza fra le due macchine vere: l'8100 del 1980 aveva le
    // bande legate quasi al 97%, l'8200 le ha libere. Legandole si torna
    // verso un compressore a banda larga, che e' il suono morbido e tutto
    // d'un pezzo; slegandole si guadagna densita' e costanza ma la macchina
    // «rifa' il mix» e a volte si sente.
    //   0%   = cinque bande indipendenti (8200)
    //   100% = una banda sola travestita da cinque (8100)
    // Si tira ogni banda verso la MEDIA delle cinque: quello che una fa da
    // sola lo fanno un po' tutte, ed e' letteralmente farle entrare una
    // nell'altra.
    // ⭐⭐ 21/08/2026 — I LIMITATORI DI BANDA. Lo stadio che mancava.
    // Luca: «l'impressione e' che le bande siano sole e non vengano
    // controllate, basta niente per andare in distorsione; non c'e' quel
    // suono gonfio fermo tipico di Orban dove avevi la CERTEZZA che nessun
    // suono uscisse o superasse la banda... sappiamo come fermarle ma non
    // sappiamo cosa succedera' con la canzone dopo». E: «quando lavoravo
    // come fonico con un Orban non ti preoccupavi mai dei livelli, ci
    // pensava lui».
    //
    // Aveva ragione, ed era un buco nostro, scritto nero su bianco nel
    // MANUALE fin dal primo giorno: «AGC per banda a bersaglio fisso ->
    // LIMITATORI DI BANDA -> clipper del basso e di banda -> somma».
    // I limitatori di banda non li abbiamo mai costruiti.
    //
    // Senza, il guadagno di banda e' l'unica cosa che tiene, ed e' LENTO per
    // forza (mezzo secondo di rivelatore, se no pompa). Quindi ogni
    // transiente scavalca il tetto e va a finire sul clipper finale: l'unica
    // rete e' la piu' brutta, e infatti «basta niente per distorcere».
    // La certezza di cui parla Luca non e' statistica, e' un muro: dentro
    // ogni banda, oltre quel livello non passa NIENTE. Attacco sullo stesso
    // campione, quindi zero sfondamento. La distorsione che genera resta
    // dentro la sua banda, dove il resto della musica la copre -- e' il
    // motivo per cui l'Optimod limita per bande invece che in fondo.
    // ⭐⭐ 21/08/2026 — LA SBAVATURA, e perche' serve.
    // Luca, dopo aver sentito la versione col muro: «come plugin e' un gran
    // plugin, spinge di brutto. Pero' se assomiglia a un Orban, no. Li' non
    // ci siamo: tutti i suoni sono chiaramente udibili, tutto lo spettro e'
    // udibile. L'Orban era un suono gonfio morbido che picchiava. L'unica
    // cosa che ci manca e' allontanarci dalla realta' per creare un suono
    // nuovo.» E il 16/08, la stessa cosa detta al contrario: «l'Orban
    // mischia tutti i suoni cosi' elude le differenze».
    //
    // Ha ragione, e il motivo e' che il nostro banco e' TROPPO PULITO. I
    // nostri incroci sono Linkwitz-Riley: sommano a uno esatto, ogni suono
    // sta in una banda sola e ci resta. Il MANUALE pero' dice, dal primo
    // giorno: «banco di filtri con bande SOVRAPPOSTE». Sovrapporre vuol dire
    // che una botta che sta nella banda 3 si sente anche nella 2 e nella 4,
    // e quindi abbassa anche loro: le bande smettono di essere scomparti e
    // il colpo si spande. E' quello che rende i suoni non piu' separabili.
    //
    // Rifare il banco con le bande sovrapposte vorrebbe dire buttare la
    // compensazione di fase che e' costata giorni. Ma l'effetto UDIBILE
    // della sovrapposizione non e' nel filtro: e' nel fatto che il guadagno
    // di una banda segue quello delle vicine. Quello si fa qui, sui
    // guadagni, senza toccare un filtro:
    //   0   = scomparti stagni (quello che abbiamo adesso)
    //   50  = ogni banda si porta dietro mezza dose delle due vicine
    // Non e' l'unione, che tira tutte e cinque verso la media: questa e'
    // locale, guarda solo chi ti sta accanto nello spettro. E' la differenza
    // fra «tutto si muove insieme» e «il colpo si spande».
    // ⭐⭐ 21/08/2026 — LA LIBERTA'. Il caso che l'ha fatta nascere, Luca:
    // «parte un brano che non ha bassi ma solo un piano e una voce
    // femminile; siccome lui tira su tutto, ha tirato fino a che sono stato
    // costretto ad abbassare».
    // Misurato sul banco con quel brano rifatto apposta (solo 180-4000 Hz):
    // la banda 5 andava a +12.0 dB con escursione 1.0 -- cioe' incollata al
    // fermo massimo per tutto il pezzo, a tirare su riverbero e soffio,
    // perche' li' musica non ce n'e'. E la banda 3, dove la musica c'era
    // davvero, stava a -4.0. Sedici dB di distanza: la macchina non stava
    // controllando un disco, ne stava ricostruendo uno che non esiste.
    //
    // Il cancello non basta: il riverbero di un piano sta sopra qualunque
    // cancello sensato. Quello che serve e' un vincolo fra le bande: una
    // banda non puo' allontanarsi piu' di tanto da quello che fanno le
    // altre. Se le altre non stanno alzando, vuol dire che il disco e' cosi'
    // -- e non c'e' niente da rimettere a posto.
    //
    // E' anche la risposta all'altra cosa che ricorda dell'Orban: «non ti
    // veniva di alzare, l'effetto si otteneva, sotto e sopra era uguale».
    // Un volume che non cambia fra un disco e l'altro nasce da qui: dal
    // fatto che la macchina non rifa' lo spettro di ogni pezzo.
    //   24 = spento, ogni banda per conto suo (com'era)
    //   6  = nessuna banda si allontana piu' di sei dB dalla media
    double liberta  = 24.0;    // dB di distanza massima dalla media
    double sbavatura = 0.0;    // 0..1 — quanto ogni banda segue le vicine
    double limDb    = 8.0;     // il muro, quanti dB sopra il tetto (24 = spento)
    // 4/9 -- misurato: qualunque muro sul grave appiattisce l'onda della
    // cassa. L'Orban il grave non lo mura. Questi sono i dB IN PIU' di muro
    // che prende la banda 1 (la 2 ne prende meta'): il grave resta libero,
    // il resto dello spettro no.
    double limGrave = 0.0;
    // 22/08 — misurato: a 60 ms il muro molla proprio sui due-tre decimi di
    // secondo, cioe' NELLA finestra in cui si giudica se un suono e' fermo,
    // e quel mollare si sente come oscillazione. A 25 ms il ballo scende da
    // 3.12 a 2.93 dB senza toccare la cresta (10.97 -> 10.89): il muro
    // smette di respirare e resta un muro.
    double limRelMs = 25.0;    // quanto ci mette a mollare
    double unione   = 0.0;     // 0..1
    double accBasso = 0.0;     // 0..1 — il grave segue la master
    // L'altro accoppiamento del pannello: la banda 4 segue la 3.
    // «alto = meno fatica; 0 = purista» (sul pannello di Luca era a 0).
    double acc34    = 0.0;     // 0..1
    int    pendenza = 2;       // 2 = 24 dB/ottava, 4 = 48
    double suMax  = 12.0;      // dB, il massimo che una banda puo' ALZARE
    double giuMax = 18.0;      // dB, il massimo che puo' ABBASSARE
    // ⚠️⚠️ 20/08/2026 — IL RIVELATORE DELLE BANDE ERA DI DIECI MILLESIMI.
    // E' il baco che spiega tutte e tre le cose che Luca sentiva insieme:
    // «oscilla», «i medio alti non sono dentro», «i bassi sono ancora
    // liberi». A dieci millesimi il guadagno di banda non insegue il
    // LIVELLO del programma, insegue l'inviluppo della forma d'onda, nota
    // per nota: il tetto smette di essere una destinazione e diventa una
    // cosa che il guadagno scavalca venti volte al secondo.
    // Misurato su September, tetto bloccato, taratura del 20 agosto:
    //
    //          esce (media)      contro un tetto di
    //   banda 1   -41.6                -24.6      17 dB sotto!
    //   banda 5   -49.1                -41.5       7.6 dB sotto
    //
    // Portando il rivelatore a mezzo secondo, TUTTE e cinque le bande si
    // siedono sul loro tetto entro un dB, e l'escursione si dimezza:
    // banda 3 da 14.0 a 9.0 dB, banda 5 da 13.0 a 6.8.
    // Il mestiere veloce resta all'attacco e al rilascio, che sono comandi
    // loro: il rivelatore deve dire QUANTO E' FORTE, non QUANTO PICCHIA.
    double rivMs = 500.0;      // la finestra con cui si misura la banda

    // ⭐⭐ 02/09/2026 — LA STRETTA. E' il pezzo che mancava, e la misura dice
    // perche'. Luca: «continua a rimanere tanti suoni... qui le frequenze
    // sono ancora libere, non e' compresso».
    //
    // Misurato sul September vero contro il nostro, scomponendo l'inviluppo
    // di ogni banda per velocita':
    //
    //                       lento(>500ms)  MEDIO(20-500ms)  veloce(<20ms)
    //   quanto si muove       uguale         uguale           uguale
    //   quanto vanno INSIEME  --             ORBAN 0.36       --
    //                                        NOI   0.25
    //   grave contro acuti                   ORBAN 0.19
    //                                        NOI  -0.00   <-- scollegati
    //
    // Non e' che comprimiamo poco: comprimiamo quanto lui. E' che le nostre
    // cinque bande, NELLA FINESTRA DOVE SI SENTE, vanno ognuna per conto
    // suo. Il grave e gli acuti da noi non si parlano affatto: zero.
    // Da qui i «tanti suoni» invece di un suono unico.
    //
    // Sbavatura e unione ce le avevamo gia', ma agiscono su ga_, cioe' sul
    // livellatore da mezzo secondo -- legano le bande in una finestra che
    // l'orecchio non sente come unita'. Il legame va messo dove la musica
    // si muove: fra i venti millesimi e il mezzo secondo. E' questo lo
    // stadio, ed e' l'unico posto dove il legame diventa udibile.
    //
    // In simulazione sui numeri veri: correlazione media 0.25 -> 0.36
    // (Orban 0.36), grave-acuti -0.00 -> 0.19 (Orban 0.19).
    double strRap = 1.0;       // rapporto; 1 = spenta
    double strMs  = 25.0;      // la finestra del rivelatore di mezzo
    double strAtt = 10.0;      // ms
    double strRil = 200.0;     // ms
    double strSog = 0.0;       // dB rispetto al tetto della banda
    double strLeg = 0.23;      // 0..1 quanto ogni banda segue le vicine
    double strCom = 0.10;      // 0..1 la quota comune a tutte -- il respiro
    // ⭐ MODO 2 (17/10/2026) — il compressore di banda alla Orban. 1 = come prima.
    //   tetto  = soglia della banda in dB RISPETTO al centro della finestra
    //            d'ingresso (inMin+inMax)/2, non piu' un numero assoluto
    //   spinta = dB in piu' DENTRO i rivelatori (multiband drive): piu'
    //            riduzione, stessa uscita
    //   solo verso il basso; sotto la soglia (cancello, anche lei relativa)
    //   il guadagno si CONGELA; ricupero fisso = spinta*(1-1/rapporto), cosi'
    //   a livello tipico il banco e' a uno; il muro sta limDb sopra
    //   l'uscita della banda (centro+tetto+trucco); lega2 = legame proprio
    //   della scheda 5, bande 2-5 (il grave resta fuori: se no la cassa
    //   muove gli acuti). Guadagno interpolato campione per campione.
    int    modo   = 1;
    double spinta = 15.0;
    double lega2  = 0.0;       // 0..1
    double mix   = 1.0;        // 0..1  parallelo col secco
    double outDb = 0.0;
};

struct Biq {
    void reset() { x1=x2=y1=y2=0; }
    inline double process(double x) {
        const double y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;
        x2=x1; x1=x; y2=y1; y1=y; return y;
    }
    void lp(double f, double q, double sr) {
        const double w = 2*M_PI*std::min(f, sr*0.45)/sr, c = std::cos(w), s = std::sin(w);
        const double al = s/(2*q), A0 = 1+al;
        b0 = (1-c)*0.5/A0; b1 = (1-c)/A0; b2 = b0; a1 = -2*c/A0; a2 = (1-al)/A0;
    }
    void hp(double f, double q, double sr) {
        const double w = 2*M_PI*std::min(f, sr*0.45)/sr, c = std::cos(w), s = std::sin(w);
        const double al = s/(2*q), A0 = 1+al;
        b0 = (1+c)*0.5/A0; b1 = -(1+c)/A0; b2 = b0; a1 = -2*c/A0; a2 = (1-al)/A0;
    }
    double b0=1,b1=0,b2=0,a1=0,a2=0,x1=0,x2=0,y1=0,y2=0;
};

// Allpass equivalente a un incrocio LR4: la somma di passa-basso e passa-alto
// di un LR4 NON e' l'unita', e' un passa-tutto del secondo ordine. Una banda
// estratta prima delle altre deve attraversare i passa-tutto di tutti gli
// incroci successivi, se no le bande si risommano storte -- e il null test lo
// ha mostrato subito: a riposo l'errore era piu' forte del segnale.
// (12/08/2026)
// I Q dei Linkwitz-Riley, usati sia dal divisore sia dal passa-tutto.
//   LR4 (24 dB/ott) = Butterworth 2o ordine al quadrato
//   LR8 (48 dB/ott) = Butterworth 4o ordine al quadrato
inline constexpr double kQ4[2] = { 0.70710678, 0.70710678 };
inline constexpr double kQ8[4] = { 0.54119610, 1.30656296, 0.54119610, 1.30656296 };

// La compensazione di fase: la banda estratta prima deve attraversare il
// passa-tutto degli incroci che vengono dopo, se no la somma non torna.
// Il passa-tutto e' esattamente «passa-basso piu' passa-alto» dello stesso
// incrocio, con gli stessi Q del divisore.
struct Ap {
    void set(double f, double sr, int pend) {
        n = (pend >= 4) ? 4 : 2;
        const double* q = (n == 4) ? kQ8 : kQ4;
        for (int i = 0; i < n; ++i) { lo[i].lp(f, q[i], sr); hi[i].hp(f, q[i], sr); }
    }
    void reset() { for (int i = 0; i < 4; ++i) { lo[i].reset(); hi[i].reset(); } }
    inline double process(double x) {
        double b = x, a = x;
        for (int i = 0; i < n; ++i) { b = lo[i].process(b); a = hi[i].process(a); }
        return b + a;
    }
    Biq lo[4], hi[4];
    int n = 2;
};


// Linkwitz-Riley 4o ordine = due Butterworth in fila
// Divisore Linkwitz-Riley a pendenza scelta.
//
//   24 dB/ottava = LR4 = Butterworth del 2o ordine al quadrato -> Q 0.707 x2
//   48 dB/ottava = LR8 = Butterworth del 4o ordine al quadrato -> Q 0.5412 e
//                        1.3066, ognuno due volte
//
// ⚠️ 16/08/2026 — PRIMO TENTATIVO SBAGLIATO, misurato: avevo messo quattro
// sezioni tutte a Q 0.707. Quello NON e' un LR8 (e' un LR4 al quadrato), e
// la somma del passa-basso col passa-alto non torna piatta: 3.35 dB di
// scarto medio, 6.14 nel punto peggiore. Coi Q giusti il conto torna.
//
// Niente 36 dB/ottava: con un numero dispari di sezioni la somma non e' piu'
// un passa-tutto e il banco non ricompone. Un multibanda che non ricompone
// e' un equalizzatore storto travestito.
struct Lr {
    void set(double f, double sr, int pend) {
        n = (pend >= 4) ? 4 : 2;
        const double* q = (n == 4) ? kQ8 : kQ4;
        for (int i = 0; i < n; ++i) { lo[i].lp(f, q[i], sr); hi[i].hp(f, q[i], sr); }
    }
    void reset() { for (int i = 0; i < 4; ++i) { lo[i].reset(); hi[i].reset(); } }
    inline void split(double x, double& basso, double& alto) {
        basso = x; alto = x;
        for (int i = 0; i < n; ++i) { basso = lo[i].process(basso); alto = hi[i].process(alto); }
    }
    Biq lo[4], hi[4];
    int n = 2;
};


class Mb {
public:
    void prepare(double sr) {
        sr_ = sr > 0 ? sr : 48000.0;
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < kB - 1; ++i) lr_[c][i].reset();
            for (int i = 0; i < kB - 2; ++i)
                for (int j = i + 1; j < kB - 1; ++j) ap_[c][i][j].reset();
            hp_[c][0].reset(); hp_[c][1].reset();
        }
        for (int i = 0; i < kB; ++i) { env_[i] = 1e-20; g_[i] = 0.0; ga_[i] = 0.0;
                                      gLim_[i] = 1.0; limSpia_[i] = 1.0;
                                      envV_[i] = 1e-20; gStr_[i] = 0.0; gSA_[i] = 0.0; }
        conta_ = 0; contaIn_ = 0; envIn_ = 1e-20; gIn_ = 0.0; bersIn_ = 0.0;
        aggiorna();
    }

    // il cancello di programma lo alza e lo abbassa Motore.mm
    bool tieni_ = false;

    void setParams(const Params& p) {
        // ⚠️⚠️ 20/08/2026 — LA PENDENZA NON FACEVA NIENTE.
        // Trovato sul banco: cambiare 24 <-> 48 dava un'uscita IDENTICA bit
        // per bit (differenza nulla a -280 dB). I filtri si ricostruivano
        // solo se cambiava il taglio o un incrocio; la pendenza no: il
        // valore veniva salvato e mai usato. Luca lo diceva da giorni --
        // «puoi cambiare tutti i setup e non cambia nulla».
        bool strutt = (p.hpfHz != p_.hpfHz) || (p.pendenza != p_.pendenza)
                   || (p.rivMs != p_.rivMs) || (p.strMs != p_.strMs)
                   || (p.strAtt != p_.strAtt) || (p.strRil != p_.strRil)
                   || (p.strRap != p_.strRap) || (p.strLeg != p_.strLeg)
                   || (p.strCom != p_.strCom);
        for (int i = 0; i < kB - 1; ++i) if (p.xover[i] != p_.xover[i]) strutt = true;
        p_ = p;
        // ⚠️⚠️ 02/09/2026 — IL MURO NON HA MAI SEGUITO I TETTI.
        // aggiorna() girava solo se cambiava la STRUTTURA (taglio, incroci,
        // pendenza). Ma dentro aggiorna() si calcolano anche sogLim_ -- la
        // soglia del muro di ogni banda, che sta limDb sopra il TETTO -- e i
        // rilasci del muro. Cambiare un tetto non faceva scattare strutt,
        // quindi il muro restava tarato sui tetti di prima: all'avvio, su
        // quelli di fabbrica (-6 dB), che non c'entrano niente con quelli
        // di un preset da radio (-23 / -40).
        // E' il motivo per cui Multiband drive muoveva i cinque tetti e non
        // cambiava quasi niente, e per cui Luca oggi dice «riesco a
        // schiacciare il basso ma non i medio alti»: il loro muro stava a
        // un'altezza che non aveva niente a che vedere col loro tetto.
        // Le tarature adesso si rifanno SEMPRE (costa microsecondi, e
        // succede solo quando si muove un comando); i filtri, che sono la
        // parte cara, restano condizionati come prima.
        aggiorna(!strutt);
        for (int i = 0; i < kB; ++i) {
            // ⚠️ il passo di controllo gira ogni 32 campioni: il coefficiente
            // va calcolato per 32 campioni, non moltiplicato per 32 -- cosi'
            // poteva superare 1 e far sbandare l'inseguitore.
            att_[i] = 1.0 - std::exp(-32.0 / (std::max(0.1, p_.b[i].attaccoMs)  * 0.001 * sr_));
            ril_[i] = 1.0 - std::exp(-32.0 / (std::max(1.0, p_.b[i].rilascioMs) * 0.001 * sr_));
        }
    }

    double riduzione(int i) const { return ga_[i] + gSA_[i]; }
    // quanto ha dovuto fermare il muro di quella banda, dall'ultima lettura
    double muro(int i) {
        const double d = 20.0 * std::log10(std::max(limSpia_[i], 1e-6));
        limSpia_[i] = 1.0; return d;
    }
    double livello(int i)   const { return 10.0 * std::log10(std::max(env_[i], 1e-20)); }
    // ⚠️ 16/08/2026 — il livello che ESCE, non quello che entra.
    // `livello()` misura la banda PRIMA che le si applichi il guadagno: e'
    // quello che serve per decidere, ma disegnare quello significa mostrare
    // una barra che non rispetta mai il tetto, per quanto lo si stringa.
    // Segnalato da Luca: «vorrei vedere che se metto il blocco la banda non
    // supera». Ecco dove si vede.
    double uscita(int i) const {
        return livello(i) + ga_[i] + gSA_[i] + p_.b[i].gainDb;
    }

    inline void process(float* L, float* R, int n) {
        const double gout = std::pow(10.0, p_.outDb * 0.05);
        for (int s = 0; s < n; ++s) {
            const double secchiL = L[s], secchiR = R[s];
            double xl = hp_[0][1].process(hp_[0][0].process(secchiL));
            double xr = hp_[1][1].process(hp_[1][0].process(secchiR));

            // --- la finestra d'ingresso ---------------------------------
            {
                const double m = 0.5 * (xl + xr);
                envIn_ += aIn_ * (m * m - envIn_);
                if (--contaIn_ <= 0) {
                    contaIn_ = 32;
                    const double lvl = 10.0 * std::log10(std::max(envIn_, 1e-20));
                    // ⛔⛔ 26/09/2026 — LE BANDE A -75 dB.
                    // Luca: «guarda di nuovo le bande completamente sature».
                    // La finestra confrontava il livello GREZZO con i suoi
                    // limiti, non quello dopo il proprio guadagno. In una
                    // pausa (grezzo a -100) alzava senza fine: misurato al
                    // banco, +78 dB in otto secondi di silenzio. Poi tornava
                    // la musica, il grezzo cadeva «dentro» la finestra e il
                    // guadagno restava a +78 per sempre: bande a -50/-75 di
                    // riduzione per tenere a bada settanta dB regalati.
                    // Adesso: si giudica il livello DOPO il guadagno, il
                    // silenzio non si insegue, e la corsa sta in +-24 dB.
                    const double dopo = lvl + gIn_;
                    if (lvl < -70.0 || tieni_) bersIn_ = gIn_;          // silenzio: fermo
                    else if (dopo < p_.inMin)  bersIn_ = p_.inMin - lvl; // sotto: alza
                    else if (dopo > p_.inMax)  bersIn_ = p_.inMax - lvl; // sopra: abbassa
                    else                       bersIn_ = gIn_;           // dentro: fermo
                    bersIn_ = std::min(24.0, std::max(-24.0, bersIn_));
                }
                const double passo = p_.inDbS / sr_;
                gIn_ += std::min(std::max(bersIn_ - gIn_, -passo), passo);
                const double lin = std::pow(10.0, gIn_ * 0.05);
                xl *= lin; xr *= lin;
            }

            double bl[kB], br[kB];
            double restoL = xl, restoR = xr;
            for (int i = 0; i < kB - 1; ++i) {
                lr_[0][i].split(restoL, bl[i], restoL);
                lr_[1][i].split(restoR, br[i], restoR);
            }
            bl[kB-1] = restoL; br[kB-1] = restoR;

            // compensazione di fase: la banda i attraversa i passa-tutto
            // degli incroci successivi, cosi' la somma torna uguale all'ingresso
            for (int i = 0; i < kB - 2; ++i)
                for (int j = i + 1; j < kB - 1; ++j) {
                    bl[i] = ap_[0][i][j].process(bl[i]);
                    br[i] = ap_[1][i][j].process(br[i]);
                }

            for (int i = 0; i < kB; ++i) {
                const double m = 0.5 * (bl[i] + br[i]);
                env_[i]  += aRms_ * (m * m - env_[i]);
                envV_[i] += aVel_ * (m * m - envV_[i]);   // la stretta guarda qui
            }
            if (--conta_ <= 0) {
                conta_ = 32; decidi();
                for (int i = 0; i < kB; ++i) {          // modo 2: rampa di 32 campioni
                    const double nuovo = p_.b[i].accesa ? (ga_[i] + gSA_[i] + ric_[i]) : 0.0;
                    gStep_[i] = (nuovo - gRun_[i]) / 32.0;
                }
            }

            double sl = 0.0, sr2 = 0.0;
            for (int i = 0; i < kB; ++i) {
                // ⚠️⚠️ 05/10/2026 — BANDA SPENTA, GUADAGNO ACCESO.
                // Luca: «se 2 non va in distorsione, nel mettere 3 e' tutto
                // in distorsione; ma come fanno le bande a mandare tutto in
                // distorsione?». Non erano le bande: era QUESTA riga.
                // Spegnendo la scheda 5 (m_ban) le bande smettono di
                // comprimere e il loro muro non lavora -- ma il trucco
                // d'uscita (gainDb: qui +4.5 ... +8.6 dB) continuava ad
                // applicarsi. Risultato: niente piu' riduzione, niente piu'
                // muro, e in piu' sei-nove dB di guadagno regalati.
                // Misurato al banco sul 7 ott: il 4.32% dei campioni sopra
                // fondo scala, valore efficace da -22.3 a -7.3 dB. Tutto
                // distorto, esattamente come diceva lui. Con la banda spenta
                // adesso non si applica NIENTE: 4.32% -> 0.03%, e nei casi
                // normali l'uscita e' identica al campione.
                // L'aiuto a schermo lo diceva gia' da sempre: «spenta passa
                // lo stesso, ma il suo guadagno sta fermo a zero».
                double gdb = p_.b[i].accesa ? (ga_[i] + gSA_[i]) : 0.0;
                if (p_.modo == 2) { gRun_[i] += gStep_[i]; gdb = gRun_[i]; }
                const double lin = std::pow(10.0,
                        (gdb + (p_.b[i].accesa ? p_.b[i].gainDb : 0.0)) * 0.05);
                double vl = bl[i] * lin, vr = br[i] * lin;
                if (limAcceso_ && p_.b[i].accesa) {
                    // il muro: attacco sullo stesso campione, nessuno sfondamento
                    const double pk = std::max(std::fabs(vl), std::fabs(vr));
                    double g = 1.0;
                    if (pk > sogLim_[i]) g = sogLim_[i] / pk;
                    if (g < gLim_[i]) gLim_[i] = g;                 // giu' subito
                    else gLim_[i] += (g - gLim_[i]) * aLimRil_[i];  // su piano
                    vl *= gLim_[i]; vr *= gLim_[i];
                    if (gLim_[i] < limSpia_[i]) limSpia_[i] = gLim_[i];
                }
                sl  += vl;
                sr2 += vr;
            }
            L[s] = (float) ((sl  * p_.mix + secchiL * (1.0 - p_.mix)) * gout);
            R[s] = (float) ((sr2 * p_.mix + secchiR * (1.0 - p_.mix)) * gout);
        }
    }

private:
    void decidi() {
        if (p_.modo == 2) {
            const double R = 0.5 * (p_.inMin + p_.inMax);
            for (int i = 0; i < kB; ++i) {
                const Banda& b = p_.b[i];
                const double lvl = 10.0 * std::log10(std::max(env_[i], 1e-20));
                const double k = 1.0 - 1.0 / std::max(1.0, b.rapporto);
                const double sopra = lvl + p_.spinta - (R + b.tetto);
                double voluto = (sopra > 0.0) ? -sopra * k : 0.0;      // solo giu'
                voluto = std::max(voluto, -std::max(0.0, p_.giuMax));
                if (lvl < R + b.soglia || tieni_) voluto = g_[i];        // cancello: si congela
                if (!b.accesa) voluto = 0.0;
                g_[i] += (voluto - g_[i]) * ((voluto < g_[i]) ? att_[i] : ril_[i]);
                // ⭐ 25/10/2026 — IL RIEMPIMENTO (modo 2). Luca: «quando un pezzo
                // e' vuoto tipo Mammagamma continua a essere vuoto». La spinta
                // porta ogni banda sopra il tetto, quindi scendono tutte e la
                // salita non scattava mai: una banda acuta che ENTRA piano
                // restava piano. L'8200 vero tiene ogni banda al suo tetto. Qui,
                // solo per la parte che entra SOTTO il tetto, aggiungo al
                // ricupero una spinta in su fino a «puo' alzare» (4.23 suMax):
                // riempie i pezzi scarni e non tocca quelli pieni (che entrano
                // gia' sopra il tetto). A suMax 0 e' il comportamento di prima.
                const double sottoTetto = (R + b.tetto) - lvl;
                const double riempi = (sottoTetto > 0.0)
                    ? std::min(sottoTetto, std::max(0.0, p_.suMax)) : 0.0;
                ric_[i] = b.accesa ? (p_.spinta * k + riempi) : 0.0;  // ricupero + riempimento
            }
        } else
        for (int i = 0; i < kB; ++i) {
            const double lvl = 10.0 * std::log10(std::max(env_[i], 1e-20));
            const Banda& b = p_.b[i];
            const double su  = std::max(0.0, p_.suMax);
            const double giu = std::max(0.0, p_.giuMax);
            // sotto il cancello: il guadagno scelto per quella banda
            double voluto = std::min(std::max(b.sottoDb, -giu), su);
            if (lvl > b.soglia) {
                if (p_.bloccaAlTetto) {
                    // modo radio: la banda viene portata al tetto e ci resta.
                    // Il cancello (soglia) dice solo se e' il caso di
                    // lavorarci; quanto puo' muoversi lo dicono su/giu.
                    voluto = std::min(std::max(b.tetto - lvl, -giu), su);
                } else {
                    // sopra la soglia si comprime col rapporto
                    voluto = -(lvl - b.soglia) * (1.0 - 1.0 / std::max(1.0, b.rapporto));
                    // e comunque non si lascia superare il tetto
                    const double oltre = (lvl + voluto) - b.tetto;
                    if (oltre > 0.0) voluto -= oltre;
                    voluto = std::min(std::max(voluto, -giu), su);
                }
            }
            if (!b.accesa) voluto = 0.0;    // spenta: passa e basta
            // ⭐⭐ 18/09/2026 — SOTTO IL CANCELLO SI TIENE, NON SI MOLLA.
            // Luca: «quando non c'e' sufficiente volume tira su tutto,
            // fruscio e rumori di fondo». Sotto la soglia `voluto` restava 0,
            // cioe' la banda RILASCIAVA tutta la riduzione: misurato al banco,
            // sul passaggio quieto la banda 2 passava da -14.3 a -0.4.
            // Rilasciare e' alzare, e quei quattordici dB finivano tutti sul
            // fruscio. A cancello chiuso il guadagno resta dov'e'.
            if (tieni_) voluto = g_[i];
            g_[i] += (voluto - g_[i]) * ((voluto < g_[i]) ? att_[i] : ril_[i]);
        }

        // --- unione e accoppiamenti ------------------------------------
        // ⚠️⚠️ 21/08/2026 — SI ACCUMULAVANO, ED E' UN BACO GROSSO.
        // Scritti cosi': g_[i] += (media - g_[i]) * k, dentro il passo di
        // controllo. Ma quel passo gira millecinquecento volte al secondo:
        // anche con k = 0.05 dopo pochi millesimi g_[i] E' la media. Quindi
        // qualunque valore diverso da zero valeva 100%, e la manopola non
        // aveva scala -- «puoi muoverlo a sinistra e destra, non succede
        // nulla», diceva Luca del Band 3&4 coupling. Non succedeva nulla
        // perche' era gia' tutto successo al primo scatto.
        // Adesso i guadagni liberi restano in g_, che continua a correre per
        // conto suo, e quello che si APPLICA e' una miscela fra il guadagno
        // libero e quello legato. Cosi' il 20% e' davvero il 20%.
        {
            double media = 0.0;
            for (int i = 0; i < kB; ++i) { ga_[i] = g_[i]; media += g_[i]; }
            media /= kB;
            // la sbavatura: ogni banda si porta dietro le due vicine.
            // Va PRIMA dell'unione: prima il colpo si spande fra i vicini,
            // poi semmai si lega tutto insieme.
            if (p_.sbavatura > 0.001) {
                const double b = std::min(1.0, p_.sbavatura) * 0.5;
                double t[kB];
                for (int i = 0; i < kB; ++i) {
                    const double sx = g_[i > 0 ? i - 1 : 0];
                    const double dx = g_[i < kB - 1 ? i + 1 : kB - 1];
                    t[i] = g_[i] * (1.0 - b) + (sx + dx) * 0.5 * b;
                }
                for (int i = 0; i < kB; ++i) ga_[i] = t[i];
            }
            // l'unione: tutte e cinque verso la media
            if (p_.unione > 0.001) {
                const double u = std::min(1.0, p_.unione);
                for (int i = 0; i < kB; ++i) ga_[i] = ga_[i] * (1.0 - u) + media * u;
            }
            // modo 2: il legame della scheda 5, bande 2-5
            if (p_.modo == 2 && p_.lega2 > 0.001) {
                const double u = std::min(1.0, p_.lega2);
                double m4 = 0.0; for (int i = 1; i < kB; ++i) m4 += g_[i]; m4 /= (kB - 1);
                for (int i = 1; i < kB; ++i) ga_[i] = ga_[i] * (1.0 - u) + m4 * u;
            }
            // il grave insegue la master, in una direzione sola (brevetto Orban)
            if (p_.accBasso > 0.001) {
                double master = 0.0;
                for (int i = 1; i < kB; ++i) master += ga_[i];
                master /= (kB - 1);
                const double k = std::min(1.0, p_.accBasso);
                ga_[0] = ga_[0] * (1.0 - k) + master * k;
            }
            // e la banda 4 insegue la 3
            if (p_.acc34 > 0.001) {
                const double k = std::min(1.0, p_.acc34);
                ga_[3] = ga_[3] * (1.0 - k) + ga_[2] * k;
            }
            // la liberta': per ultima, perche' e' un vincolo, non una
            // miscela. Qualunque cosa abbiano deciso i comandi qui sopra,
            // nessuna banda puo' allontanarsi dalla media piu' di tanto.
            if (p_.liberta < 23.5) {
                double m2 = 0.0;
                for (int i = 0; i < kB; ++i) m2 += ga_[i];
                m2 /= kB;
                const double d = std::max(0.5, p_.liberta);
                for (int i = 0; i < kB; ++i)
                    ga_[i] = std::min(m2 + d, std::max(m2 - d, ga_[i]));
            }
        }

        // --- LA STRETTA -------------------------------------------------
        // Il compressore di mezzo: rivelatore da venticinque millesimi,
        // rapporto vero, attacco e rilascio nella finestra dove la musica
        // vive. Da solo dice quanto togliere; il legame qui sotto dice
        // quanto di quel movimento passa alle altre bande.
        if (strAcceso_) {
            for (int i = 0; i < kB; ++i) {
                // ⚠️⚠️ 02/09 — SBAGLIATO AL PRIMO COLPO, e il banco l'ha
                // visto in un minuto: confrontavo il livello IN INGRESSO
                // con una soglia che e' un livello IN USCITA (il tetto).
                // Cosi' la stretta toglieva altri 7-12 dB SOTTO il tetto:
                // le bande uscivano nove dB sotto e il tetto non fermava
                // piu' niente. Luca: «le bande scappano ancora, non sono
                // schiacciate come dovrebbero». Erano schiacciate troppo, e
                // nel posto sbagliato -- senza un tetto contro cui stare.
                const double lvl = 10.0 * std::log10(std::max(envV_[i], 1e-20))
                                 + ga_[i];
                const double sog = p_.b[i].tetto + p_.strSog;
                double voluto = 0.0;
                if (lvl > sog)
                    voluto = -(lvl - sog) * (1.0 - 1.0 / std::max(1.0, p_.strRap));
                if (!p_.b[i].accesa) voluto = 0.0;
                gStr_[i] += (voluto - gStr_[i])
                          * ((voluto < gStr_[i]) ? aStrA_ : aStrR_);
            }
            // ⚠️ come per l'unione: MISCELA, non accumulo. Il guadagno libero
            // resta in gStr_ e continua a correre; quello che si applica e'
            // una miscela fra il proprio e quello delle altre. Cosi' il 20%
            // e' davvero il 20% e non diventa 100% al primo scatto.
            double com = 0.0;
            for (int i = 0; i < kB; ++i) com += gStr_[i];
            com /= kB;
            const double q = std::max(0.0, std::min(1.0, p_.strCom));
            for (int i = 0; i < kB; ++i) {
                double v = 0.0;
                for (int j = 0; j < kB; ++j) v += pesoStr_[i][j] * gStr_[j];
                gSA_[i] = v * (1.0 - q) + com * q;
            }
        } else {
            for (int i = 0; i < kB; ++i) { gStr_[i] = 0.0; gSA_[i] = 0.0; }
        }
    }

    void aggiorna(bool soloTarature = false) {
        aRms_ = 1.0 - std::exp(-1.0 / (std::max(5.0, p_.rivMs) * 0.001 * sr_));
        // il muro di ogni banda: sta limDb sopra il suo tetto. Il tetto e'
        // un livello efficace, il muro un picco, e fra i due ci sono i dieci
        // dB di cresta della musica: per questo la distanza parte da otto.
        limAcceso_ = (p_.limDb < 24.0);
        static const double lento[kB] = { 2.0, 1.4, 1.0, 0.8, 0.6 };
        static const double piu[kB] = { 1.0, 0.5, 0.0, 0.0, 0.0 };
        for (int i = 0; i < kB; ++i) {
            const double muroDb = std::min(24.0, p_.limDb + p_.limGrave * piu[i]);
            const double base = (p_.modo == 2)
                ? 0.5 * (p_.inMin + p_.inMax) + p_.b[i].tetto + p_.b[i].gainDb
                : p_.b[i].tetto;
            sogLim_[i] = std::pow(10.0, (base + muroDb) * 0.05);
            const double ms = std::max(2.0, p_.limRelMs) * lento[i];
            aLimRil_[i] = 1.0 - std::exp(-1.0 / (ms * 0.001 * sr_));
        }
        // la stretta: rivelatore per campione, attacco e rilascio per passo
        strAcceso_ = (p_.strRap > 1.001);
        aVel_  = 1.0 - std::exp( -1.0 / (std::max(3.0,  p_.strMs)  * 0.001 * sr_));
        aStrA_ = 1.0 - std::exp(-32.0 / (std::max(1.0,  p_.strAtt) * 0.001 * sr_));
        aStrR_ = 1.0 - std::exp(-32.0 / (std::max(10.0, p_.strRil) * 0.001 * sr_));
        // i pesi del legame: molto dalle vicine, poco ma MAI zero dalle
        // lontane. Il decadimento e' fisso a 0.3 -- e' il valore che sui
        // numeri veri avvicina di piu' la nostra matrice a quella dell'Orban.
        {
            const double f = std::max(0.0, std::min(1.0, p_.strLeg));
            for (int i = 0; i < kB; ++i) {
                double som = 0.0;
                for (int j = 0; j < kB; ++j) {
                    const int d = (i > j) ? i - j : j - i;
                    double w = 1.0;
                    for (int k = 0; k < d; ++k) w *= 0.3;
                    pesoStr_[i][j] = (i == j) ? 1.0 : f * w;
                    som += pesoStr_[i][j];
                }
                for (int j = 0; j < kB; ++j) pesoStr_[i][j] /= som;
            }
        }
        aIn_  = 1.0 - std::exp(-1.0 / (0.300 * sr_));      // 300 ms sull'ingresso
        if (soloTarature) return;          // i filtri no: sono la parte cara
        for (int c = 0; c < 2; ++c) {
            hp_[c][0].hp(p_.hpfHz, 0.70710678, sr_);
            hp_[c][1].hp(p_.hpfHz, 0.70710678, sr_);
            for (int i = 0; i < kB - 1; ++i) lr_[c][i].set(p_.xover[i], sr_, p_.pendenza);
            for (int i = 0; i < kB - 2; ++i)
                for (int j = i + 1; j < kB - 1; ++j) ap_[c][i][j].set(p_.xover[j], sr_, p_.pendenza);
        }
    }

    Params p_;
    double sr_ = 48000.0, aRms_ = 0.0;
    double env_[kB] = {1e-20,1e-20,1e-20,1e-20,1e-20};
    double g_[kB] = {0,0,0,0,0};    // il guadagno LIBERO di ogni banda
    double ga_[kB] = {0,0,0,0,0};   // ...e quello che si applica davvero
    // il muro di banda
    bool   limAcceso_ = true;
    double sogLim_[kB] = {1,1,1,1,1}, gLim_[kB] = {1,1,1,1,1};
    double aLimRil_[kB] = {0,0,0,0,0}, limSpia_[kB] = {1,1,1,1,1};
    double att_[kB] = {0,0,0,0,0}, ril_[kB] = {0,0,0,0,0};
    double ric_[kB] = {0,0,0,0,0}, gRun_[kB] = {0,0,0,0,0}, gStep_[kB] = {0,0,0,0,0};   // modo 2
    // la stretta
    bool   strAcceso_ = false;
    double envV_[kB] = {1e-20,1e-20,1e-20,1e-20,1e-20};
    double gStr_[kB] = {0,0,0,0,0};   // il suo guadagno libero
    double gSA_[kB]  = {0,0,0,0,0};   // ...e quello legato, che si applica
    double aVel_ = 0.0, aStrA_ = 0.0, aStrR_ = 0.0;
    double pesoStr_[kB][kB] = {};
    Lr  lr_[2][kB - 1];
    Ap  ap_[2][kB - 1][kB - 1];
    Biq hp_[2][2];
    int conta_ = 0, contaIn_ = 0;
    double envIn_ = 1e-20, aIn_ = 0.0, gIn_ = 0.0, bersIn_ = 0.0;
public:
    double guadagnoIngresso() const { return gIn_; }
};

} // namespace omb
