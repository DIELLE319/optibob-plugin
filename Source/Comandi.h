// L'elenco dei comandi del plugin unico, in un posto solo.
//
// Tre sezioni in fila: AGC (livella) -> MB (multibanda a finestra) -> MAX
// (densita' e tetto). Gli identificativi hanno un prefisso per sezione --
// a_ m_ x_ -- cosi' non si pestano i piedi.
//
// Ogni comando porta il nome e la spiegazione in DUE lingue. Il prodotto
// esce in inglese; Luca lavora in italiano. Il tastino IT/EN in alto a
// destra ribalta tutto senza ricostruire niente.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace uno {

// Vero = si lavora in italiano. Sta qui e non nell'editor perche' la
// leggono sia le pagine dei cursori sia i tasti in fondo.
inline bool& italiano() { static bool i = false; return i; }

struct Voce {
    const char* id; const char* nome; const char* unita;
    float min, max, passo, def;
    const char* aiuto;
    const char* nomeIt; const char* aiutoIt;
};

inline const char* nomeDi (const Voce& v) { return italiano() ? v.nomeIt  : v.nome;  }
inline const char* aiutoDi(const Voce& v) { return italiano() ? v.aiutoIt : v.aiuto; }

// --------------------------------------------------- EQ (davanti a tutto)
// Gli stessi comandi del Channel EQ di Logic, uno per uno: i valori si
// travasano senza conversioni. Sta PRIMA dell'AGC, come i Low bass / Mid
// bass / Presence / Brilliance dell'8200.
inline const std::vector<Voce>& vociEq() {
    static const std::vector<Voce> v = {
        {"e_hp","High-pass"," Hz",0,300,1,0,"0 turns it off.",
         "Passa-alto","A 0 e' spento."},
        {"e_hpq","High-pass: Q","",0.1f,2,0.01f,0.707f,"Low Q is gentle, high Q puts a bump before the cut.",
         "Passa-alto: Q","Q basso e' dolce, alto mette una gobba prima del taglio."},
        {"e_hpsez","High-pass: slope"," x12dB",1,2,1,1,"1 = 12 dB/oct, 2 = 24.",
         "Passa-alto: pendenza","1 = 12 dB/ottava, 2 = 24."},
        {"e_lshz","Low shelf: where"," Hz",30,300,1,68,"",
         "Scaffale basso: dove",""},
        {"e_lsdb","Low shelf"," dB",-12,12,0.1f,0,"Weight below that frequency.",
         "Scaffale basso","Il peso sotto quella frequenza."},
        {"e_b1hz","Bell 1: where"," Hz",30,500,1,117,"",
         "Campana 1: dove",""},
        {"e_b1db","Bell 1"," dB",-15,15,0.1f,0,"The knock around 110-130 Hz lives here.",
         "Campana 1","La bussata fra 110 e 130 Hz sta qui."},
        {"e_b1q","Bell 1: width","",0.2f,8,0.05f,2.1f,"",
         "Campana 1: larghezza",""},
        {"e_b2hz","Bell 2: where"," Hz",150,2000,5,250,"",
         "Campana 2: dove",""},
        {"e_b2db","Bell 2"," dB",-15,15,0.1f,0,"",
         "Campana 2",""},
        {"e_b2q","Bell 2: width","",0.2f,8,0.05f,1.0f,"",
         "Campana 2: larghezza",""},
        {"e_b3hz","Bell 3: where"," Hz",800,12000,10,2500,"The region that tires the ear.",
         "Campana 3: dove","La zona che stanca l'orecchio."},
        {"e_b3db","Bell 3"," dB",-15,15,0.1f,0,"",
         "Campana 3",""},
        {"e_b3q","Bell 3: width","",0.2f,8,0.05f,1.0f,"",
         "Campana 3: larghezza",""},
        {"e_hshz","High shelf: where"," Hz",2000,16000,50,10000,"",
         "Scaffale alto: dove",""},
        {"e_hsdb","High shelf"," dB",-12,12,0.1f,0,"Air, above that frequency.",
         "Scaffale alto","L'aria, sopra quella frequenza."},
        // stesso difetto del muro in cima: sotto i 1000 Hz il motore lo ignorava
        // gia', ma il cursore ci andava lo stesso e sembrava rotto. Adesso
        // spento sta in cima.
        {"e_lp","Low-pass"," Hz",1000,20000,50,20000,"All the way up = off.",
         "Passa-basso","Tutto in alto e' spento."},
        {"e_lpq","Low-pass: Q","",0.1f,2,0.01f,0.707f,"",
         "Passa-basso: Q",""},
    };
    return v;
}

