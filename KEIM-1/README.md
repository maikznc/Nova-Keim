# NOVA KEIM-1

Ein Synthesizer für Bässe, Leads, Pads und Plucks mit Morph-Pad, Effekten und Arpeggiator.
Als **VST3**, **Audio Unit** und **Standalone-App** für macOS (Apple Silicon und Intel).

Der Klangkern in `Source/dsp/` ist reines C++ ohne JUCE. Den gleichen Code bringen wir später auf den Daisy Seed.

---

## Schnellstart: GitHub baut das Plugin für dich

Du brauchst dafür nichts zu installieren. GitHub kompiliert auf einem Mac in der Cloud.

**1. Repository anlegen.** Auf github.com: oben rechts **+** > *New repository*, Name zum Beispiel `keim-1`, privat geht auch. Ohne README anlegen.

**2. Ordner hochladen.** Am einfachsten im Terminal (im KEIM-1-Ordner):

```bash
cd ~/Downloads/KEIM-1
git init
git add .
git commit -m "KEIM-1 erste Version"
git branch -M main
git remote add origin https://github.com/DEIN-NAME/keim-1.git
git push -u origin main
```

> Lädst du stattdessen über die Webseite hoch, fehlt oft der versteckte Ordner `.github`, und dann baut GitHub nichts. Im Finder machst du ihn mit **Cmd + Shift + .** sichtbar. GitHub Desktop nimmt ihn automatisch mit.

**3. Warten (ca. 10 Minuten).** Im Repository auf **Actions** klicken. Der Lauf *macOS-Build* wird grün, sobald alles gebaut und geprüft ist.

**4. Herunterladen.** Den Lauf öffnen, unten bei **Artifacts** auf *KEIM-1-macOS* klicken. Entpacken (GitHub packt die Zip noch einmal ein, also zweimal entpacken).

**5. Installieren.** Terminal öffnen und eintippen:

```bash
bash ~/Downloads/KEIM-1/Installieren.command
```

Das Skript entfernt die Download-Sperre von macOS und kopiert VST3 und AU nach `~/Library/Audio/Plug-Ins`.

**6. In Ableton testen.** *Einstellungen > Plug-ins*: die Schalter für den VST3-Systemordner und für Audio Units einschalten, dann *Neu scannen*. KEIM-1 findest du im Browser unter *Plug-ins > NOVA*.

### Eine feste Version veröffentlichen

```bash
git tag v0.1.0
git push --tags
```

Dann legt GitHub zusätzlich ein **Release** mit einer einzelnen Zip-Datei an (ohne doppeltes Entpacken).

---

## Selbst auf dem Mac bauen

Voraussetzungen: Xcode-Befehlszeilenwerkzeuge (`xcode-select --install`) und CMake (`brew install cmake`).

```bash
bash scripts/build-macos.sh
```

Das Skript lädt JUCE, baut alles, prüft die Sounds und legt VST3 und AU direkt in `~/Library/Audio/Plug-Ins`. Selbst gebaute Plugins blockiert macOS nicht.

---

## Testen

| Was | Wie |
|---|---|
| Ohne DAW ausprobieren | `KEIM-1.app` starten, unter *Options > Audio/MIDI Settings* Ausgang und MIDI-Keyboard wählen. Die Bildschirmtastatur spielt auch mit der Maus. |
| Audio Unit prüfen | `auval -v aumu Kei1 Nova` im Terminal, am Ende muss **AU VALIDATION SUCCEEDED** stehen |
| Klang ohne Plugin prüfen | `build/keim_render build/render` rendert alle Werkssounds als WAV und meldet Fehler |

GitHub führt den Klangtest, `auval` und [pluginval](https://github.com/Tracktion/pluginval) bei jedem Push automatisch aus.

---

## Was drin ist

- **Morph-Pad:** vier komplette Sounds in den Ecken, Ziehen blendet zwischen ihnen. Jede Ecke lässt sich neu belegen, auch mit dem aktuellen Sound.
- **Oszillatoren:** zwei Oszillatoren (Säge, Rechteck, Dreieck, Sinus) mit wenig Aliasing, dazu Sub und Rauschen, Detune und Stereobreite
- **Filter:** 24-dB-Tiefpass, Hochpass und Bandpass mit eigener Hüllkurve und Keytracking
- **Spielweise:** 8 Stimmen polyfon, Mono und Legato mit Glide
- **LFO** auf Filter und Tonhöhe, das Modrad gibt Vibrato
- **Effekte:** Drive, Chorus, Ping-Pong-Delay im DAW-Tempo, Hall
- **Arpeggiator:** 4 Laufrichtungen, 1 bis 3 Oktaven, Halten, Mutation (zufällige Pausen, Oktavsprünge und Doppelschläge), läuft im Tempo der DAW
- **9 Werkssounds** im Preset-Menü der DAW, dazu *Würfeln* für Zufallssounds
- Alle Regler lassen sich automatisieren und werden mit dem Song gespeichert. Pitchbend (±2 Halbtöne) und Sustain-Pedal funktionieren.

---

## Aufbau

```
KEIM-1/
├── CMakeLists.txt              Build-Beschreibung (lädt JUCE automatisch)
├── Source/
│   ├── dsp/                    Klangerzeugung, reines C++ ohne JUCE (auch für den Daisy Seed)
│   │   ├── Params.h            alle Parameter, Bereiche, Anzeigeformate
│   │   ├── Presets.h           Werkssounds, Morph, Würfeln
│   │   └── Engine.h            Oszillatoren, Filter, Hüllkurven, Stimmen, Arp, Effekte
│   ├── PluginProcessor.*       Verbindung zur DAW: Parameter, MIDI, Speichern
│   └── PluginEditor.*          Oberfläche
├── tools/keim_render.cpp       Klangtest: rendert alle Sounds als WAV
├── scripts/                    Build- und Installationsskripte
├── web/index.html              der Browser-Prototyp
└── .github/workflows/          automatischer Mac-Build bei GitHub
```

---

## Gut zu wissen

- **Nicht signiert:** Für die eigene Nutzung reicht die Ad-hoc-Signatur. Willst du KEIM-1 weitergeben oder verkaufen, brauchst du ein Apple-Entwicklerkonto (Signieren und Notarisieren).
- **Lizenz:** KEIM-1 nutzt [JUCE](https://juce.com). Für private Projekte und Open Source passt die freie AGPLv3-Lizenz von JUCE. Für einen Verkauf solltest du dir die Lizenzbedingungen von JUCE und Steinberg (VST3) genau ansehen.
- **JUCE-Version:** steht in `CMakeLists.txt` (`KEIM_JUCE_TAG`).
