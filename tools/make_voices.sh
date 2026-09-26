#!/bin/sh
# Renders every line in assets/dialogue.txt to assets/voices/<npc>_<n>.wav with Piper TTS.
# Usage: tools/make_voices.sh /path/to/piper_dir   (containing piper/piper and the .onnx voices)
#   voices: en_GB-cori-high (public domain data), en_GB-vctk-medium (CC BY 4.0 data)
set -e
P=${1:?path to piper directory}
mkdir -p assets/voices
grep -v '^#' assets/dialogue.txt | awk -F'|' 'NF>=4' | awk -F'|' '
  { n[$1]++; printf "%s\t%s\t%d\t%s\n", $1, $2, n[$1]-1, $4 }' |
while IFS="$(printf '\t')" read -r npc voice idx text; do
    out="assets/voices/${npc}_${idx}.wav"
    [ -f "$out" ] && continue
    if [ "$voice" = cori ]; then
        echo "$text" | "$P/piper/piper" --model "$P/en_GB-cori-high.onnx" --length_scale 1.05 --output_file "$out" 2>/dev/null
    else
        id=$(python3 -c "import json;print(json.load(open('$P/en_GB-vctk-medium.onnx.json'))['speaker_id_map']['$voice'])")
        echo "$text" | "$P/piper/piper" --model "$P/en_GB-vctk-medium.onnx" --speaker "$id" --length_scale 1.08 --output_file "$out" 2>/dev/null
    fi
    echo "$out"
done
