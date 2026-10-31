# Preparazione del routing misto — 31 ottobre 2026

Il candidato è `hardware/kicad/ESP-DubSiren-mixed-diagnostic.kicad_pcb`.
Il PCB attivo `ESP-DubSiren.kicad_pcb` resta invariato rispetto a main
`3b00a55b289f3d6aac7a9ad020a800adb24b5f17`. I file `.kicad_pro` e
`.kicad_sch` con il suffisso diagnostico sono copie identiche degli originali:
servono ad aprire e verificare il candidato con le stesse regole e lo stesso schema.

Preparazione **parziale**, valida come base KiCad; il successivo passaggio a
Freerouting è **bloccato dalla diversa interpretazione delle connessioni ai pad**.
Non è stato eseguito un terzo autorouting. Le invocazioni Freerouting di questa
fase caricano e verificano i dati con fanout, autorouter e optimizer disabilitati.
Nessun Computer Use, piano di massa, Gerber, merge o PR.

| Verifica KiCad 10.0.6 | Risultato |
|---|---|
| Connessioni | 160 iniziali → 119 aperte; 41 completate |
| Rame aggiunto | 136 segmenti, 13 via; tutti bloccati contro modifiche accidentali |
| Larghezze | Default 0,2 mm, USB 0,6 mm, Power 0,8 mm; nessun segmento sottodimensionato |
| Via | 0,6/0,3 mm Default; 0,7/0,3 mm Power |
| Errori geometrici | Gli stessi 18 preesistenti U5/U6; nessun nuovo errore |
| Avvisi | Gli stessi 92 geometrici/libreria e 68 di parità; nessuna esclusione aggiunta |
| Integrità | Footprint, pad, reti, posizioni, rotazioni, lati, grafica e archi invariati |
| Scheda | Due layer, profilo originale 86 × 126 mm; cinque keepout M3/U2 preservati |

| Reti critiche | Preparazione effettiva |
|---|---|
| I²S | BCLK, LRCK, DIN collegati; lunghezze 36,335 / 28,411 / 37,872 mm, senza compensazione di lunghezza |
| Audio | U6 OUTL → J2 T e TP5 collegati; collegamento GND U6.7 → J2 S preparato |
| ADC | U3.3, U1.4, C9.1 e TP4 collegati |
| USB dati | Collegati solo R2/R3 → ESP32; a monte restano 4 connessioni D− e 4 D+ |
| Alimentazione | VBUS_RAW, VBUS_PROTECTED e VOUT_S collegati; percorso U7 → U2 e U2 → U1 preparato |
| Da completare | +5V: 2 connessioni; +3V3: 20; GND: 42; altre reti e CT/CC ancora aperte |

Questa preparazione non valida impedenza USB, temporizzazioni I²S, ritorni GND,
rumore analogico, corrente erogabile o producibilità. Gli errori dello stack
meccanico U5/U6 rimangono irrisolti. Le piste USB corte misurano 2,925 e 3,712 mm:
non sono presentate come coppia a impedenza o lunghezza controllata.

Il trasferimento delle regole è riproducibile con
`hardware/tools/export_freerouting.py`, usando il Python di KiCad:

```powershell
& 'C:\Program Files\KiCad\10.0\bin\python.exe' hardware/tools/export_freerouting.py hardware/kicad/ESP-DubSiren-mixed-diagnostic.kicad_pcb tmp/mixed-routing-export
```

La directory di destinazione deve essere nuova. Lo script esporta tramite pcbnew,
adatta soltanto il DSN, produce un manifesto e gli argomenti CLI, e non avvia routing.
È specifico per questo progetto e si arresta se cambiano le classi, i keepout o le
geometrie dei fori previste. Non implementa un autorouter.

