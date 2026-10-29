# Eight Echo (Studio Wiga)

Plugin VST3 (Windows / macOS, + AU sur Mac) reproduisant le fonctionnement d'un delay compact
11 modes de type DD-8 : mêmes plages, mêmes commandes, mêmes interactions.

## Compilation (GitHub)
Tous les fichiers sont à la racine. Pousser le dossier sur le dépôt : le workflow
`.github/workflows/build.yml` compile sous `windows-2022` (VST3) et macOS (VST3 + AU).
Le VST3 se récupère dans l'onglet **Actions → artefacts**, à copier dans
`C:\Program Files\Common Files\VST3`.

## Commandes (visuel Eight Echo)
| Élément | Rôle |
|---|---|
| E.LEVEL | Niveau d'effet. Vers 3 h = même niveau que le son direct. REVERSE au maximum : effet seul. LOOP : volume de la boucle |
| MIX | Équilibre direct/effet. **À midi = comme la DD-8** (direct + effet pleins). À gauche : le direct seul ; à droite : l'effet seul (« Direct Mute ») |
| FEEDBACK | Répétitions (auto-oscillation possible tout en haut). En GLT : profondeur |
| TIME | Temps (plage selon le mode). Tourner TIME annule le tempo tapé |
| MODE | 11 positions, étiquetées autour du potard : clic sur une étiquette, glisser ou molette |
| BYPASS (footswitch droit) | Le footswitch de la DD-8 : clic = on/off ; maintien 2 s = mode TEMPO ; en WARP/GLT maintien = effet ; en LOOP : rec → lecture → overdub, double appui = stop, maintien 2 s = effacer |
| TAP (footswitch gauche) | Tap tempo ; **maintenu = TWIST** ; en LOOP : appui = stop, maintien 2 s = effacer |
| LED gauche | Bat la mesure : calée sur la mesure du DAW quand SYNC est actif et que le DAW joue (1er temps plus fort) ; relancée exactement à chaque tap (1er tap = temps 1) ; sinon au temps de retard. Scintille pendant le Twist |
| LED droite | Le témoin CHECK de la DD-8 : rouge = effet actif, vert = mode tempo / lecture loop, orange = overdub, clignotements = enregistrement / tap |
| X2 | Temps doublé (sortie « Long » de la DD-8). S'illumine quand actif |
| LED 1 à 7 | Mémoires : clic = rappel, **maintien 1 s = sauvegarde** (la LED clignote pour confirmer). LED pleine = réglage identique à la mémoire, LED adoucie = mémoire modifiée |
| SYNC | Cale le delay sur le tempo du DAW (avec la division du tap). Bouton allumé bleu glace qui respire au tempo ; ligne d'état « SYNC120 » (« SYNC? » = pas de tempo reçu). Re-clic = retour à TIME / tap |
| △ / ▽ | Mémoire précédente / suivante |
| Ligne d'état (en haut) | Mémoire, mode, temps réel en ms, niveaux |

**Clic droit** (réglages cachés de la DD-8) : presets d'usine (chargés dans la mémoire en cours), mode stéréo
(Independent / Wide / Panning), division du tap, sauvegarde en mémoire 1-7, carryover, SYNC, affectation de la pédale d'expression
(paramètre EXP), taille de fenêtre (Default size / 60% / 75% / 100%, comme Textural et Freeze Shimmer). La fenêtre s'ouvre à sa taille par défaut (390 x 494) et ne peut pas être réduite en dessous.

Tout est automatisable / assignable en MIDI dans Reaper (footswitch, tap, twist compris).

## Plages de temps (ms)
| Mode | Normal | Long |
|---|---|---|
| ANALOG, STANDARD, TAPE, WARM, +RV, MOD, WARP | 20–800 | 40–1600 |
| REVERSE | 300–5000 | 600–10000 |
| SHIM | 200–800 | 400–1600 |
| GLT | 10–400 | 20–800 |
| Tap tempo | 67–10000 | |
| LOOP | 40 s | |

## Ce qui a été mesuré sur les vidéos (et appliqué)
- **TWIST** (vidéo officielle, TAPE/TWIST) : la hauteur monte d'environ 2,7 octaves en ~4 s,
  le feedback part en auto-oscillation, puis tout retombe au relâchement. Ajout d'une rotation stéréo.
