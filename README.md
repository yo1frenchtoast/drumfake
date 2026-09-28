# Drumf(re)ake

Groovebox open source, inspirée du prototype de la DrumFreak d'Arturia (produit non commercialisé
à ce jour). Projet indépendant, sans lien avec Arturia. Elle est construite sur les DSP open source
de Mutable Instruments (sous-module `third_party/eurorack`, non modifié) :

- **Plaits** : une voix par piste (8 pistes, 24 moteurs, dont grosse caisse, caisse claire et hi-hat
  analogiques ou synthétiques, modal, FM…) ;
- **Grids** : génération algorithmique de rythmes sur le pad X/Y (mode « Spice ») ;
- **Rings** : reverb et chorus du bus master.

## Construction

```sh
git submodule update --init --recursive
cmake -B build
cmake --build build -j
./build/drumfreake_tests   # tests du séquenceur, de Grids, de la sauvegarde et du rendu
./build/drumfreake         # application
```

Dépendances : SDL3 et nlohmann-json (Homebrew sur ce système). Dear ImGui est téléchargé par CMake.
`-DDRUMFREAKE_GUI=OFF` compile une version sans interface.

### Android

Prérequis : SDK Android avec le NDK 30.0.16248370 et CMake 4.1.2 (`sdkmanager`), Java récent.
SDL3 est compilé depuis le sous-module `third_party/SDL`.

