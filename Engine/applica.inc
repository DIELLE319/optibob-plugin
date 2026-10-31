void Catena::applica() {
    const agc::Params da; const omb::Params dm; const omax::Params dx;
    pa.targetDb=q("a_target",da.targetDb); pa.upDbS=q("a_up",da.upDbS);
    pa.downDbS=q("a_down",da.downDbS);
    pa.maxUpDb=pa.maxDownDb=q("a_win",da.maxUpDb);
    pa.gateDb=q("a_gate",da.gateDb); pa.holdMs=q("a_hold",da.holdMs);
    pa.anticipoMs=q("a_ant",0.0);    pa.rmsMs=q("a_det",da.rmsMs);    pa.hpfHz=q("a_cut",da.hpfHz);
    pa.saltoDb=q("a_salto",da.saltoDb); pa.saltoMs=q("a_saltoms",da.saltoMs);
    pa.saltoX=q("a_saltox",da.saltoX);
    pa.frenoDb=q("a_freno",da.frenoDb);   pa.frenoDbS=q("a_frenov",da.frenoDbS);
    pa.frenoMs=q("a_frenoms",da.frenoMs);
    pa.hpfSez=1+(int)q("a_slope",1); pa.pesato=q("a_pesato",1)>0.5;
    pa.soloGiu=q("a_sologiu",0)>0.5;
    pa.subHz=q("a_subhz",da.subHz);  pa.subDb=q("a_sub",da.subDb);
    pa.boomHz=q("a_boomhz",da.boomHz); pa.boomDb=q("a_boom",da.boomDb);
    pa.boomQ=q("a_boomq",da.boomQ);

    for (int i = 0; i < 5; ++i) {
        const std::string n = std::to_string(i+1);
        pm.b[i].soglia=q("m_sog"+n,dm.b[i].soglia);
        pm.b[i].tetto=q("m_tet"+n,dm.b[i].tetto);
        pm.b[i].rapporto=q("m_rap"+n,dm.b[i].rapporto);
        pm.b[i].attaccoMs=q("m_att"+n,dm.b[i].attaccoMs);
        pm.b[i].rilascioMs=q("m_ril"+n,dm.b[i].rilascioMs);
        pm.b[i].gainDb=q("m_gai"+n,dm.b[i].gainDb);
        pm.b[i].sottoDb=q("m_sot"+n,0.0);
        pm.b[i].accesa=q("m_bon"+n,1.0)>0.5;
    }
    for (int i = 0; i < 4; ++i)
        pm.xover[i]=q("m_xo"+std::to_string(i+1),dm.xover[i]);
    pm.inMin=q("m_inmin",dm.inMin); pm.inMax=q("m_inmax",dm.inMax);
    pm.inDbS=q("m_indbs",dm.inDbS); pm.hpfHz=q("m_hpf",dm.hpfHz);
    pm.mix=q("m_mix",100.0)*0.01;   pm.outDb=q("m_out",dm.outDb);
    pm.rivMs=q("m_det",dm.rivMs); pm.suMax=q("m_sumax",dm.suMax); pm.giuMax=q("m_giumax",dm.giuMax);
    pm.liberta=q("m_lib",dm.liberta); pm.sbavatura=q("m_sbav",0.0)*0.01; pm.limDb=q("m_lim",dm.limDb); pm.limRelMs=q("m_limrel",dm.limRelMs); pm.limGrave=q("m_limgr",0.0);
    pm.unione=q("m_unione",0.0)*0.01; pm.accBasso=q("m_acc",0.0)*0.01; pm.acc34=q("m_acc34",0.0)*0.01;
    // la stretta — lo stadio di mezzo, 02/09
    pm.strRap=q("m_str",dm.strRap);     pm.strMs =q("m_strms", dm.strMs);
    pm.strAtt=q("m_stratt",dm.strAtt);  pm.strRil=q("m_strril",dm.strRil);
    pm.strLeg=q("m_strleg",23.0)*0.01;  pm.strCom=q("m_strcom",10.0)*0.01;
    pm.strSog=q("m_strsog",dm.strSog);
    pm.pendenza=(q("m_pend",24.0) >= 36.0) ? 4 : 2;
    pm.bloccaAlTetto=q("m_lock",0)>0.5;
    pm.modo=(int)q("m_modo",1.0); pm.spinta=q("m_spinta",15.0); pm.lega2=q("m_lega2",40.0)*0.01;

    // ⭐ 26/09 — le schede 4 e 5 hanno ciascuna il suo interruttore.
    // Luca: «voglio poter selezionare tutti i 6 punti uno staccato
    // dall'altro, in questo momento il 4 e il 5 sono uniti». Erano tutti e
    // due m_on, cioe' il banco intero. Adesso:
    //   m_on  (scheda 3) = il banco intero, come prima;
    //   m_com (scheda 4) = cio' che lega le bande fra loro: muro, stretta,
    //                      sbavatura, unione, accoppiamenti, liberta'.
    //                      Spento, le bande lavorano ognuna per conto suo;
    //   m_ban (scheda 5) = i cinque compressori: spento, le bande passano
    //                      a guadagno zero e il loro muro non lavora.
    // ⭐ 26/09 — SCHIACCIA. Luca: «non voglio cambiare il preset, mi serve un
    // unico cursore che ne raggruppi 5-6, quelli che schiacciano». Quindi
    // non riscrive i comandi: si SOMMA a loro qui, al momento di applicarli.
    // La taratura resta com'e'; a 0 non cambia niente. Per tacca (misurato
    // al banco): guadagno delle bande +1.5 dB contro un muro -1 dB, stretta
    // +0.2, unione +3%.
    {
        const double sc = std::min(10.0, std::max(0.0, q("o_schiaccia", 0.0)));
        if (sc > 1e-6) {
            if (pm.modo == 2) pm.spinta += 1.5 * sc;     // modo 2: spinge DENTRO i rivelatori
            else {
            for (int i = 0; i < 5; ++i) pm.b[i].gainDb += 1.5 * sc;
            pm.limDb  = std::max(4.0, std::min(24.0, pm.limDb) - 1.0 * sc);
            }
            pm.strRap = std::min(8.0, pm.strRap + 0.2 * sc);
            pm.unione = std::min(1.0, pm.unione + 0.03 * sc);
        }
    }
    if (q("m_com", 1.0) < 0.5) {
        pm.sbavatura = 0.0; pm.unione = 0.0; pm.accBasso = 0.0; pm.acc34 = 0.0;
        pm.liberta = 24.0; pm.strRap = 1.0;
        if (pm.modo != 2) { pm.limDb = 24.0; pm.limGrave = 0.0; }   // modo 2: il muro e' della scheda 5
    }
    if (q("m_ban", 1.0) < 0.5)
        for (int i = 0; i < 5; ++i) pm.b[i].accesa = false;

    // ⭐ 03/09 — il modo comanda sugli interruttori: in ASCOLTO la tosatura,
    // il limitatore di sicurezza e l'anticipo restano spenti comunque, perche'
    // servono solo a stare sotto il tetto di modulazione FM.
    const bool radio = q("x_radio", 0.0) > 0.5;
    px.tosaOn = radio && (q("x_tosaon",1.0)>0.5);  px.inDb=q("x_in",dx.inDb);      px.shape=q("x_shape",0.0)*0.01;
    px.mix=q("x_mix",100.0)*0.01;   px.outDb=q("x_out",dx.outDb);
    px.rilascioMs=q("x_rel",dx.rilascioMs); px.anticipoMs=radio ? q("x_ant",dx.anticipoMs) : 0.0; px.limit=radio && (q("x_limit",1)>0.5);
    px.maHz=q("x_mahz",dx.maHz);    px.maFisso=q("x_mafix",dx.maFisso);
    px.maDin=q("x_madin",dx.maDin); px.maRel=q("x_marel",dx.maRel);
    px.maQ=q("x_maq",dx.maQ);
    px.bsHz=q("x_bshz",dx.bsHz);    px.bsDin=q("x_bsdin",dx.bsDin); px.bsFisso=q("x_bsfix",0.0);
    px.bsRel=q("x_bsrel",dx.bsRel); px.bsQ=q("x_bsq",dx.bsQ);
    px.rotSez=(int)lround(q("x_rot",0.0));
    px.armDb=q("x_arm",dx.armDb); px.armHz=q("x_armhz",dx.armHz);
    px.bcDb=q("x_bcdb",0.0);        px.bcHz=q("x_bchz",dx.bcHz);
    px.spazio=q("x_spazio",0.0)*0.01; px.spazioHz=q("x_spaziohz",1500.0);
    px.preUs=q("x_pre",0.0); px.topHz=q("x_top",0.0);        px.hfDb=q("x_hfdb",0.0);
    px.hfHz=q("x_hfhz",dx.hfHz);    px.clipDb=q("x_clip",0.0);

    pe.hpHz=q("e_hp",0.0); pe.hpQ=q("e_hpq",0.707); pe.hpSez=(int)q("e_hpsez",1);
    pe.lsHz=q("e_lshz",68.0);  pe.lsDb=q("e_lsdb",0.0);
    pe.b1Hz=q("e_b1hz",117.0); pe.b1Db=q("e_b1db",0.0); pe.b1Q=q("e_b1q",2.1);
    pe.b2Hz=q("e_b2hz",250.0); pe.b2Db=q("e_b2db",0.0); pe.b2Q=q("e_b2q",1.0);
    pe.b3Hz=q("e_b3hz",2500.0);pe.b3Db=q("e_b3db",0.0); pe.b3Q=q("e_b3q",1.0);
    pe.hsHz=q("e_hshz",10000.0); pe.hsDb=q("e_hsdb",0.0);
    pe.lpHz=q("e_lp",0.0); pe.lpQ=q("e_lpq",0.707);
    equalizzatore.setParams(pe);
    onEq.store(q("e_on", 1.0) > 0.5);
    onAgc.store(q("a_on", 1.0) > 0.5);
    onMb .store(q("m_on", 1.0) > 0.5);
    onMax.store(q("x_on", 1.0) > 0.5);

    livellatore.setParams(pa); multibanda.setParams(pm); finale.setParams(px);
    gPre_ = std::pow(10.0, q("g_pre", 0.0) * 0.05);
    cancDb_ = q("g_canc", -200.0);   cancMs_ = q("g_cancms", 60.0);
    finOn_  = q("f_on", 0.0) > 0.5;
    finMin_ = q("f_min", -20.0);  finMax_ = q("f_max", -20.0);  finVel_ = q("f_vel", 1.0);
    if (finMax_ < finMin_) finMax_ = finMin_;
    porta_.imposta(q("g_porta", 0.0));
}