# Sequences

Synthé VST3 de Phragma, dans l'esprit d'Alaskan Bites (Alien Chaos) : 2 oscillateurs wavetable,
filtre, distorsion, polyphonique (8 voix) ou mono avec glide, et un **séquenceur de LFO**.

## Oscillateurs A et B
- Wavetables intégrées (BASIC SHAPES, PWM, RESO SAW/TRI/TRAP, SAW-PULSE, HARMONICS, DBL SINE, SINE)
  ou **importées** : menu IMPORT WAV…, raccourcis vers les dossiers de Serum 2 / Serum, ou un fichier
  .wav glissé depuis le Finder sur l'affichage de la table. ◀ ▶ : table suivante du même dossier.
  Les tables Serum (bloc « clm », frames de 2048 ou 1024) et les WAV ordinaires sont reconnus.
  La table importée est enregistrée dans le projet et dans les presets.
- LEVEL, POS (position dans la table), OCT (octaves), COARSE (±48 demi-tons, continu : il glisse),
  FINE, et WARP : SYNC, FM / AM / RM depuis l'autre oscillateur (A est modulé par B, B par A).
  Un oscillateur coupé (LEVEL à 0) module quand même.
- Calcul suréchantillonné x2 et tables à bande limitée : pas de repliement audible.

## Modulation
- Sources : LFO 1–6, CHAOS 1–2, SEQ (le séquenceur), ENV 2, VELO.
- **On glisse une pastille de source sur un potard** : l'anneau de couleur montre la profondeur.
  Tirer sur l'anneau (ou Alt + glisser) règle la profondeur ; clic droit : inverser, supprimer.
- Liste MODULATION : tous les routages (16 au plus), source / destination / profondeur modifiables.

## Le séquenceur de LFO
- LFO BANK : 6 LFO (période 1/64 à 8 mesures, triolets et pointés ; 8 formes ; STEP = LFO en marches ;
  phase ; BI / UNI) et 2 CHAOS (aléatoire en marches ou lissé).
- SEQUENCER : 1 à 32 pas, chaque pas choisit **quelle source joue** (matrice). DIV, swing, RESTART,
  XFADE, NOTE / HOST. RANDOM : nouveau motif ; RANDOM ALL : nouvelle banque + motif.

## Compiler
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSEQUENCES_BUILD_TESTS=ON
cmake --build build --config Release --target Sequences_VST3
```