// ---------------------------------------------------------------- AGC
inline const std::vector<Voce>& vociAgc() {
    static const std::vector<Voce> v = {
        // ⭐⭐ 03/09/2026 — LA PREAMPLIFICAZIONE.
        // Luca: «bastava non distorcere, che l'Orban faceva tutto da solo;
        // creiamo una preamplificazione prima di Orban».
        // Ha ragione, e questa manopola non c'era mai stata. Sull'8200 il
        // livello d'ingresso si imposta una volta, con la macchina spenta, e
        // da li' in poi non si tocca piu': tutto il resto della catena e'
        // tarato per quel punto di lavoro. Noi invece prendevamo quello che
        // arrivava, e se arrivava venti dB sotto era l'AGC a doverli
        // recuperare -- tirando su anche il rumore, che e' il «gracchiante»
        // che abbiamo inseguito per mezza giornata dentro il clipper.
        // Sta PRIMA di tutto: prima dell'equalizzatore, prima del
        // livellatore, prima delle bande.
        {"g_porta","Delay"," ms",0,1000,10,0,"The stage before everything. It delays the sound by this much and meanwhile LOOKS AHEAD: it knows the peak of the audio that has not played yet, so the gain is already right when it arrives. It never chases and never overshoots - the chain always gets the level it was tuned for. 0 turns it off. It costs latency, one for one: 200 ms is plenty for listening, 1000 makes it immovable.",
         "Delay","Lo stadio prima di tutto. Ritarda il suono di tanto cosi' e intanto GUARDA AVANTI: conosce il picco del suono che deve ancora uscire, quindi mette il guadagno giusto PRIMA che arrivi. Non insegue e non sfonda mai: la catena riceve sempre il livello per cui e' tarata. A 0 e' spenta. Si paga in ritardo, uno per uno: 200 ms bastano per ascoltare, 1000 la rendono incrollabile."},
        {"g_pbers","Gate: target"," dB",-24,-3,0.5f,-12,"Where the peaks are put. The whole chain is tuned for -12.",
         "Porta: bersaglio","Dove mette i picchi. Tutta la catena e' tarata per -12."},
        // ⭐⭐ 18/09/2026 — «MAI SOTTO» ANCHE PER LA PORTA. Luca: «vicino a
        // un ingresso mai sopra ci vorrebbe anche uno mai sotto». Aveva
        // ragione e mancava proprio qui: le bande la finestra ce l'hanno da
        // sempre (Ingresso: mai sotto / mai sopra), la Porta no -- aveva solo
        // il bersaglio, cioe' meta' coppia. Senza pavimento la Porta tira su
        // qualunque cosa fino a +40 dB, fruscio compreso: misurato su un
        // passaggio quieto, fra musica e silenzio restava un decibel e mezzo
        // invece di quarantadue.
        {"g_canc","Gate: never below"," dB",-90,-20,1,-90,"Quieter than this at the INPUT, nothing is lifted any more: the look-ahead gate, the leveller and the bands all hold their gain where it is instead of climbing. It is the other half of the target above - the floor to its ceiling. Measured on the raw input, before any gain, which is the whole point: every other threshold in the chain sits after the preamp, where hiss lifted by forty dB no longer looks like silence. At -90 it never closes.",
         "Porta: mai sotto","Piu' piano di cosi' ALL'INGRESSO, non si alza piu' niente: la Porta, il livellatore e le bande tengono il guadagno dov'e' invece di salire. E' l'altra meta' del bersaglio qui sopra -- lui e' il tetto, questo e' il pavimento. Si misura sull'ingresso GREZZO, prima di ogni guadagno, ed e' tutto il punto: ogni altra soglia della catena sta dopo il preamplificatore, e li' un fruscio alzato di quaranta dB non si distingue piu' dalla musica. A -90 non si chiude mai. Sul materiale normale non interviene: verificato, uscita identica campione per campione."},
        {"g_cancms","Gate: never below — window"," ms",5,300,5,60,"How long a window the floor measures over. This matters more than the threshold itself: at 300 ms it arrives after the gains have already climbed, at 60 ms it catches them in time - measured, 8.9 dB against 15.4 dB of separation between music and a quiet passage.",
         "Porta: mai sotto — finestra","Su che finestra misura il pavimento. Conta piu' della soglia: a 300 ms arriva quando i guadagni sono gia' saliti, a 60 ms li prende in tempo. Misurato: 8.9 dB contro 15.4 dB di distanza fra musica e passaggio quieto."},
        {"g_pvel","Gate: speed"," dB/s",0.2f,24,0.2f,3,"How fast the gate moves its gain. Slow does not touch the dynamics: it only moves the working point.",
         "Porta: velocita'","Quanto in fretta la porta muove il guadagno. Lenta non tocca la dinamica: sposta solo il punto di lavoro."},
        // ⭐⭐ 19/09/2026 — LA FINESTRA DEL PROGRAMMA. Idea di Luca: «io voglio
        // un livello minimo e un livello max sull'input». Quattro dischi
        // diversi, quattordici dB di scarto in ingresso, misurati in uscita
        // come li sente l'orecchio: AGC acceso 2.92 dB di scarto, AGC spento
        // 1.21, questa finestra 0.49. Misura la LOUDNESS, non l'energia: e'
        // la ragione per cui riesce dove il livellatore falliva.
        {"f_on","Programme window",""  ,0,1,1,0,"Turns the programme window on. Off, each record arrives as it was mastered.",
         "Finestra del programma","Accende la finestra. Spenta, ogni disco arriva com'e' stato masterizzato."},
        {"f_min","Programme: never below"," LUFS",-36,-8,0.5f,-20,"Quieter than this, the whole programme is brought up. Measured as loudness, the way the ear weighs it - not as energy: two records with the same energy and different spectrum do not sound equally loud, and that is exactly what the leveller in front of the bands kept missing.",
         "Programma: mai sotto","Piu' piano di cosi', tutto il programma viene tirato su. Si misura la LOUDNESS, come la pesa l'orecchio, non l'energia: due dischi con la stessa energia e spettro diverso non suonano uguali, ed e' esattamente quello che al livellatore sfuggiva."},
        {"f_max","Programme: never above"," LUFS",-36,-8,0.5f,-20,"Louder than this, the whole programme is brought down. Set it equal to «never below» for one destination and no window at all; open it a couple of dB to let the quiet records stay a little quieter.",
         "Programma: mai sopra","Piu' forte di cosi', tutto il programma viene abbassato. Messo uguale a «mai sotto» diventa una destinazione sola e non c'e' finestra; aperto di un paio di dB lascia ai dischi piano un po' del loro piano."},
        {"f_vel","Programme: speed"," dB/s",0.2f,6,0.1f,1,"How fast it moves. Slow is the point, and the measurement says so: 1 dB/s gives 0.49 dB of spread between records, 3 dB/s gives 0.63, because above a decibel per second it starts chasing inside the track instead of moving the working point between records. Levelling is not chasing.",
         "Programma: velocita'","Quanto in fretta si muove. Lenta e' il punto, e lo dice la misura: a 1 dB/s lo scarto fra dischi e' 0.49, a 3 dB/s e' 0.63, perche' sopra il decibel al secondo comincia a inseguire DENTRO al brano invece di spostare il punto di lavoro FRA un disco e l'altro. Livellare non e' inseguire."},
        {"g_pre","Input gain"," dB",-24,40,0.5f,0,"How much the signal is lifted before anything else. Set it once, with the machine bypassed, so the input meter peaks around -12: from there on the whole chain works at the point it was tuned for. It is not a volume control - it is the entry level.",
         "Preamplificazione","Quanto viene alzato il segnale prima di ogni altra cosa. Si imposta UNA volta, guardando il metro d'ingresso: deve arrivare intorno a -12 sui picchi. Da li' in poi non si tocca piu', perche' tutta la catena e' tarata per quel punto di lavoro. Non e' un volume: e' il livello d'ingresso, quello che sull'Orban si regolava col cacciavite e poi si lasciava stare."},
        {"a_target","Target"," dB",-70,-6,0.5f,-18,"The level every record is brought to. Not a threshold: a destination.",
         "Bersaglio","Il livello a cui viene portato ogni disco. Non una soglia: una destinazione."},
        {"a_up","Rise"," dB/s",0.05f,6,0.05f,1,"How fast it may turn up. Slow is invisible.",
         "Salita","Quanto in fretta puo' alzare. Lenta non si sente."},
        {"a_win","Window"," dB",3,24,0.5f,18,"Total travel allowed, up and down.",
         "Finestra","Quanto gli e' concesso muoversi in tutto, su e giu'."},
        {"a_gate","Leveller: gate"," dB",-70,-20,1,-45,"Below this the gain freezes.",
         "AGC: cancello","Sotto questo livello il guadagno si congela: nelle pause non tira su il fruscio."},
        {"a_hold","Hold"," ms",0,1000,10,200,"Wait after a fall before rising again.",
         "Attesa","Quanto aspetta dopo una discesa prima di ricominciare a salire."},
        {"a_ant","Leveller: look-ahead"," ms",0,120,5,0,"The sound is delayed by this much while the detector reads it undelayed: when the hit arrives at the output the gain is already right. Measured 4 Sept: the wobble of the AGC output falls from 4.47 to 3.94 dB between 0 and 80 ms. It costs latency, one for one.",
         "AGC: anticipo","Il suono viene ritardato di tanto cosi', mentre il rivelatore lo legge senza ritardo: quando la botta arriva all'uscita il guadagno e' gia' quello giusto. E' come lavora un Optimod vero. Misurato il 4/9: il ballo dell'uscita dell'AGC scende da 4.47 a 3.94 dB fra 0 e 80 ms. Si paga in ritardo, uno per uno."},
        {"a_det","Detector"," ms",50,1000,10,300,"Long judges the song, short chases the phrases.",
         "Rivelatore","Lungo giudica il pezzo, corto insegue le frasi."},
        {"a_sub","Sub lift"," dB",0,9,0.1f,0,"Weight added around 68 Hz.",
         "Peso al grave","Peso aggiunto intorno ai 68 Hz."},
        {"a_subhz","Sub lift: where"," Hz",45,110,1,68,"Where the weight lives.",
         "Peso al grave: dove","Dove sta il peso."},
        {"a_boom","Boom cut"," dB",-12,0,0.1f,0,"Removes the ringing that follows the hit.",
         "Taglio del rimbombo","Toglie la coda che segue il colpo."},
        {"a_boomhz","Boom cut: where"," Hz",90,220,1,117,"Usually 110-130 Hz.",
         "Rimbombo: dove","Di solito fra 110 e 130 Hz."},
        {"a_boomq","Boom cut: width","",0.7f,4,0.1f,2.1f,"Narrow: take the ringing, not the body.",
         "Rimbombo: larghezza","Stretto prende la coda e lascia il corpo."},
        {"a_salto","Track change: from"," dB",0,20,0.5f,10,"How far off the level must be before it counts as a NEW record instead of a loud chorus. Below this, it stays slow.",
         "Cambio disco: da","Di quanto dev'essere sbagliato il livello perche' conti come disco NUOVO invece che come ritornello. Sotto, resta lenta."},
        {"a_saltoms","Track change: for"," ms",0,3000,50,800,"And for how long. A pause does not count.",
         "Cambio disco: per","E per quanto tempo. Una pausa non conta."},
        {"a_saltox","Track change: how fast"," x",1,20,0.5f,8,"How many times faster it catches up on a new record. 1 turns it off: then a quiet track after a loud one takes 40 seconds to come up.",
         "Cambio disco: quanto svelto","Quante volte piu' svelto recupera su un disco nuovo. A 1 e' spento: e allora un brano piano dopo uno forte ci mette quaranta secondi a salire."},
        // ⚠️⚠️ 22/08/2026 — CINQUE COMANDI CHE NESSUNO SALVAVA.
        // Il motore li leggeva, ma non erano in questa tabella: quindi il
        // salvataggio non li scriveva e il caricamento (che dal 20/08 parte
        // pulito) li riportava al valore di fabbrica. Il peggiore era
        // m_lock: ricaricando una taratura, il TETTO BLOCCATO si spegneva
        // da solo e la macchina tornava un multibanda qualunque. Luca lo ha
        // sentito subito: «mi sembra un po' confuso».
        {"a_pesato","Ear-weighted detector","",0,1,1,1,"Measure the way the ear hears instead of by raw energy. Off and an intro made of mid-highs looks quiet to the leveller, so it turns it up - right where the ear is most sensitive.",
         "Rivelatore pesato","Misura come sente l'orecchio invece che a energia. Spento, un intro fatto di medio-alti sembra piano al livellatore, e lui alza -- proprio dove l'orecchio e' piu' sensibile."},
        {"a_slope","Low cut: slope"," x12dB",1,3,1,2,"1 = 12 dB/oct, 2 = 24, 3 = 36.",
         "AGC: taglio sotto — pendenza","1 = 12 dB/ottava, 2 = 24. (3 oggi suona come 2.)"},
        {"a_sologiu","Down only","",0,1,1,0,"Never turns up, only down above the target: the leveller becomes a very slow compressor.",
         "Solo in discesa","Non alza mai, abbassa soltanto sopra il bersaglio: il livellatore diventa un compressore lentissimo."},
        {"a_cut","Leveller: low cut"," Hz",0,150,1,60,"High-pass at the input. 0 turns it off.",
         "AGC: taglio sotto"," Passa-alto all'ingresso. A 0 e' spento."},
        // ⭐ 18/08 — IL FRENO. Luca: «certe volte quando cambia il brano
        // entrano delle botte e ci impiega a scendere». Misurato sul banco,
        // brano che entra 18 dB piu' forte: la botta passava a +3.7 dB sopra
        // il bersaglio e ci metteva 0.36 s a rientrare. Col freno: -3.2 dB
        // e 0.02 s, e il movimento DENTRO il brano resta identico (5.2 dB
        // contro 5.35: e' rumore di misura).
        {"a_freno","Brake: window"," dB",2,24,0.5f,6,"How far the output may overshoot the target before the fast brake grabs it. Wide = the brake never works; narrow = it starts eating normal peaks.",
         "Freno: finestra","Di quanto l'uscita puo' scavalcare il bersaglio prima che il freno rapido la prenda. Largo = il freno non lavora mai; stretto = comincia a mangiare i picchi normali. Sotto 4 si sente."},
        {"a_frenov","Brake: speed"," dB/s",0,2000,50,800,"How fast it pulls the overshoot back. 0 turns the brake off, and the blast at a track change lasts a third of a second.",
         "Freno: velocita'","Quanto in fretta riporta dentro lo scavalco. A 0 il freno e' spento, e la botta al cambio disco dura un terzo di secondo."},
    };
    return v;
}