- Rimosso dall'esportazione lo sconto SMD-SMD da 0,15 a 0,0375 mm non presente nelle regole KiCad.
- I 16 intagli ricevono una classe di distanza dedicata: 0,5 mm più 6 µm per l'approssimazione degli archi e la quantizzazione DSN. La regola KiCad resta 0,5 mm.
- M3/U2 mantengono le aree approvate. Ai loro contorni viene aggiunto soltanto un quanto DSN di 0,1 µm, per evitare che il controllo Freerouting ignori sovrapposizioni con distanza esattamente zero.
- Verificati contro i drill nativi i 54 keepout circolari NPTH esportati: contengono già 0,25 mm di distanza. Non si applica un secondo aumento globale. Per i PTH è verificato che annulus più clearance rame coprano il minimo foro-rame.
- Confermati nel `settings_snapshot` di Freerouting 2.5.0: `automatic_neckdown=false`, `neck_width_um=0`, `fanout.enabled=false`, `optimizer.enabled=false`; limite futuro 8 passate/9:50. Il fanout è disabilitato perché può introdurre restringimenti anche con neckdown automatico disabilitato.
- Prova su una copia sacrificabile, senza routing: tre violazioni presso gli intagli delle classi Default/USB/Power vengono ora rilevate. Tre ulteriori intrusioni M3/U2/NPTH rimangono rilevate: 3 segnalazioni con l'esportazione nativa, 6 con quella corretta. Nessuna di queste piste di prova entra nel candidato.

Rimane un ostacolo distinto dalle regole: al semplice caricamento del candidato,
Freerouting conta **132 connessioni aperte e 16 segnalazioni presso J1**, contro
119 aperte e nessun nuovo errore di KiCad. Le 16 segnalazioni riguardano VBUS_RAW
e i pad vicini GND/CC/SBU. Le distanze calcolate da Freerouting sono circa 0,1001 mm
a fronte di 0,15 mm richiesti. Non sono state escluse o dichiarate risolte.

Il codice di Freerouting riconosce il contatto pista-pad quando un estremo coincide
esattamente con il centro del pad. Gli ingressi decentrati e alcune giunzioni
riconosciute da KiCad non soddisfano questo modello. Su J1, portare al centro una
pista da 0,8 mm lascia solo `0,85 − 0,35 − 0,40 = 0,10 mm` dal pad adiacente:
la soluzione non è semplicemente allungare la pista fino al centro.
Un'uscita centrata richiederebbe al massimo 0,7 mm, da valutare elettricamente
e autorizzare prima di cambiare la prescrizione attuale. In alternativa resta
possibile continuare il routing manuale nativo con gli ingressi decentrati.

Anche USB richiede una scelta esplicita prima del tracciato principale: la
netclass normale è 0,6 mm, ma i parametri differenziali ereditati sono
0,2 mm / gap 0,25 mm. Non sono stati resi equivalenti né dimensionati senza
stackup verificato. Il candidato conserva quindi il tratto a monte aperto.

Un SES prodotto senza routing è stato importato **in una copia dello stesso
candidato**: i 149 oggetti rame bloccati sono rimasti presenti, con 119 connessioni
aperte e lo stesso DRC. Il SES è incrementale: non contiene le piste fisse come
nuove piste da applicare a una scheda vuota.

Evidenze: [verifica](mixed-routing/verification.json),
[DRC KiCad](mixed-routing/drc.json),
[parametri e statistiche Freerouting](mixed-routing/freerouting-settings-verification.json),
[DRC Freerouting dettagliato](mixed-routing/freerouting-drc.json),
[manifesto del trasferimento](mixed-routing/transfer.json),
[percorsi espliciti](mixed-routing/manual-routes.json).
La prosecuzione automatica resta sospesa; il candidato non sostituisce il PCB attivo.

Riferimenti primari della versione installata:
[fanout e micro-neckdown](https://github.com/freerouting/freerouting/blob/b647a2f824cfbc54bc4dcaee71b03b7cd945eb76/src/main/java/app/freerouting/autoroute/path/FoundConnectionInserter.java),
[contatti al centro dei pad](https://github.com/freerouting/freerouting/blob/b647a2f824cfbc54bc4dcaee71b03b7cd945eb76/src/main/java/app/freerouting/board/model/items/DrillItem.java#L273),
[lettura delle clearance DSN](https://github.com/freerouting/freerouting/blob/b647a2f824cfbc54bc4dcaee71b03b7cd945eb76/src/main/java/app/freerouting/io/specctra/parser/Structure.java).
