#!/bin/bash
# Installiert NOVA KEIM-1 (VST3, Audio Unit, Standalone-App) für den aktuellen Benutzer.
# Aufruf im Terminal:  bash Installieren.command
set -e
cd "$(dirname "$0")"

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
APP_DIR="$HOME/Applications"

for f in KEIM-1.vst3 KEIM-1.component; do
  if [ ! -d "$f" ]; then
    echo "Fehler: $f fehlt. Starte das Skript im entpackten KEIM-1-Ordner."
    exit 1
  fi
done

echo "Entferne die Download-Sperre von macOS ..."
xattr -dr com.apple.quarantine KEIM-1.vst3 KEIM-1.component KEIM-1.app 2>/dev/null || true

mkdir -p "$VST3_DIR" "$AU_DIR" "$APP_DIR"
rm -rf "$VST3_DIR/KEIM-1.vst3" "$AU_DIR/KEIM-1.component"
cp -R KEIM-1.vst3 "$VST3_DIR/"
cp -R KEIM-1.component "$AU_DIR/"
echo "VST3        -> $VST3_DIR/KEIM-1.vst3"
echo "Audio Unit  -> $AU_DIR/KEIM-1.component"

if [ -d KEIM-1.app ]; then
  rm -rf "$APP_DIR/KEIM-1.app"
  cp -R KEIM-1.app "$APP_DIR/"
  echo "App         -> $APP_DIR/KEIM-1.app"
fi

# Audio-Unit-Liste von macOS neu einlesen lassen
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "Prüfe die Audio Unit ..."
if auval -v aumu Kei1 Nova >/tmp/keim1-auval.txt 2>&1; then
  echo "Audio Unit: in Ordnung."
else
  echo "Audio Unit: auval meldet ein Problem, Details in /tmp/keim1-auval.txt"
fi

echo ""
echo "Fertig. In Ableton: Einstellungen > Plug-ins > 'Neu scannen'."