// ---------------------------------------------------------------- MB
// ⚠️⚠️ 30/08/2026 — IL FONDO DELLA CORSA NON DEVE ESSERE UNA TRAPPOLA.
// Luca: «vale per tutte le bande, se provo a mettere tutti i valori a zero
// gratta, tutto e' confuso». Ed e' vero, ed e' colpa di come li avevo fatti:
// per tre di questi comandi lo zero NON vuol dire spento, vuol dire il
// MASSIMO di severita'.
//   · rivelatore a 5 ms  -> il guadagno insegue la forma d'onda nota per
//     nota: e' esattamente il baco che abbiamo scoperto il 20 agosto, quando
//     stava a 10 ms, rimesso in mano all'utente come se fosse una scelta
//   · muro a +2 dB sopra il tetto, che molla in 5 ms -> non e' piu' un
//     limitatore, e' una scatola per distorcere
// Nessuno che gira una manopola verso il basso si aspetta di trovarci una
// cosa rotta. I minimi adesso partono da dove il comando ha ancora senso:
// rivelatore 50 ms, muro +4 dB, rilascio 15 ms.
inline const std::vector<Voce>& vociMbGlob() {
    static const std::vector<Voce> v = {
        {"m_inmin","Input: never below"," dB",-50,-6,0.5f,-30,"Quieter than this, it is brought up.",
         "Ingresso: mai sotto","Piu' piano di cosi', lo tira su. In modo 2 (4.26) e' anche il riferimento di tetti, cancelli e muri delle bande: spostarlo cambia quanto schiacciano e il volume d'uscita (misurato: da -4.7 a +2 dB)."},
        {"m_inmax","Input: never above"," dB",-40,0,0.5f,-12,"Louder than this, it is brought down. Between: untouched.",
         "Ingresso: mai sopra","Piu' forte di cosi', lo abbassa. In mezzo non lo tocca. In modo 2 (4.26) e' anche il riferimento di tetti, cancelli e muri delle bande, come 4.1."},
        {"m_indbs","Input: speed"," dB/s",0.5f,30,0.5f,6,"How fast it climbs back into the window.",
         "Ingresso: velocita'","Quanto in fretta rientra dentro la finestra."},
        {"m_xo1","Crossover 1"," Hz",40,400,1,100,"Between band 1 and 2.",
         "Incrocio 1","Fra la banda 1 e la 2. Attento a non tagliare la cassa in due."},
        {"m_xo2","Crossover 2"," Hz",150,1200,1,400,"Between band 2 and 3.",
         "Incrocio 2","Fra la banda 2 e la 3."},
        {"m_xo3","Crossover 3"," Hz",600,4000,1,1500,"Between band 3 and 4.",
         "Incrocio 3","Fra la banda 3 e la 4."},
        {"m_xo4","Crossover 4"," Hz",2000,12000,1,5000,"Between band 4 and 5.",
         "Incrocio 4","Fra la banda 4 e la 5."},
        {"m_hpf","Bands: low cut"," Hz",20,120,1,36,"Below the first band.",
         "Bande: taglio sotto","Sotto la prima banda."},
        {"m_acc","Bass coupling"," %",0,100,1,0,"How much the LOW band follows the master (everything else). One way only. 0 = independent, and the machine adds its own bass. 100 = they move together and you get closer to the sound from the desk. Orban recommends 70-100 only for classical; for pop, rock, soul, oldies stay low.",
         "Accoppiamento del grave","Quanto la banda GRAVE segue la master (tutto il resto). In una direzione sola. 0 = indipendenti, e la macchina ci mette il suo tocco di gravi. 100 = si muovono insieme e ci si avvicina al suono che arriva dal mixer. Orban raccomanda 70-100 solo per la classica; per pop, rock, soul, oldies si sta bassi."},
        {"m_acc34","Band 3&4 coupling"," %",0,100,1,0,"How much band 4 follows band 3. High = less listening fatigue on the highs. 0 = the purist setting, and it is what was on the panel.",
         "Accoppiamento bande 3-4","Quanto la banda 4 segue la 3. Alto = meno fatica sugli acuti. 0 = la scelta purista, ed e' quella che c'era sul pannello."},
        {"m_pend","Crossover slope"," dB/oct",24,48,24,24,"24 or 48 only: with an odd number of sections the bands no longer add back to the original signal. 48 separates the bands more, 24 lets them breathe into each other.",
         "Pendenza degli incroci","Solo 24 o 48: con un numero dispari di sezioni le bande non si risommano piu' nel segnale originale. 48 separa di piu', 24 le lascia respirare l'una nell'altra."},
        {"m_lib","Band freedom"," dB",2,24,0.5f,24,"How far a band may drift from what the others are doing. With it wide open, a track with no bass gets its empty bands lifted to the stop - the machine rebuilds a spectrum the record never had, and you reach for the volume. 3 to 5 keeps it honest (the number is the distance from the average, so the total spread is double): if the others are not lifting, the record is simply like that. 24 turns it off.",
         "Liberta' delle bande","Di quanto una banda puo' allontanarsi da quello che fanno le altre. Spalancata, un brano senza bassi si vede tirare su le bande vuote fino al fermo: la macchina ricostruisce uno spettro che il disco non ha, e tu vai ad abbassare il volume. Fra 3 e 5 resta onesta (il numero e' la distanza dalla MEDIA, quindi l'apertura totale fra la banda piu' alta e la piu' bassa e' il doppio): se le altre non alzano, il disco e' cosi'. A 24 e' spenta."},
        {"m_sbav","Band bleed"," %",0,100,1,0,"How much each band's gain follows the two next to it. Our crossovers are surgical: a sound sits in one band and stays there, and you hear every band separately. Real broadcast banks overlap - a hit in one band ducks its neighbours too, and the sounds stop being separable. This is that, done on the gains. 30-50 is where a machine stops sounding like a plugin.",
         "Sbavatura fra bande","Quanto il guadagno di ogni banda segue quello delle due vicine. I nostri incroci sono chirurgici: un suono sta in una banda e ci resta, e tu senti ogni banda separata. I banchi da trasmissione veri hanno le bande SOVRAPPOSTE - una botta abbassa anche le vicine, e i suoni smettono di essere separabili. Questo fa la stessa cosa, sui guadagni. Fra 30 e 50 una macchina smette di sembrare un plugin."},
        {"m_lim","Band wall"," dB",4,24,0.5f,8,"How far above each band ceiling the wall sits. Nothing gets past it, ever: instant attack, no overshoot. This is the certainty an Optimod gives you - you stop watching the meters. Low = quieter and flatter (at 4 you lose about 4 dB); 24 turns the wall off and the final stage does all the work - and the peak goes up to the ceiling.",
         "Muro di banda"," Quanti dB sopra il tetto sta il muro di ogni banda. Oltre quello non passa NIENTE, mai: attacco sullo stesso campione, zero sfondamento. E' la certezza che dava l'Orban, quella per cui il fonico smetteva di guardare i livelli. Basso = piu' piano e piu' piatto (a 4 si perdono circa 4 dB); a 24 il muro e' spento, tutto il lavoro torna al finale e il picco sale fino al tetto."},
        {"m_limgr","Band wall: bass extra"," dB",0,16,0.5f,0,"Extra dB of wall on the bass band (band 2 gets half). Measured 4 Sept: any wall on the bass flattens the kick waveform - the Orban does not wall its bass. Raise this and the bass keeps its shape while the rest of the spectrum stays walled.",
         "Muro: piu' sul grave","dB di muro IN PIU' sulla banda 1 (la 2 ne prende meta'). Misurato il 4/9: qualunque muro sul grave appiattisce l'onda della cassa, e l'Orban il grave non lo mura. Alzando questo il grave tiene la forma e il resto dello spettro resta murato."},
        {"m_limrel","Band wall: release"," ms",15,400,5,25,"How fast each wall lets go. Short = louder and more forward; long = calmer, and it breathes less. The bass wall is always slower than the treble one, in proportion.",
         "Muro: rilascio","Quanto in fretta ogni muro molla. Corto = piu' forte e piu' avanti; lungo = piu' calmo e respira meno. Quello del grave e' sempre piu' lento di quello dell'acuto, in proporzione."},
        {"m_unione","Band bonding"," %",0,100,1,0,"How much the five bands move together instead of each on its own. 0 is the 8200: bands free, dense and constant, but the machine remixes the record. 100 is the 8100: one wideband compressor wearing five bands, soft and all of a piece.",
         "Unione delle bande","Quanto le cinque bande si muovono insieme invece che ognuna per conto suo. A 0 e' l'8200: bande libere, denso e costante, ma la macchina rifa' il mix. A 100 e' l'8100 del 1980: un compressore a banda larga travestito da cinque bande, morbido e tutto d'un pezzo. Per ammorbidire si sale piano: 20-40 si sente gia'."},
        {"m_lock","Hold at ceiling","",0,1,1,1,"THE control that makes this an Optimod instead of a multiband compressor. On: above the gate each band is TAKEN to its ceiling and kept there - not 'do not exceed' but 'stay there'. Off: threshold and ratio, and the level goes on breathing.",
         "Tetto bloccato","E' IL comando che fa la differenza fra un Optimod e un multibanda qualunque. Acceso: sopra il cancello ogni banda viene PORTATA al suo tetto e ci resta -- non «non superare», ma «stai li'». Spento: soglia e rapporto, e il livello continua a respirare. Vale solo in modo 1 (4.26): in modo 2 non viene letto e sparisce."},
        // ⭐⭐ 02/09/2026 — LA STRETTA. Misurato: nella finestra dove si
        // sente (20-500 ms) l'Orban tiene le bande legate a 0.36 di
        // correlazione, noi a 0.25; e fra grave e acuti lui sta a 0.19,
        // noi a ZERO. Sbavatura e unione ce l'avevamo, ma agivano sul
        // livellatore da mezzo secondo: legavano le bande dove l'orecchio
        // non lo sente. Questi cinque comandi sono lo stadio di mezzo, ed
        // e' li' che «tanti suoni» diventa «un suono unico».
        {"m_str","Bonding compressor",":1",1,8,0.1f,1,"The middle compressor, and the only place where band bonding becomes audible. 1 = off. From 2 to 3 the bands stop being separable; above 4 it starts to flatten. The 500 ms leveller sets the level, the wall stops the peaks, and this one closes the gap between them - the window where music actually moves.",
         "La stretta","Il compressore di mezzo, e l'unico posto dove il legame fra le bande si SENTE. A 1 e' spenta. Fra 2 e 3 le bande smettono di essere separabili e il suono diventa uno solo; oltre 4 comincia a schiacciare. Il livellatore da mezzo secondo fa il livello, il muro ferma i picchi, e in mezzo - fra i venti millesimi e il mezzo secondo, dove la musica si muove davvero - finora non c'era niente."},
        {"m_strms","Bonding: window"," ms",3,120,1,25,"How wide a window the middle compressor measures over. Short and it chases notes; long and it stops being the middle stage.",
         "Stretta: finestra","Su che finestra misura il compressore di mezzo. Corta e insegue le note; lunga e smette di essere lo stadio di mezzo."},
        {"m_stratt","Bonding: attack"," ms",1,80,1,10,"How fast the middle compressor grabs. Short = the transient is caught and the sound is very together; long = the attacks stay through and it breathes.",
         "Stretta: attacco","Quanto in fretta prende. Corto = prende anche l'attacco e il suono e' tutto d'un pezzo; lungo = gli attacchi passano e respira."},
        {"m_strril","Bonding: release"," ms",30,1200,10,200,"How fast it lets go. This sets the density: short = dense and forward, long = the sound holds and never trails off.",
         "Stretta: rilascio","Quanto in fretta molla. E' qui che si decide la densita': corto = denso e avanti, lungo = il suono tiene e non scoda mai. E' la sensazione del basso che non se ne va."},
        {"m_strsog","Bonding: above ceiling"," dB",0,12,0.5f,6,"How far above each band ceiling the middle compressor starts working. The ceiling is a half-second average and this stage measures over 25 ms, so the peaks sit above it: from here up is what it closes.",
         "Stretta: sopra il tetto","Quanti dB sopra il tetto di ogni banda comincia a lavorare il compressore di mezzo. Il tetto e' una media da mezzo secondo e questa misura su venticinque millesimi: i picchi stanno sopra, ed e' quella la finestra che chiude. A zero stringe anche il livello e la banda finisce sotto il tetto."},
        {"m_strleg","Bonding: to neighbours"," %",0,100,1,23,"How much of each band's middle gain passes to the others, most to the ones next door and less - but never nothing - to the far ones. This is the shape measured on the real machine.",
         "Stretta: verso le vicine","Quanta parte del movimento di ogni banda passa alle altre: molta alle vicine, poca ma MAI zero alle lontane. E' la forma misurata sulla macchina vera: sull'Orban due bande agli estremi restano legate, da noi erano a zero."},
        {"m_strcom","Bonding: shared"," %",0,100,1,10,"The share of the middle gain that is common to all five - the whole programme breathing as one. This is what keeps the bass tied to everything else instead of living its own life.",
         "Stretta: quota comune","La parte del guadagno di mezzo che e' comune a tutte e cinque: il programma che respira tutto insieme. E' quello che tiene il grave attaccato al resto invece di lasciarlo vivere per conto suo - il basso che avvolge e non scoda mai."},
        {"m_det","Band detector"," ms",50,2000,5,500,"How wide a window each band is measured over. Short and the gain chases every note instead of the level, and the ceiling stops holding. 10 ms was the bug of 20 August.",
         "Rivelatore bande","Su che finestra si misura ogni banda. Corto e il guadagno insegue ogni nota invece del livello, e il tetto smette di tenere: era il baco del 20 agosto. Mezzo secondo tiene le bande sedute sul tetto."},
        {"m_sumax","Bands: may lift up to"," dB",0,24,0.5f,12,"How far a band may be brought UP toward its ceiling. This is the knob that makes a quiet record come out like a loud one.",
         "Bande: puo' alzare fino a","Quanto una banda VUOTA puo' salire verso il suo tetto. In modo 2 riempie i pezzi scarni (una banda acuta che entra piano viene tirata su), senza toccare i pezzi gia' pieni. A 0 e' spento: i pezzi vuoti restano vuoti."},
        {"m_giumax","Bands: may pull down up to"," dB",0,30,0.5f,18,"How far a band may be pulled DOWN. Raise it when a record refuses to be squashed.",
         "Bande: puo' abbassare fino a","Quanto una banda puo' essere ABBASSATA. Alzalo quando un disco non si lascia schiacciare."},
        {"m_mix","Bands: mix"," %",0,100,0.5f,100,"Parallel: below 100 the untreated signal keeps the transients.",
         "Bande: miscela","In parallelo: sotto 100 il segnale non lavorato tiene gli attacchi."},
        {"m_modo","Band mode","",1,2,1,1,"1 = old bands (absolute ceilings, upward gain). 2 = new bands (17/10): always pulling down, ceiling and gate relative to the input window, drive goes into the detectors, bank at unity, band wall belongs to section 5.",
         "Bande: modo","1 = bande vecchie (tetti assoluti, alzano). 2 = bande nuove (17/10): abbassano sempre, tetto e cancello relativi alla finestra d'ingresso, la spinta entra nei rivelatori, livello invariato, il muro e' della sezione 5."},
        {"m_spinta","Multiband drive"," dB",0,30,0.5f,15,"Mode 2 only: more signal into the band detectors = more squash, NOT more output.",
         "Bande: spinta","Solo modo 2: piu' segnale nei rivelatori = schiaccia di piu', NON alza l'uscita."},
        {"m_lega2","Band link 2-5"," %",0,100,1,40,"Mode 2 only: ties bands 2-5 together into one sound. Band 1 stays free so the kick does not move the treble.",
         "Bande: legame 2-5","Solo modo 2: lega le bande 2-5 in un suono unico. La banda 1 resta libera cosi' la cassa non muove gli acuti."},
    };
    return v;
}

