# Recupero limitato del primo autorouting

Candidato recuperato: [ESP-DubSiren.kicad_pcb](../../kicad/ESP-DubSiren-recovered-diagnostic.kicad_pcb).
Il primo candidato originale, il candidato misto e il PCB attivo restano invariati.
Nessun nuovo script, adattatore DSN, autorouting, piano di rame o file di fabbricazione.
Le modifiche sono state applicate con pcbnew e verificate con il DRC nativo KiCad 10.0.6.

**Risultato: 0 nuovi errori DRC, 0 connessioni aperte, 820 segmenti e 67 via.**
Rimangono i 18 errori geometrici U5/U6 della baseline, senza esclusioni o cambi di regole.
Anche gli avvisi e la parità schema/PCB sono identici alla baseline.

Sono stati modificati 17 segmenti preesistenti e aggiunti 12 segmenti per piccoli
scostamenti locali attorno agli intagli. Tutti i via sono rimasti identici, incluse
posizione, rete, diametro e drill. Verificata l'integrità di footprint, pad, netlist,
placement, lati, grafica, archi del profilo e keepout M3/U2.

Gli 8 errori di distanza dagli intagli riguardavano POT5/RV4, ENC_B/SW7,
BTN1/RV4, MASTER/RV1 (tre segnalazioni), USB_DP_CONN/RV2 e POT7_TOP/RV7.
Sono stati corretti spostando soltanto tratti delle stesse reti e i raccordi adiacenti.
I 4 errori di larghezza erano due segmenti GND e due TFT_DC: portati da
0,120 a 0,127 mm, lasciando invariato il minimo globale e tutte le netclass.

I **113 segmenti sotto larghezza nominale non erano 113 violazioni DRC**:
109 rispettavano già il minimo; i quattro corretti restano sotto la larghezza
preferita di 0,2 mm. Non sono state trovate regole `.kicad_dru` più specifiche.
Il [raggruppamento originale completo](narrow-segments.csv) riporta rete,
larghezza, numero di segmenti, lunghezza totale e segmento più lungo.
Il [raggruppamento finale](narrow-segments-after.csv) include le quattro correzioni.

| Rete | Larghezza effettiva | Segmenti | Lunghezza totale dei restringimenti |
|---|---:|---:|---:|
| +3V3 | 0,3998 / 0,600 mm | 2 / 8 | 0,5566 / 3,7022 mm |
| +5V | 0,3998 / 0,600 mm | 3 / 2 | 1,9782 / 3,5005 mm |
| VBUS_RAW | 0,600 mm | 8 | 9,0802 mm |
| VBUS_PROTECTED | 0,601 mm | 2 | 1,1693 mm |
| USB_DM_CONN | 0,450 mm | 8 | 5,2488 mm |
| ADC_MUX | 0,150 mm | 2 | 2,5716 mm |
| GND, dopo correzione | 0,127 / 0,150 mm | 2 / 19 | 1,6683 / 11,9466 mm |
| AUDIO_MONO e tutte le I²S | 0,200 mm | nessun restringimento | — |

**Accettabili rispetto alle regole attuali:** tutti i restringimenti ora passano
il DRC. Quelli ADC sono tratti di segnale; VBUS_PROTECTED da 0,601 mm termina
sul pin ON di U7, distinto dal percorso di potenza VIN→VOUT. Non emerge una
violazione di larghezza da questi tratti. Audio e I²S mantengono la larghezza nominale.