```sh
cd android
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Au doigt, tout est multitouch : plusieurs pads à la fois, un pad et un potard, le pad X/Y, et
les combinaisons de boutons (*Last Step* ou *Copy* + pages, *Erase* + potard…).
Un appui long (0,5 s) remplace le clic droit (sélection d'un pas, édition d'un Chord sur les
pads 1–8) et un double tap sur un potard le remet à sa valeur par défaut.

L'APK (arm64-v8a, Android 8 minimum) s'ouvre en plein écran paysage. Les polices Noto Sans
(licence OFL) sont embarquées, le projet est sauvegardé dans le stockage privé de l'application.

Rendu hors ligne :

```sh
./build/drumfreake --render demo.wav --bars 4 [--grids] [--pattern N] [--project fichier.json]
```

## Contrôles

| Façade | Souris | Clavier |
|---|---|---|
| Pads A–H | clic = joue et sélectionne la piste (Maj = accent) | `Q W E R T Y U I` |
| Pads 1–8 | selon le mode : Chords, Sounds, Repeats, clavier | `A S D F G H J K` |
| Potards | glisser verticalement (Maj = fin), molette, double-clic = défaut | |
| Pas 1–16 | clic = on/off ; clic droit = sélectionner le pas (p-lock) | |
| Play / Rec | | `Espace` / `Entrée` |
| Pages Sound…Assign | | `1`–`6` |
| Pages de pas A–D | | `Z` / `X` |
| Last Step (maintenu) / Poly / Shift (maintenu) | | `L` / `P` / `Alt` |
| Save, Undo, Copy, Paste, Erase | | `Ctrl+S`, `Ctrl+Z`, `Ctrl+C`, `Ctrl+V`, `Suppr` (au relâchement) |
| Retour (↺) | ferme Edit/Browse, relâche le pas sélectionné | `Échap` |

- **Shift** (en bas à gauche, ou `Alt`) maintenu, comme sur la DrumBrute : les pas deviennent
  des options — 1 à 5 : division temporelle du pattern (1/8, 1/8T, 1/16, 1/16T, 1/32) ; 13 :
  métronome ; 14 : *At End* (changer de pattern à la fin ou tout de suite) ; 15 : les pas suivent
  la page jouée ; 16 : Poly. Shift + pad : sélectionner une piste sans la jouer.
- **Pas maintenu** (au doigt, 0,5 s ; clic droit à la souris pour le garder sélectionné) :
  tant qu'il est tenu, les potards posent des parameter locks, les pads 1–4 règlent ses
  répétitions (1 à 4 coups) et l'encodeur le décale en avance ou en retard (±50 % d'un pas, par
  pas de 5 %, trait ambré sous le pas). Relâché, on revient à l'édition normale.
- **Last Step** maintenu (ou `L`) : les pas montrent la longueur ; appuyer sur un pas fixe le
  dernier pas du pattern (de la piste sélectionnée en Poly), une page le fixe à 16/32/48/64.
- **Parameter locks** : clic droit sur un pas, puis tourner un potard (anneau orange = verrouillé ;
  double-clic pour retirer le lock). L'écran permet d'éditer vélocité, probabilité, ratchets et note.
- **Pages A|16 à D|64** : pendant la lecture, les pas suivent la page jouée par la piste
  sélectionnée ; choisir une page à la main fige l'affichage jusqu'au prochain Play.
  *Last Step* maintenu + une page = longueur du pattern (16, 32, 48 ou 64 pas).
  *Copy* maintenu + une page source + une page destination : par ex. A|16 puis D|64 répète les
  16 premiers pas (avec leurs locks, sur toutes les pistes) jusqu'au pas 64 et allonge le pattern ;
  une destination avant la source copie simplement la page. *Last Step* et *Copy* relâchés seuls
  gardent leur action habituelle.
- **Polyrythme** : en Poly, *Last Step* + un pas = dernier pas de la piste sélectionnée
  (re-appuyer sur le même pas = longueur du pattern). *Poly* (par pattern) : chaque piste boucle sur sa propre
  longueur sans se recaler à la fin du pattern ; tout se réaligne au Play et au changement de
  pattern. Sans Poly, les pistes courtes repartent avec le pattern.
- **Retour visuel sur les potards** : halo orange = la note en cours joue un lock sur ce paramètre ;
  arc + point = écart entre le potard et la valeur réellement jouée (orange pour un lock, bleu pour
  un LFO ou une macro) ; pastille orange = au moins un pas de la piste verrouille ce paramètre.
- **Effacer une automation** : maintenir *Erase* (ou `Suppr`) et toucher un potard efface les
  locks de ce paramètre sur tous les pas de la piste sélectionnée (pastilles rouges pendant le
  maintien ; *Undo* pour revenir). Relâché seul, *Erase* garde son action habituelle.
- **Motion recording** : Rec + Play, puis tourner un potard : le lock s'écrit sur le pas en cours.
- **Pad X/Y** : *Spice* = position dans la carte Grids (recliquer sur Spice pour activer/désactiver ;
  pistes A–C, densités et chaos sur la page Assign) ; *FX* = filtre DJ (X) et envoi delay (Y) ;
  *Looper* = boucle du séquenceur tant que le pad est touché (Y = longueur) ; *Macros* = paramètres
  assignés sur la page Assign.
- **Pads 1–8** : *Chords* joue des combinaisons de pistes (clic droit sur un pad pour l'éditer) ;
  *Sounds* charge un son d'usine (re-cliquer Sounds = banque 2) ; *Repeats* répète les pistes tenues
  à la division choisie ; *Keys* joue la piste sélectionnée sur la gamme choisie et écrit la note
  du pas sélectionné.
- **Mode Keys** : tonique, gamme (majeure, mineure, modes, mineure harmonique, pentatoniques,
  blues, chromatique) et octave, communes à tout le projet. Le bouton *Keys* affiche une page
  « Keys » (potards 1–3 = tonique, gamme, octave ; re-cliquer Keys pour revenir aux pages de son) ;
  l'écran montre les réglages et les notes des pads, et l'encodeur modifie le champ sélectionné.
  Raccourcis : `←`/`→` octave, `↑`/`↓` tonique, `Tab`/`Maj+Tab` gamme. Les notes s'ajoutent au
  Pitch de la piste : une piste réglée sur C joue dans la tonalité choisie.
- **Encodeur** : change le moteur Plaits de la piste (ou le son dans Browse).
- **Song** (chaîne de patterns, comme sur les Arturia) : appuyer sur *Song* (il clignote), puis
  sur les boutons de pas 1–16 dans l'ordre voulu ; un pattern peut revenir plusieurs fois
  (ex. 1-1-2-3), jusqu'à 64 entrées. Le premier appui démarre une nouvelle chaîne ; *Undo* retire
  la dernière entrée, *Erase* vide la chaîne. Ré-appuyer sur *Song* lance la chaîne en boucle
  (depuis le début au Play ; en cours de lecture, après le pattern en cours). Choisir un pattern
  en mode *Patterns* quitte le mode Song. La chaîne est sauvegardée avec le projet.
- **Kits** : chaque pattern a ses propres sons, chargés avec lui (y compris en mode Song) ; les
  réglages faits sur les potards restent dans le pattern courant. *Copy*/*Paste* d'un pattern
  copie aussi son kit.
- **Panel** (comme sur la MatrixBrute) : le son en cours reste en place quand on change de
  pattern, et ses réglages ne sont enregistrés nulle part tant qu'on ne fait pas *Edit* →
  *Store kit* (qui l'écrit dans le pattern courant). Éteindre Panel recharge le kit du pattern
  courant. L'écran affiche « PANEL » quand il est actif.
- **Edit** : longueur du pattern, swing, longueur par piste (polymétrie), BPM, lecture du Song,
  métronome, « Bake Grids » (écrit le rythme Grids dans A–C).
- **Mute** / **Solo** (maintenus + pads, ou tapés pour verrouiller le mode) : coupent ou isolent
  des pistes ; *Shift* + Mute/Solo efface tous les mutes/solos. Pour sélectionner une piste sans
  la jouer : *Shift* + pad.
- **Maintiens avec les pads** (comme sur la DrumBrute) : *Erase* + pad efface la piste à
  l'arrêt, ou pendant la lecture efface ses coups au passage de la tête tant que le pad est tenu ;
  *Copy* + pad + pad copie la séquence d'une piste sur une autre.
- **Repeats en enregistrement** : les répétitions s'écrivent dans le pattern (une par pas, ou en
  ratchets si elles sont plus rapides que les pas), comme le Roller de la DrumBrute.
- **MIDI en entrée** (PC : tous les ports ALSA, branchés à chaud, plus un port virtuel
  « Drumf(re)ake » ; Android : périphériques USB et applications MIDI) :
  - un instrument = un canal : la piste A écoute le canal 1, B le canal 2… H le canal 8, et joue
    les notes reçues à leur hauteur, avec la vélocité (elles s'enregistrent en Rec) ;
  - à défaut (canaux affectés à aucune piste, ex. canal 10), les 8 notes de la gamme majeure à
    partir de la note *Trig* (C2 par défaut : touches blanches C2 à C3) déclenchent A–H à leur
    propre hauteur ;
  - réglages dans *Edit* → *MIDI settings* : canal de chaque piste (off possible), note *Trig*,
    *Follow MIDI clock* (tempo, Start/Stop/Continue et Song Position de l'horloge MIDI, 24 PPQN ;
    l'écran affiche alors « EXT » avec le tempo mesuré) ;
  - le témoin « MIDI » de l'écran clignote à chaque message ; `--midi-monitor` affiche les
    messages reçus dans le terminal.
- La sauvegarde est dans `~/.local/share/drumfreake/project.json`, rechargée au démarrage.

## Architecture

```
src/core/    modèle (paramètres, pas, patterns, sons d'usine), JSON, portage de Grids
src/audio/   TrackVoice (Plaits + filtre + drive + crush + choke), MasterFx, Engine (séquenceur)
src/ui/      widgets ImGui et façade
shims/       en-têtes AVR minimaux pour compiler les tables de grids/resources.cc
```

L'audio est rendu par blocs de 12 échantillons à 48 kHz ; le séquenceur est précis au bloc près
(0,25 ms). L'interface modifie le modèle sous le verrou du moteur ; lecture, niveaux et oscilloscope
passent par des atomiques.

## Limites

- Le sampler (bouton Sample) n'est pas émulé.
- Les fonctions exactes de la DrumFreak ne sont pas publiques : la façade reprend la disposition
  de la photo du prototype, et le comportement de chaque bouton est une interprétation.

## Licence

Drumf(re)ake est distribué sous licence GNU GPL version 3 (voir `LICENSE`).

Code tiers utilisé :
- Plaits, Rings et stmlib (Émilie Gillet) : licence MIT ;
- les tables de Grids (`grids/resources.cc`) : GPLv3.