// ---------------------------------------------------------------- MAX
inline const std::vector<Voce>& vociMax() {
    static const std::vector<Voce> v = {
        {"x_in","Input"," dB",0,30,0.01f,0,"How hard it is driven. The density knob.",
         "Ingresso","Quanto viene spinto dentro. E' la manopola della densita'."},
        {"x_shape","Shape"," %",0,100,0.5f,0,"At 0 a straight line. The travel is squared: the first half is nearly clean.",
         "Forma","A 0 e' una retta. La corsa e' al quadrato: la prima meta' e' quasi pulita."},
        {"x_mix","Output: mix"," %",0,100,0.5f,100,"100 = processed only.",
         "Finale: miscela","100 = solo il lavorato. Sotto 100 si mescola il secco, che e' molto piu' piano: fa da volume (a 50 si perdono 6 dB)."},
        {"x_out","Output"," dB",-12,0,0.01f,-1,"The ceiling.",
         "Uscita","Il tetto: oltre non passa niente, finche' il 6.10 Limitatore di sicurezza e' acceso e sei in modo Radio (6.8)."},
        {"x_ant","Output: look-ahead"," ms",0.5f,20,0.5f,10,"How far ahead the limiter looks. Long is clean but SMEARS transients: it turns the gain down before the hit arrives. Short keeps the punch and risks distortion on low notes.",
         "Finale: anticipo"," Di quanto il limitatore guarda avanti. Lungo e' pulito ma SMUSSA gli attacchi: abbassa il guadagno prima che il colpo arrivi. Corto tiene la botta e rischia distorsione sulle note basse."},
        {"x_rel","Output: release"," ms",20,600,5,120,"How long the limiter takes to let go.",
         "Finale: rilascio","Quanto ci mette il limitatore a mollare."},
        {"x_mahz","Upper-mids: where"," Hz",1200,7000,50,2400,"The region that tires the ear.",
         "Medio-alti: dove","La zona che stanca l'orecchio. Le tue 2400 stanno qui."},
        {"x_mafix","Upper-mids: always"," dB",-9,0,0.25f,0,"Held down at all times.",
         "Medio-alti: sempre","Tenuti giu' in ogni caso, anche quando non spingono."},
        {"x_madin","Upper-mids: when loud"," dB",-9,0,0.25f,0,"Held down only when that region pushes.",
         "Medio-alti: quando spingono","Tenuti giu' solo quando quella zona spinge davvero."},
        {"x_marel","Upper-mids: release"," ms",100,5000,50,1000,"Long is what creates distance.",
         "Medio-alti: rilascio","Lungo e' quello che li tiene lontani."},
        {"x_maq","Upper-mids: width","",0.4f,2.5f,0.05f,0.8f,"Low = a wide, natural dip.",
         "Medio-alti: larghezza","Bassa = una conca larga e naturale."},
        {"x_bshz","Bass tail: where"," Hz",50,180,1,90,"Where the tail lives.",
         "Coda del basso: dove","Dove sta la coda che scoda."},
        {"x_bsfix","Low bell: always"," dB",-12,0,0.25f,0,"A fixed cut (or nothing) around the Bass tail frequency, AFTER the bands - so the multiband cannot fill it back in. This is the -6.8 dB at 117 Hz you draw in Logic.",
         "Campana bassa: sempre","Un taglio fisso attorno alla frequenza della coda del basso, DOPO le bande - cosi' il multibanda non puo' rimettercelo. E' il -6.8 dB a 117 Hz che disegni in Logic."},
        {"x_bsdin","Bass tail: tighten"," dB",-9,0,0.25f,0,"Shortens the tail; the hit still goes through.",
         "Coda del basso: stringi","Accorcia la coda; il colpo passa lo stesso."},
        {"x_bsq","Bass tail: width","",0.5f,2.5f,0.05f,0.9f,"",
         "Coda del basso: larghezza",""},

    };
    return v;
}