**Ancora da verificare elettricamente:** i restringimenti da circa 0,4 mm sulle
alimentazioni sono presso U2. A passo 0,65 mm, con pad da 0,4 mm e clearance
0,15 mm, una pista centrata da 0,8 mm non entra fra i pad; il massimo locale
centrato è 0,6 mm. Su J1 il massimo centrato è 0,7 mm, inferiore a 0,8 mm.
Questo giustifica una riduzione locale, ma non dimostra che ogni restringimento
introdotto dall'autorouter sia necessario, né ne certifica la portata.
Mancano un bilancio di corrente/spunto e il rame di produzione confermato.
Particolare attenzione ai 1,6683 mm di GND da 0,127 mm presso C1/U2 e ai tratti
GND da 0,15 mm del convertitore: passare il DRC non qualifica questi ritorni.
Il [datasheet MPS](../MPM3612GLQ-33.pdf), p.17, richiede percorsi
VIN/PGND/VOUT brevi e larghi e bassa impedenza del rame di alimentazione e massa.

**Fabbricazione, limitatamente alle larghezze:** la preferenza generica Konnect
è JLCPCB con minimo 0,15 mm; non è una regola specifica del progetto e non ha
sostituito il minimo KiCad 0,127 mm indicato dall'utente. Le capacità pubblicate
JLCPCB per FR4 a due layer sono 0,10/0,10 mm con rame 1 oz e 0,16/0,16 mm
con rame 2 oz. Le larghezze conservate risultano quindi compatibili con il limite
pubblicato per 1 oz, **non** con 2 oz nei tratti da 0,127/0,15 mm. Non è una
validazione dell'intero PCB o un'approvazione di uno stackup non ancora scelto.
[Fonte JLCPCB, consultata il 31/10/2026](https://jlcpcb.com/capabilities/pcb-capabilities/).

**USB:** gli otto segmenti D− da 0,45 mm sono distribuiti fra J1, il corridoio
centrale e R2, non soltanto a ridosso dei pad. D+ resta a 0,6 mm. I percorsi non
costituiscono una coppia differenziale validata; impedenza, accoppiamento,
discontinuità e ritorno GND rimangono da verificare. Le lunghezze aggregate delle
reti a monte sono circa 58,83 mm D+ e 43,53 mm D−, ma includono diramazioni e
collegamenti ai pin ESD: **la loro differenza non è una misura dello skew USB**.

**I²S, ADC e audio:** I²S BCLK/LRCK/DIN misurano rispettivamente circa
36,10 / 26,83 / 27,96 mm; non risultano sottolarghezze e non sono state eseguite
compensazioni o validazioni temporali. ADC_MUX misura complessivamente circa
39,99 mm, inclusi filtro e test point; AUDIO_MONO circa 54,29 mm, inclusa la
diramazione TP5. Restano da valutare ritorni, accoppiamento digitale/analogico
e rumore; non è stato aggiunto un piano GND per presumere di risolverli.

**67 via:** 57 da 0,6/0,3 mm e 10 da 0,7/0,3 mm, conformi alle regole KiCad
attuali. Non risultano coppie di via della stessa rete distanti meno di 2 mm.
USB ha un via su ciascuna rete a monte; LRCK due, ADC due, audio uno.
Il numero totale da solo non dimostra un problema. UART_TX presenta un percorso
di 80,11 mm con quattro via, contro circa 56,07 mm fra i due terminali in linea
d'aria: è un candidato a futura revisione dell'efficienza, non una prova che i
via siano eliminabili senza interferire con le altre reti. Nessuna ottimizzazione eseguita.

**Giudizio:** il primo candidato è recuperabile e costituisce una base più avanzata
del misto per una successiva revisione mirata di USB e alimentazione/ritorni GND.
Le 12 violazioni non richiedevano di rifare il routing. Non è ancora un PCB
elettricamente validato o fabbricabile, anche per i 18 errori meccanici preesistenti.

Evidenze: [DRC prima](before-drc.json), [DRC finale](after-drc.json),
[invarianti e modifiche puntuali](verification.json), [vista rame](copper.svg).

I file diagnostici .kicad_pro e .kicad_sch sono copie identiche di quelli usati
per la verifica, conservate accanto al PCB per mantenere regole e schema associati.
I percorsi nelle evidenze JSON identificano la copia temporanea verificata prima
dell'archiviazione; il PCB archiviato ne conserva esattamente il contenuto.
