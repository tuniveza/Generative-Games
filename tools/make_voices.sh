#!/bin/sh
# Renders every line in assets/dialogue.txt to assets/voices/*.wav with Piper TTS.
#   everyday lines:        <npc>_<n>.wav   (n counts that speaker's untagged lines)
#   quest lines "npc:tag": <npc>_<tag>.wav
# Usage: tools/make_voices.sh /path/to/piper_dir   (containing piper/piper and the .onnx voices)
#   voices: en_GB-cori-high (public domain data), en_GB-vctk-medium (CC BY 4.0 data)
#   a voice written "p260~slow" is slowed and deepened, "p277~child" pitched up (needs sox)
set -e
P=${1:?path to piper directory}
mkdir -p assets/voices
grep -v '^#' assets/dialogue.txt | awk -F'|' 'NF>=4' | awk -F'|' '
  { split($1, a, ":"); id = a[1]; tag = a[2];
    if (tag == "") { n[id]++; name = id "_" (n[id] - 1) } else name = id "_" tag;
    printf "%s\t%s\t%s\n", name, $2, $4 }' |
while IFS="$(printf '\t')" read -r name voice text; do
    out="assets/voices/${name}.wav"
    [ -f "$out" ] && continue
    fx=${voice#*~}
    [ "$fx" = "$voice" ] && fx=""
    voice=${voice%%~*}
    speed=1.08
    [ "$fx" = slow ] && speed=1.3
    tmp="$out.tmp.wav"
    if [ "$voice" = cori ]; then
        echo "$text" | "$P/piper/piper" --model "$P/en_GB-cori-high.onnx" --length_scale 1.05 --output_file "$tmp" 2>/dev/null
    else
        id=$(python3 -c "import json;print(json.load(open('$P/en_GB-vctk-medium.onnx.json'))['speaker_id_map']['$voice'])")
        echo "$text" | "$P/piper/piper" --model "$P/en_GB-vctk-medium.onnx" --speaker "$id" --length_scale $speed --output_file "$tmp" 2>/dev/null
    fi
    case "$fx" in
        slow) sox "$tmp" "$out" pitch -350 reverb 45 50 60 ;;
        child) sox "$tmp" "$out" pitch 320 tempo 1.05 ;;
        *) mv "$tmp" "$out" ;;
    esac
    rm -f "$tmp"
    echo "$out"
done