// ------------------------------------------------- CLIPPING (i quattro stadi)
// Non stanno insieme al resto del FINALE per due motivi: non ci stavano in
// altezza, e soprattutto sono un blocco a se'. Questi quattro sono la
// differenza fra un limitatore e un Optimod.
inline const std::vector<Voce>& vociTosa() {
    static const std::vector<Voce> v = {
        {"x_rot","Phase rotator"," x",0,4,1,0,"Four all-pass sections. Straightens asymmetric waveforms before clipping. 0 = off.",
         "Rotatore di fase","Quattro passa-tutto. Raddrizza le forme asimmetriche prima di tosare: la voce ci guadagna 2-3 dB. 0 = spento."},
        {"x_arm","Bass harmonics"," dB",0,20,0.5f,0,"Synthesises the second harmonic of the bass and adds it, without touching the fundamental. Below 60 Hz the ear hears almost nothing at low volume, at 100-200 it still does: this is why an Orban bass is still there when you turn it down. 9 matches the real machine.",
         "Armoniche del grave","Sintetizza la seconda armonica del basso e la aggiunge, senza toccare la fondamentale. Sotto i 60 Hz a volume basso l'orecchio non sente quasi niente, a 100-200 sente ancora bene: e' per questo che il basso dell'Orban c'e' anche quando abbassi. A 9 sta dove sta la macchina vera. Il clipper del basso fa la stessa cosa togliendo invece che aggiungendo, e costa quattro dB di sporco: per quello e' a zero."},
        {"x_armhz","Bass harmonics: below"," Hz",40,200,5,120,"Which part of the bass gets harmonised. Lower = only the deepest notes.",
         "Armoniche: sotto","Quale parte del grave viene armonizzata. Piu' basso = solo le note piu' profonde."},
        {"x_bcdb","Bass clipper: drive"," dB",0,24,0.25f,0,"Clips the low band on its own, so the kick does not muddy the mids. 0 = off.",
         "Clipper del basso: spinta","Tosa il grave per conto suo, cosi' la cassa non impasta i medi. 0 = spento."},
        {"x_bchz","Bass clipper: where"," Hz",60,400,5,200,"Where the low band ends.",
         "Clipper del basso: dove","Dove finisce il grave. Sopra questa, non lo tocca."},
        // ⚠️ 18/08 — «muro in cima: appena si tocca una virgola cambia
        // tutto». Non era il filtro, era la scala: andava da 0 a 20000 a
        // scatti di 250, quindi il primo scatto sopra lo spento metteva un
        // passa-basso a 250 Hz. Adesso vive fra 10 e 20 kHz, dove un muro
        // ha senso, e SPENTO sta in cima: a 20k non filtra piu' niente.
        {"x_spazio","Space"," %",0,100,1,0,"Widens only the sides, and only above the corner frequency. The centre is never touched, so voice and bass stay put. Measured: our mid-highs already match a real Orban for width and for reverb tails, so this is not emulation any more - it is a choice. 0 leaves the machine faithful.",
         "Spaziosita'","Allarga SOLO i lati e SOLO sopra la frequenza scelta. Il centro non si tocca mai, quindi voce e basso restano dove sono e non si apre il buco in mezzo. Misurato: sui medio-alti la nostra immagine e' gia' quella dell'Orban vero e sulle code lo battiamo -- quindi questo non e' piu' emulare, e' scegliere. A 0 la macchina resta fedele."},
        {"x_spaziohz","Space: from"," Hz",200,6000,50,1500,"Below this the sides are left alone. Low and it opens the body of the mix too; 1200-2000 keeps it to the air and the reverb.",
         "Spaziosita': da","Sotto questa frequenza i lati restano com'erano. Bassa apre anche il corpo del pezzo; fra 1200 e 2000 resta sull'aria e sul riverbero."},
        // ⭐⭐ 03/09/2026 — I DUE MODI.
        // Luca: «mi devi spiegare perche' se tolgo il clipping finale sembra
        // andare meglio». La risposta e' che quel blocco, per chi ascolta,
        // non serve a niente. Sull'8200 la tosatura esiste per un vincolo
        // fisico della trasmissione FM: la modulazione ha un tetto che non si
        // puo' superare, e il clipper e' li' per spremere l'ultimo dB entro
        // quel tetto. E' il pedaggio che paghi per essere piu' forte in radio.
        // Chi ascolta in cuffia quel tetto non ce l'ha, e paga il pedaggio
        // senza ricevere niente in cambio: misurato, le cime dell'onda del
        // grave passano da 13.1 a 2.4 con la tosatura accesa (l'Orban sta a
        // 22.3), piu' la distorsione dei due clipper.
        // Quindi: ASCOLTO e' l'Orban senza il pedaggio della radio. RADIO e'
        // la macchina intera, per chi trasmette davvero.
        {"x_radio","Broadcast mode","",0,1,1,0,"Off = LISTENING: the clipping block is bypassed. The final clipper, the safety limiter and the look-ahead exist to squeeze the last dB under the FM modulation ceiling - a limit that does not exist when you listen on headphones or monitors, so they only cost you distortion and flattened bass peaks. On = BROADCAST: the whole machine, for actual transmission.",
         "Modo radio","Spento = ASCOLTO: il blocco della tosatura viene saltato. Il clipper finale, il limitatore di sicurezza e l'anticipo esistono per spremere l'ultimo dB sotto il tetto di modulazione FM -- un limite che in cuffia non esiste, quindi da te costano distorsione e cime del basso spianate senza dare niente in cambio. Acceso = TRASMISSIONE: la macchina intera, per chi manda davvero in FM."},
        {"x_tosaon","Clipping on","",0,1,1,1,"The clipping half of the last stage: phase rotator, bass clipper, pre-emphasis with the HF limiter and the clipper, and the 15 kHz wall. Off, the output half still works.",
         "Clipping acceso","La meta' che tosa dell'ultimo stadio: rotatore di fase, clipper del basso, pre-enfasi col limitatore acuti e il clipper, e il muro a 15 kHz. Spenta, la meta' d'uscita lavora lo stesso."},
        {"x_limit","Safety limiter","",0,1,1,1,"The last net at the end of the chain. With the clipper working it should barely move; turning it off means nothing guarantees the ceiling any more.",
         "Limitatore di sicurezza","L'ultima rete in fondo alla catena. Col clipper che lavora non dovrebbe quasi muoversi; spegnendolo non c'e' piu' niente che garantisca il tetto."},
        {"x_top","Top cut"," Hz",10000,20000,50,20000,"The 15 kHz wall. Measured on a real Orban: -34 dB at 15k, -56 at 16k. In FM nothing may go above 15 kHz, and that wall is part of the sound. All the way up = off.",
         "Muro in cima","Il taglio a 15 kHz. Misurato su un'uscita Orban vera: -34 dB a 15k, -56 a 16k. In FM sopra i 15 kHz non ci puo' andare niente, e quel muro fa parte del suono. Tutto in alto e' spento; la macchina vera sta a 15000."},
        {"x_pre","Pre-emphasis"," us",0,100,5,0,"The Optimod domain: 50 in Europe, 75 in the USA. Highs get held far harder than lows, automatically. 0 = off.",
         "Pre-enfasi","Il dominio dell'Optimod: 50 in Europa, 75 in America. Gli acuti vengono frenati molto piu' dei gravi, da soli. 0 = spento."},
        {"x_hfdb","HF limiter"," dB",-12,0,0.25f,0,"How much the highs may be held down when they shout. Works inside pre-emphasis.",
         "Limitatore acuti","Quanto puo' tenere giu' gli acuti quando gridano. Lavora dentro la pre-enfasi."},
        // Il minimo era 3000 e non arrivava dove serve a Luca: la sua zona
        // fastidiosa sono le 2400. Sceso a 2000. E' uno scaffale, quindi da
        // qui in su tiene giu' tutto: portarlo a 2400 vuol dire frenare
        // proprio quella regione, non solo i piatti.
        {"x_hfhz","HF limiter: where"," Hz",2000,12000,100,6000,"Where it listens to the highs. It is a shelf: from here upwards. Bring it down to 2400 to hold the region that tires the ear.",
         "Limitatore acuti: dove","Dove sente gli acuti. E' uno scaffale: da qui in su. Portalo a 2400 per frenare proprio la zona che stanca."},
        {"x_clip","Clipper"," dB",0,15,0.25f,0,"The density. It TRIMS the peaks instead of turning down: this is where the forward, in-your-face sound comes from. 3-6 dB is plenty; above 10 it is an effect.",
         "Clipper","TOSA le punte invece di abbassarle. Misurato: non alza il volume, aggiunge grana (a 3 dB si sente, a 6 gratta). E' il suono «cattivo»; per un suono pulito tienilo a 0."},
    };
    return v;
}

