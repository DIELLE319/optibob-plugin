#!/bin/bash
# Riporta nel repo KVR gli header del motore e il Source dagli originali di
# OptiBob Live, cosi' il plugin per KVR suona ESATTAMENTE come l'app. Da
# lanciare dopo ogni modifica al motore, prima di caricare su GitHub.
cd "$(dirname "$0")" || exit 1
A=".."
cp "$A/optibob-uno/Source/PluginProcessor.cpp" "$A/optibob-uno/Source/PluginEditor.cpp" \
   "$A/optibob-uno/Source/PluginEditor.h" "$A/optibob-uno/Source/Comandi.h" Source/
cp "$A/optibob-uno/Source/PluginProcessor.h" /tmp/pp.h
/usr/bin/sed -e 's|../../optibob-agc/Engine/Eq.h|../Engine/Eq.h|' \
             -e 's|../../optibob-agc/Engine/Agc.h|../Engine/Agc.h|' \
             -e 's|../../optibob-mb/Engine/Mb.h|../Engine/Mb.h|' \
             -e 's|../../optibob-max/Engine/Max.h|../Engine/Max.h|' /tmp/pp.h > Source/PluginProcessor.h
cp "$A/optibob-agc/Engine/Eq.h" "$A/optibob-agc/Engine/Agc.h" \
   "$A/optibob-mb/Engine/Mb.h" "$A/optibob-max/Engine/Max.h" Engine/
echo "sincronizzato dal motore di OptiBob Live."
