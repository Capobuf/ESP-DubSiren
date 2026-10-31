# Evidenze dello studio #38

La [relazione](../issue-38-rear-placement.md) descrive la proposta nell'unico
PCB attivo. I file `before` sono export della base #36 preservata; `after`
sono export della proposta finale. `candidate-courtyards.json` conserva la
diagnosi del primo spostamento, prima di liberare i passivi. Non sono PCB
alternativi: nessun secondo file `.kicad_pcb` è stato creato.

- SVG `*-native.svg`, GenCAD e DRC: export KiCad tramite Konnect MCP.
- `*-pads.json`: query Konnect di geometrie, UUID e reti del file salvato.
- `*-components.json`: coordinate lette dall'export GenCAD (pollici convertiti
  in millimetri, Y invertita per il riferimento KiCad).
- `*-courtyards.json`: query Konnect `check_clearance`, sorgente `saved_file`.
- `*-mutation.json`: risposte delle tre operazioni di placement, non script
  che modificano i sorgenti.
- `positions.csv`, `verification.json`, `signal-mst-comparison.json`: confronto
  geometrico e di connettività. Le lunghezze sono rette/MST, non routing.
- PNG/SVG di confronto, ratsnest e stack: derivati esplicativi, non output
  produttivi né prove di altezze.
- `header-study.json`: calcoli e inviluppi per il riferimento commerciale
  SLxG1 aggiunto alla issue; [riesame](../issue-38-header-stack-review.md).
  La sezione aggiornata distingue la prova nominale da 2,50 mm dalle altezze
  dei moduli ancora ignote. I dati non qualificano gli header inclusi Adafruit.

Per rigenerare verifiche sui dati esportati e tavole, dalla radice del repository:

```powershell
python hardware/docs/placement-38/verify_exports.py
if ($LASTEXITCODE -ne 0) { throw 'Verifica export fallita' }
python hardware/docs/placement-38/build_review.py
if ($LASTEXITCODE -ne 0) { throw 'Generazione tavole fallita' }
```

Il secondo comando usa CairoSVG. Entrambi lavorano sui dati esportati e sui
derivati di documentazione: non scrivono sorgenti KiCad. Non sostituiscono una
nuova esecuzione DRC se il PCB viene cambiato. L'hash della proposta verificata
è in `verification.json`; confrontarlo prima di attribuirle questi risultati.

Il DRC completo risiede in `before-drc.json` e `after-drc.json`. I corrispondenti
`*-drc-result.json` sono riepiloghi MCP con limite dettagli zero: `truncated`
non significa che il file DRC completo sia incompleto. I conteggi della
relazione provengono dai file completi, separando violazioni geometriche,
160 connessioni non instradate e 68 segnalazioni di parità.
