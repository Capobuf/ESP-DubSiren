# Meccanica e assemblaggio V1

## Quote

- Hammond 29830PSLA esterno: 120.80 × 95.50 × 33.80 mm.
- Vano corpo: 104.71 × 79.41 mm.
- Zona lid: 102.83 × 77.13 mm.
- Profondità interna corpo: circa 29.7–30.0 mm.
- PCB: 100 × 74 mm, R6, FR4 1.6 mm.

## Coordinate PCB

Origine nell’angolo alto sinistro; X a destra, Y verso il bordo posteriore.

| Ref | X,Y mm | Lato |
|---|---:|---|
| POT1 | 20.00, 14.00 | top / B |
| POT2 | 49.00, 14.00 | top / B |
| POT3 | 78.00, 14.00 | top / B |
| POT4 | 20.00, 36.00 | top / B |
| POT5 | 49.00, 36.00 | top / B |
| ENC1 | 68.50, 33.50 | top / B |
| BTN1 | 14.00, 52.00 | top / B |
| BTN2 | 37.33, 52.00 | top / B |
| BTN3 | 60.67, 52.00 | top / B |
| BTN4 | 84.00, 52.00 | top / B |
| MASTER | 26.00, 66.00 | rear / F |
| USB-C | 45.00, 70.50 | rear / F |
| RCA assembly | 72.00, 66.00 | rear / F |

Fori M3: H1 (5,5), H2 (95,31), H3 (5,69), H4 (95,69).

## Tavole pannello

I file SVG/DXF/PDF in [mechanical](mechanical) sono 1:1. Le X derivano dal PCB centrato nel box; le aperture restano provvisorie.

- top: POT Ø9.2 mm, encoder Ø7.5 mm, pulsanti Ø7.2 mm;
- rear: MASTER Ø9.2 mm, USB-C 10.2 × 4.2 mm, RCA Ø8.5 mm, vite Ø3.2 mm;
- quota Z rear provvisoria: 17.0 mm.

Non forare il box senza misurare i componenti acquistati.

## Verifica

1. Montare a secco RV1–RV5, SW1–SW4 e SW7 sul lato B.
2. Misurare parete-top → PCB e code terminali.
3. Montare U6 con l’header scelto e misurare il punto più basso.
4. Controllare RV5/U6, RV3/U1, SW4/U5 e antenna WROOM.
5. Verificare raggi, bossoli e chiusura lid con la PCB centrata.
6. Forare un pannello campione prima del box definitivo.
7. Verificare accesso RESET/BOOT e ritenzione USB-C/RCA.

I WRL in 3dmodels/generated sono involucri dimensionali provvisori.
