# Sequences

Synthé VST3 de Phragma, construit sur l'architecture d'Alaskan Bites 3 (Alien Chaos), avec en plus un
**séquenceur de LFO** : une banque de LFO dont le séquenceur choisit, pas après pas, lequel joue.

## Onglet MAIN
- **WAVETABLE** : tables intégrées (CLASSIC, BASIC SHAPES, PWM, RESO SAW/TRI/TRAP, SAW-PULSE, HARMONICS,
  DBL SINE, SINE) ou **importées** (LOAD, menu IMPORT WAV…, raccourcis vers Serum 2 / Serum, ou un .wav
  glissé depuis le Finder). Tables Serum reconnues, table enregistrée dans le projet et les presets.
  POS, WARP (SYNC, BEND +, BEND -, MIRROR), OCT, SEMI (continu : il glisse), FINE, LEVEL,
  UNISON (1 à 8), DETUNE, SPREAD, PAN, PHASE, RND.
- **PHASE DISTORTION** (façon Casio CZ) : SAW, SQUARE, PULSE, DBL SINE, SAW-PULSE, RESO 1-3 ;
  PD AMT, SHAPE, CRUSH, FOLD, LEVEL, OCT, SEMI, FINE.
- **ALGORITHMIC** : le vrai code de Plaits (Mutable Instruments, licence MIT), ses 24 modèles ;
  HARM, TIMBRE, MORPH, DECAY / COLOUR (LPG), AUX, OCT, SEMI, FINE, LEVEL, PAN.
- **SUB + NOISE** : sub sinus / triangle / carré / scie (-1 ou -2 octaves), bruit blanc / rose / brun / digital.
- **FM MATRIX** : la ligne module la colonne (WT, PD, ALGO, NOISE vers WT, PD et ALGO), diagonale = feedback.
  L'ALGO (Plaits) reçoit sa FM par la hauteur, échantillon par échantillon (±48 demi-tons à 100 %).
  Clic = 0 ou 100 %, glisser = quantité, double-clic = 0. DEPTH, mode PHASE ou LINEAR.
  Un oscillateur à LEVEL 0 module quand même.
- **BITE!** : FOLD, CRUSH, SOFT, avant ou après le filtre (POST FILTER), ASYM.
- **FILTER** : LADDER ou SVF ; LP / BP / HP / NOTCH ; 12 ou 24 dB ; CUTOFF, RESO, KEY TRK, DRIVE, ENV AMT.
- 3 enveloppes (AMP, FILTER = ENV 2, MOD = ENV 3), **MAIN LFO** (synchro ou Hz, 7 formes, RETRIG,
  2 destinations directes), VOICE (POLY / MONO / LEGATO, GLIDE, BEND, VELO, VOLUME, qualité 1x / 2x / 4x).

## Modulation
- Sources : SEQ, LFO 1-6, CHAOS 1-2, ENV 2, ENV 3, VELO, MAIN LFO, MOD WHEEL.
- **On glisse une pastille de source sur un potard** (la barre des pastilles est visible dans tous les onglets).
  Tirer sur l'anneau (ou Alt + glisser) règle la profondeur ; clic droit : inverser, supprimer.
- Onglet **MATRIX** : les 24 routages, + ADD ROUTING, source / destination / profondeur modifiables.

## Onglet SEQ : le séquenceur de LFO
- LFO BANK : 6 LFO (1/64 à 8 mesures, triolets et pointés ; 8 formes ; STEP ; phase ; BI / UNI) et 2 CHAOS.
- SEQUENCER : 1 à 32 pas, chaque pas choisit quelle source sort de la pastille SEQ. DIV, swing, RESTART,
  XFADE, NOTE / HOST. RANDOM : nouveau motif ; RANDOM ALL : nouvelle banque + motif.

## Onglets ARP et FX
- ARP : UP, DOWN, UP/DOWN, DOWN/UP, RANDOM, AS PLAYED, CHORD ; 1/4 à 1/32 ; 1 à 4 octaves ; GATE, SWING, LATCH.
- FX : CHORUS -> DELAY (SYNC ou temps libre en ms, ping-pong, TONE) -> REVERB.
  Tous les potards des effets sont des destinations de modulation (les effets suivent la dernière note jouée
  et gardent sa modulation pendant les queues).
- RANDOM ne crée aucune modulation ; RANDOM ALL ne touche pas aux 2 CHAOS.

## Presets d'usine
INIT, HITECH ZAPS, PD ACID, PLAITS PLUCKS, FM GROWL, CHAOS BLIPS, SUB BASS, SUPERSAW, WOBBLE LFO.

## Compiler
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSEQUENCES_BUILD_TESTS=ON
cmake --build build --config Release --target Sequences_VST3
```

ThirdParty/plaits et ThirdParty/stmlib : © Emilie Gillet, licence MIT (voir ThirdParty/plaits/LICENSE).