// i sei comandi per banda, ripetuti cinque volte
inline const std::vector<Voce>& vociBanda() {
    static const std::vector<Voce> v = {
        {"m_bon","Band on","",0,1,1,1,"Turn this band off to hear what it was doing. Off it still passes - the spectrum stays whole - but its gain sits at zero and its wall does nothing.",
         "Banda accesa","Spegnila per sentire cosa stava facendo. Spenta passa lo stesso -- lo spettro resta intero -- ma il suo guadagno sta fermo a zero e il suo muro non lavora."},
        {"m_sog","Band: gate"," dB",-70,0,0.5f,-55,"The silence gate, nothing else: below this the band is frozen so the gap between records is not pulled up. It is NOT a floor for the gain - how far it may lift is set in BANDS: setup.",
         "Banda: cancello","Solo il cancello del silenzio: sotto, la banda si congela, cosi' il vuoto fra un pezzo e l'altro non viene tirato su. NON e' un pavimento del guadagno: quanto puo' alzare si decide in BANDE: assetto."},
        {"m_sot","Below gate: gain"," dB",-30,12,0.5f,0,"Where the band's gain goes when the band drops below its gate. 0 = back to unity, as before. Set it near the reduction the band usually works at and it stays put instead of jumping up when the music gets quiet.",
         "Sotto il cancello: guadagno","Dove va il guadagno della banda quando la banda scende sotto il suo cancello. A 0 torna a zero, come prima: una banda che lavora a -15 risale di quindici dB appena la musica cala. Mettilo vicino alla riduzione a cui la banda lavora di solito e resta ferma."},
        {"m_tet","Ceiling"," dB",-80,0,0.5f,-8,"Never allowed above this. The maximum.",
         "Tetto","Da dove la banda comincia a schiacciare. In modo 2 si conta dal centro della finestra 4.1-4.2: intorno a -10 schiaccia come l'8200, piu' giu' schiaccia di piu'."},
        {"m_rap","Ratio",":1",1,60,0.1f,4,"How hard it squashes inside the window.",
         "Rapporto","Quanto schiaccia sopra il tetto: 1 = niente, 10 = quasi fermo. In modo 2 conta sempre."},
        {"m_att","Attack"," ms",0.2f,120,0.1f,15,"Slow low, fast high.",
         "Attacco","Lento sul grave, veloce sull'acuto. Lento lascia passare il colpo."},
        {"m_ril","Band: release"," ms",20,2000,5,200,"Short = dry. Long = the band sits back.",
         "Banda: rilascio","Corto = asciutto. Lungo = la banda sta indietro e da' meno fastidio."},
        {"m_gai","Gain"," dB",-12,30,0.1f,0,"Make-up for this band.",
         "Guadagno","Recupero di volume per questa banda sola."},
    };
    return v;
}

} // namespace uno
