# Sequences

Synthé VST3 de Phragma, dans l'esprit d'Alaskan Bites (Alien Chaos) : FM + oscillateur analogique
avec hard sync, polyphonique (8 voix) ou mono avec glide, filtre résonant, distorsion,
et un **séquenceur de LFO**.

## Le séquenceur de LFO
- **LFO BANK** : 6 LFO (période synchro 1/64 à 8 mesures, triolets et pointés ; 8 formes dont
  EXP DOWN pour les « pew » ; STEP = LFO découpé en marches ; phase ; BI / UNI) et 2 CHAOS
  (aléatoire en marches ou lissé).
- **SEQUENCER** : 1 à 32 pas. Chaque pas choisit **quelle source joue** (matrice : une ligne par
  source). DIV 1/1 à 1/64, triolet, pointé, swing. RESTART : chaque pas fait repartir sa source
  de sa phase. XFADE : fondu entre deux sources. Éteint, c'est LFO SWITCH qui choisit la source.
- **MOD 1 à 4** : source (SEQ ou une source de la banque) -> destination (CUTOFF, RESO, FM INDEX,
  FM RATIO, PITCH ±24 demi-tons, PW, DRIVE, AMP, FM LEVEL, OSC LEVEL, NOISE, PAN, FEEDBACK,
  SYNC, TIMBRE), profondeur ±.
- **RANDOM** tire un nouveau motif parmi les sources audibles ; **RANDOM ALL** tire aussi la banque.
- NOTE : le temps part de chaque note (pas au legato). HOST : il suit le morceau.
- LFO CIRCUIT affiche la courbe qui sort du séquenceur sur une boucle.

## Compiler
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSEQUENCES_BUILD_TESTS=ON
cmake --build build --config Release --target Sequences_VST3
```
