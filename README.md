# Sequences

Synthé VST3 de Phragma : FM + oscillateur analogique, polyphonique (8 voix) ou mono avec glide,
filtre résonant, distorsion, et **deux séquenceurs de LFO** pensés pour les séquences hi-tech.

## Le séquenceur de LFO
Une rangée de 1 à 32 pas. Chaque pas a une valeur (on la dessine à la souris) et un glide
(la valeur glisse vers celle du pas suivant). Le séquenceur avance au tempo de l'hôte
(1/1 à 1/64, triolet, pointé, swing) et envoie sa valeur vers 3 destinations au choix,
chacune avec sa profondeur ± : CUTOFF, RESO, FM INDEX, FM RATIO, PITCH (demi-tons), FINE,
PW, DRIVE, AMP, FM LEVEL, OSC LEVEL, NOISE, PAN, FEEDBACK.

- NOTE : le motif repart du premier pas à chaque note (pas au legato en mono).
- HOST : le motif suit la position du morceau.
- RANDOM : un motif aléatoire « musical » (niveaux, marche, rampes, accents, miroir, chaos).
- ◀ ▶ : décale le motif d'un pas. CLEAR : tout à zéro. Double-clic sur un pas : zéro.

## Compiler
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSEQUENCES_BUILD_TESTS=ON
cmake --build build --config Release --target Sequences_VST3
```