- **Auto-oscillation TAPE** : le niveau plafonne (pas d'emballement) et le son s'éclaircit / se salit
  avec le temps → saturation + limiteur doux dans la boucle.
- **MOD** : LFO ≈ 1,5 Hz, ≈ ±15 cents (~1 ms), voies gauche/droite en opposition de phase.
- **TAPE** : modulation légère (quelques cents), wow lent + flutter.
- **GLT** : tranches répétées et hachées (voir v1.1).
- **+RV** : TIME agit aussi sur la durée de la reverb ; même au minimum de feedback il reste une répétition.
- **Wide stereo** : léger balancement gauche/droite (moins dur que le ping-pong).
- **ANALOG / TAPE** : tourner TIME fait glisser la hauteur ; les modes numériques changent de temps par fondu.

## Version 1.1 – ajustements d'après les manuels Boss et les tests
- **Couleur dès la 1re répétition** : WARM, MOD, WARP, +RV… sortaient brillants au premier écho, ils ont maintenant leur couleur tout de suite.
- **WARM** : « un STANDARD moins brillant », rond et moderne (Delay Dude, Bonedo) → moins sombre qu'avant.
- **TAPE** : percussif et pas « usé » (Bonedo) → moins de saturation, de souffle et de perte d'aigus. Modèle de base type RE-201 (famille Boss).
- **SHIM** : octave plus discrète, comme décrite par Bonedo ; architecture Boss (balance octave / son direct envoyée dans le delay + feedback).
- **GLT** : la même séquence courte boucle tant que le pied est maintenu, sa longueur suit TIME/tempo ; la profondeur (FEEDBACK) va d'un bégaiement doux à des coupures franches.
- **MOD** : modulation « bi-phase » (gauche/droite en opposition), comme le mode BI-PHASE des delays Boss.
- **TWIST** : montée puis retombée de la rotation (logique RISE → FALL des multi-effets Boss).
- **REVERSE au maximum** : effet seul au même niveau que l'entrée (avant, un peu plus fort).
- **Direct Mute** actif aussi en LOOP.

## Presets d'usine (clic droit → Factory presets, ou menu de presets du DAW)
Classiques : Clean Slapback, Rockabilly Tape, Doubler, Bread & Butter, Dotted 8th Arena, Ping Pong Eighths  
Vintage : BBD Crunch Lead, Dark BBD Wash, Floating Tape Solo, Long Tape Ambience  
Doux / modernes : Soft Ballad, Jazz Room, Lush Chorus Echo, 80s Stereo Lead  
Espace : Echo Hall, Cathedral, Heaven Swell, Octave Haze, Dream Swell (hold), Space Freeze (hold)  
À l'envers / glitch : Backwards Solo, Backwards Pad, Machine Gun (hold), Stutter Groove (hold), Granular Buzz (hold)  
Looper : Looper

« (hold) » = maintenir le footswitch BYPASS.

## Version 1.3 – visuel à l'identique
- Potards : ce sont ceux de l'image (moletage, reflet conique, ombre). Seul le repère blanc, découpé sur l'image, tourne : l'éclairage reste fixe comme sur une vraie pédale.
- LED 1 à 7 : tubes de lumière d'origine avec leur diffusion sur la plaque ; version éteinte en acrylique dépoli de même forme.
- LED rondes : lentilles d'origine allumées en bleu glace / rouge / vert / orange, avec la lumière qui déborde sur la bague et la plaque.
- Fond : motifs d'anneaux de la pédale en transparence autour d'elle.
- « Studio Wiga » en gris-noir, légèrement gravé, entre les deux footswitchs.

## Version 1.3.1
- LED CHECK (rouge / vert / orange) refaite comme la LED bleue : même profil de lumière, cœur pastel lumineux et même halo.

## Version 1.3.2
- Logo Studio Wiga (cercles orange/blanc + texte) a la place du texte, entre TAP et BYPASS.


## Version 1.3.4
- Lumiere autour des LED bleue et CHECK : halo rond qui s eteint en douceur (plus de bord carre).

## Version 1.4.0
- Logo Studio Wiga integre a la plaque (fixe, ne clignote plus quand on appuie sur un footswitch), detoure en gris-noir pour mieux ressortir.
- Potard MODE : cranté exactement sur chacun des 11 modes inscrits (le repere pointait a cote auparavant).
- X2 : c'est la serigraphie X2 elle-meme qui s'illumine (meme forme, meme taille).
- Looper : une boucle enregistree continue de tourner quand on choisit un autre mode ; on joue par-dessus avec cet effet, la boucle reste seche (elle ne passe jamais dans le delay). Son volume est celui regle avec E.LEVEL en mode LOOP. Pour l'arreter : revenir sur LOOP (double appui / maintien 2 s pour effacer). La ligne d'etat affiche « +LOOP » quand une boucle tourne sous l'effet.
- Ligne d'etat agrandie ; DC 9V remonte pour lui laisser la place.

## Version 1.4.1
- Preset « Runaway Tape (Twist) » supprime (26 presets d'usine).
- Presets « (hold) » et effets maintenus (WARP, GLT, TWIST) : niveau securise. Limiteur automatique : pendant le maintien, l'effet ne depasse jamais environ +2 dB du niveau d'avant l'appui. WARP monte en nappe sans emballement, GLT au niveau du delay. Presets hold avec E.LEVEL reduit.

## Version 1.4.2
- Redimensionnement : impossible de reduire la fenetre sous sa taille par defaut (380 x 493), que ce soit par le coin, par un bord, par l hote ou a la reouverture du projet. Le rapport largeur/hauteur reste fixe ; agrandissement jusqu a 200 %.

## Version 1.5.0
- Bouton SAVE remplacé par **SYNC** (sérigraphie refaite dans la police de la pédale) : un clic cale le delay sur le tempo du DAW. Le capuchon s'éclaire en bleu glace et respire au tempo.
- Mémoires : maintien 1 s sur une LED 1-7 = sauvegarde (ou clic droit › Save current settings to memory).
- LED de tempo vérifiée et corrigée : elle tournait sur une horloge libre, décalée des taps et des temps du DAW. Elle est maintenant calée sur la position du DAW (temps de la mesure, signature rythmique comprise, 1er temps accentué) et repart exactement sur chaque tap.
- Fenêtre et pédale calées sur Freeze Shimmer : 390 x 494 par défaut (minimum), boîtier haut de 465 px, tailles Default / 60% / 75% / 100% (2000 px de haut).
- Aspect : éclairage doux derrière la pédale, ombre portée sous le boîtier, vignettage, ligne d'état ombrée pour la lisibilité, motifs d'anneaux replacés.

## Version 1.5.1
- Inscription « DC 9V » retirée du haut de la plaque ; la ligne d'état remonte légèrement.

## Version 1.5.2
- Ligne d'état discrète : elle n'apparaît que lorsqu'une information change (potard, mode, mémoire, tap, SYNC, looper…), reste affichée environ 2 s puis s'efface en fondu.

## Version 1.5.3
- Correctif anti-gel de la fenêtre sous Windows (Reaper notamment) : l'interface est dessinée avec le moteur logiciel au lieu de Direct2D, qui pouvait figer la fenêtre avec certains pilotes graphiques.
- L'interface ne redessine plus que ce qui change réellement (LED, ligne d'état, SYNC) : aucun travail au repos.
- Redimensionnement : tolérance de 2 px pour éviter une boucle avec l'hôte quand Windows est à 125 / 150 %.

## Version 1.6.0
- v1.6.0 : anti-gel v2 (JUCE 8.0.15, caches refaits par le timer, chien de garde, aucun appel à l'hôte depuis le thread audio)

## Version 1.7.1
- v1.7.1 : LED des mémoires 1 à 7 et LED SYNC d'un bleu glacier plus marqué (seule la lumière est teintée, la plaque ne change pas)

## Version 1.7.0
- v1.7.0 : MIDI Learn sur toutes les commandes (clic droit sur une commande ou menu MIDI), assignations sauvegardées avec le projet
- CC par défaut : E.LEVEL 20, MIX 21, FEEDBACK 22, TIME 23, MODE 24, X2 25, SYNC 26, EXP 27, BYPASS 80, TAP 81, PREV MEMORY 82, NEXT MEMORY 83.
